#!/usr/bin/env python3
"""全指令 SDC:物理真实口径(-fi-blind + fi-target-type=all),故障可落在保护码自身。

逐类别口径豁免 voter.internal,保护码不被注入 → 部署栈看起来 1.16×/~0 SDC。
真实 SEU 不认元数据,会打中副本/表决器/校验和。本脚本用 -fi-blind 把这些也纳入注入池,
单故障(k=1)均匀注入,看各策略 SDC/DUE,并和防御面(指令增量)对齐,量"保护码自身易损"的净效应。
"""
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / "build"
CLANG = os.environ.get("ATFT_CLANG", "clang-15")
OPT = os.environ.get("ATFT_OPT", "opt-15")
PASS_POLICY = BUILD / "libAdaptiveFTPass.so"
PASS_FI = BUILD / "libFaultInjectorPass.so"
LIB_ATFT = BUILD / "libatft_rt.a"
LIB_HARNESS = BUILD / "libharness_rt.a"
SSA = BUILD / "benchmarks_ssa.ll"
INC = ["-Iinclude", "-Iharness", "-Ibenchmarks"]

STRUCT = ["-atft-dmr-generic", "-atft-addr-dmr", "-atft-int-residue",
          "-atft-cmp-dmr", "-atft-abft-insert", "-atft-layer-exclusive",
          "-atft-no-cse-barrier"]
# (tag, mode, extra_flags)
POLICIES = [
    ("native",      "native",      []),
    ("exact_tmr",   "exact-tmr",   []),
    ("approx_simd", "approx-simd", []),
    ("struct_full", "approx-simd", STRUCT),
]
KERNELS = ["pb_gemm", "gnc_kalman", "orbit_rk4", "aocs_pid", "cnn_cls_decision"]
TRIALS = "500"


def run(cmd):
    return subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)


def build_blind(tag, mode, extra):
    sut = BUILD / f"sut_blind_{tag}.ll"
    fi = BUILD / f"sut_blind_{tag}_fi.ll"
    binp = BUILD / f"bin_blind_{tag}"
    r = run([OPT, f"-load-pass-plugin={PASS_POLICY}", "-passes=adaptive-ft,verify",
             f"-atft-mode={mode}", *extra, "-atft-rank=semantic", "-atft-budget=1.0",
             "-atft-delta=5e-5", "-atft-w1=2.5", "-atft-w2=1.8", "-atft-w3=0.8",
             "-S", str(SSA), "-o", str(sut)])
    if r.returncode:
        print(r.stderr[-1500:]); sys.exit(f"policy fail {tag}")
    # -fi-blind:不豁免 voter.internal,保护码一并纳入注入池
    r = run([OPT, f"-load-pass-plugin={PASS_FI}", "-passes=standalone-fi,verify",
             "-fi-target-type=all", "-fi-blind", "-S", str(sut), "-o", str(fi)])
    if r.returncode:
        print(r.stderr[-1500:]); sys.exit(f"fi fail {tag}")
    r = run([CLANG, "-O3", "-march=native", "experiments/eval_suite.c", str(fi),
             str(LIB_ATFT), str(LIB_HARNESS), *INC, "-lm", "-o", str(binp)])
    if r.returncode:
        print(r.stderr[-1500:]); sys.exit(f"cc fail {tag}")
    return binp


def stress_k1(binp, tag, kernel):
    env = dict(os.environ)
    env.update({"ATFT_BENCH_SUBSET": kernel, "ATFT_STRESS_TRIALS": TRIALS,
                "ATFT_BURST_COUNTS": "1", "ATFT_SDC_REL_ERR": "1e-2"})
    r = subprocess.run([str(binp), "--stress", tag], cwd=ROOT, capture_output=True,
                       text=True, env=env, timeout=900)
    for line in r.stdout.splitlines():
        if line.startswith("STRESS_CSV,") and "Temporal_Burst" in line:
            p = line.split(",")
            return float(p[6]), float(p[7])  # sdc%, due%
    return float("nan"), float("nan")


def main():
    print("=== 构建 4 策略 (fi-target-type=all + fi-blind, 保护码也可被打) ===")
    bins = {tag: build_blind(tag, mode, extra) for tag, mode, extra in POLICIES}
    print("  done\n")

    print(f"=== 全指令单故障 SDC/DUE% (k=1, {TRIALS} trials/核, 物理真实口径) ===")
    tags = [t for t, _, _ in POLICIES]
    print(f"{'kernel':18}" + "".join(f"{t:>16}" for t in tags))
    agg = {t: [0.0, 0.0, 0] for t in tags}
    for k in KERNELS:
        cells = []
        for t in tags:
            sdc, due = stress_k1(bins[t], t, k)
            cells.append(f"{sdc:>6.1f}/{due:<5.1f}")
            agg[t][0] += sdc; agg[t][1] += due; agg[t][2] += 1
        print(f"{k:18}" + "".join(f"{c:>16}" for c in cells))
    print(f"{'平均 SDC/DUE':18}" + "".join(
        f"{(str(round(agg[t][0]/agg[t][2],1))+'/'+str(round(agg[t][1]/agg[t][2],1))):>16}"
        for t in tags))
    print("\n格式 = SDC%/DUE%。对比逐类别(豁免保护码)口径:struct_full 在那口径下 SDC≈0;")
    print("此处保护码纳入注入池后,看它的大防御面是否把 SDC 反推高 → 验证'保护码自身易损'。")


if __name__ == "__main__":
    main()
