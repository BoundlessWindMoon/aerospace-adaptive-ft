#!/usr/bin/env python3
"""
Standalone Evaluation Harness & SOTA Baseline Reproduction Runner

Benchmarks (12 Standard Workloads across 4 Suites):
  - PolyBench/C 4.2.1: pb_gemm, pb_2mm, pb_atax, pb_bicg, pb_jacobi2d
  - PARSEC 3.0 & AxC:  parsec_blackscholes, axc_sobel, axc_gaussian
  - Signal Processing: dsp_kissfft, dsp_fir
  - Spacecraft GNC:    gnc_pid, gnc_ekf

Reproduced Baselines & Proposed Methods:
  [A] Fault-Tolerance Mechanisms (RQ1):
      - Native       (Unprotected baseline)
      - EDDI         (Oh et al., IEEE TR 2002: Full instruction & memory load duplication + checker)
      - SWIFT        (Reis et al., CGO 2005: Register-chain duplication with ECC assumption + checker)
      - COAST-TMR    (Chielle / BYU, IEEE TNS 2019: Triplicated exact execution + majority voting)
      - Approx-Scalar (1x Exact FP64 + 2x Scalar FP32/Poly Approx + Dynamic Threshold Voting)
      - Approx-SIMD   (Proposed: 1x Exact FP64 + <2 x float> SIMD Vector Approx + Inline Fast-Path Voter)
      - Adaptive-LOW / Adaptive-HIGH (Proposed: Criticality-guided multi-version runtime switching)
  [B] Selective Protection Ranking Algorithms (RQ2):
      - Random       (Uniform random instruction selection)
      - HotPath      (Lu et al.: Dynamic execution frequency / loop-depth ranking)
      - Shoestring   (Feng et al., ASPLOS 2010: High fan-out & sink-distance heuristic)
      - SDCTune      (Lu et al., HPDC 2014: Static error amplification & store-distance model)
      - Semantic     (Proposed: Multi-variable model integrating output closeness, loop depth,
                      cross-block liveness, and recurrent state-feedback PHI cycles)
"""

import csv
import os
import subprocess
from pathlib import Path

ROOT_DIR = Path("/home/users/wangtianyu15/wtyWorkspace/aerospace-adaptive-ft")
BUILD_DIR = ROOT_DIR / "build"
TMP_DIR = BUILD_DIR / "tmp"
RES_DIR = ROOT_DIR / "results"
GOLDEN_DIR = RES_DIR / "golden"

