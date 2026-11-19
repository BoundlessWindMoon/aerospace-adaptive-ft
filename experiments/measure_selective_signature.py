#!/usr/bin/env python3
"""选择性 + XOR 签名 的开销-覆盖前沿：simd64+loop-vote 下扫 budget，
量 时间/指令税 + fp64 SDC/检出。对标参考线：CONDA 1.58×、粗粒度 3×。"""
import math, sys, csv
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import benchmark_harness as H

BASE = ["-atft-delta-runtime", "-atft-replica-exact", "-atft-no-cse-barrier",
        "-atft-simd-f64", "-atft-loop-vote"]
TRIALS = 150
BUDGETS = [1.0, 0.7, 0.5, 0.3]

SSA = H.build_infrastructure()
# native baseline
pn, fn = H.compile_and_evaluate_policy(SSA, "selsig_native", "native", budget=1.0,
                                       fi_trials=TRIALS, run_fi=True, fi_target_type="fp64")
NT = {r["benchmark"]: r["exec_time_us"] for r in pn}
NI = {r["benchmark"]: r["hw_insts"] for r in pn}
NSDC = {r["benchmark"]: r["sdc_pct"] for r in fn}
B = list(NT.keys())

def geo(xs):
    xs=[x for x in xs if x>0]; return math.exp(sum(map(math.log,xs))/len(xs)) if xs else float("nan")

print(f"\n{'budget':>7}{'t_tax':>7}{'i_tax':>7}{'fp64_sdc':>9}{'fp64_det':>9}")
print(f"{'native':>7}{'1.00':>7}{'1.00':>7}"
      f"{sum(NSDC.values())/len(NSDC):>9.2f}{'0.00':>9}")
rows=[]
for bud in BUDGETS:
    p, f = H.compile_and_evaluate_policy(SSA, f"selsig_b{int(bud*100)}", "approx-simd",
            budget=bud, fi_trials=TRIALS, run_fi=True, fi_target_type="fp64", pass_flags=BASE)
    T={r["benchmark"]:r["exec_time_us"] for r in p}; I={r["benchmark"]:r["hw_insts"] for r in p}
    sdc={r["benchmark"]:r["sdc_pct"] for r in f}; det={r["benchmark"]:r["det_recovered_pct"] for r in f}
    tt=geo([T[b]/NT[b] for b in B if b in T]); it=geo([I[b]/NI[b] for b in B if b in I])
    ms=sum(sdc.values())/len(sdc); md=sum(det.values())/len(det)
    print(f"{bud:>7.1f}{tt:>7.2f}{it:>7.2f}{ms:>9.2f}{md:>9.2f}")
    rows.append({"budget":bud,"time_tax":round(tt,3),"inst_tax":round(it,3),
                 "fp64_sdc":round(ms,3),"fp64_det":round(md,3)})

csv_path = Path(H.ROOT_DIR)/"results"/"opt"/"selective_signature.csv"
with open(csv_path,"w",newline="") as fh:
    w=csv.DictWriter(fh,fieldnames=["budget","time_tax","inst_tax","fp64_sdc","fp64_det"]); w.writeheader(); w.writerows(rows)
print(f"\n参考线：CONDA 1.58× / 粗粒度单核 3×。wrote {csv_path}")
