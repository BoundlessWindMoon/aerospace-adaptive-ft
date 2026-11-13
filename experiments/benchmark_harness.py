#!/usr/bin/env python3
"""
Standalone Evaluation Harness & SOTA Baseline Reproduction Runner

Benchmarks (17 Standard Workloads across 5 Upstream Suites):
  - PolyBench/C 4.2.1 (FP64) : pb_gemm, pb_2mm, pb_atax, pb_bicg, pb_jacobi2d,
                               pb_ludcmp, pb_durbin, pb_deriche
  - PolyBench/C 4.2.1 (INT32): pb_floyd_warshall, pb_nussinov
  - PARSEC Black-Scholes (llvm-loop-perforation single-file port): parsec_blackscholes
  - KissFFT v1.3.1           : dsp_kissfft
  - llvm-loop-perforation    : axc_sobel (upstream benchmarks/sobel/sobel.c)
  - COAST tests (BYU/LANL)   : coast_crc16, coast_quicksort
  - OBPMark v0.3 (ESA + BSC) : obpmark_image_prep, obpmark_image_proc
                               (benchmark 1.1 on-board image corrections & calibrations)

All 17 kernels are compiled directly from unmodified upstream sources in third_party/;
no self-written compute kernels remain.

CLI Examples:
  # Run all research questions (RQ1 - RQ8):
  python3 experiments/benchmark_harness.py --rq all

  # Run only RQ8 (Instruction-Category Breakdown + RISC-V RVV Cross-Platform Verification):
  python3 experiments/benchmark_harness.py --rq rq8

  # Run a quick policy check on a subset of policies and diff against a saved snapshot:
  python3 experiments/benchmark_harness.py --rq rq1 --policy native,approx_simd --diff-snapshot baseline_v1
"""

import argparse
import csv
import json
import math
import os
import re
import shutil
import subprocess
from pathlib import Path


def wilson_ci95(k, n):
    """SDC/故障率的 95% 置信区间（Wilson score interval，百分比形式）。

    故障注入是伯努利抽样，单点率值必须附置信区间才有可比性
    （口径见 Leveugle et al., "Statistical fault injection: Quantified
    error and confidence", DATE 2009）。小 p 时 Wilson 区间优于正态近似。
    """
    if n <= 0:
        return 0.0, 0.0, 0.0
    z = 1.959963985
    p = k / n
    denom = 1.0 + z * z / n
    center = (p + z * z / (2 * n)) / denom
    half = (z / denom) * math.sqrt(p * (1 - p) / n + z * z / (4 * n * n))
    lo = max(0.0, center - half)
    hi = min(1.0, center + half)
    return 100.0 * lo, 100.0 * hi, 100.0 * (hi - lo) / 2.0

# ---------------------------------------------------------------------------
# 路径与工具链：全部可配置，不写死绝对路径与版本号
#   - 仓库根目录由本文件位置推导（experiments/ 的上一级）
#   - 工具名与交叉编译环境走环境变量，便于换 LLVM 版本或换机器
# ---------------------------------------------------------------------------
ROOT_DIR = Path(os.environ.get("ATFT_ROOT", Path(__file__).resolve().parent.parent))
CLANG = os.environ.get("ATFT_CLANG", "clang-15")
CLANGXX = os.environ.get("ATFT_CLANGXX", "clang++-15")
OPT = os.environ.get("ATFT_OPT", "opt-15")
RISCV_TRIPLE = os.environ.get("ATFT_RISCV_TRIPLE", "riscv64-linux-gnu")
RISCV_SYSROOT = os.environ.get("ATFT_RISCV_SYSROOT", "/usr/riscv64-linux-gnu")
QEMU_RISCV = os.environ.get("ATFT_QEMU_RISCV", "qemu-riscv64")

BUILD_DIR = ROOT_DIR / "build"
TMP_DIR = BUILD_DIR / "tmp"
RES_DIR = ROOT_DIR / "results"
GOLDEN_DIR = RES_DIR / "golden"
SNAP_DIR = RES_DIR / "snapshots"

os.environ["TMPDIR"] = str(TMP_DIR)

PASS_POLICY_SO = BUILD_DIR / "libAdaptiveFTPass.so"
PASS_FI_SO = BUILD_DIR / "libFaultInjectorPass.so"
LIB_ATFT_RT = BUILD_DIR / "libatft_rt.a"
LIB_HARNESS_RT = BUILD_DIR / "libharness_rt.a"

CFLAGS = ["-O2", "-fno-unroll-loops", "-fno-vectorize", "-fno-slp-vectorize"]

