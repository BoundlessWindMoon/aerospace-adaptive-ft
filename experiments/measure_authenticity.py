#!/usr/bin/env python3
"""数据真实性复核：对三个 headline 负载做 SDC 容差扫描 + 防御面指令占比。

动机：native Kalman 在峰值 burst 下报 SDC~0%，怀疑是 rel_l2 容差(默认 1%)太松，
把已经发散的状态估计也算成 benign。做法：固定注入种子，把 ATFT_SDC_REL_ERR 从
1% 收紧到 1e-4，看 SDC 占比是否随容差抬升——若几乎不动，则鲁棒性为真；若台阶式
跳升，则原 0% 是容差口径虚高。

同时用 FI pass 的 FI_STATIC_SITES / 指令总数口径，量每个策略的"防御面"：
保护插桩指令 / 原始程序指令。防御面越小越好。
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

POLICIES = [("native", "native"), ("exact_tmr", "exact-tmr"), ("approx_simd", "approx-simd")]
KERNELS = ["gnc_kalman", "cnn_cls_decision", "pb_gemm"]
TOLS = ["1e-2", "1e-3", "1e-4"]


def run(cmd, **kw):
    return subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, **kw)


def build_policy(tag, mode):
    sut = BUILD / f"sut_auth_{tag}.ll"
    fi = BUILD / f"sut_auth_{tag}_fi.ll"
    binp = BUILD / f"bin_auth_{tag}"
    r = run([OPT, f"-load-pass-plugin={PASS_POLICY}", "-passes=adaptive-ft,verify",
             f"-atft-mode={mode}", "-atft-rank=semantic", "-atft-budget=1.0",
             "-atft-delta=5e-5", "-atft-w1=2.5", "-atft-w2=1.8", "-atft-w3=0.8",
             "-S", str(SSA), "-o", str(sut)])
    if r.returncode:
        print(r.stderr[-2000:]); sys.exit(f"policy transform failed: {tag}")
    r = run([OPT, f"-load-pass-plugin={PASS_FI}", "-passes=standalone-fi,verify",
             "-fi-target-type=all", "-S", str(sut), "-o", str(fi)])
    if r.returncode:
        print(r.stderr[-2000:]); sys.exit(f"FI instrument failed: {tag}")
    static_sites = {}
    for line in r.stderr.splitlines():
        if line.startswith("FI_STATIC_SITES,"):
            _, cat, cnt = line.split(",")
            static_sites[cat] = int(cnt)
    r = run([CLANG, "-O3", "-march=native", "experiments/eval_suite.c", str(fi),
             str(LIB_ATFT), str(LIB_HARNESS), *INC, "-lm", "-o", str(binp)])
    if r.returncode:
        print(r.stderr[-2000:]); sys.exit(f"compile failed: {tag}")
    return binp, static_sites


def stress(binp, tag, kernel, tol):
    env = dict(os.environ)
    env["ATFT_BENCH_SUBSET"] = kernel
    env["ATFT_STRESS_TRIALS"] = "200"
    env["ATFT_BURST_COUNTS"] = "1,2,4,8"
    env["ATFT_SDC_REL_ERR"] = tol
    r = subprocess.run([str(binp), "--stress", tag], cwd=ROOT, capture_output=True,
                       text=True, env=env, timeout=600)
    rows = {}
    for line in r.stdout.splitlines():
        if not line.startswith("STRESS_CSV,"):
            continue
        p = line.split(",")
        # policy,group,label,trials,ok,det_rec,sdc,due
        rows[(p[2], p[3])] = (float(p[6]), float(p[8]))  # label -> (ok%, sdc%)
    return rows


def main():
    print("=== 构建三策略(fi-target-type=all)并统计防御面 ===")
    bins, sites = {}, {}
    orig = None
    for tag, mode in POLICIES:
        b, s = build_policy(tag, mode)
        bins[tag] = b
        sites[tag] = s
        tot = sum(s.values())
        if tag == "native":
            orig = tot  # native 的 hookable 指令数 ≈ 原始程序动态可注入面
        print(f"  {tag:12} 静态注入点(按类别) 合计={tot}  {s}")

    print("\n=== 防御面指令占比 (保护插桩相对 native 的增量) ===")
    print(f"{'策略':14}{'注入点合计':>10}{'相对native增量':>16}")
    for tag, _ in POLICIES:
        tot = sum(sites[tag].values())
        delta = (tot - orig) / orig * 100.0 if orig else 0.0
        print(f"{tag:14}{tot:>10}{delta:>15.1f}%")
    print("注：fi-target-type=all 下注入点=全部可注入指令；exact/approx 的增量即保护码引入的额外指令面。")

    print("\n=== SDC 容差扫描 (峰值 burst, 固定种子, 200 trials/点) ===")
    for kernel in KERNELS:
        print(f"\n--- {kernel} ---")
        hdr = "label".ljust(22) + "".join(f"{t:>12}" for t in TOLS)
        for tag, _ in POLICIES:
            print(f"[{tag}]")
            print("  " + hdr)
            by_tol = {t: stress(bins[tag], tag, kernel, t) for t in TOLS}
            labels = list(next(iter(by_tol.values())).keys())
            for lab in labels:
                cells = []
                for t in TOLS:
                    ok, sdc = by_tol[t].get(lab, (float("nan"), float("nan")))
                    cells.append(f"{sdc:>11.1f}")
                print("  " + f"{lab[0]+'/'+lab[1]:22}" + "".join(cells))
    print("\n解读：若某行 SDC 随容差收紧(1e-2→1e-4)几乎不变，则该数字是真实鲁棒性；")
    print("若明显抬升，则宽容差把发散结果误判为 benign，原结论需按收紧口径修正。")


if __name__ == "__main__":
    main()

