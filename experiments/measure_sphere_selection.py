#!/usr/bin/env python3
"""P3 demo:边界选择问题,分离模型 vs 联合模型(含 P2 耦合 κ),扫 λ 看最优圈。

全部参数从实测 CSV 读入,不硬编码常数:
  - effect_profile.csv     : native 各类 SDC、V(向量化 value)对 fp64 的覆盖
  - crossing_coverage.csv  : A(int-tmr) / C(cmp-tmr) 的 coverage 与 overhead
  - nonseparability.csv    : 耦合 κ(向量化使 cast 抬升的绝对百分点)
  - framework_eval.csv     : V 的 overhead(dmr_gen)
证明:分离优化器忽略 κ,会选一个真实残余超标的圈(静默不可行),联合优化器选对。"""
import sys, csv
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import benchmark_harness as H

R = H.RES_DIR
def load(name):
    with open(R / name) as fp:
        return list(csv.DictReader(fp))

eff = {(r["config"], r["category"]): r for r in load("effect_profile.csv")}
cov = {(r["config"], r["category"]): r for r in load("crossing_coverage.csv")}
nons = {(r["config"], r["category"]): r for r in load("nonseparability.csv")}
fw = {r["config"]: r for r in load("framework_eval.csv")}

# --- 实测参数 ---
s = {c: float(eff[("native", c)]["sdc_pct"])
     for c in ["fp64", "int_arith", "cast", "cmp_branch", "addr", "load"]}
# V 对 fp64 的覆盖(dmr_gen 残余 / native)
cov_V_fp = 1 - float(eff[("DMR_gen", "fp64")]["sdc_pct"]) / s["fp64"]
# A 覆盖 cast / int(crossing_coverage, scalar 实测)
cov_A_cast = 1 - float(cov[("cast_in", "cast")]["sdc_pct"]) / float(cov[("base", "cast")]["sdc_pct"])
cov_A_int  = 1 - float(cov[("cast_in", "int_arith")]["sdc_pct"]) / float(cov[("base", "int_arith")]["sdc_pct"])
cov_C_cmp  = 1 - float(cov[("ctrl_in", "cmp_branch")]["sdc_pct"]) / float(cov[("base", "cmp_branch")]["sdc_pct"])
# 耦合 κ:向量化(无注入)使 cast 抬升的绝对百分点
kappa = float(nons[("vector_noinject", "cast")]["sdc_pct"]) - float(nons[("scalar_native", "cast")]["sdc_pct"])
# overhead
ov_V = float(fw["dmr_gen"]["overhead"])
ov_A = float(cov[("cast_in", "cast")]["overhead"])
ov_C = float(cov[("ctrl_in", "cmp_branch")]["overhead"])

print(f"实测参数: s={ {k:round(v,2) for k,v in s.items()} }")
print(f"  cov[V,fp64]={cov_V_fp:.3f} cov[A,cast]={cov_A_cast:.3f} cov[A,int]={cov_A_int:.3f} cov[C,cmp]={cov_C_cmp:.3f}")
print(f"  kappa(cast 耦合)={kappa:.1f}pp  overhead V={ov_V:.2f} A={ov_A:.2f} C={ov_C:.2f}")

# <!-- MODEL -->
from itertools import product

# 背景(A/C/V 都不覆盖的类别),两模型相同,计入总残余保证量级真实
bg = s["addr"] + s["load"]

def residual(S, model):
    V, A, C = S
    r_fp  = s["fp64"] * (1 - cov_V_fp * V)
    r_int = s["int_arith"] * (1 - cov_A_int * A)
    r_cmp = s["cmp_branch"] * (1 - cov_C_cmp * C)
    s_cast = s["cast"] + (kappa * V if model == "joint" else 0.0)  # 分离模型忽略 κ
    r_cast = s_cast * (1 - cov_A_cast * A)
    return r_fp + r_int + r_cmp + r_cast + bg

def cost(S):
    V, A, C = S
    c = 1.0
    if V: c *= ov_V
    if A: c *= ov_A
    if C: c *= ov_C
    return c

SPHERES = list(product([0, 1], repeat=3))
def name(S): return "{" + ",".join(n for n, b in zip("VAC", S) if b) + "}" or "{}"

def pick(tau, model):
    feas = [S for S in SPHERES if residual(S, model) <= tau]
    return min(feas, key=cost) if feas else None

# τ 扫描:从宽到紧 = λ 从低到高(τ=B/λ,B 固定,仅作标注)
B = 60.0
lo = residual((1, 1, 1), "joint")   # 最紧:全圈联合残余
hi = residual((0, 0, 0), "joint")   # 最松:空圈残余
grid = [round(hi - (hi - lo) * i / 16, 2) for i in range(17)]

print(f"\n{'τ(容忍Σr)':>10} {'λ∝B/τ':>7} | {'分离选圈':>10} {'cost':>5} {'真实残余':>7} | "
      f"{'联合选圈':>10} {'cost':>5} {'真实残余':>7} | 判定")
rows = []
for tau in grid:
    Ss = pick(tau, "separable"); Sj = pick(tau, "joint")
    lam = B / tau if tau > 0 else float("inf")
    def fmt(S):
        if S is None: return f"{'不可行':>10} {'-':>5} {'-':>7}"
        return f"{name(S):>10} {cost(S):5.2f} {residual(S,'joint'):7.1f}"
    # 分离优化器的"真实"残余按联合模型评估;若 > τ 则其选择静默不可行
    verdict = ""
    if Ss is not None and residual(Ss, "joint") > tau + 1e-9:
        verdict = "分离静默不可行!"
    elif Ss != Sj:
        verdict = "选圈分歧"
    print(f"{tau:10.1f} {lam:7.2f} | {fmt(Ss)} | {fmt(Sj)} | {verdict}")
    rows.append((tau, name(Ss) if Ss else "INFEAS", name(Sj) if Sj else "INFEAS", verdict))

band = [r for r in rows if r[3]]
print(f"\n分歧/误判 τ 区间:{len(band)}/{len(rows)} 个采样点")
print("要点:存在 λ 带,分离优化器选便宜的含 V 圈并判其达标,但按实测 κ 评估其 cast")
print("      残余已超 τ(静默不可行);联合优化器因 κ 把 A 也纳入圈 → 选对。")
print("      → A 的入圈价值依赖 V 是否入圈,二者不能独立决策(选择层的非可分离)。")

OUT = R / "sphere_selection.csv"
with open(OUT, "w", newline="") as fp:
    w = csv.writer(fp); w.writerow(["tau", "separable_pick", "joint_pick", "verdict"])
    w.writerows(rows)
print("CSV ->", OUT)
