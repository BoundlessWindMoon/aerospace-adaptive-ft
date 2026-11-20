#!/usr/bin/env python3
"""把完整圈 {V,A,C} 的开销压到最好:叠加代码里已有的便宜变体,
测 6 类别残余不掉覆盖。先优化到极限,再看剩什么问题(那才是学术问题)。
便宜旋钮:no-cse-barrier(去屏障,~2×降税,已标注对 approx-simd+int-tmr 安全)、
int-tmr-boundary(只在边界投票)、int-dmr(双份+重算,替三模)。"""
import sys, csv, statistics
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import benchmark_harness as H

TRIALS = 200
CATS = ["fp64", "int_arith", "cast", "cmp_branch", "addr", "load"]
BASE = ["-atft-dmr-generic", "-atft-int-tmr-wire", "-atft-int-tmr-i64", "-atft-cmp-tmr"]
CFGS = [
    ("native", "native",      ["-atft-isa-avx512"], True),
    ("full",   "approx-simd", BASE, False),
    ("nobar",  "approx-simd", BASE + ["-atft-no-cse-barrier"], False),
    ("bound",  "approx-simd", BASE + ["-atft-no-cse-barrier", "-atft-int-tmr-boundary"], False),
    ("intdmr", "approx-simd", BASE + ["-atft-no-cse-barrier", "-atft-int-dmr"], False),
]
SSA = H.build_infrastructure()
def pct(k, n): return 100.0 * k / n if n else 0.0

res = {}; etime = {}
for tag, mode, flags, gold in CFGS:
    for cat in CATS:
        p, f = H.compile_and_evaluate_policy(
            SSA, f"opt_{tag}_{cat}", mode, budget=1.0, fi_trials=TRIALS,
            run_fi=True, fi_target_type=cat, dump_golden=True, pass_flags=flags)
        ks = sum(round(r["sdc_pct"] / 100 * r["trials"]) for r in f)
        kd = sum(round(r["due_pct"] / 100 * r["trials"]) for r in f)
        n = sum(r["trials"] for r in f)
        res[(tag, cat)] = (ks, kd, n)
        if cat == "fp64":
            etime[tag] = {r["benchmark"]: r["exec_time_us"] for r in p}

def overhead(tag):
    bt = etime["native"]; pt = etime[tag]
    rs = [pt[b] / bt[b] for b in bt if b in pt and bt[b] > 0]
    return statistics.median(rs) if rs else float("nan")

print("\n=== SDC% 各优化变体 (对自身 golden) ===")
print(f"{'config':9}" + "".join(f"{c:>11}" for c in CATS) + f"{'overhead':>10}")
for tag, *_ in CFGS:
    row = f"{tag:9}" + "".join(f"{pct(res[(tag,c)][0],res[(tag,c)][2]):>11.1f}" for c in CATS)
    print(row + f"{overhead(tag):9.2f}x")
print("\n=== DUE% ===")
print(f"{'config':9}" + "".join(f"{c:>11}" for c in CATS))
for tag, *_ in CFGS:
    print(f"{tag:9}" + "".join(f"{pct(res[(tag,c)][1],res[(tag,c)][2]):>11.1f}" for c in CATS))

OUT = H.RES_DIR / "sphere_opt.csv"
with open(OUT, "w", newline="") as fp:
    w = csv.writer(fp); w.writerow(["config", "category", "sdc_pct", "due_pct", "n", "overhead"])
    for tag, *_ in CFGS:
        ov = round(overhead(tag), 3)
        for c in CATS:
            ks, kd, n = res[(tag, c)]
            w.writerow([tag, c, round(pct(ks, n), 2), round(pct(kd, n), 2), n, ov])
print("CSV ->", OUT)
