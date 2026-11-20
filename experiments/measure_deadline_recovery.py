#!/usr/bin/env python3
"""硬 deadline 下的恢复:后向(重跑到干净) vs deadline 感知(限次+超时降级交付)。
全部锚 RAD750 profile(300 DMIPS, GNC 100ms 截止)。λ 取 LEO+耀斑轨迹。
坐实:deadline 把恢复目标从'跑到对'改成'截止前尽量对,超时必须降级交付'——
后向在耀斑期丢 deadline(控制回路致命),deadline 感知零超时、只留有界残差。"""
import math, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import atft_core as C

D = C.GNC_DEADLINE_S                 # 100ms 硬截止(RAD750/GNC 锚定)
T0 = 0.040                           # 无保护控制周期计算耗时 40ms(留 60ms 裕度)
M_HIGH = 2.0                         # HIGH 保护倍率(结构匹配完整圈 ~2×,见前文)
Cc = 0.002                           # 一次重试的存点/恢复开销 2ms

PERIOD = C.LEO_PERIOD_S
SAA = C.LEO_SAA_FRAC * PERIOD
FL_ON = 3*PERIOD
FL = (0.5*3600, 2.0*3600, 1.0*3600)
FLF = C.GEO_FLARE_FACTOR

def flare_gain(t):
    d=t-FL_ON
    if d<0: return 1.0
    if d<FL[0]: return FLF**(d/FL[0])
    if d<FL[0]+FL[1]: return FLF
    if d<sum(FL): return FLF**(1-(d-FL[0]-FL[1])/FL[2])
    return 1.0

def lam_at(t):
    base = C.LAM_LEO_SAA if (t%PERIOD)<SAA else C.LAM_LEO_QUIET
    return base*flare_gain(t)

def analyze(lam, m):
    """一个控制周期:每次尝试耗 t=T0*m+Cc,干净概率 p=e^{-λ·T0·m}。
    返回 (后向超时概率, deadline感知残差事件概率, 可用重试次数K)。"""
    t_try = T0*m + Cc
    p = math.exp(-min(lam*T0*m, 60))
    K = max(1, int(D // t_try))          # 截止内放得下几次尝试
    miss_backward = (1-p)**K             # K 次都没干净 → 后向会继续跑 → 超时
    resid_aware = (1-p)**K               # deadline感知:超时就交降级件,留这个残差概率
    return miss_backward, resid_aware, K

def main():
    print(f"\n=== 硬 deadline 恢复 (RAD750 300DMIPS, 截止 {D*1000:.0f}ms, 周期计算 {T0*1000:.0f}ms, HIGH {M_HIGH}×) ===")
    print(f"{'阶段':12}{'λ(/s)':>11}{'干净概率p':>11}{'可重试K':>9}{'后向超时%':>11}{'感知残差%':>11}")
    probes=[("平静",C.LAM_LEO_QUIET),("SAA内",C.LAM_LEO_SAA),
            ("耀斑×quiet",C.LAM_LEO_QUIET*FLF),("耀斑×SAA",C.LAM_LEO_SAA*FLF)]
    for name,lam in probes:
        mb,ra,K = analyze(lam, M_HIGH)
        t_try=T0*M_HIGH+Cc; p=math.exp(-min(lam*T0*M_HIGH,60))
        print(f"{name:12}{lam:>11.2e}{p:>11.4f}{K:>9d}{100*mb:>11.4f}{100*ra:>11.4f}")
    # 全程积分:后向平均超时率 vs 感知平均超时率(=0) + 平均残差
    n=int(10*PERIOD/30); miss_b=0.0; miss_a=0.0; resid_a=0.0
    for i in range(n):
        lam=lam_at(i*30.0); mb,ra,K=analyze(lam,M_HIGH)
        miss_b+=mb; resid_a+=ra  # 感知永不超时 miss_a=0
    print(f"\n全程(10轨):")
    print(f"  后向(跑到干净)   平均 deadline 超时率 = {100*miss_b/n:.3f}%   ← 控制回路致命")
    print(f"  deadline感知     deadline 超时率 = 0.000%   平均残差(降级件)概率 = {100*resid_a/n:.3f}%")
    print("\n读法:")
    print(" 后向恢复在耀斑期干净概率塌→K 次跑不完→继续重跑→丢 deadline(控制回路不可接受)")
    print(" deadline感知:限 K 次,超时就交 best-effort/降级件→永不超时,只把'超时'换成'有界残差'")
    print(" 这就是 deadline 把恢复目标从'跑到对'改成'截止前尽量对,否则降级交付'的量化证据")

if __name__=="__main__":
    main()
