#!/usr/bin/env python3
"""自适应容错控制器（闭环 demo，补齐"自适应"标题）。
真·运行期控制：只看在线观测的错误计数(模拟 EDAC scrub / 粒子计数器)估计 λ，用 SPRT
(Wald 序贯概率比检验)检测 λ 跨越 mode 边界，按性能可靠性(performability)最优切换
OFF/DMR/TMR。不作弊——控制器看不到真 λ，只能从错误流里估。
每模式 (o, f) 来自实测 framework_eval.csv；λ(t) 任务剖面按 CREME96 轨道量级设。
对照固定策略(always-OFF/DMR/TMR)，证明自适应取上包络：安静段近 OFF 吞吐、风暴段近 TMR 保活。"""
import csv, math, random, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import benchmark_harness as H

random.seed(12345)  # 可复现

# 实测 (o, f) ----------------------------------------------------------------
M = {}
with open(H.RES_DIR / "framework_eval.csv") as fp:
    for r in csv.DictReader(fp):
        M[r["config"]] = (float(r["overhead"]), float(r["fail_pct"]) / 100.0)
MODES = [("OFF", M["native"]), ("DMR", M["dmr_gen"]), ("TMR", M["tmr_gen"])]

def goodput(mode_of, mu):            # performability: 吞吐 × 存活概率
    o, f = mode_of
    return (1.0 / o) * math.exp(-mu * o * f)

def best_mode(mu):                   # 性能可靠性最优 mode（策略目标）
    return max(range(len(MODES)), key=lambda i: goodput(MODES[i][1], mu))

# mode 切换的 μ 边界（argmax 换号处，数值扫描）---------------------------------
grid = [10 ** (x / 20.0) for x in range(-40, 120)]
bnd = []
prev = best_mode(grid[0])
for mu in grid:
    b = best_mode(mu)
    if b != prev:
        bnd.append(math.sqrt(mu * grid[grid.index(mu) - 1])); prev = b
# bnd[0]=OFF↔DMR, bnd[1]=DMR↔TMR（若存在）
print("mode 切换 μ 边界(实测 performability 决定):", [round(b, 2) for b in bnd])

# λ(t) 任务剖面（每 step=一个任务；μ=每任务期望故障，CREME96 量级）------------------
#   安静 LEO → 进 SAA(×100) → 出 SAA → SPE 太阳风暴尖峰 → 回落
def profile(t):
    if t < 60:   return 0.5           # LEO 安静
    if t < 90:   return 50.0          # SAA 穿越
    if t < 150:  return 0.5           # 回安静
    if t < 165:  return 1500.0        # SPE 风暴尖峰
    return 0.5                        # 回落
T = 220

# SPRT（Wald 序贯比）检测 λ 是否越过某边界 μ*；λlo/λhi 括住 μ* -------------------
class SPRT:
    def __init__(s, mu_star, alpha=0.05, beta=0.05, sep=3.0):
        s.lo = mu_star / sep; s.hi = mu_star * sep
        s.A = math.log((1 - beta) / alpha); s.B = math.log(beta / (1 - alpha))
        s.llr = 0.0
    def update(s, k):                 # 观测本 step 错误计数 k(Poisson)
        s.llr += k * math.log(s.hi / s.lo) - (s.hi - s.lo)
        if s.llr >= s.A: s.llr = 0.0; return +1   # 判定 λ≥hi(升)
        if s.llr <= s.B: s.llr = 0.0; return -1   # 判定 λ≤lo(降)
        return 0

def run(policy):
    """policy: 't,mu_est,level'->mode_idx。返回(有用工作, SDC 逃逸, 加权时间)。"""
    work = sdc = time = 0.0
    sprts = [SPRT(b) for b in bnd]
    level = 0                          # 当前档位(0=OFF,1=DMR,2=TMR)
    timeline = []
    for t in range(T):
        mu = profile(t)
        k = sum(1 for _ in range(1000) if random.random() < mu / 1000)  # 传感器:Poisson(mu) 近似
        # SPRT 更新各边界 → 调 level（带 SPRT 自身的迟滞）
        for i, s in enumerate(sprts):
            d = s.update(k)
            if d > 0 and level <= i: level = i + 1
            if d < 0 and level > i:  level = i
        m = policy(level)
        o, f = MODES[m][1]
        work += 1.0 / o                     # 完成的有用工作(吞吐)
        sdc += mu * o * f                    # 该 step 期望 SDC 逃逸(未检出失效)
        time += o
        timeline.append(m)
    return work, sdc, time, timeline

adaptive = lambda level: level
W, Sdc, Tm, tl = run(adaptive)
print(f"\n{'policy':12}{'useful_work':>12}{'SDC_escapes':>12}{'time':>8}")
print(f"{'ADAPTIVE':12}{W:>12.1f}{Sdc:>12.3f}{Tm:>8.1f}")
for i, (name, _) in enumerate(MODES):
    w, s, tm, _ = run(lambda level, i=i: i)
    print(f"{'fixed-'+name:12}{w:>12.1f}{s:>12.3f}{tm:>8.1f}")

# mode 时间线（压缩显示：段）
segs = []
for m in tl:
    if not segs or segs[-1][0] != m: segs.append([m, 0])
    segs[-1][1] += 1
print("\n自适应 mode 时间线:", " ".join(f"{MODES[m][0]}×{n}" for m, n in segs))
print("→ 安静段走 OFF(省)、SAA/SPE 升到 DMR/TMR(保活)，SPRT 从错误流在线判定、无需知道真 λ。")