# 被测负载的头文件搜索路径：新增一个上游套件只需在这里加一行
BENCH_INCLUDE_DIRS = [
    "include",
    "benchmarks/obpmark_shim",
    "third_party/polybench-c/utilities",
    "third_party/kissfft",
    "third_party/obpmark/src/common",
    "third_party/obpmark/src/1.1-image/src/ports/cpu",
]
# 评测器（eval_suite + harness 运行时）的头文件搜索路径
HARNESS_INCLUDE_DIRS = ["include", "harness", "benchmarks"]

def inc_flags(dirs):
    return [f"-I{d}" for d in dirs]


# ---------------------------------------------------------------------------
# 配置加载：内置默认值 <- YAML <- 命令行
# 实验参数（策略清单、参数网格、判据阈值）都在 YAML 里，换一组对比不改源码。
# ---------------------------------------------------------------------------
DEFAULT_CONFIG_PATH = ROOT_DIR / "experiments" / "config" / "default.yaml"

BUILTIN_CONFIG = {
    "trials": 1000,
    "thresholds": {
        "sdc_rel_err": 0.01,
        "masked_rel_err": 1.0e-12,
        "trial_watchdog_ms": 100,
        "step_budget_factor": 50,
    },
    "smoke": {"trials": 60, "policies": ["native", "exact_tmr", "approx_simd"]},
    "policies": [
        {"tag": "native", "mode": "native", "golden": True, "fi": True},
        {"tag": "exact_tmr", "mode": "exact-tmr", "fi": True},
        {"tag": "approx_simd", "mode": "approx-simd", "fi": True},
    ],
    "grids": {
        "rq2": {"budgets": [0.25, 0.50, 0.75, 1.00],
                "rank_methods": ["semantic", "sdctune", "shoestring", "hotpath", "random"]},
        "rq3": {"deltas": [1e-7, 1e-5, 5e-5, 1e-3, 5e-2, 5e-1]},
        "rq8": {"categories": ["fp", "int", "address", "branch"],
                "policies": ["native", "swift", "lltfi_sid", "exact_tmr", "approx_simd"]},
    },
    "defaults": {"budget": 1.0, "delta": 5.0e-5, "weights": {"w1": 2.5, "w2": 1.8, "w3": 0.8}},
}


def _deep_merge(base, override):
    out = dict(base)
    for k, v in (override or {}).items():
        if isinstance(v, dict) and isinstance(out.get(k), dict):
            out[k] = _deep_merge(out[k], v)
        else:
            out[k] = v
    return out


def load_config(path=None):
    cfg_path = Path(path) if path else DEFAULT_CONFIG_PATH
    if not cfg_path.exists():
        if path:
            raise RuntimeError(f"配置文件不存在: {cfg_path}")
        return dict(BUILTIN_CONFIG)
    try:
        import yaml
    except ImportError:
        print(f"[config] 未安装 PyYAML，忽略 {cfg_path}，使用内置默认值")
        return dict(BUILTIN_CONFIG)
    with open(cfg_path, encoding="utf8") as f:
        loaded = yaml.safe_load(f) or {}
    return _deep_merge(BUILTIN_CONFIG, loaded)


def apply_threshold_env(cfg):
    """把判据阈值写入环境变量，由 eval_suite 在启动时读取。"""
    th = cfg.get("thresholds", {})
    os.environ["ATFT_SDC_REL_ERR"] = str(th.get("sdc_rel_err", 0.01))
    os.environ["ATFT_MASKED_REL_ERR"] = str(th.get("masked_rel_err", 1.0e-12))
    os.environ["ATFT_TRIAL_WATCHDOG_MS"] = str(th.get("trial_watchdog_ms", 100))
    os.environ["ATFT_STEP_BUDGET_FACTOR"] = str(th.get("step_budget_factor", 50))


def policy_entries(cfg):
    """YAML 的 policies 展开为 (tag, mode, budget, delta, is_golden, do_fi)。"""
    dflt = cfg.get("defaults", {})
    out = []
    for p in cfg.get("policies", []):
        out.append((
            p["tag"], p["mode"],
            float(p.get("budget", dflt.get("budget", 1.0))),
            float(p.get("delta", dflt.get("delta", 5e-5))),
            bool(p.get("golden", False)),
            bool(p.get("fi", False)),
        ))
    return out


# 单条命令墙钟上限：注错后的被测程序可能死循环（整数/分支位翻转会破坏循环边界），
# 没有上限时一次挂死就会无限阻塞整个 campaign。超时按失败处理并返回已有 stdout，
# 由调用方决定是否致命。
DEFAULT_CMD_TIMEOUT_S = 900


def run_cmd(cmd, cwd=ROOT_DIR, allow_nonzero=False, timeout_s=DEFAULT_CMD_TIMEOUT_S):
    try:
        res = subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                             text=True, timeout=timeout_s)
    except subprocess.TimeoutExpired as e:
        partial = e.stdout.decode() if isinstance(e.stdout, bytes) else (e.stdout or "")
        msg = f"[Timeout {timeout_s}s] {' '.join(map(str, cmd))}"
        if allow_nonzero:
            print("  !! " + msg)
            return partial
        raise RuntimeError(msg) from None
    if res.returncode != 0 and not allow_nonzero:
        raise RuntimeError(f"Command failed ({res.returncode}): {' '.join(map(str, cmd))}\n{res.stderr}")
    return res.stdout


