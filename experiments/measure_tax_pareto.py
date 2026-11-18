#!/usr/bin/env python3
"""税-覆盖 Pareto：在确认可去的屏障基础上，叠 边界校验 / 选择性预算 / SIMD-f64 宽度，
量 时间税(主) + 指令税 + SDC 覆盖(fp64/int_arith)。目标：看哪条组合把单核税压进粗粒度 3× 以下。"""
import math, sys, csv
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import benchmark_harness as H

OPT = ["-atft-cmp-tmr", "-atft-delta-runtime", "-atft-replica-exact",
       "-atft-int-tmr-wire", "-atft-int-tmr-i64"]
NB  = ["-atft-no-cse-barrier"]
BND = ["-atft-int-tmr-boundary"]
F64 = ["-atft-simd-f64"]
TRIALS = 120
TARGETS = ["fp64", "int_arith"]

# (tag, mode, extra_flags, budget)
CFGS = [
    ("native",        "native",      [],              1.0),
    ("ref_bar",       "approx-simd", OPT,             1.0),
    ("nobar",         "approx-simd", OPT+NB,          1.0),
    ("nobar_bnd",     "approx-simd", OPT+NB+BND,      1.0),
    ("nobar_bnd_b50", "approx-simd", OPT+NB+BND,      0.5),
    ("nobar_bnd_b30", "approx-simd", OPT+NB+BND,      0.3),
    ("f64_bar",       "approx-simd", OPT+F64,         1.0),
    ("f64_nobar",     "approx-simd", OPT+F64+NB,      1.0),
]

SSA = H.build_infrastructure()
# time[tag][bench]=us, inst[tag][bench]=insts, sdc[tag][target][bench]=pct
T, I, S = {}, {}, {}
for tag, mode, flags, bud in CFGS:
    T[tag], I[tag], S[tag] = {}, {}, {}
    for tgt in TARGETS:
        p, f = H.compile_and_evaluate_policy(
            SSA, f"{tag}_{tgt}", mode, budget=bud, fi_trials=TRIALS,
            run_fi=True, fi_target_type=tgt, pass_flags=flags)
        for r in p:
            T[tag][r["benchmark"]] = r["exec_time_us"]
            I[tag][r["benchmark"]] = r["hw_insts"]
        S[tag][tgt] = {r["benchmark"]: r["sdc_pct"] for r in f}

def geomean(xs):
    xs = [x for x in xs if x > 0]
    return math.exp(sum(math.log(x) for x in xs)/len(xs)) if xs else float("nan")

benches = list(T["native"].keys())
out = ROWS = []
print(f"\n{'config':16} {'t_tax(geo)':>11} {'i_tax(geo)':>11} "
      f"{'maxSDC%':>8} {'meanSDC%':>9}  (SDC 越低覆盖越好)")
for tag, *_ in CFGS:
    if tag == "native":
        sdcs = [S[tag][t][b] for t in TARGETS for b in benches if b in S[tag][t]]
        print(f"{tag:16} {'1.00':>11} {'1.00':>11} "
              f"{max(sdcs):>8.2f} {sum(sdcs)/len(sdcs):>9.2f}  (未保护基线)")
        continue
    ttax = geomean([T[tag][b]/T['native'][b] for b in benches if b in T[tag]])
    itax = geomean([I[tag][b]/I['native'][b] for b in benches if b in I[tag]])
    sdcs = [S[tag][t][b] for t in TARGETS for b in benches if b in S[tag][t]]
    print(f"{tag:16} {ttax:>11.2f} {itax:>11.2f} {max(sdcs):>8.2f} {sum(sdcs)/len(sdcs):>9.2f}")
    ROWS.append({"config": tag, "time_tax_geo": round(ttax,3), "inst_tax_geo": round(itax,3),
                 "max_sdc_pct": round(max(sdcs),3), "mean_sdc_pct": round(sum(sdcs)/len(sdcs),3)})

csv_path = Path(H.ROOT_DIR) / "results" / "opt" / "pareto_tax.csv"
with open(csv_path, "w", newline="") as fh:
    w = csv.DictWriter(fh, fieldnames=["config","time_tax_geo","inst_tax_geo","max_sdc_pct","mean_sdc_pct"])
    w.writeheader(); w.writerows(ROWS)
print(f"\nwrote {csv_path}")
