#!/usr/bin/env python3
"""
独立评测裁判驱动器 (Standalone Evaluation Harness & Policy Evaluator)

将“容错优化策略 (Policy)”与“实验评测裁判 (Referee)”彻底解耦：
  - 输入任意策略配置: (mode, rank_method, budget, delta, w1, w2, w3)
  - 步骤 1 (策略编译): 调用 libAdaptiveFTPass.so 对基准 SSA IR 执行容错变换 -> sut.ll
  - 步骤 2 (硬件 PMU 评测): 直接链接 libharness_rt.a，通过 Linux perf_event_open 硬件计数器
                            测量真实 CPU 退休指令数、CPU 周期数、执行时间与输出质量 (MRE/PSNR)
  - 步骤 3 (独立后置故障注入评测): 在 sut.ll 之上运行独立的 libFaultInjectorPass.so，
                                   按 LLFI/SASSIFI/SC'19 标准执行 One-Fault-Per-Run 全位宽均匀采样，
                                   输出 [Masked%, Detected_Recovered%, Benign%, SDC%, DUE%] 与 MWTF 综合评分
"""

import argparse
import csv
import math
import os
import subprocess
import sys
from pathlib import Path

ROOT_DIR = Path("/home/users/wangtianyu15/wtyWorkspace/aerospace-adaptive-ft")
BUILD_DIR = ROOT_DIR / "build"
RES_DIR = ROOT_DIR / "results"
GOLDEN_DIR = RES_DIR / "golden"

PASS_POLICY_SO = BUILD_DIR / "libAdaptiveFTPass.so"
PASS_FI_SO = BUILD_DIR / "libFaultInjectorPass.so"
LIB_ATFT_RT = BUILD_DIR / "libatft_rt.a"
LIB_HARNESS_RT = BUILD_DIR / "libharness_rt.a"

CFLAGS = ["-O2", "-fno-unroll-loops", "-fno-vectorize", "-fno-slp-vectorize"]


def run_cmd(cmd, cwd=ROOT_DIR):
    res = subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if res.returncode != 0:
        raise RuntimeError(f"Command failed ({res.returncode}): {' '.join(map(str, cmd))}\n{res.stderr}")
    return res.stdout


def build_infrastructure():
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    RES_DIR.mkdir(parents=True, exist_ok=True)
    GOLDEN_DIR.mkdir(parents=True, exist_ok=True)

    run_cmd(["cmake", "..", "-DCMAKE_C_COMPILER=clang-15", "-DCMAKE_CXX_COMPILER=clang++-15"], cwd=BUILD_DIR)
    run_cmd(["make", "-j4"], cwd=BUILD_DIR)

    # 生成未变换的规范化 SSA 基准 IR
    raw_ll = BUILD_DIR / "benchmarks_raw.ll"
    ssa_ll = BUILD_DIR / "benchmarks_ssa.ll"
    run_cmd([
        "clang-15", "-O1", "-Xclang", "-disable-llvm-passes",
        "-S", "-emit-llvm", "benchmarks/benchmarks.c",
        "-Iinclude", "-o", str(raw_ll)
    ])
    run_cmd([
        "opt-15", "-passes=mem2reg,loop-simplify",
        "-S", str(raw_ll), "-o", str(ssa_ll)
    ])
    return ssa_ll


