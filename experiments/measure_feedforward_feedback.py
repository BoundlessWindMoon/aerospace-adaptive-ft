#!/usr/bin/env python3
"""开环(前馈排程) / 闭环(SPRT检测) / 混合 三种控制架构对比。
轨迹 = 周期 SAA(星历已知) + 叠加一次突发耀斑(不可预测)。
坐实:纯前馈在耀斑下漏保护;纯反馈对周期段有回落浪费+对耀斑有滞后;混合两头占。
附:重执行开销——检测到的错触发重跑,λ 越大越贵(耀斑期尤甚)。"""
import math, random, csv, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import atft_core as C

random.seed(7)
DT = 30.0
ORBITS = 10
PERIOD = C.LEO_PERIOD_S
SAA = C.LEO_SAA_FRAC * PERIOD           # 每轨 SAA 窗口(星历已知)
LAM_Q, LAM_SAA = C.LAM_LEO_QUIET, C.LAM_LEO_SAA
R = LAM_SAA / LAM_Q                      # 名义 SAA 速率比 ~200(检测器按这个调)

# 耀斑:第 3 轨起爆,0.5h 升 / 2h 平台 / 1h 衰减,峰值 1e4× 背景,叠加在 SAA 方波之上
FL_ON = 3 * PERIOD
FL_RISE, FL_PLAT, FL_DECAY = 0.5*3600, 2.0*3600, 1.0*3600
FL_FACTOR = 1.0e4

def flare_gain(t):
    d = t - FL_ON
    if d < 0: return 1.0
    if d < FL_RISE: return FL_FACTOR ** (d/FL_RISE)
    if d < FL_RISE+FL_PLAT: return FL_FACTOR
    if d < FL_RISE+FL_PLAT+FL_DECAY:
        return FL_FACTOR ** (1.0 - (d-FL_RISE-FL_PLAT)/FL_DECAY)
    return 1.0

def in_saa(t):
    return (t % PERIOD) < SAA

def build_trace():
    n = int(ORBITS*PERIOD/DT)
    tr = []
    for i in range(n):
        t = i*DT
        base = LAM_SAA if in_saa(t) else LAM_Q
        tr.append((t, base*flare_gain(t)))
    return tr

# ---- SPRT 检测器(复刻 runtime 时间驱动 LLR) ----
class SPRT:
    def __init__(self, r, alpha=0.01, beta=0.01):
        self.r, self.logr = r, math.log(r)
        self.A = math.log((1-beta)/alpha)
        self.B = math.log(beta/(1-alpha))
        self.llr, self.mode = 0.0, 0   # 0 LOW / 1 HIGH
        self.lam0 = LAM_Q
    def step(self, lam_true, dt, lam0=None):
        # lam0 = H0 基线速率。常数 → 纯反馈;传入排程基线 λ_sched(t) → 只检残差(耀斑)
        base = self.lam0 if lam0 is None else lam0
        self.llr -= (self.r-1.0)*base*dt                # 时间驱动:安静证据连续累减
        ev = _pois(lam_true*dt)                         # 本步事件数
        self.llr += ev*self.logr
        if self.llr > self.A: self.llr = self.A
        if self.llr < self.B: self.llr = self.B
        if self.mode==0 and self.llr>=self.A: self.mode=1
        elif self.mode==1 and self.llr<=self.B: self.mode=0
        return self.mode

def _pois(mean):
    # 小均值 Poisson(Knuth);均值大时退化为取整期望,避免慢
    if mean > 30: return int(mean + random.gauss(0, math.sqrt(mean)))
    L, k, p = math.exp(-mean), 0, 1.0
    while True:
        k += 1; p *= random.random()
        if p <= L: return k-1

# ---- 开销/泄漏/重执行模型 ----
OV_HIGH, OV_LOW = 1.641, 1.0     # capstone 时序开销
RERUN_FRAC = 0.5                 # 每个被检出的错重跑半个工作单元(保守)

