#!/usr/bin/env python3
"""跑 TRIDENT 式分析(免 FI 输出锚定 SDC 可达性),把预测的逐类别权重和我们 FI 实测
(effect_profile.csv native 行)对比——看复用 TRIDENT 方法能否自动预测"该保护什么"。"""
import sys, csv, subprocess
from collections import defaultdict
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import benchmark_harness as H

SSA = H.build_infrastructure()
OUT = H.RES_DIR / "trident_rank.csv"
cmd = [H.OPT, f"-load-pass-plugin={H.PASS_POLICY_SO}", "-passes=adaptive-ft",
       "-atft-mode=native", f"-atft-trident-rank={OUT}", "-S", str(SSA), "-o", "/dev/null"]
subprocess.run(cmd, check=True)

# 聚合 TRIDENT 权重到类别
w = defaultdict(float)
with open(OUT) as fp:
    for r in csv.DictReader(fp):
        w[r["category"]] += float(r["weight"])
# fp -> fp64 对齐 effect_profile
w["fp64"] = w.pop("fp", 0.0)
tot = sum(v for k, v in w.items() if k != "other") or 1.0

# FI 实测(native 逐类别 SDC)
fi = {}
with open(H.RES_DIR / "effect_profile.csv") as fp:
    for r in csv.DictReader(fp):
        if r["config"] == "native":
            fi[r["category"]] = float(r["sdc_pct"])
fitot = sum(fi.values()) or 1.0

CATS = ["cmp_branch", "load", "int_arith", "fp64", "cast", "addr"]
print(f"\n{'category':12}{'TRIDENT权重%':>14}{'FI实测SDC%':>14}{'FI占比%':>10}")
for c in CATS:
    tw = w.get(c, 0.0) / tot * 100
    print(f"{c:12}{tw:>14.1f}{fi.get(c,0):>14.1f}{fi.get(c,0)/fitot*100:>10.1f}")

# 排名对比:TRIDENT 预测的类别重要性顺序 vs FI 实测顺序
tri_order = sorted(CATS, key=lambda c: -w.get(c, 0.0))
fi_order = sorted(CATS, key=lambda c: -fi.get(c, 0.0))
print(f"\nTRIDENT 预测重要性序: {tri_order}")
print(f"FI 实测重要性序:      {fi_order}")
print(f"Top-3 预测 {tri_order[:3]}  vs 实测 {fi_order[:3]}  "
      f"命中 {len(set(tri_order[:3]) & set(fi_order[:3]))}/3")