def build_infrastructure(arch="x86_64"):
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    TMP_DIR.mkdir(parents=True, exist_ok=True)
    RES_DIR.mkdir(parents=True, exist_ok=True)
    GOLDEN_DIR.mkdir(parents=True, exist_ok=True)
    SNAP_DIR.mkdir(parents=True, exist_ok=True)

    run_cmd(["cmake", "..", f"-DCMAKE_C_COMPILER={CLANG}", f"-DCMAKE_CXX_COMPILER={CLANGXX}"], cwd=BUILD_DIR)
    run_cmd(["make", "-j4"], cwd=BUILD_DIR)

    suffix = f"_{arch}" if arch != "x86_64" else ""
    raw_ll = BUILD_DIR / f"benchmarks_raw{suffix}.ll"
    ssa_ll = BUILD_DIR / f"benchmarks_ssa{suffix}.ll"
    target_flags = [f"--target={RISCV_TRIPLE}", "-march=rv64gc"] if arch == "riscv64" else []
    run_cmd([
        CLANG, *target_flags, "-O1", "-Xclang", "-disable-llvm-passes",
        "-S", "-emit-llvm", "benchmarks/benchmarks.c",
        *inc_flags(BENCH_INCLUDE_DIRS),
        "-o", str(raw_ll)
    ])
    run_cmd([
        OPT, "-passes=always-inline,sroa,mem2reg,loop-simplify",
        "-S", str(raw_ll), "-o", str(ssa_ll)
    ])
    return ssa_ll


