#!/usr/bin/env python3
"""整数保护的"开销 vs 覆盖"曲线:native 模式 + 整数三模,按预算砍 top-k 站点。
隔离整数(不掺 FP),看偏斜能否换成开销节省。凸曲线=可兑现,近线性=脆弱即热点。"""
import sys, csv, statistics
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import benchmark_harness as H

TRIALS = 200
BUDGETS = [1.0, 0.5, 0.25, 0.1]
SSA = H.build_infrastructure()
def pct(k, n): return 100.0 * k / n if n else 0.0

def measure(tag, flags, budget):
    p, f = H.compile_and_evaluate_policy(SSA, tag, "native", budget=budget, fi_trials=TRIALS,
                                         run_fi=True, fi_target_type="int_arith",
                                         dump_golden=True, pass_flags=flags)
    k = sum(round(r["sdc_pct"]/100*r["trials"]) for r in f); n = sum(r["trials"] for r in f)
    et = {r["benchmark"]: r["exec_time_us"] for r in p}
    return pct(k, n), et

# baseline
nat_sdc, nat_et = measure("cc_native", ["-atft-isa-avx512"], 1.0)
def ov(et):
    rs = [et[b]/nat_et[b] for b in nat_et if b in et and nat_et[b] > 0]
    return statistics.median(rs) if rs else float("nan")

print(f"\n=== 整数保护 开销 vs 覆盖(int_arith) ===")
print(f"{'配置':16}{'int SDC%':>10}{'覆盖%':>8}{'开销':>8}")
print(f"{'native(不保护)':16}{nat_sdc:>10.1f}{'—':>8}{'1.00x':>8}")
rows = []
for b in BUDGETS:
    sdc, et = measure(f"cc_b{b}", ["-atft-int-tmr-wire", "-atft-int-tmr-i64"], b)
    cov = (1 - sdc/nat_sdc)*100 if nat_sdc else 0
    o = ov(et)
    rows.append((b, sdc, cov, o))
    print(f"{'budget='+str(b):16}{sdc:>10.1f}{cov:>7.1f}%{o:>7.2f}x")

print("\n读法:从 budget=1.0 到 0.1,开销若明显下降而覆盖基本保住 → 偏斜可兑现(凸);")
print("      若开销不怎么降(砍了站点数但热点还在)→ 脆弱即热点,没免费午餐(线性/凹)。")
# 量化:覆盖/开销增量比
full = rows[0]; light = rows[-1]
print(f"\nbudget 1.0→0.1: 覆盖 {full[2]:.0f}%→{light[2]:.0f}%,开销 {full[3]:.2f}x→{light[3]:.2f}x")
if full[3] - light[3] > 0.3 and light[2] > full[2]*0.7:
    print("→ 开销显著降、覆盖基本保住:偏斜可兑现,细粒度对整数有收益。")
else:
    print("→ 开销没怎么降 / 覆盖掉太多:脆弱站点就是热点,细粒度收益有限。")
