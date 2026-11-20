#!/usr/bin/env python3
"""层③ 整数残差码 vs 层② 整数复制:看编码检查能否既覆盖整数计算故障、又比复制便宜。
预期诚实边界:残差码检测环运算(+,−,×)链上的计算故障,但 load 故障在编码处共模,可能抓不住。"""
import sys, csv, statistics
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import benchmark_harness as H

TRIALS = 200
CATS = ["fp64", "int_arith", "load"]
BAR = ["-atft-no-cse-barrier"]
CFGS = [
    ("native",  "native",      ["-atft-isa-avx512"]),
    ("int_dmr", "approx-simd", ["-atft-dmr-generic", "-atft-int-data-dmr"] + BAR),
    ("int_res", "approx-simd", ["-atft-dmr-generic", "-atft-int-residue"] + BAR),
]
SSA = H.build_infrastructure()
def pct(k, n): return 100.0 * k / n if n else 0.0

res = {}; etime = {}
for tag, mode, flags in CFGS:
    for cat in CATS:
        p, f = H.compile_and_evaluate_policy(
            SSA, f"res_{tag}_{cat}", mode, budget=1.0, fi_trials=TRIALS,
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

print("\n=== SDC% / overhead ===")
print(f"{'config':9}" + "".join(f"{c:>12}" for c in CATS) + f"{'overhead':>10}")
for tag, *_ in CFGS:
    print(f"{tag:9}" + "".join(f"{pct(res[(tag,c)][0],res[(tag,c)][3]):>12.1f}" for c in CATS)
          + f"{overhead(tag):9.2f}x")
print("\n=== int_arith / load 细分(SDC/DUE/det_rec) ===")
for cat in ["int_arith", "load"]:
    print(f"  [{cat}]")
    for tag, *_ in CFGS:
        ks, kd, krec, n = res[(tag, cat)]
        print(f"    {tag:9} SDC {pct(ks,n):5.1f}  DUE {pct(kd,n):5.1f}  det_rec {pct(krec,n):5.1f}")

OUT = H.RES_DIR / "int_residue.csv"
with open(OUT, "w", newline="") as fp:
    w = csv.writer(fp); w.writerow(["config","category","sdc_pct","due_pct","det_rec_pct","n","overhead"])
    for tag, *_ in CFGS:
        ov = round(overhead(tag), 3)
        for c in CATS:
            ks, kd, krec, n = res[(tag, c)]
            w.writerow([tag, c, round(pct(ks,n),2), round(pct(kd,n),2), round(pct(krec,n),2), n, ov])
print("CSV ->", OUT)