def compile_and_evaluate_policy(ssa_ll, tag, mode, rank="semantic", budget=1.0,
                                delta=5e-5, w1=2.5, w2=1.8, w3=0.8,
                                fi_trials=600, dump_golden=False, run_fi=True,
                                fi_target_type="fp", arch="x86_64"):
    sut_ll = BUILD_DIR / f"sut_{tag}.ll"
    sut_bin = BUILD_DIR / f"bin_{tag}_{arch}"

    opt_cmd = [
        OPT, f"-load-pass-plugin={PASS_POLICY_SO}",
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

    if arch == "riscv64":
        cc_cmd = [
            CLANG, f"--target={RISCV_TRIPLE}", "-march=rv64gc",
            *CFLAGS, "experiments/eval_suite.c", str(sut_ll),
            "runtime/atft_runtime.c", "harness/harness_rt.c",
            *inc_flags(HARNESS_INCLUDE_DIRS), "-lm", "-o", str(sut_bin)
        ]
        exec_prefix = [QEMU_RISCV, "-L", RISCV_SYSROOT]
    else:
        cc_cmd = [
            CLANG, *CFLAGS, "experiments/eval_suite.c", str(sut_ll),
            str(LIB_ATFT_RT), str(LIB_HARNESS_RT),
            *inc_flags(HARNESS_INCLUDE_DIRS), "-lm", "-o", str(sut_bin)
        ]
        exec_prefix = []

    run_cmd(cc_cmd)

    if dump_golden:
        run_cmd([*exec_prefix, str(sut_bin), "--dump-golden", str(GOLDEN_DIR)])

    pmu_out = run_cmd([*exec_prefix, str(sut_bin), "--pmu-perf", tag, str(GOLDEN_DIR)])
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
        sut_fi_ll = BUILD_DIR / f"sut_{tag}_fi_{fi_target_type}.ll"
        sut_fi_bin = BUILD_DIR / f"bin_{tag}_fi_{fi_target_type}_{arch}"
        run_cmd([
            OPT, f"-load-pass-plugin={PASS_FI_SO}",
            "-passes=standalone-fi,verify",
            f"-fi-target-type={fi_target_type}",
            "-S", str(sut_ll), "-o", str(sut_fi_ll)
        ])
        if arch == "riscv64":
            fi_cc_cmd = [
                CLANG, f"--target={RISCV_TRIPLE}", "-march=rv64gc",
                *CFLAGS, "experiments/eval_suite.c", str(sut_fi_ll),
                "runtime/atft_runtime.c", "harness/harness_rt.c",
                *inc_flags(HARNESS_INCLUDE_DIRS), "-lm", "-o", str(sut_fi_bin)
            ]
        else:
            fi_cc_cmd = [
                CLANG, *CFLAGS, "experiments/eval_suite.c", str(sut_fi_ll),
                str(LIB_ATFT_RT), str(LIB_HARNESS_RT),
                *inc_flags(HARNESS_INCLUDE_DIRS), "-lm", "-o", str(sut_fi_bin)
            ]
        run_cmd(fi_cc_cmd)
        fi_out = run_cmd([*exec_prefix, str(sut_fi_bin), "--fi-one", tag, str(GOLDEN_DIR), str(fi_trials)],
                         allow_nonzero=True, timeout_s=300)
        for line in fi_out.strip().splitlines():
            if line.startswith("FI5_CSV,"):
                parts = line.split(",")
                fi_rows.append({
                    "policy": parts[1],
                    "benchmark": parts[2],
                    "fi_target_type": fi_target_type,
                    "arch": arch,
                    "dyn_target_insts": int(parts[3]),
                    "trials": int(parts[4]),
                    "masked_pct": float(parts[5]),
                    "det_recovered_pct": float(parts[6]),
                    "benign_pct": float(parts[7]),
                    "sdc_pct": float(parts[8]),
                    "due_pct": float(parts[9]),
                    "avg_valid_rel_err": float(parts[10]),
                })
                n_trials = fi_rows[-1]["trials"]
                k_sdc = round(fi_rows[-1]["sdc_pct"] / 100.0 * n_trials)
                lo, hi, half = wilson_ci95(k_sdc, n_trials)
                fi_rows[-1]["sdc_ci95_lo"] = round(lo, 3)
                fi_rows[-1]["sdc_ci95_hi"] = round(hi, 3)
                fi_rows[-1]["sdc_ci95_halfwidth"] = round(half, 3)
                k_det = round(fi_rows[-1]["det_recovered_pct"] / 100.0 * n_trials)
                dlo, dhi, dhalf = wilson_ci95(k_det, n_trials)
                fi_rows[-1]["det_recovered_ci95_halfwidth"] = round(dhalf, 3)

    return pmu_rows, fi_rows


def run_rq8_instruction_category_and_riscv(ssa_ll, cfg):
    """
    RQ8:
      (A) Multi-Category Instruction Fault Injection Breakdown (FP vs Int vs Address vs Branch)
          comparing Native, SWIFT (CGO'05), LLTFI-SID (reimplemented from LLTFI semantics), COAST-TMR (reimplemented from COAST, IEEE TNS'19), and Approx-SIMD.
      (B) RISC-V (rv64gc / rv64gcv RVV 1.0) Cross-Compilation Codegen Analysis & QEMU Execution Verification.
    """
    print("=== [RQ8-A] Evaluating Multi-Category Instruction Fault Injection (fp / int / address / branch) ===")
    tag_to_mode = {p["tag"]: p["mode"] for p in cfg.get("policies", [])}
    cat_policies = [(t, tag_to_mode.get(t, t.replace("_", "-")))
                    for t in cfg["grids"]["rq8"]["policies"]]
    categories = list(cfg["grids"]["rq8"]["categories"])
    cat_summary_rows = []

    for cat in categories:
        for pol_tag, pol_mode in cat_policies:
            run_tag = f"cat_{pol_tag}_{cat}"
            print(f"  -> Injecting category [{cat}] on policy [{pol_tag}]...")
            _, fi_rows = compile_and_evaluate_policy(
                ssa_ll, run_tag, mode=pol_mode, rank="semantic", budget=1.0,
                delta=float(cfg.get("defaults", {}).get("delta", 5e-5)),
                fi_trials=int(cfg["grids"]["rq8"].get("trials", 120)),
                dump_golden=False, run_fi=True,
                fi_target_type=cat, arch="x86_64"
            )
            valid_rows = [r for r in fi_rows if r["dyn_target_insts"] > 0]
            if not valid_rows:
                continue
            n_b = len(valid_rows)
            cat_summary_rows.append({
                "fi_category": cat,
                "policy": pol_tag,
                "benchmarks_covered": n_b,
                "avg_dyn_sites": round(sum(r["dyn_target_insts"] for r in valid_rows) / n_b, 1),
                "masked_pct": round(sum(r["masked_pct"] for r in valid_rows) / n_b, 2),
                "det_recovered_pct": round(sum(r["det_recovered_pct"] for r in valid_rows) / n_b, 2),
                "benign_pct": round(sum(r["benign_pct"] for r in valid_rows) / n_b, 2),
                "sdc_pct": round(sum(r["sdc_pct"] for r in valid_rows) / n_b, 2),
                "due_pct": round(sum(r["due_pct"] for r in valid_rows) / n_b, 2),
            })

    with open(RES_DIR / "rq8_inst_category_breakdown.csv", "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(cat_summary_rows[0].keys()))
        writer.writeheader()
        writer.writerows(cat_summary_rows)

    print("=== [RQ8-B] Evaluating RISC-V (rv64gc + rv64gcv RVV 1.0) Cross-Platform Codegen & QEMU Execution ===")
    ssa_rv_ll = build_infrastructure(arch="riscv64")
    rv_policies = [
        ("native",        "native"),
        ("exact_tmr",     "exact-tmr"),
        ("approx_scalar", "approx-scalar"),
        ("approx_simd",   "approx-simd"),
        ("adaptive_low",  "adaptive"),
    ]
    rv_rows = []
    for pol_tag, pol_mode in rv_policies:
        rv_tag = f"{pol_tag}_rv64"
        do_rv_fi = pol_tag in ("native", "exact_tmr", "approx_simd")
        print(f"  -> Cross-compiling & executing RISC-V ELF under {QEMU_RISCV} for [{pol_tag}] (run_fi={do_rv_fi})...")
        pmu_rv, fi_rv = compile_and_evaluate_policy(
            ssa_rv_ll, rv_tag, mode=pol_mode, rank="semantic", budget=1.0,
            delta=float(cfg.get("defaults", {}).get("delta", 5e-5)),
            fi_trials=(int(cfg["grids"]["rq8"].get("trials", 120)) if do_rv_fi else 0),
            dump_golden=(pol_tag == "native"),
            run_fi=do_rv_fi, fi_target_type="fp", arch="riscv64"
        )
        sut_rv_ll = BUILD_DIR / f"sut_{rv_tag}.ll"

        # Generate RISC-V RVV 1.0 assembly (-march=rv64gcv) to audit static instructions & vectorization
        rvv_ll = BUILD_DIR / f"sut_{pol_tag}_rv64gcv.ll"
        rvv_ll.write_text(
            sut_rv_ll.read_text(encoding="utf-8").replace(
                '"+64bit,+a,+c,+d,+f,+m,+relax,-save-restore"',
                '"+64bit,+a,+c,+d,+f,+m,+v,+zvl128b,+relax,-save-restore"'
            ),
            encoding="utf-8"
        )
        rvv_s = BUILD_DIR / f"sut_{pol_tag}_rv64gcv.s"
        run_cmd([
            CLANG, f"--target={RISCV_TRIPLE}", "-march=rv64gcv",
            "-mllvm", "-riscv-v-vector-bits-min=128",
            *CFLAGS, "-S", str(rvv_ll), "-o", str(rvv_s)
        ])
        asm_text = rvv_s.read_text(encoding="utf-8")

        # Parse per-function static assembly instruction counts and RVV vector ops
        fn_blocks = re.split(r"\.type\s+(bench_[a-zA-Z0-9_]+),@function", asm_text)
        fn_asm_stats = {}
        for idx in range(1, len(fn_blocks), 2):
            fn_name = fn_blocks[idx].replace("bench_", "")
            body = fn_blocks[idx + 1].split(".Lfunc_end")[0]
            total_insts = 0
            fp64_ops = 0
            fp32_scalar_ops = 0
            rvv_vec_ops = 0
            for ln in body.splitlines():
                s = ln.strip()
                if not s or s.startswith(".") or s.startswith("#") or s.endswith(":"):
                    continue
                op = s.split()[0]
                total_insts += 1
                if op.startswith(("vf", "vset", "vmv", "vle", "vse", "vrgather", "vslide")):
                    rvv_vec_ops += 1
                elif op.endswith(".d"):
                    fp64_ops += 1
                elif op.endswith(".s"):
                    fp32_scalar_ops += 1
            fn_asm_stats[fn_name] = {
                "static_insts": total_insts,
                "fp64_ops": fp64_ops,
                "fp32_scalar_ops": fp32_scalar_ops,
                "rvv_vec_ops": rvv_vec_ops,
            }

        fi_rv_map = {r["benchmark"]: r for r in fi_rv}

        for pr in pmu_rv:
            bname = pr["benchmark"]
            ast = fn_asm_stats.get(bname, {"static_insts": 0, "fp64_ops": 0, "fp32_scalar_ops": 0, "rvv_vec_ops": 0})
            fr = fi_rv_map.get(bname, {})
            rv_rows.append({
                "policy": pol_tag,
                "benchmark": bname,
                "rv64gcv_static_insts": ast["static_insts"],
                "rv64gcv_fp64_ops": ast["fp64_ops"],
                "rv64gcv_fp32_scalar_ops": ast["fp32_scalar_ops"],
                "rv64gcv_rvv_vec_ops": ast["rvv_vec_ops"],
                "qemu_rel_l2_err": pr["rel_l2_err"],
                "qemu_psnr_db": pr["psnr_db"],
                "qemu_fi_det_recovered_pct": fr.get("det_recovered_pct", -1.0),
                "qemu_fi_sdc_pct": fr.get("sdc_pct", -1.0),
            })

    with open(RES_DIR / "rq8_riscv_rvv_evaluation.csv", "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rv_rows[0].keys()))
        writer.writeheader()
        writer.writerows(rv_rows)


def save_or_diff_snapshot(save_name=None, diff_name=None):
    SNAP_DIR.mkdir(parents=True, exist_ok=True)
    tracked_csvs = [
        "rq1_pmu_performance_qor.csv",
        "rq1_fault_injection_outcomes.csv",
        "rq8_inst_category_breakdown.csv",
        "rq8_riscv_rvv_evaluation.csv",
    ]
    if save_name:
        target = SNAP_DIR / save_name
        target.mkdir(parents=True, exist_ok=True)
        for cname in tracked_csvs:
            src = RES_DIR / cname
            if src.exists():
                shutil.copyfile(src, target / cname)
        print(f"=== [Snapshot] Saved evaluation snapshot to: {target} ===")

    if diff_name:
        base_dir = SNAP_DIR / diff_name
        if not base_dir.exists():
            print(f"[Snapshot Warning] Snapshot '{diff_name}' does not exist in {SNAP_DIR}")
            return
        print(f"=== [Snapshot Diff] Comparing current results against snapshot [{diff_name}] ===")
        cur_fi = list(csv.DictReader(open(RES_DIR / "rq1_fault_injection_outcomes.csv")))
        old_fi = list(csv.DictReader(open(base_dir / "rq1_fault_injection_outcomes.csv")))
        for pol in ["native", "eddi", "swift", "lltfi_sid", "exact_tmr", "approx_simd"]:
            c_sub = [r for r in cur_fi if r["policy"] == pol]
            o_sub = [r for r in old_fi if r["policy"] == pol]
            if c_sub and o_sub:
                c_inst = sum(float(r["norm_hw_inst_ratio"]) for r in c_sub) / len(c_sub)
                o_inst = sum(float(r["norm_hw_inst_ratio"]) for r in o_sub) / len(o_sub)
                c_sdc = sum(float(r["sdc_pct"]) for r in c_sub) / len(c_sub)
                o_sdc = sum(float(r["sdc_pct"]) for r in o_sub) / len(o_sub)
                print(f"  Policy [{pol:12s}]: InstRatio {o_inst:.2f}x -> {c_inst:.2f}x ({c_inst-o_inst:+.2f}x) | "
                      f"SDC {o_sdc:.2f}% -> {c_sdc:.2f}% ({c_sdc-o_sdc:+.2f}%)")


def print_smoke_summary(pmu_rows, fi_rows):
    native = {r["benchmark"]: r["hw_insts"] for r in pmu_rows if r["policy"] == "native"}
    fi_map = {(r["policy"], r["benchmark"]): r for r in fi_rows}
    pols = []
    for r in pmu_rows:
        if r["policy"] not in pols:
            pols.append(r["policy"])
    print("\n=== [Smoke] 每策略汇总（指令倍率为全部负载均值） ===")
    print(f"{'policy':14s} {'instRatio':>9s} {'maxRelErr':>11s} {'SDC%':>7s} {'±CI':>6s} {'DUE%':>6s}")
    for pol in pols:
        rs = [r for r in pmu_rows if r["policy"] == pol]
        ratio = sum(r["hw_insts"] / max(1, native.get(r["benchmark"], 1)) for r in rs) / len(rs)
        max_err = max(r["rel_l2_err"] for r in rs)
        frs = [fi_map[(pol, r["benchmark"])] for r in rs if (pol, r["benchmark"]) in fi_map]
        if frs:
            sdc = sum(x["sdc_pct"] for x in frs) / len(frs)
            ci = sum(x["sdc_ci95_halfwidth"] for x in frs) / len(frs)
            due = sum(x["due_pct"] for x in frs) / len(frs)
            print(f"{pol:14s} {ratio:9.2f} {max_err:11.3e} {sdc:7.2f} {ci:6.2f} {due:6.2f}")
        else:
            print(f"{pol:14s} {ratio:9.2f} {max_err:11.3e} {'-':>7s} {'-':>6s} {'-':>6s}")
    print("=== [Smoke] 未写入 results/*.csv（smoke 模式不覆盖正式数据） ===\n")


def run_selected_research_questions(args):
    cfg = load_config(args.config)
    apply_threshold_env(cfg)
    if args.trials is None:
        args.trials = int(cfg.get("trials", 1000))
    if args.smoke:
        smoke_cfg = cfg.get("smoke", {})
        args.rq = "rq1"
        args.trials = args.smoke_trials if args.smoke_trials is not None \
            else int(smoke_cfg.get("trials", 60))
        if args.policy == "all":
            args.policy = args.smoke_policies or ",".join(
                smoke_cfg.get("policies", ["native", "exact_tmr", "approx_simd"]))
        args.save_snapshot = None
        print(f"=== [Smoke 模式 | 默认] RQ1 | policies={args.policy} | "
              f"trials={args.trials} | 不写 results/*.csv | 全量请加 --full ===")
    print("=== [Step 0] Building Decoupled Policy Pass & Standalone Evaluation Harness ===")
    ssa_ll = build_infrastructure()

    rqs = [x.strip().lower() for x in args.rq.split(",")]
    run_all = "all" in rqs
    selected_policies = None
    if args.policy != "all":
        selected_policies = set(x.strip() for x in args.policy.split(","))

    native_inst_map = {}
    if (RES_DIR / "rq1_pmu_performance_qor.csv").exists():
        for r in csv.DictReader(open(RES_DIR / "rq1_pmu_performance_qor.csv")):
            if r["policy"] == "native":
                native_inst_map[r["benchmark"]] = int(r["hw_insts"])

    # =========================================================================
    # RQ1: Redundancy Mechanism Comparison across 17 Standard Benchmarks
    # =========================================================================
    if run_all or "rq1" in rqs:
        print("=== [RQ1] Evaluating SOTA Baselines & Proposed Methods across 17 Benchmarks ===")
        core_configs = policy_entries(cfg)
        wts = cfg.get("defaults", {}).get("weights", {})

        rq1_pmu_all = []
        rq1_fi_all = []

        for tag, mode, budget, delta, is_gold, do_fi in core_configs:
            if selected_policies and tag not in selected_policies and tag != "native":
                continue
            print(f"  -> Running policy [{tag}] (mode={mode}, arch={args.arch}, fi_type={args.fi_type})...")
            pmu_rows, fi_rows = compile_and_evaluate_policy(
                ssa_ll, tag, mode, rank="semantic", budget=budget, delta=delta,
                w1=wts.get("w1", 2.5), w2=wts.get("w2", 1.8), w3=wts.get("w3", 0.8),
                fi_trials=args.trials, dump_golden=is_gold, run_fi=do_fi,
                fi_target_type=args.fi_type, arch=args.arch
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
            fail_prob = max(0.5 / (fr["trials"] + 1.0), (fr["sdc_pct"] + fr["due_pct"]) / 100.0)
            fr["norm_hw_inst_ratio"] = round(norm_inst_overhead, 3)
            fr["mwtf_sdc"] = round(1.0 / (norm_inst_overhead * sdc_prob), 2)
            fr["mwtf_total"] = round(1.0 / (norm_inst_overhead * fail_prob), 2)

        if args.smoke:
            print_smoke_summary(rq1_pmu_all, rq1_fi_all)

        # fp campaign 写入主 CSV（看板默认口径）；其他注错类别写入带后缀的 CSV，
        # 避免"跑了但没落盘"导致结论与数据不一致
        if not selected_policies and args.arch == "x86_64" and args.fi_type != "fp":
            suffix = args.fi_type
            with open(RES_DIR / f"rq1_pmu_performance_qor_{suffix}.csv", "w", newline="") as f:
                writer = csv.DictWriter(f, fieldnames=list(rq1_pmu_all[0].keys()))
                writer.writeheader()
                writer.writerows(rq1_pmu_all)
            with open(RES_DIR / f"rq1_fault_injection_outcomes_{suffix}.csv", "w", newline="") as f:
                writer = csv.DictWriter(f, fieldnames=list(rq1_fi_all[0].keys()))
                writer.writeheader()
                writer.writerows(rq1_fi_all)
            print(f"  -> Wrote rq1_fault_injection_outcomes_{suffix}.csv ({len(rq1_fi_all)} rows)")

        if not selected_policies and args.arch == "x86_64" and args.fi_type == "fp":
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
    # =========================================================================
    if run_all or "rq2" in rqs:
        print("=== [RQ2] Evaluating Selective Protection Ranking Baselines ===")
        rq2_rows = []
        budgets = cfg["grids"]["rq2"]["budgets"]
        rank_methods = cfg["grids"]["rq2"]["rank_methods"]
        rq2_trials = int(cfg["grids"]["rq2"].get("trials", 300))
        grid_delta = float(cfg.get("defaults", {}).get("delta", 5e-5))

        for rmethod in rank_methods:
            for bgt in budgets:
                tag = f"sel_{rmethod}_b{int(bgt*100)}"
                print(f"  -> Running selective ranking [{rmethod}] @ budget={int(bgt*100)}%...")
                pmu_rows, fi_rows = compile_and_evaluate_policy(
                    ssa_ll, tag, mode="approx-simd", rank=rmethod, budget=bgt,
                    delta=grid_delta, fi_trials=rq2_trials, dump_golden=False, run_fi=True
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
    if run_all or "rq3" in rqs:
        print("=== [RQ3] Evaluating Voter Threshold (delta) Sensitivity Trade-off ===")
        rq3_rows = []
        deltas = [float(d) for d in cfg["grids"]["rq3"]["deltas"]]
        rq3_trials = int(cfg["grids"]["rq3"].get("trials", 300))
        for dval in deltas:
            tag = f"delta_{dval:.0e}".replace("-", "m").replace("+", "p")
            print(f"  -> Running threshold sensitivity [delta={dval:.1e}]...")
            pmu_rows, fi_rows = compile_and_evaluate_policy(
                ssa_ll, tag, mode="approx-simd", rank="semantic", budget=1.0,
                delta=dval, fi_trials=rq3_trials, dump_golden=False, run_fi=True
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
    if run_all or "rq4" in rqs:
        print("=== [RQ4] Running Dynamic Mode Switching Trace with Hardware PMU ===")
        adaptive_bin = BUILD_DIR / "bin_adaptive_low_x86_64"
        if not adaptive_bin.exists():
            adaptive_bin = BUILD_DIR / "bin_adaptive_low"
        trace_out = run_cmd([str(adaptive_bin), "--trace-pmu"])
        with open(RES_DIR / "rq4_adaptive_solar_flare_trace.csv", "w") as f:
            f.write(trace_out)

    # =========================================================================
    # RQ5: IEEE-754 Bit-Field Vulnerability Anatomy
    # =========================================================================
    if run_all or "rq5" in rqs:
        print("=== [RQ5] Running IEEE-754 Bit-Field Vulnerability Anatomy ===")
        rq5_rows = []
        for pol in ["native", "swift", "approx_simd"]:
            fi_bin = BUILD_DIR / f"bin_{pol}_fi_fp_x86_64"
            if not fi_bin.exists():
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
    if run_all or "rq6" in rqs:
        print("=== [RQ6] Running Spatial MBU (1/2/4-bit) & Temporal Multi-Fault Burst (k=1..8) Stress Test ===")
        rq6_rows = []
        for pol in ["native", "swift", "exact_tmr", "approx_simd"]:
            fi_bin = BUILD_DIR / f"bin_{pol}_fi_fp_x86_64"
            if not fi_bin.exists():
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
    if run_all or "rq7" in rqs:
        print("=== [RQ7] Running Problem Size Scaling across Cache Hierarchy (L1 -> L2 -> L3/DRAM) ===")
        rq7_rows = []
        for pol in ["native", "eddi", "exact_tmr", "approx_scalar", "approx_simd"]:
            sbin = BUILD_DIR / f"bin_{pol}_x86_64"
            if not sbin.exists():
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

    # =========================================================================
    # RQ8: Multi-Category Instruction Breakdown + RISC-V RVV Cross-Platform Eval
    # =========================================================================
    if run_all or "rq8" in rqs:
        run_rq8_instruction_category_and_riscv(ssa_ll, cfg)

    save_or_diff_snapshot(args.save_snapshot, args.diff_snapshot)
    print("=== Requested Evaluations Completed! ===")


def parse_args():
    parser = argparse.ArgumentParser(description="Decoupled Compiler Fault-Tolerance Evaluation Harness")
    parser.add_argument("--rq", default="all",
                        help="Comma-separated RQs to run: all | rq1,rq2,rq3,rq4,rq5,rq6,rq7,rq8")
    parser.add_argument("--policy", default="all",
                        help="Comma-separated policies for RQ1 (e.g., native,swift,approx_simd)")
    parser.add_argument("--arch", default="x86_64", choices=["x86_64", "riscv64"],
                        help="Target architecture for compilation and execution")
    parser.add_argument("--fi-type", default="fp", choices=["fp", "int", "address", "branch", "all"],
                        help="Fault injection target instruction category")
    parser.add_argument("--config", default=None,
                        help="YAML 配置文件路径（默认 experiments/config/default.yaml）")
    parser.add_argument("--trials", type=int, default=None,
                        help="每个负载的注入次数（默认取配置文件的 trials）")
    parser.add_argument("--save-snapshot", default=None,
                        help="Save resulting CSVs into results/snapshots/<NAME>")
    parser.add_argument("--diff-snapshot", default=None,
                        help="Compare current results against results/snapshots/<NAME>")
    parser.add_argument("--smoke", dest="smoke", action="store_true", default=None,
                        help="Smoke 模式（默认）：只跑 RQ1、3 个策略、少量注入（~10s），"
                             "打印汇总表且不覆盖 results/*.csv")
    parser.add_argument("--full", dest="smoke", action="store_false",
                        help="全量模式：按 --rq/--trials 跑完整 campaign 并写入 results/*.csv")
    parser.add_argument("--smoke-trials", type=int, default=None,
                        help="Smoke 模式每个负载的注入次数（默认取配置文件 smoke.trials）")
    parser.add_argument("--smoke-policies", default=None,
                        help="Smoke 模式评估的策略列表（默认取配置文件 smoke.policies）")
    args = parser.parse_args()
    if args.smoke is None:
        # 默认 smoke：全量要显式 --full（全量约 8 分钟，smoke 约 10 秒）
        args.smoke = True
    return args


if __name__ == "__main__":
    run_selected_research_questions(parse_args())
