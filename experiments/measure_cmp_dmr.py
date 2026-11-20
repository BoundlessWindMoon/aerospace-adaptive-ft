#!/usr/bin/env python3
"""控制平面:cmp-DMR(复制一份检错)vs cmp-TMR(三模多数决)的开销与覆盖。
均在 no-cse-barrier + int-TMR 的优化圈上叠加。看控制从 TMR 换 DMR 能省多少、覆盖掉不掉。
注:cmp-DMR 检错后交回滚,harness 里应记为 det_recovered 而非 SDC。"""
import sys, csv, statistics
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import benchmark_harness as H

TRIALS = 200
CATS = ["fp64", "int_arith", "cast", "cmp_branch", "addr", "load"]
OPT = ["-atft-dmr-generic", "-atft-int-tmr-wire", "-atft-int-tmr-i64", "-atft-no-cse-barrier"]
CFGS = [
    ("native", "native",      ["-atft-isa-avx512"], ),
    ("cmp_tmr", "approx-simd", OPT + ["-atft-cmp-tmr"]),
    ("cmp_dmr", "approx-simd", OPT + ["-atft-cmp-dmr"]),
]
SSA = H.build_infrastructure()
def pct(k, n): return 100.0 * k / n if n else 0.0

res = {}; etime = {}
for tag, mode, flags in CFGS:
    for cat in CATS:
        p, f = H.compile_and_evaluate_policy(
            SSA, f"cdmr_{tag}_{cat}", mode, budget=1.0, fi_trials=TRIALS,
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
print("\n=== cmp_branch 细分 ===")
for tag, *_ in CFGS:
    ks, kd, krec, n = res[(tag, "cmp_branch")]
    print(f"  {tag:9} SDC {pct(ks,n):5.1f}%  DUE {pct(kd,n):5.1f}%  det_recovered {pct(krec,n):5.1f}%")

OUT = H.RES_DIR / "cmp_dmr.csv"
with open(OUT, "w", newline="") as fp:
    w = csv.writer(fp); w.writerow(["config","category","sdc_pct","due_pct","det_rec_pct","n","overhead"])
    for tag, *_ in CFGS:
        ov = round(overhead(tag), 3)
        for c in CATS:
            ks, kd, krec, n = res[(tag, c)]
            w.writerow([tag, c, round(pct(ks,n),2), round(pct(kd,n),2), round(pct(krec,n),2), n, ov])
print("CSV ->", OUT)