def compile_and_evaluate_policy(ssa_ll, tag, mode, rank="semantic", budget=1.0,
                                delta=5e-5, w1=2.5, w2=1.8, w3=0.8,
                                fi_trials=1000, dump_golden=False, run_fi=True):
    """
    给定任意容错策略参数，自动编译并通过独立 Harness 执行硬件 PMU 测量与 One-Fault-Per-Run 故障注入
    """
    sut_ll = BUILD_DIR / f"sut_{tag}.ll"
    sut_bin = BUILD_DIR / f"bin_{tag}"

    # 1. 运行被测策略 Pass (Policy Layer)
    opt_cmd = [
        "opt-15", f"-load-pass-plugin={PASS_POLICY_SO}",
        "-passes=adaptive-ft,verify",
        f"-atft-mode={mode}",
        f"-atft-rank={rank}",
        f"-atft-budget={budget}",
        f"-atft-delta={delta}",
        f"-atft-w1={w1}", f"-atft-w2={w2}", f"-atft-w3={w3}",
        "-S", str(ssa_ll), "-o", str(sut_ll)
    ]
    if tag == "native":
        opt_cmd.append(f"-atft-report={RES_DIR / 'criticality_analysis.csv'}")
    run_cmd(opt_cmd)

    # 2. 编译无故障注入的被测二进制，用硬件 PMU 测量真实性能与 QoR
    run_cmd([
        "clang-15", *CFLAGS, "experiments/eval_suite.c", str(sut_ll),
        str(LIB_ATFT_RT), str(LIB_HARNESS_RT),
        "-Iinclude", "-Iharness", "-Ibenchmarks", "-lm", "-o", str(sut_bin)
    ])

    if dump_golden:
        run_cmd([str(sut_bin), "--dump-golden", str(GOLDEN_DIR)])

    pmu_out = run_cmd([str(sut_bin), "--pmu-perf", tag, str(GOLDEN_DIR)])
    pmu_rows = []
    for line in pmu_out.strip().splitlines():
        if line.startswith("PMU_CSV,"):
            parts = line.split(",")
            pmu_rows.append({
                "policy": parts[1],
                "benchmark": parts[2],
                "exec_time_us": float(parts[3]),
                "hw_insts": int(parts[4]),
                "hw_cycles": int(parts[5]),
                "rel_l2_err": float(parts[6]),
                "mean_rel_err": float(parts[7]),
                "max_abs_err": float(parts[8]),
                "psnr_db": float(parts[9]),
                "votes_per_iter": int(parts[10]),
                "false_alarms": int(parts[11]),
            })

    fi_rows = []
    if run_fi and fi_trials > 0:
        # 3. 在 sut.ll 之上叠加独立的后置故障注入裁判 Pass (libFaultInjectorPass.so)
        sut_fi_ll = BUILD_DIR / f"sut_{tag}_fi.ll"
        sut_fi_bin = BUILD_DIR / f"bin_{tag}_fi"
        run_cmd([
            "opt-15", f"-load-pass-plugin={PASS_FI_SO}",
            "-passes=standalone-fi,verify",
            "-S", str(sut_ll), "-o", str(sut_fi_ll)
        ])
        run_cmd([
            "clang-15", *CFLAGS, "experiments/eval_suite.c", str(sut_fi_ll),
            str(LIB_ATFT_RT), str(LIB_HARNESS_RT),
            "-Iinclude", "-Iharness", "-Ibenchmarks", "-lm", "-o", str(sut_fi_bin)
        ])
        fi_out = run_cmd([str(sut_fi_bin), "--fi-one", tag, str(GOLDEN_DIR), str(fi_trials)])
        for line in fi_out.strip().splitlines():
            if line.startswith("FI5_CSV,"):
                parts = line.split(",")
                fi_rows.append({
                    "policy": parts[1],
                    "benchmark": parts[2],
                    "dyn_target_insts": int(parts[3]),
                    "trials": int(parts[4]),
                    "masked_pct": float(parts[5]),
                    "det_recovered_pct": float(parts[6]),
                    "benign_pct": float(parts[7]),
                    "sdc_pct": float(parts[8]),
                    "due_pct": float(parts[9]),
                    "avg_valid_rel_err": float(parts[10]),
                })

    return pmu_rows, fi_rows


