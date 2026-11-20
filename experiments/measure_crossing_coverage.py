#!/usr/bin/env python3
"""P3 输入:补测 cast / control 穿越口纳入圈后的实测 coverage。

native 模式下可组合叠加 int-TMR(cast/int 通路)与 cmp-TMR(控制通路),
measure 它们单独纳入圈时对 cast / int_arith / cmp_branch 三类的 SDC 降幅,
给 P3 的选择模型提供**实测** Δcoverage(不拍脑袋)。每配置对自身 golden,Wilson CI。"""
import sys, csv
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import benchmark_harness as H

TRIALS = 200
CATS = ["cast", "int_arith", "cmp_branch"]
# native 模式 + 可组合的穿越口旗标;每个配置 = 把某穿越口纳入圈
CFGS = [
    ("base",    "native", []),
    ("cast_in", "native", ["-atft-int-tmr-wire", "-atft-int-tmr-i64"]),  # address/cast 穿越口入圈
    ("ctrl_in", "native", ["-atft-cmp-tmr"]),                            # control 穿越口入圈
]

SSA = H.build_infrastructure()

def pct(k, n): return 100.0 * k / n if n else 0.0

res = {}
etime = {}   # etime[(tag,cat)] = {benchmark: exec_time_us}
for tag, mode, flags in CFGS:
    for cat in CATS:
        p, f = H.compile_and_evaluate_policy(
            SSA, f"cov_{tag}_{cat}", mode, budget=1.0, fi_trials=TRIALS,
            run_fi=True, fi_target_type=cat, dump_golden=True, pass_flags=flags)
        k = sum(round(r["sdc_pct"] / 100 * r["trials"]) for r in f)
        n = sum(r["trials"] for r in f)
        res[(tag, cat)] = (k, n)
        etime[(tag, cat)] = {r["benchmark"]: r["exec_time_us"] for r in p}

# 开销:每配置相对 base 的 per-benchmark 执行时间比,取中位数(用 cast 轮的计时)
import statistics
def overhead(tag):
    bt = etime[("base", "cast")]; pt = etime[(tag, "cast")]
    ratios = [pt[b] / bt[b] for b in bt if b in pt and bt[b] > 0]
    return statistics.median(ratios) if ratios else float("nan")

print("\n=== SDC% 每穿越口入圈 (对自身 golden, Wilson 95%CI) ===")
print(f"{'config':10}" + "".join(f"{c:>16}" for c in CATS))
for tag, *_ in CFGS:
    row = f"{tag:10}"
    for c in CATS:
        k, n = res[(tag, c)]; _, _, h = H.wilson_ci95(k, n)
        row += f"{pct(k,n):9.1f}±{h:4.1f} "
    print(row)

print("\n=== coverage = 1 - sdc(入圈)/sdc(base) ===")
for tag, *_ in CFGS:
    if tag == "base":
        continue
    for c in CATS:
        kb, nb = res[("base", c)]; kp, np_ = res[(tag, c)]
        base, prot = pct(kb, nb), pct(kp, np_)
        cov = (1 - prot / base) * 100 if base > 0 else float("nan")
        print(f"  {tag:8} {c:12} {base:5.1f}% → {prot:5.1f}%  coverage {cov:6.1f}%")

RES = H.RES_DIR / "crossing_coverage.csv"
with open(RES, "w", newline="") as fp:
    w = csv.writer(fp); w.writerow(["config", "category", "sdc_pct", "sdc_ci95_half", "n", "overhead"])
    for tag, *_ in CFGS:
        ov = round(overhead(tag), 3)
        for c in CATS:
            k, n = res[(tag, c)]; _, _, h = H.wilson_ci95(k, n)
            w.writerow([tag, c, round(pct(k, n), 2), round(h, 2), n, ov])
print("CSV ->", RES)
print("\n=== 穿越口开销(相对 base 执行时间比,中位) ===")
for tag, *_ in CFGS:
    print(f"  {tag:10} {overhead(tag):.3f}x")
