#!/usr/bin/env python3
"""跨环境自适应占优图（Fig C）：λ 用 CREME96/SPENVIS 轨道 SEU 率钉刻度，
性能可靠性合成用 performability（Meyer）。每模式的(开销 o, 单故障失效率 f=SDC+DUE)
来自实测 framework_eval.csv；λ 来自公开 CREME96 量级值。结论：无单一固定策略全程最优，
自适应取各模式上包络 → 全程占优。
performability/goodput = (1/o)·exp(-μ·o·f)，μ=每任务期望故障数(∝曝光时间∝o)。"""
import csv, math, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import benchmark_harness as H

# 实测 (o, f) —— 从框架评估 CSV 读
M = {}
with open(H.RES_DIR / "framework_eval.csv") as fp:
    for r in csv.DictReader(fp):
        M[r["config"]] = (float(r["overhead"]), float(r["fail_pct"]) / 100.0)
MODES = [("OFF(native)", "native"), ("DMR", "dmr_gen"), ("TMR", "tmr_gen")]
modes = [(n, M[t][0], M[t][1]) for n, t in MODES if t in M]

# CREME96 量级 SEU 率(upsets/bit/day, worst-week figure-of-merit; 出处 CREME96 / TI SLVK046)
# 场景: 工作集 1MB=8e6 bit, 任务累计曝光 1 天 → μ = rate × bits
BITS = 8e6
ORBITS = [("LEO-quiet", 1e-7), ("LEO/ISS", 1e-6), ("GEO(worst-wk)", 1e-5),
          ("SAA-peak", 1e-4), ("SPE-storm", 2e-3)]

def goodput(o, f, mu):
    return (1.0 / o) * math.exp(-mu * o * f)

print(f"\n实测模式 (o=开销, f=单故障失效率 SDC+DUE):")
for n, o, f in modes:
    print(f"  {n:12} o={o:.2f}  f={f*100:.2f}%")

print(f"\n{'orbit':14}{'SEU/bit/day':>12}{'μ=faults':>10}" +
      "".join(f"{n:>12}" for n, *_ in modes) + f"{'adaptive→':>14}")
env_gain = []
for oname, rate in ORBITS:
    mu = rate * BITS
    gs = [(n, goodput(o, f, mu)) for n, o, f in modes]
    best = max(gs, key=lambda x: x[1])
    row = f"{oname:14}{rate:>12.0e}{mu:>10.1f}"
    row += "".join(f"{g:>12.3f}" for _, g in gs)
    row += f"{best[0]:>14}"
    print(row)

# 自适应 = 上包络；对比"全程只用一个固定模式"的最优者
print("\n--- 无单一固定策略全程最优（跨 orbit 对比）---")
mus = [rate * BITS for _, rate in ORBITS]
# 每个固定模式的 goodput 向量
fixed = {n: [goodput(o, f, mu) for mu in mus] for n, o, f in modes}
adaptive = [max(fixed[n][i] for n in fixed) for i in range(len(mus))]
# 固定策略里"最差情况最好"的那个(minimax)作对手
best_fixed_name = max(fixed, key=lambda n: min(fixed[n]))
print(f"最优固定策略(按最差 orbit 最大化) = {best_fixed_name}")
print(f"{'orbit':14}{'adaptive':>10}{'best-fixed':>12}{'adaptive/fixed':>16}")
for i, (oname, _) in enumerate(ORBITS):
    bf = fixed[best_fixed_name][i]
    print(f"{oname:14}{adaptive[i]:>10.3f}{bf:>12.3f}{adaptive[i]/bf:>16.2f}")
winners = sorted(set(max(((n, fixed[n][i]) for n in fixed), key=lambda x: x[1])[0]
                     for i in range(len(mus))))
print(f"\n自适应实际用到的模式(菜单): {winners}")
print("→ 低 λ 用 OFF(省开销)、高 λ 切 TMR(保活)，自适应是上包络，任何固定策略都在某段被它压。")
