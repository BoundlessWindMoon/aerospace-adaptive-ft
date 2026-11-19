#!/usr/bin/env python3
"""根因诊断：选择性前沿为何差。逐负载看 budget 1.0→0.3 的 time_tax 与 SDC 怎么变，
并数每 budget 实际签名的 store 数(lv.sigA.next)——判断是 budget 没咬住(impl) 还是
热循环恒被保护、砍掉的是冷指令(workload)。"""
import sys, re
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import benchmark_harness as H

BASE = ["-atft-delta-runtime", "-atft-replica-exact", "-atft-no-cse-barrier",
        "-atft-simd-f64", "-atft-loop-vote"]
TRIALS = 120
BUDGETS = [1.0, 0.5, 0.3]

def signed_stores(tag):
    try:
        return len(re.findall(r"lv\.sigA\.next", (H.BUILD_DIR / f"sut_{tag}.ll").read_text()))
    except OSError:
        return -1

SSA = H.build_infrastructure()
pn, _ = H.compile_and_evaluate_policy(SSA, "diag_native", "native", budget=1.0, fi_trials=0, run_fi=False)
NT = {r["benchmark"]: r["exec_time_us"] for r in pn}

data = {}
sstores = {}
for bud in BUDGETS:
    tag = f"diag_b{int(bud*100)}"
    p, f = H.compile_and_evaluate_policy(SSA, tag, "approx-simd", budget=bud,
            fi_trials=TRIALS, run_fi=True, fi_target_type="fp64", pass_flags=BASE)
    data[bud] = ({r["benchmark"]: r["exec_time_us"] for r in p},
                 {r["benchmark"]: r["sdc_pct"] for r in f})
    sstores[bud] = signed_stores(tag)

print("\n签名 store 数(整模块): " + "  ".join(f"b{b}={sstores[b]}" for b in BUDGETS))
print(f"\n{'kernel':20}" + "".join(f"{'t@'+str(b):>8}{'sdc@'+str(b):>9}" for b in BUDGETS))
for k in NT:
    row = f"{k:20}"
    for b in BUDGETS:
        T, S = data[b]
        tt = T[k]/NT[k] if k in T and NT[k] > 0 else float("nan")
        row += f"{tt:>8.2f}{S.get(k, float('nan')):>9.2f}"
    print(row)
