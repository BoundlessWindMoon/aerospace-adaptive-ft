#!/usr/bin/env python3
"""把 opt_i64 全家桶逐层剥开：在 simd-f64 + 去屏障的底座上，依次加 FP数据通路 / cmp三模 /
整数通路 / i64下标，量每层的 时间税 与 fp64/int_arith 两类覆盖，看 5.75× 税花在哪、买到啥。"""
import math, sys, csv
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import benchmark_harness as H

BASE = ["-atft-delta-runtime", "-atft-replica-exact", "-atft-simd-f64", "-atft-no-cse-barrier"]
CMP  = ["-atft-cmp-tmr"]
IW   = ["-atft-int-tmr-wire"]
I64  = ["-atft-int-tmr-i64"]
TRIALS = 150
TARGETS = ["fp64", "int_arith"]

CFGS = [
    ("native",          "native",      [],                1.0),
    ("fp_only",         "approx-simd", BASE,              1.0),
    ("fp_cmp",          "approx-simd", BASE+CMP,          1.0),
    ("fp_cmp_iw",       "approx-simd", BASE+CMP+IW,       1.0),
    ("fp_cmp_iw_i64",   "approx-simd", BASE+CMP+IW+I64,   1.0),
]

SSA = H.build_infrastructure()
T, S = {}, {}
for tag, mode, flags, bud in CFGS:
    T[tag] = {}
    S[tag] = {t: {} for t in TARGETS}
    for tgt in TARGETS:
        p, f = H.compile_and_evaluate_policy(
            SSA, f"dec_{tag}_{tgt}", mode, budget=bud, fi_trials=TRIALS,
            run_fi=True, fi_target_type=tgt, pass_flags=flags)
        for r in p:
            T[tag][r["benchmark"]] = r["exec_time_us"]
        S[tag][tgt] = {r["benchmark"]: r["sdc_pct"] for r in f}

def geomean(xs):
    xs = [x for x in xs if x > 0]
    return math.exp(sum(map(math.log, xs))/len(xs)) if xs else float("nan")

benches = list(T["native"].keys())
def msdc(tag, tgt):
    v = [S[tag][tgt][b] for b in benches if b in S[tag][tgt]]
    return (sum(v)/len(v), max(v))

print(f"\n{'config':16} {'t_tax':>7}  {'fp64 mean/max':>15}  {'int mean/max':>15}")
rows = []
for tag, *_ in CFGS:
    if tag == "native":
        fm, fx = msdc(tag, "fp64"); im, ix = msdc(tag, "int_arith")
        print(f"{tag:16} {'1.00':>7}  {fm:>6.2f}/{fx:<7.2f}  {im:>6.2f}/{ix:<7.2f} (未保护)")
        continue
    tt = geomean([T[tag][b]/T['native'][b] for b in benches if b in T[tag]])
    fm, fx = msdc(tag, "fp64"); im, ix = msdc(tag, "int_arith")
    print(f"{tag:16} {tt:>7.2f}  {fm:>6.2f}/{fx:<7.2f}  {im:>6.2f}/{ix:<7.2f}")
    rows.append({"config": tag, "time_tax_geo": round(tt,3),
                 "fp64_sdc_mean": round(fm,3), "fp64_sdc_max": round(fx,3),
                 "int_sdc_mean": round(im,3), "int_sdc_max": round(ix,3)})

csv_path = Path(H.ROOT_DIR) / "results" / "opt" / "component_decomp.csv"
with open(csv_path, "w", newline="") as fh:
    w = csv.DictWriter(fh, fieldnames=["config","time_tax_geo","fp64_sdc_mean","fp64_sdc_max","int_sdc_mean","int_sdc_max"])
    w.writeheader(); w.writerows(rows)
print(f"\nwrote {csv_path}")
