#!/usr/bin/env python3
"""RAD750 双预算(100ms deadline + 功率)下的保护选择。
每个 λ(t)+功率预算,从保护菜单里选 deadline 与功率都可行、且残差 λ·e 最小的档;
对比无约束可靠性最优(总选最强档)。标出哪个预算 binding、何时被迫降级。
菜单 (m=开销×, e=残余每故障失效概率) 锚我们实测:full≈结构匹配完整圈2×、残差~0.02。"""
import math, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import atft_core as C

D = C.GNC_DEADLINE_S          # 100ms 硬截止
T0 = 0.040                    # 无保护周期计算 40ms
P_CPU = C.RAD750["cpu_power_w"]  # 5W 满算功率
PERIOD = C.LEO_PERIOD_S
SAA = C.LEO_SAA_FRAC*PERIOD
FL_ON = 3*PERIOD; FL=(0.5*3600,2.0*3600,1.0*3600); FLF=C.GEO_FLARE_FACTOR

# 保护菜单:名字,开销m,残余e,最坏尝试次数(retry 会把最坏墙钟翻倍)
MENU = [
    ("native",      1.0, 1.00, 1),
    ("partial",     1.4, 0.15, 1),
    ("full",        2.0, 0.02, 1),   # 结构匹配完整圈(SDC+addr-correct)
    ("full+retry",  2.0, 0.001, 2),  # 再加一次按需重执行 → 最坏墙钟×2
]

def twc(m, k):  return T0*m*k                       # 最坏墙钟
def avg_power(m, k): return P_CPU*(twc(m,k)/D)      # 周期内平均功率(active 占比×5W)

def feasible(m, k, pbudget):
    return twc(m,k) <= D + 1e-9 and avg_power(m,k) <= pbudget + 1e-9

def choose(lam, pbudget):
    best=None
    for name,m,e,k in MENU:
        if not feasible(m,k,pbudget): continue
        resid = lam*e
        if best is None or resid < best[3]:
            # 记 binding:为何更强的档不可行
            best=(name,m,e,resid,k)
    return best

def flare_gain(t):
    d=t-FL_ON
    if d<0: return 1.0
    if d<FL[0]: return FLF**(d/FL[0])
    if d<FL[0]+FL[1]: return FLF
    if d<sum(FL): return FLF**(1-(d-FL[0]-FL[1])/FL[2])
    return 1.0
def lam_at(t):
    base=C.LAM_LEO_SAA if (t%PERIOD)<SAA else C.LAM_LEO_QUIET
    return base*flare_gain(t)

def binding_reason(pbudget):
    # 哪个预算先卡住"更强档"。full+retry 最坏 160ms:deadline 先卡(>100ms)。
    reasons=[]
    if twc(2.0,2) > D: reasons.append("deadline(full+retry 160ms>100ms)")
    if avg_power(2.0,1) > pbudget: reasons.append(f"功率(full 需{avg_power(2.0,1):.1f}W>{pbudget}W)")
    if avg_power(1.4,1) > pbudget: reasons.append(f"功率(partial 需{avg_power(1.4,1):.1f}W>{pbudget}W)")
    return ",".join(reasons) if reasons else "无(可选最强可行档)"

def main():
    print(f"\n=== RAD750 双预算保护选择 (deadline {D*1000:.0f}ms, CPU {P_CPU}W) ===")
    print("菜单最坏墙钟/平均功率:", {n:(f"{twc(m,k)*1000:.0f}ms",f"{avg_power(m,k):.1f}W") for n,m,e,k in MENU})
    for scen,pb in [("日照 6W", 6.0), ("地影 2.5W", 2.5)]:
        print(f"\n--- {scen} ---   binding: {binding_reason(pb)}")
        print(f"{'阶段':12}{'λ(/s)':>11}{'无约束最优':>14}{'双预算选档':>14}{'残差λ·e':>12}{'降级?':>8}")
        for name,lam in [("平静",C.LAM_LEO_QUIET),("SAA内",C.LAM_LEO_SAA),
                         ("耀斑×quiet",C.LAM_LEO_QUIET*FLF),("耀斑×SAA",C.LAM_LEO_SAA*FLF)]:
            unc = min(MENU, key=lambda x:lam*x[2])[0]       # 无约束:选 e 最小(full+retry)
            ch = choose(lam, pb)
            degraded = "是" if ch[0]!=unc else ""
            print(f"{name:12}{lam:>11.2e}{unc:>14}{ch[0]:>14}{ch[3]:>12.2e}{degraded:>8}")
    print("\n读法:")
    print(" 无约束总选 full+retry(残差最低);但 full+retry 最坏 160ms>100ms,deadline 直接判不可行")
    print(" → 双预算下最强只能到 full(80ms),这是 deadline 对恢复的硬约束(不能加按需重试)")
    print(" 地影 2.5W:功率把 full(4W)也卡掉,只能 partial/native → 功率 binding,被迫进一步降级")
    print(" 结论:最优保护档由'可靠性'与'deadline+功率双预算'共同决定;耀斑+地影同压时被迫裸奔")

if __name__=="__main__":
    main()
