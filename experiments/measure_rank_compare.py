#!/usr/bin/env python3
"""切 impl(排序烂) vs workload(脆弱性弥散)：固定 budget=0.5、simd64+loop-vote，
只换 -atft-rank ∈ {semantic, sdctune, shoestring, random}，看 B 类(cholesky/lu/
covariance)覆盖是否随排序变好。若 random≈semantic 全崩→弥散；若有排序明显更优→排序问题。"""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import benchmark_harness as H

BASE = ["-atft-delta-runtime", "-atft-replica-exact", "-atft-no-cse-barrier",
        "-atft-simd-f64", "-atft-loop-vote"]
TRIALS = 150
RANKS = ["semantic", "sdctune", "shoestring", "random"]
WATCH = ["pb_cholesky", "pb_lu", "pb_covariance", "pb_gemm", "pb_jacobi2d"]

SSA = H.build_infrastructure()
res = {}
for rk in RANKS:
    p, f = H.compile_and_evaluate_policy(SSA, f"rankcmp_{rk}", "approx-simd", rank=rk,
            budget=0.5, fi_trials=TRIALS, run_fi=True, fi_target_type="fp64", pass_flags=BASE)
    res[rk] = {r["benchmark"]: r["sdc_pct"] for r in f}

print(f"\nbudget=0.5, fp64 SDC%（越低覆盖越好）")
print(f"{'kernel':16}" + "".join(f"{rk:>11}" for rk in RANKS))
for k in WATCH:
    print(f"{k:16}" + "".join(f"{res[rk].get(k, float('nan')):>11.2f}" for rk in RANKS))
print("\n判据：同一 kernel 跨排序差异大=排序问题(可修)；random≈最优且都高=脆弱性弥散(负载本质)")
