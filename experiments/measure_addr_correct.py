#!/usr/bin/env python3
"""correct-before-use 防 DUE:地址下标 TMR 取中,越界崩溃在源头掐掉。
对比 native(无保护) vs addr-correct 在 cast/int_arith 的 DUE% 与 SDC%。
预期:cast/int_arith 的 DUE 显著下降(坏下标被中值盖掉,不再 segfault)。"""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import benchmark_harness as H

TRIALS = 300
CATS = ["cast", "int_arith", "addr"]
SSA = H.build_infrastructure()

BASE = ["-atft-isa-avx512"]
CORR = ["-atft-isa-avx512", "-atft-dmr-generic", "-atft-dmr-no-inject",
        "-atft-addr-dmr", "-atft-addr-correct"]

def run(tag, flags, cat):
    _, fi = H.compile_and_evaluate_policy(SSA, tag, "native", fi_trials=TRIALS,
                                          run_fi=True, fi_target_type=cat,
                                          dump_golden=True, pass_flags=flags)
    n = sum(r["trials"] for r in fi)
    due = sum(r["due_pct"]/100*r["trials"] for r in fi)
    sdc = sum(r["sdc_pct"]/100*r["trials"] for r in fi)
    return (100*due/n if n else 0.0), (100*sdc/n if n else 0.0), n

print(f"\n=== correct-before-use 防 DUE ({TRIALS} trials/bench) ===")
print(f"{'类别':10}{'native DUE%':>13}{'corr DUE%':>12}{'native SDC%':>13}{'corr SDC%':>12}")
for cat in CATS:
    db, sb, nb = run(f"ac_base_{cat}", BASE, cat)
    dc, sc, nc = run(f"ac_corr_{cat}", CORR, cat)
    print(f"{cat:10}{db:>13.1f}{dc:>12.1f}{sb:>13.1f}{sc:>12.1f}")
print("\n读法:corr 列 DUE 明显低于 native → 坏下标被 TMR 中值盖掉,越界崩溃被源头掐掉。")
print("     SDC 也应降(下标纠正顺带救了部分数据正确性);addr 本就低,看是否保持。")
