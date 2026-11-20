#!/usr/bin/env python3
"""给 struct_full 补上 load 的 nZDC reload-compare,量两个端到端锚点:
留 load 洞(便宜)vs 补 load(全六类)。供上层自适应选区间。"""
import sys, csv, statistics
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import benchmark_harness as H

TRIALS = 200
CATS = ["fp64", "int_arith", "cast", "cmp_branch", "addr", "load"]
BASE = ["-atft-dmr-generic", "-atft-addr-dmr", "-atft-int-residue", "-atft-cmp-dmr",
        "-atft-no-cse-barrier"]
CFGS = [
    ("native",         "native",      ["-atft-isa-avx512"]),
    ("struct_full",    "approx-simd", BASE),
    ("struct_full_ldr","approx-simd", BASE + ["-atft-load-reload"]),
]
SSA = H.build_infrastructure()
def pct(k, n): return 100.0 * k / n if n else 0.0

res = {}; etime = {}
for tag, mode, flags in CFGS:
    for cat in CATS:
        p, f = H.compile_and_evaluate_policy(
            SSA, f"eld_{tag}_{cat}", mode, budget=1.0, fi_trials=TRIALS,
            run_fi=True, fi_target_type=cat, dump_golden=True, pass_flags=flags)
        ks = sum(round(r["sdc_pct"] / 100 * r["trials"]) for r in f)
        kd = sum(round(r["due_pct"] / 100 * r["trials"]) for r in f)
        n = sum(r["trials"] for r in f)
        res[(tag, cat)] = (ks, kd, n)
        if cat == "fp64":
            etime[tag] = {r["benchmark"]: r["exec_time_us"] for r in p}

def ov(tag):
    bt = etime["native"]; pt = etime[tag]
    rs = [pt[b] / bt[b] for b in bt if b in pt and bt[b] > 0]
    return statistics.median(rs) if rs else float("nan")

print("\n=== 端到端 SDC% + 开销(两个锚点) ===")
print(f"{'config':17}" + "".join(f"{c:>11}" for c in CATS) + f"{'overhead':>10}")
for tag, *_ in CFGS:
    print(f"{tag:17}" + "".join(f"{pct(res[(tag,c)][0],res[(tag,c)][2]):>11.1f}" for c in CATS)
          + f"{ov(tag):9.2f}x")

OUT = H.RES_DIR / "e2e_load.csv"
with open(OUT, "w", newline="") as fp:
    w = csv.writer(fp); w.writerow(["config","category","sdc_pct","due_pct","n","overhead"])
    for tag, *_ in CFGS:
        o = round(ov(tag), 3)
        for c in CATS:
            ks, kd, n = res[(tag, c)]
            w.writerow([tag, c, round(pct(ks,n),2), round(pct(kd,n),2), n, o])
print("CSV ->", OUT)
