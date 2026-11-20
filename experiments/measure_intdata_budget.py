#!/usr/bin/env python3
"""选择性整数数据保护(Shoestring 思路):按 SDC 关键度只保护 top 比例的整数 store,
其余靠症状(崩溃=DUE→回滚)自现。扫 budget,看 int/load SDC 与开销的权衡曲线。
整数故障本就 ~半数直接崩溃,故静默 SDC 集中在少数值上,低 budget 应已抓住大部分。"""
import sys, csv, statistics
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import benchmark_harness as H

TRIALS = 200
CATS = ["fp64", "int_arith", "load", "cast"]
COMMON = ["-atft-dmr-generic", "-atft-addr-dmr", "-atft-int-data-dmr",
          "-atft-cmp-dmr", "-atft-no-cse-barrier"]
CFGS = [
    ("native", "native",      ["-atft-isa-avx512"]),
    ("b1.00",  "approx-simd", COMMON),
    ("b0.50",  "approx-simd", COMMON + ["-atft-int-data-budget=0.5"]),
    ("b0.25",  "approx-simd", COMMON + ["-atft-int-data-budget=0.25"]),
    ("b0.10",  "approx-simd", COMMON + ["-atft-int-data-budget=0.1"]),
]
SSA = H.build_infrastructure()
def pct(k, n): return 100.0 * k / n if n else 0.0

res = {}; etime = {}
for tag, mode, flags in CFGS:
    for cat in CATS:
        p, f = H.compile_and_evaluate_policy(
            SSA, f"idb_{tag}_{cat}", mode, budget=1.0, fi_trials=TRIALS,
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

print("\n=== SDC% vs 整数数据保护 budget ===")
print(f"{'config':8}" + "".join(f"{c:>11}" for c in CATS) + f"{'overhead':>10}")
for tag, *_ in CFGS:
    print(f"{tag:8}" + "".join(f"{pct(res[(tag,c)][0],res[(tag,c)][2]):>11.1f}" for c in CATS)
          + f"{overhead(tag):9.2f}x")
print("\n=== DUE% (崩溃=症状,整数靠它兜底) ===")
for tag, *_ in CFGS:
    print(f"{tag:8}" + "".join(f"{pct(res[(tag,c)][1],res[(tag,c)][2]):>11.1f}" for c in CATS))

OUT = H.RES_DIR / "intdata_budget.csv"
with open(OUT, "w", newline="") as fp:
    w = csv.writer(fp); w.writerow(["config","category","sdc_pct","due_pct","n","overhead"])
    for tag, *_ in CFGS:
        ov = round(overhead(tag), 3)
        for c in CATS:
            ks, kd, n = res[(tag, c)]
            w.writerow([tag, c, round(pct(ks,n),2), round(pct(kd,n),2), n, ov])
print("CSV ->", OUT)