def evaluate(trace, controller):
    hi=0; ov=0.0; leaked=0.0; rerun=0.0
    leaked_flare=0.0; hi_flare=0; n_flare=0
    for (t,lam) in trace:
        m = controller(t,lam)
        isf = FL_ON <= t < FL_ON+FL_RISE+FL_PLAT+FL_DECAY
        if isf: n_flare+=1
        if m==1:
            hi+=1; ov+=OV_HIGH
            rerun += lam*DT*RERUN_FRAC          # HIGH:检出→重跑
            if isf: hi_flare+=1
        else:
            ov+=OV_LOW
            leaked += lam*DT                     # LOW:未检出→SDC 泄漏
            if isf: leaked_flare += lam*DT
    n=len(trace)
    return {"high_duty": 100*hi/n, "overhead": ov/n,
            "leaked_faults": leaked, "rerun_overhead_units": rerun,
            "leaked_in_flare": leaked_flare,
            "high_duty_in_flare": 100*hi_flare/max(1,n_flare)}

def main():
    tr = build_trace()
    sprt_fb = SPRT(R)
    sprt_hy = SPRT(R)
    sprt_res = SPRT(100.0)   # 残差检测器:H1=比排程基线高 100×(耀斑量级),对 SAA 周期不反应
    # 四控制器
    def sched(t): return 1 if in_saa(t) else 0
    def lam_sched(t): return LAM_SAA if in_saa(t) else LAM_Q
    def ff(t,lam):  return sched(t)                        # 前馈:只看星历 SAA
    def fb(t,lam):  return sprt_fb.step(lam, DT)           # 反馈:SPRT,H0=常数静默率
    def hy(t,lam):  return max(sched(t), sprt_hy.step(lam, DT))   # 朴素混合:max
    def hy2(t,lam):                                        # 残差混合:排程+只检耀斑
        return max(sched(t), sprt_res.step(lam, DT, lam0=lam_sched(t)))
    THRESH = 1.0e-4   # 保护值得与否的阈值(eps/e_low);静默<阈值<SAA
    def oracle(t,lam): return 1 if lam > THRESH else 0     # 完美 λ 预知,零滞后零误报
    res = {"纯前馈(排程)": evaluate(tr, ff),
           "纯反馈(SPRT)": evaluate(tr, fb),
           "朴素混合(max)": evaluate(tr, hy),
           "残差混合(排程+检耀斑)": evaluate(tr, hy2),
           "Oracle(λ预知)": evaluate(tr, oracle)}
    print(f"\n=== 开环/闭环/混合 对比 (LEO {ORBITS}轨 + 第3轨耀斑 {FL_FACTOR:.0e}×) ===")
    print(f"{'控制器':22}{'HIGH占空%':>10}{'开销×':>9}{'SDC泄漏':>12}{'重执行单元':>12}"
          f"{'耀斑期HIGH%':>13}{'耀斑期泄漏':>12}")
    for k,s in res.items():
        print(f"{k:22}{s['high_duty']:>10.1f}{s['overhead']:>9.3f}"
              f"{s['leaked_faults']:>12.2f}{s['rerun_overhead_units']:>12.2f}"
              f"{s['high_duty_in_flare']:>12.1f}%{s['leaked_in_flare']:>12.2f}")
    print("\n读法:")
    print(" 纯前馈:周期段占空比理想,但耀斑期 HIGH% 低、SDC 泄漏大 → 对突发失效")
    print(" 纯反馈:耀斑能抓到(λ巨大),但周期段有回落浪费(占空比虚高)")
    print(" 朴素混合(max):最安全但占空比最高(周期+检测叠加,没省到)")
    print(" 残差混合:排程管 SAA(零滞后)、检测器 H0 跟随排程只抓耀斑 → 占空比贴近前馈且耀斑不漏")
    print(" 重执行:耀斑期 λ 巨大,HIGH 下重跑开销猛涨 → 光靠提档不够,需配节流/降级")

if __name__ == "__main__":
    main()
