#!/usr/bin/env python3
"""便宜完整圈:FP(dmr-generic) + 地址(addr-dmr) + 整数数据(int-data-dmr) + 控制(cmp-dmr),
全是结构型检错,去 CSE 屏障。对照 int-TMR 全整数三模。看能否全覆盖且压到 ~2×。"""
import sys, csv, statistics
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import benchmark_harness as H

TRIALS = 200
CATS = ["fp64", "int_arith", "cast", "cmp_branch", "addr", "load"]
BAR = ["-atft-no-cse-barrier"]
CFGS = [
    ("native",     "native",      ["-atft-isa-avx512"]),
    ("int_tmr",    "approx-simd", ["-atft-dmr-generic","-atft-int-tmr-wire","-atft-int-tmr-i64","-atft-cmp-dmr"]+BAR),
    ("cheap_full", "approx-simd", ["-atft-dmr-generic","-atft-addr-dmr","-atft-int-data-dmr","-atft-cmp-dmr"]+BAR),
]
SSA = H.build_infrastructure()
def pct(k, n): return 100.0 * k / n if n else 0.0

res = {}; etime = {}
for tag, mode, flags in CFGS:
    for cat in CATS:
        p, f = H.compile_and_evaluate_policy(
            SSA, f"chp_{tag}_{cat}", mode, budget=1.0, fi_trials=TRIALS,
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

print("\n=== SDC% ===")
print(f"{'config':11}" + "".join(f"{c:>11}" for c in CATS) + f"{'overhead':>10}")
for tag, *_ in CFGS:
    print(f"{tag:11}" + "".join(f"{pct(res[(tag,c)][0],res[(tag,c)][2]):>11.1f}" for c in CATS)
          + f"{overhead(tag):9.2f}x")
print("\n=== DUE% ===")
for tag, *_ in CFGS:
    print(f"{tag:11}" + "".join(f"{pct(res[(tag,c)][1],res[(tag,c)][2]):>11.1f}" for c in CATS))

OUT = H.RES_DIR / "cheap_sphere.csv"
with open(OUT, "w", newline="") as fp:
    w = csv.writer(fp); w.writerow(["config","category","sdc_pct","due_pct","n","overhead"])
    for tag, *_ in CFGS:
        ov = round(overhead(tag), 3)
        for c in CATS:
            ks, kd, n = res[(tag, c)]
            w.writerow([tag, c, round(pct(ks,n),2), round(pct(kd,n),2), n, ov])
print("CSV ->", OUT)
