#!/usr/bin/env python3
"""B 的 A/B：simd64(现最优) vs simd64+loop-vote。比 时间/指令税、voter 指令数，
以及注错结局(sdc / 检出det_recovered / due)——看循环级检错把税降到多少、检错率如何。"""
import math, sys, csv, re
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import benchmark_harness as H

BASE = ["-atft-delta-runtime", "-atft-replica-exact", "-atft-no-cse-barrier", "-atft-simd-f64"]
TRIALS = 150
TARGETS = ["fp64", "int_arith"]
CFGS = [
    ("native",     "native",      []),
    ("simd64",     "approx-simd", BASE),
    ("simd64_lv",  "approx-simd", BASE + ["-atft-loop-vote"]),
]

def voter_count(tag):
    f = H.BUILD_DIR / f"sut_lvab_{tag}_fp64.ll"
    try:
        return len(re.findall(r"atft\.voter\.internal", Path(f).read_text()))
    except OSError:
        return -1

SSA = H.build_infrastructure()
T, I, FI, VC = {}, {}, {}, {}
for tag, mode, flags in CFGS:
    T[tag], I[tag], FI[tag] = {}, {}, {}
    for tgt in TARGETS:
        p, f = H.compile_and_evaluate_policy(SSA, f"lvab_{tag}_{tgt}", mode, budget=1.0,
                                             fi_trials=TRIALS, run_fi=True, fi_target_type=tgt,
                                             pass_flags=flags)
        for r in p:
            T[tag][r["benchmark"]] = r["exec_time_us"]; I[tag][r["benchmark"]] = r["hw_insts"]
        FI[tag][tgt] = {r["benchmark"]: r for r in f}
    VC[tag] = voter_count(tag)

def geo(xs):
    xs=[x for x in xs if x>0]; return math.exp(sum(map(math.log,xs))/len(xs)) if xs else float("nan")
B = list(T["native"].keys())
def agg(tag, tgt, key):
    v=[FI[tag][tgt][b][key] for b in B if b in FI[tag][tgt]]; return sum(v)/len(v) if v else float("nan")

print(f"\n{'config':11}{'t_tax':>7}{'i_tax':>7}{'voter#':>8}  "
      f"{'fp64 sdc/det':>13}  {'int sdc/det':>13}")
for tag,*_ in CFGS:
    vc=VC[tag]
    if tag=="native":
        print(f"{tag:11}{'1.00':>7}{'1.00':>7}{vc:>8}  "
              f"{agg(tag,'fp64','sdc_pct'):>6.1f}/{agg(tag,'fp64','det_recovered_pct'):<6.1f}  "
              f"{agg(tag,'int_arith','sdc_pct'):>6.1f}/{agg(tag,'int_arith','det_recovered_pct'):<6.1f}")
        continue
    tt=geo([T[tag][b]/T['native'][b] for b in B if b in T[tag]])
    it=geo([I[tag][b]/I['native'][b] for b in B if b in I[tag]])
    print(f"{tag:11}{tt:>7.2f}{it:>7.2f}{vc:>8}  "
          f"{agg(tag,'fp64','sdc_pct'):>6.1f}/{agg(tag,'fp64','det_recovered_pct'):<6.1f}  "
          f"{agg(tag,'int_arith','sdc_pct'):>6.1f}/{agg(tag,'int_arith','det_recovered_pct'):<6.1f}")
print("\nsdc=残余静默错(越低越好)；det=检出(循环级检错应把 sdc 转成 det)")
