#!/usr/bin/env python3
"""效果剖面：通用 DMR/TMR 在全 6 类故障下的覆盖(SDC/DUE)，不只 fp64。
回答"效果到什么程度、哪类是洞"。同 host ISA，Wilson 95%CI。
类别: fp64(计算) int_arith(整数) cast cmp_branch(控制流) addr(地址) load(访存)。"""
import sys, csv
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import benchmark_harness as H

ISA = ["-atft-isa-avx512"]
TRIALS = 200
CATS = ["fp64", "int_arith", "cast", "cmp_branch", "addr", "load"]
CFGS = [
    ("eff_native", "native",      ISA,                                         True),
    ("eff_dmr",    "approx-simd", ["-atft-dmr-generic"],                       False),
    ("eff_tmr",    "approx-simd", ["-atft-dmr-generic", "-atft-tmr-generic"],  False),
]

SSA = H.build_infrastructure()
# 先用 fp64 那轮产 golden(native)
res = {}   # res[(tag,cat)] = (sdc%, due%)
for tag, mode, flags, gold in CFGS:
    for ci, cat in enumerate(CATS):
        p, f = H.compile_and_evaluate_policy(SSA, f"{tag}_{cat}", mode, budget=1.0,
                                             fi_trials=TRIALS, run_fi=True, fi_target_type=cat,
                                             dump_golden=(gold and ci == 0), pass_flags=flags)
        B = [r["benchmark"] for r in f]
        ks = sum(round(r["sdc_pct"] / 100 * r["trials"]) for r in f)
        kd = sum(round(r["due_pct"] / 100 * r["trials"]) for r in f)
        n = sum(r["trials"] for r in f)
        res[(tag, cat)] = (ks / n * 100 if n else 0, kd / n * 100 if n else 0, n)

def tagname(t): return {"eff_native": "native", "eff_dmr": "DMR_gen", "eff_tmr": "TMR_gen"}[t]
print(f"\n=== SDC% (残余静默错，越低越好) ===")
print(f"{'config':10}" + "".join(f"{c:>12}" for c in CATS))
for tag, *_ in CFGS:
    print(f"{tagname(tag):10}" + "".join(f"{res[(tag,c)][0]:>12.1f}" for c in CATS))
print(f"\n=== DUE% (崩溃/检出不可恢复) ===")
print(f"{'config':10}" + "".join(f"{c:>12}" for c in CATS))
for tag, *_ in CFGS:
    print(f"{tagname(tag):10}" + "".join(f"{res[(tag,c)][1]:>12.1f}" for c in CATS))

# 覆盖判定：TMR 相对 native 的 SDC 降幅
print(f"\n=== TMR_gen 相对 native 的 SDC 降幅(覆盖强度) ===")
for c in CATS:
    nat = res[("eff_native", c)][0]; tmr = res[("eff_tmr", c)][0]
    cov = (1 - tmr / nat) * 100 if nat > 0 else float("nan")
    verdict = "强" if cov >= 90 else ("部分" if cov >= 40 else ("弱/洞" if nat > 1 else "native本就低"))
    print(f"  {c:12} native {nat:5.1f}% → TMR {tmr:5.1f}%   覆盖 {cov:5.1f}%  [{verdict}]")

RES = H.RES_DIR / "effect_profile.csv"
with open(RES, "w", newline="") as fp:
    w = csv.writer(fp); w.writerow(["config", "category", "sdc_pct", "due_pct", "n"])
    for tag, *_ in CFGS:
        for c in CATS:
            s, d, n = res[(tag, c)]; w.writerow([tagname(tag), c, round(s, 2), round(d, 2), n])
print("CSV ->", RES)