def run_all_research_questions():
    print("=== [Step 0] Building Decoupled Policy Pass & Standalone Evaluation Harness ===")
    ssa_ll = build_infrastructure()

    # =========================================================================
    # RQ1: 核心容错方案对比 (Native vs Exact-TMR vs Approx-Scalar vs Approx-SIMD vs Adaptive)
    #      + 离线近似固有误差表征 (Pure-Approx)
    # =========================================================================
    print("=== [RQ1] Evaluating Core Protection Policies (Hardware PMU + 1000 One-Fault-Per-Run Trials) ===")
    core_configs = [
        ("native",        "native",        1.0, 5e-5, True,  True),
        ("pure_approx",   "pure-approx",   1.0, 5e-5, False, False),
        ("exact_tmr",     "exact-tmr",     1.0, 5e-5, False, True),
        ("approx_scalar", "approx-scalar", 1.0, 5e-5, False, True),
        ("approx_simd",   "approx-simd",   1.0, 5e-5, False, True),
        ("adaptive_low",  "adaptive",      1.0, 5e-5, False, False),
        ("adaptive_high", "adaptive",      1.0, 5e-5, False, True),
    ]

    rq1_pmu_all = []
    rq1_fi_all = []
    native_inst_map = {}

    for tag, mode, budget, delta, is_gold, do_fi in core_configs:
        print(f"  -> Running policy [{tag}] (mode={mode}, budget={budget}, delta={delta})...")
        pmu_rows, fi_rows = compile_and_evaluate_policy(
            ssa_ll, tag, mode, rank="semantic", budget=budget, delta=delta,
            fi_trials=1000, dump_golden=is_gold, run_fi=do_fi
        )
        if tag == "native":
            for r in pmu_rows:
                native_inst_map[r["benchmark"]] = r["hw_insts"]
        rq1_pmu_all.extend(pmu_rows)
        rq1_fi_all.extend(fi_rows)

    # 结合 Normalized Overhead (基于硬件退休指令数比值) 与 SDC Rate 计算顶会标准 MWTF 指标
    pmu_lookup = {(r["policy"], r["benchmark"]): r for r in rq1_pmu_all}
    for fr in rq1_fi_all:
        pol = fr["policy"]
        bname = fr["benchmark"]
        norm_inst_overhead = pmu_lookup[(pol, bname)]["hw_insts"] / max(1, native_inst_map[bname])
        # 为防止 SDC=0 时除零，采用 Laplace 平滑 (sdc_count + 0.5) / (trials + 1)
        sdc_prob = max(0.5 / (fr["trials"] + 1.0), fr["sdc_pct"] / 100.0)
        fr["norm_hw_inst_ratio"] = round(norm_inst_overhead, 3)
        fr["mwtf_score"] = round(1.0 / (norm_inst_overhead * sdc_prob), 2)

    with open(RES_DIR / "rq1_pmu_performance_qor.csv", "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rq1_pmu_all[0].keys()))
        writer.writeheader()
        writer.writerows(rq1_pmu_all)

    with open(RES_DIR / "rq1_fault_injection_outcomes.csv", "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rq1_fi_all[0].keys()))
        writer.writeheader()
        writer.writerows(rq1_fi_all)

    # =========================================================================
    # RQ2: 选择性保护曲线 (SC'19 / Liu et al. 标准实验)
    #      横轴: 保护比例预算 Budget ∈ {20%, 40%, 60%, 80%, 100%}
    #      对比三种关键性排序算法: Semantic (本文公式3-1) vs Loop-Only vs Random
    # =========================================================================
    print("=== [RQ2] Evaluating Selective Protection Curves (Semantic vs Loop-Only vs Random across Budgets) ===")
    rq2_rows = []
    budgets = [0.25, 0.50, 0.75, 1.00]
    rank_methods = ["semantic", "loop-only", "random"]

    for rmethod in rank_methods:
        for bgt in budgets:
            tag = f"sel_{rmethod}_b{int(bgt*100)}"
            print(f"  -> Running selective protection [{tag}]...")
            pmu_rows, fi_rows = compile_and_evaluate_policy(
                ssa_ll, tag, mode="approx-simd", rank=rmethod, budget=bgt,
                delta=5e-5, fi_trials=400, dump_golden=False, run_fi=True
            )
            pmu_map = {r["benchmark"]: r for r in pmu_rows}
            for fr in fi_rows:
                bname = fr["benchmark"]
                hw_ratio = pmu_map[bname]["hw_insts"] / max(1, native_inst_map[bname])
                sdc_prob = max(0.5 / (fr["trials"] + 1.0), fr["sdc_pct"] / 100.0)
                rq2_rows.append({
                    "rank_method": rmethod,
                    "budget": bgt,
                    "benchmark": bname,
                    "hw_inst_ratio": round(hw_ratio, 3),
                    "sdc_pct": fr["sdc_pct"],
                    "det_recovered_pct": fr["det_recovered_pct"],
                    "masked_pct": fr["masked_pct"],
                    "mwtf": round(1.0 / (hw_ratio * sdc_prob), 2),
                })

    with open(RES_DIR / "rq2_selective_protection_curves.csv", "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rq2_rows[0].keys()))
        writer.writeheader()
        writer.writerows(rq2_rows)

    # =========================================================================
    # RQ3: 混合误差投票阈值 delta 敏感度分析 (验证开题报告 4.4.4 节阈值风险与权衡)
    #      扫描 delta ∈ {1e-7, 1e-5, 5e-5, 1e-3, 5e-2, 5e-1}
    #      观察: delta 过小 -> 无故障误报率 (False Alarm) 高; delta 过大 -> SDC 漏检率上升
    # =========================================================================
    print("=== [RQ3] Evaluating Voter Threshold (delta) Sensitivity Trade-off ===")
    rq3_rows = []
    deltas = [1e-7, 1e-5, 5e-5, 1e-3, 5e-2, 5e-1]
    for dval in deltas:
        tag = f"delta_{dval:.0e}".replace("-", "m").replace("+", "p")
        print(f"  -> Running threshold sensitivity [delta={dval:.1e}]...")
        pmu_rows, fi_rows = compile_and_evaluate_policy(
            ssa_ll, tag, mode="approx-simd", rank="semantic", budget=1.0,
            delta=dval, fi_trials=400, dump_golden=False, run_fi=True
        )
        pmu_map = {r["benchmark"]: r for r in pmu_rows}
        for fr in fi_rows:
            bname = fr["benchmark"]
            pr = pmu_map[bname]
            false_alarm_rate = 100.0 * pr["false_alarms"] / max(1, pr["votes_per_iter"])
            rq3_rows.append({
                "delta": dval,
                "benchmark": bname,
                "false_alarm_pct_no_fault": round(false_alarm_rate, 2),
                "no_fault_rel_l2_err": pr["rel_l2_err"],
                "fi_det_recovered_pct": fr["det_recovered_pct"],
                "fi_sdc_pct": fr["sdc_pct"],
                "fi_avg_valid_rel_err": fr["avg_valid_rel_err"],
            })

    with open(RES_DIR / "rq3_delta_threshold_sensitivity.csv", "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rq3_rows[0].keys()))
        writer.writeheader()
        writer.writerows(rq3_rows)

    # =========================================================================
    # RQ4: 在轨太阳耀斑辐射脉冲下的运行时自适应状态切换 (硬件 PMU 实测指令轨迹)
    # =========================================================================
    print("=== [RQ4] Running Solar-Flare Dynamic Mode Switching Trace with Hardware PMU ===")
    adaptive_bin = BUILD_DIR / "bin_adaptive_low"
    trace_out = run_cmd([str(adaptive_bin), "--trace-pmu"])
    with open(RES_DIR / "rq4_adaptive_solar_flare_trace.csv", "w") as f:
        f.write(trace_out)

    print("=== Rendering Interactive HTML Evaluation Dashboard ===")
    from generate_html_report import generate_html_dashboard
    generate_html_dashboard()

    print("=== All Decoupled Benchmark Evaluations (RQ1 - RQ4) Completed! ===")


if __name__ == "__main__":
    run_all_research_questions()
