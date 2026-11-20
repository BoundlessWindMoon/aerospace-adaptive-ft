#!/usr/bin/env python3
"""fi-cache 排名 vs 启发式排名:同 budget 下谁把 SDC 压得更低(覆盖更高)。
B 机制验证——实测脆弱性缓存喂选择性保护,应在相同保护预算下优于启发式。
注:cache 与此处 FI 用同一组输入,属 in-sample;跨输入泛化见 input-dependence 说明。"""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import benchmark_harness as H

TRIALS = 200
BUDGETS = [0.5, 0.25, 0.1]
CAT = "int_arith"
CACHE = H.RES_DIR / "vuln_cache" / f"vuln_cache_{CAT}.csv"
assert CACHE.exists(), f"先跑 vuln_cache.py 生成 {CACHE}"

SSA = H.build_infrastructure()
def pct(k, n): return 100.0 * k / n if n else 0.0

def measure(tag, rank, budget, extra=None):
    flags = ["-atft-int-tmr-wire", "-atft-int-tmr-i64"] + (extra or [])
    p, f = H.compile_and_evaluate_policy(SSA, tag, "native", rank=rank, budget=budget,
                                         fi_trials=TRIALS, run_fi=True, fi_target_type=CAT,
                                         dump_golden=True, pass_flags=flags)
    k = sum(round(r["sdc_pct"]/100*r["trials"]) for r in f); n = sum(r["trials"] for r in f)
    return pct(k, n)

nat = measure("fc_native", "semantic", 1.0, extra=[])  # budget 1.0 → 全保护做上界参考
# 上面带 wire 的 budget=1.0 是"全保护";真 native 不保护另测
pn, fn = H.compile_and_evaluate_policy(SSA, "fc_raw", "native", budget=1.0, fi_trials=TRIALS,
                                       run_fi=True, fi_target_type=CAT, dump_golden=True,
                                       pass_flags=["-atft-isa-avx512"])
kk = sum(round(r["sdc_pct"]/100*r["trials"]) for r in fn); nn = sum(r["trials"] for r in fn)
raw = pct(kk, nn)

print(f"\n=== fi-cache vs semantic(int_arith, {TRIALS} trials/bench) ===")
print(f"不保护(native)          int SDC% = {raw:5.1f}")
print(f"全保护(budget=1.0)       int SDC% = {nat:5.1f}  (覆盖上界)\n")
print(f"{'budget':>8}{'semantic SDC%':>16}{'fi-cache SDC%':>16}{'semantic 覆盖':>14}{'fi-cache 覆盖':>14}")
for b in BUDGETS:
    s = measure(f"fc_sem_{b}", "semantic", b)
    c = measure(f"fc_cache_{b}", "fi-cache", b, extra=[f"-atft-vuln-cache={CACHE}"])
    cov_s = (1 - s/raw)*100 if raw else 0
    cov_c = (1 - c/raw)*100 if raw else 0
    print(f"{b:>8}{s:>16.1f}{c:>16.1f}{cov_s:>13.1f}%{cov_c:>13.1f}%")

print("\n读法:同 budget 下 fi-cache 的 SDC 更低/覆盖更高 → 实测排名吃到了启发式看不见的 skew。")
print("in-sample 说明:cache 与评测用同输入,结论是排名是否可用,非跨输入泛化保证。")