os.environ["TMPDIR"] = str(TMP_DIR)

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
    TMP_DIR.mkdir(parents=True, exist_ok=True)
    RES_DIR.mkdir(parents=True, exist_ok=True)
    GOLDEN_DIR.mkdir(parents=True, exist_ok=True)

    run_cmd(["cmake", "..", "-DCMAKE_C_COMPILER=clang-15", "-DCMAKE_CXX_COMPILER=clang++-15"], cwd=BUILD_DIR)
    run_cmd(["make", "-j4"], cwd=BUILD_DIR)

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
                                fi_trials=600, dump_golden=False, run_fi=True):
    sut_ll = BUILD_DIR / f"sut_{tag}.ll"
    sut_bin = BUILD_DIR / f"bin_{tag}"

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
    # RQ1: Redundancy Mechanism Comparison across 12 Standard Benchmarks
    #      (Native vs EDDI vs SWIFT vs COAST-TMR vs Approx-Scalar vs Approx-SIMD vs Adaptive)
    # =========================================================================
    print("=== [RQ1] Evaluating SOTA Baselines (EDDI, SWIFT, COAST-TMR) & Proposed Methods across 12 Benchmarks ===")
    core_configs = [
        ("native",        "native",        1.0, 5e-5, True,  True),
        ("pure_approx",   "pure-approx",   1.0, 5e-5, False, False),
        ("eddi",          "eddi",          1.0, 5e-5, False, True),
        ("swift",         "swift",         1.0, 5e-5, False, True),
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
        print(f"  -> Running policy [{tag}] (mode={mode})...")
        pmu_rows, fi_rows = compile_and_evaluate_policy(
            ssa_ll, tag, mode, rank="semantic", budget=budget, delta=delta,
            fi_trials=600, dump_golden=is_gold, run_fi=do_fi
        )
        if tag == "native":
            for r in pmu_rows:
                native_inst_map[r["benchmark"]] = r["hw_insts"]
        rq1_pmu_all.extend(pmu_rows)
        rq1_fi_all.extend(fi_rows)

    pmu_lookup = {(r["policy"], r["benchmark"]): r for r in rq1_pmu_all}
    for fr in rq1_fi_all:
        pol = fr["policy"]
        bname = fr["benchmark"]
        norm_inst_overhead = pmu_lookup[(pol, bname)]["hw_insts"] / max(1, native_inst_map[bname])
        sdc_prob = max(0.5 / (fr["trials"] + 1.0), fr["sdc_pct"] / 100.0)
        # Failure rate includes both unmasked SDC and unrecovered DUE for holistic availability MWTF_avail
        fail_prob = max(0.5 / (fr["trials"] + 1.0), (fr["sdc_pct"] + fr["due_pct"]) / 100.0)
        fr["norm_hw_inst_ratio"] = round(norm_inst_overhead, 3)
        fr["mwtf_sdc"] = round(1.0 / (norm_inst_overhead * sdc_prob), 2)
        fr["mwtf_total"] = round(1.0 / (norm_inst_overhead * fail_prob), 2)

    with open(RES_DIR / "rq1_pmu_performance_qor.csv", "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rq1_pmu_all[0].keys()))
        writer.writeheader()
        writer.writerows(rq1_pmu_all)

    with open(RES_DIR / "rq1_fault_injection_outcomes.csv", "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rq1_fi_all[0].keys()))
        writer.writeheader()
        writer.writerows(rq1_fi_all)

    # =========================================================================
    # RQ2: Selective Protection Ranking Comparison across Protection Budgets
    #      Comparing: Semantic (Proposed) vs SDCTune (HPDC'14) vs Shoestring (ASPLOS'10)
    #                 vs HotPath (Frequency) vs Random
    # =========================================================================
    print("=== [RQ2] Evaluating Selective Protection Ranking Baselines (Semantic vs SDCTune vs Shoestring vs HotPath vs Random) ===")
    rq2_rows = []
    budgets = [0.25, 0.50, 0.75, 1.00]
    rank_methods = ["semantic", "sdctune", "shoestring", "hotpath", "random"]

    for rmethod in rank_methods:
        for bgt in budgets:
            tag = f"sel_{rmethod}_b{int(bgt*100)}"
            print(f"  -> Running selective ranking [{rmethod}] @ budget={int(bgt*100)}%...")
            pmu_rows, fi_rows = compile_and_evaluate_policy(
                ssa_ll, tag, mode="approx-simd", rank=rmethod, budget=bgt,
                delta=5e-5, fi_trials=300, dump_golden=False, run_fi=True
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
    # RQ3: Voter Tolerance Threshold (delta) Sensitivity Analysis
    # =========================================================================
    print("=== [RQ3] Evaluating Voter Threshold (delta) Sensitivity Trade-off ===")
    rq3_rows = []
    deltas = [1e-7, 1e-5, 5e-5, 1e-3, 5e-2, 5e-1]
    for dval in deltas:
        tag = f"delta_{dval:.0e}".replace("-", "m").replace("+", "p")
        print(f"  -> Running threshold sensitivity [delta={dval:.1e}]...")
        pmu_rows, fi_rows = compile_and_evaluate_policy(
            ssa_ll, tag, mode="approx-simd", rank="semantic", budget=1.0,
            delta=dval, fi_trials=300, dump_golden=False, run_fi=True
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
    # RQ4: Runtime Dynamic Mode Switching under Time-Varying Fault Injection Trace
    # =========================================================================
    print("=== [RQ4] Running Dynamic Mode Switching Trace with Hardware PMU ===")
    adaptive_bin = BUILD_DIR / "bin_adaptive_low"
    trace_out = run_cmd([str(adaptive_bin), "--trace-pmu"])
    with open(RES_DIR / "rq4_adaptive_solar_flare_trace.csv", "w") as f:
        f.write(trace_out)

    # =========================================================================
    # RQ5: IEEE-754 Bit-Field Vulnerability Anatomy (Sign / Exponent / Mantissa-High / Mantissa-Low)
    # =========================================================================
    print("=== [RQ5] Running IEEE-754 Bit-Field Vulnerability Anatomy (Native vs SWIFT vs Approx-SIMD) ===")
    rq5_rows = []
    for pol in ["native", "swift", "approx_simd"]:
        fi_bin = BUILD_DIR / f"bin_{pol}_fi"
        bf_out = run_cmd([str(fi_bin), "--bitfield", pol])
        for line in bf_out.strip().splitlines():
            if line.startswith("BITFIELD_CSV,"):
                p = line.split(",")
                rq5_rows.append({
                    "policy": p[1],
                    "bit_field": p[2],
                    "trials": int(p[3]),
                    "masked_pct": float(p[4]),
                    "det_recovered_pct": float(p[5]),
                    "benign_pct": float(p[6]),
                    "sdc_pct": float(p[7]),
                    "due_pct": float(p[8]),
                })
    with open(RES_DIR / "rq5_bitfield_sensitivity.csv", "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rq5_rows[0].keys()))
        writer.writeheader()
        writer.writerows(rq5_rows)

    # =========================================================================
    # RQ6: Spatial MBU (1/2/4-bit) & Temporal Multi-Fault Burst (k=1..8) Stress Test
    # =========================================================================
    print("=== [RQ6] Running Spatial MBU (1/2/4-bit) & Temporal Multi-Fault Burst (k=1..8) Stress Test ===")
    rq6_rows = []
    for pol in ["native", "swift", "exact_tmr", "approx_simd"]:
        fi_bin = BUILD_DIR / f"bin_{pol}_fi"
        st_out = run_cmd([str(fi_bin), "--stress", pol])
        for line in st_out.strip().splitlines():
            if line.startswith("STRESS_CSV,"):
                p = line.split(",")
                rq6_rows.append({
                    "policy": p[1],
                    "stress_type": p[2],
                    "stress_level": p[3],
                    "trials": int(p[4]),
                    "survive_ok_pct": float(p[5]),
                    "det_recovered_pct": float(p[6]),
                    "sdc_pct": float(p[7]),
                    "due_pct": float(p[8]),
                })
    with open(RES_DIR / "rq6_mbu_burst_stress.csv", "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rq6_rows[0].keys()))
        writer.writeheader()
        writer.writerows(rq6_rows)

    # =========================================================================
    # RQ7: Workload Problem Size Scaling across Cache Hierarchy (N=32..256)
    # =========================================================================
    print("=== [RQ7] Running Problem Size Scaling across Cache Hierarchy (L1 -> L2 -> L3/DRAM) ===")
    rq7_rows = []
    for pol in ["native", "eddi", "exact_tmr", "approx_scalar", "approx_simd"]:
        sbin = BUILD_DIR / f"bin_{pol}"
        sc_out = run_cmd([str(sbin), "--scale", pol])
        for line in sc_out.strip().splitlines():
            if line.startswith("SCALE_CSV,"):
                p = line.split(",")
                rq7_rows.append({
                    "policy": p[1],
                    "matrix_n": int(p[2]),
                    "cache_tier": p[3],
                    "exec_time_us": float(p[4]),
                    "hw_insts": int(p[5]),
                    "hw_cycles": int(p[6]),
                    "ipc": float(p[7]),
                })
    with open(RES_DIR / "rq7_cache_scaling.csv", "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rq7_rows[0].keys()))
        writer.writeheader()
        writer.writerows(rq7_rows)

    print("=== All Evaluations (RQ1 - RQ7) Completed! ===")


if __name__ == "__main__":
    run_all_research_questions()
