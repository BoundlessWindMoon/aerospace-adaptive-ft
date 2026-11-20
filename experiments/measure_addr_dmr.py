#!/usr/bin/env python3
"""地址平面:addr-DMR(只复制 GEP 下标切片+检错)vs int-TMR(全整数三模)。
均在 dmr-generic(FP) + cmp-dmr(控制) + no-cse-barrier 的圈上,只换地址平面的机制。
看 addr/cast/int 覆盖掉不掉(检错应记 det_recovered)、开销能否从 int-TMR 降下来。"""
import sys, csv, statistics
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import benchmark_harness as H

TRIALS = 200
CATS = ["fp64", "int_arith", "cast", "cmp_branch", "addr", "load"]
COMMON = ["-atft-dmr-generic", "-atft-cmp-dmr", "-atft-no-cse-barrier"]
CFGS = [
    ("native",   "native",      ["-atft-isa-avx512"]),
    ("int_tmr",  "approx-simd", COMMON + ["-atft-int-tmr-wire", "-atft-int-tmr-i64"]),
    ("addr_dmr", "approx-simd", COMMON + ["-atft-addr-dmr"]),
]
SSA = H.build_infrastructure()
def pct(k, n): return 100.0 * k / n if n else 0.0

res = {}; etime = {}
for tag, mode, flags in CFGS:
    for cat in CATS:
        p, f = H.compile_and_evaluate_policy(
            SSA, f"adr_{tag}_{cat}", mode, budget=1.0, fi_trials=TRIALS,
            run_fi=True, fi_target_type=cat, dump_golden=True, pass_flags=flags)
        ks = sum(round(r["sdc_pct"] / 100 * r["trials"]) for r in f)
        kd = sum(round(r["due_pct"] / 100 * r["trials"]) for r in f)
        krec = sum(round(r["det_recovered_pct"] / 100 * r["trials"]) for r in f)
        n = sum(r["trials"] for r in f)
        res[(tag, cat)] = (ks, kd, krec, n)
        if cat == "fp64":
            etime[tag] = {r["benchmark"]: r["exec_time_us"] for r in p}

def overhead(tag):
    bt = etime["native"]; pt = etime[tag]
    rs = [pt[b] / bt[b] for b in bt if b in pt and bt[b] > 0]
    return statistics.median(rs) if rs else float("nan")

print("\n=== SDC% (对自身 golden) ===")
print(f"{'config':9}" + "".join(f"{c:>11}" for c in CATS) + f"{'overhead':>10}")
for tag, *_ in CFGS:
    print(f"{tag:9}" + "".join(f"{pct(res[(tag,c)][0],res[(tag,c)][3]):>11.1f}" for c in CATS)
          + f"{overhead(tag):9.2f}x")
print("\n=== DUE% ===")
for tag, *_ in CFGS:
    print(f"{tag:9}" + "".join(f"{pct(res[(tag,c)][1],res[(tag,c)][3]):>11.1f}" for c in CATS))
print("\n=== addr/cast/int 细分(SDC / DUE / det_recovered) ===")
for cat in ["addr", "cast", "int_arith"]:
    print(f"  [{cat}]")
    for tag, *_ in CFGS:
        ks, kd, krec, n = res[(tag, cat)]
        print(f"    {tag:9} SDC {pct(ks,n):5.1f}  DUE {pct(kd,n):5.1f}  det_rec {pct(krec,n):5.1f}")

OUT = H.RES_DIR / "addr_dmr.csv"
with open(OUT, "w", newline="") as fp:
    w = csv.writer(fp); w.writerow(["config","category","sdc_pct","due_pct","det_rec_pct","n","overhead"])
    for tag, *_ in CFGS:
        ov = round(overhead(tag), 3)
        for c in CATS:
            ks, kd, krec, n = res[(tag, c)]
            w.writerow([tag, c, round(pct(ks,n),2), round(pct(kd,n),2), round(pct(krec,n),2), n, ov])
print("CSV ->", OUT)
