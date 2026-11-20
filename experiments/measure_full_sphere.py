#!/usr/bin/env python3
"""完整圈 {V,A,C} 接入向量化便宜路径后的 6 类别残余 + 开销。
V=dmr/tmr-generic(FP value),A=int-TMR(整数/cast/地址 load),C=cmp-TMR(控制)。
给出框架的最终效果剖面:便宜圈把哪些穿越口都纳入后,每类残余 SDC/DUE。"""
import sys, csv
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import benchmark_harness as H

TRIALS = 200
CATS = ["fp64", "int_arith", "cast", "cmp_branch", "addr", "load"]
A = ["-atft-int-tmr-wire", "-atft-int-tmr-i64"]
C = ["-atft-cmp-tmr"]
CFGS = [
    ("native",   "native",      ["-atft-isa-avx512"], True),
    ("full_dmr", "approx-simd", ["-atft-dmr-generic"] + A + C, False),
    ("full_tmr", "approx-simd", ["-atft-dmr-generic", "-atft-tmr-generic"] + A + C, False),
]
SSA = H.build_infrastructure()
def pct(k, n): return 100.0 * k / n if n else 0.0

res = {}; etime = {}
for tag, mode, flags, gold in CFGS:
    for ci, cat in enumerate(CATS):
        p, f = H.compile_and_evaluate_policy(
            SSA, f"full_{tag}_{cat}", mode, budget=1.0, fi_trials=TRIALS,
            run_fi=True, fi_target_type=cat, dump_golden=True, pass_flags=flags)
        ks = sum(round(r["sdc_pct"] / 100 * r["trials"]) for r in f)
        kd = sum(round(r["due_pct"] / 100 * r["trials"]) for r in f)
        n = sum(r["trials"] for r in f)
        res[(tag, cat)] = (ks, kd, n)
        if cat == "fp64":
            etime[tag] = {r["benchmark"]: r["exec_time_us"] for r in p}

import statistics
def overhead(tag):
    bt = etime["native"]; pt = etime[tag]
    rs = [pt[b] / bt[b] for b in bt if b in pt and bt[b] > 0]
    return statistics.median(rs) if rs else float("nan")

print("\n=== SDC% 完整圈 6 类别 (对自身 golden, Wilson CI) ===")
print(f"{'config':10}" + "".join(f"{c:>13}" for c in CATS) + f"{'overhead':>10}")
for tag, *_ in CFGS:
    row = f"{tag:10}"
    for c in CATS:
        ks, _, n = res[(tag, c)]; _, _, h = H.wilson_ci95(ks, n)
        row += f"{pct(ks,n):8.1f}±{h:3.1f}"
    row += f"{overhead(tag):9.2f}x"
    print(row)

print("\n=== DUE% ===")
print(f"{'config':10}" + "".join(f"{c:>13}" for c in CATS))
for tag, *_ in CFGS:
    print(f"{tag:10}" + "".join(f"{pct(res[(tag,c)][1],res[(tag,c)][2]):>13.1f}" for c in CATS))

OUT = H.RES_DIR / "full_sphere.csv"
with open(OUT, "w", newline="") as fp:
    w = csv.writer(fp); w.writerow(["config", "category", "sdc_pct", "due_pct", "n", "overhead"])
    for tag, *_ in CFGS:
        ov = round(overhead(tag), 3)
        for c in CATS:
            ks, kd, n = res[(tag, c)]
            w.writerow([tag, c, round(pct(ks, n), 2), round(pct(kd, n), 2), n, ov])
print("CSV ->", OUT)
