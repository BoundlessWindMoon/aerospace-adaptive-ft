#!/usr/bin/env python3
"""集中度路由判据：全套件在 budget=0.5 下，metric = random_SDC - best_heuristic_SDC。
正 = 脆弱性集中(启发式选择性真能赢 random)→ 路由到选择性 TMR；
≤0 = 弥散(选择性赢不了随机)→ 路由到全保护/ABFT。看它能否干净二分。"""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import benchmark_harness as H

BASE = ["-atft-delta-runtime", "-atft-replica-exact", "-atft-no-cse-barrier",
        "-atft-simd-f64", "-atft-loop-vote"]
TRIALS = 200
HEUR = ["semantic", "shoestring", "sdctune"]
RANKS = HEUR + ["random"]
BUDGET = float(sys.argv[1]) if len(sys.argv) > 1 else 0.5

SSA = H.build_infrastructure()
sdc = {}
for rk in RANKS:
    p, f = H.compile_and_evaluate_policy(SSA, f"conc_{rk}", "approx-simd", rank=rk,
            budget=BUDGET, fi_trials=TRIALS, run_fi=True, fi_target_type="fp64", pass_flags=BASE)
    sdc[rk] = {r["benchmark"]: r["sdc_pct"] for r in f}

ks = list(sdc["random"].keys())
print(f"\nbudget={BUDGET}, TRIALS={TRIALS}")
print(f"{'kernel':20}{'best_heur':>10}{'random':>8}{'metric':>8}  route")
rows = []
for k in sorted(ks, key=lambda x: -(sdc['random'].get(x,0) - min(sdc[r].get(x,99) for r in HEUR))):
    bh = min(sdc[r].get(k, float('nan')) for r in HEUR)
    rnd = sdc["random"].get(k, float('nan'))
    m = rnd - bh
    route = "选择性TMR" if m > 0.5 else ("全保护/ABFT" if m < -0.5 else "边界(不确定)")
    print(f"{k:20}{bh:>10.2f}{rnd:>8.2f}{m:>+8.2f}  {route}")
    rows.append((k, bh, rnd, m, route))
pos = [r for r in rows if r[3] > 0.5]; neg = [r for r in rows if r[3] < -0.5]
print(f"\n集中(选择性有效) {len(pos)} 个 | 弥散(选择性无效) {len(neg)} 个 | 边界 {len(rows)-len(pos)-len(neg)} 个")
print("判据阈值取 ±0.5(噪声带)；看两类是否被 0 干净分开")
