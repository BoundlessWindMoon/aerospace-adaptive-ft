#!/usr/bin/env python3
"""自适应 checkpoint 间隔 解决耀斑期重计算爆炸。
模型:一个窗口(有用工作 W + 存点 C)一次跑干净的概率 = e^{-λ(W+C)};
失败就回滚重做 → 期望尝试 e^{λ(W+C)} 次。有效吞吐 η = W / ((W+C)·e^{λ(W+C)}).
对比:固定间隔(按平静段调) vs 自适应 Young/Daly W*=√(2C/λ) vs Chinchilla 反应式减半。
坐实:固定间隔在耀斑下 η 塌到~0(爆炸);自适应把 η 救回;λC>1 时谁都救不了→该降级。"""
import math, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import atft_core as C_

C = 1.0        # 存点开销(秒)
E = 1.0        # 每故障致失效概率(=1,保守:窗口内任一故障即需重做)
PERIOD = C_.LEO_PERIOD_S
SAA = C_.LEO_SAA_FRAC * PERIOD
LAM_Q, LAM_SAA = C_.LAM_LEO_QUIET, C_.LAM_LEO_SAA
FL_ON = 3*PERIOD
FL_RISE, FL_PLAT, FL_DECAY = 0.5*3600, 2.0*3600, 1.0*3600
FL_FACTOR = 1.0e4
DT = 30.0
ORBITS = 10

def flare_gain(t):
    d = t-FL_ON
    if d < 0: return 1.0
    if d < FL_RISE: return FL_FACTOR**(d/FL_RISE)
    if d < FL_RISE+FL_PLAT: return FL_FACTOR
    if d < FL_RISE+FL_PLAT+FL_DECAY: return FL_FACTOR**(1.0-(d-FL_RISE-FL_PLAT)/FL_DECAY)
    return 1.0

def lam_at(t):
    base = LAM_SAA if (t % PERIOD) < SAA else LAM_Q
    return base*flare_gain(t)

def eff(W, lam):
    # 有效吞吐:有用工作 / 期望墙钟
    expo = lam*(W+C)*E
    if expo > 60: return W/((W+C)*math.exp(60))*math.exp(-(expo-60))  # 防溢出,仍单调
    return W/((W+C)*math.exp(expo))

def best_W(lam):
    # Young/Daly 解析 + 数值细修(目标最大化 eff)
    w0 = math.sqrt(2*C/max(lam*E, 1e-12))
    best, bw = -1, w0
    for f in (0.25,0.5,1,2,4):
        w = w0*f
        e = eff(w, lam)
        if e > best: best, bw = e, w
    return bw, best

def main():
    n = int(ORBITS*PERIOD/DT)
    trace = [(i*DT, lam_at(i*DT)) for i in range(n)]
    W_calm, _ = best_W(LAM_Q)      # 固定间隔:按平静段 Young/Daly 调死
    print(f"存点开销 C={C}s;固定间隔 W_fix={W_calm:.1f}s(按平静段调)")
    print(f"\n{'阶段':10}{'λ(/s)':>11}{'λ·C':>8}{'固定η':>10}{'自适应η':>10}{'自适应W(s)':>12}{'状态':>8}")
    # 采样几个代表点:平静 / SAA / 耀斑峰(叠SAA) / 耀斑峰(叠quiet)
    probes = [("平静", LAM_Q), ("SAA内", LAM_SAA),
              ("耀斑×quiet", LAM_Q*FL_FACTOR), ("耀斑×SAA", LAM_SAA*FL_FACTOR)]
    for name, lam in probes:
        wa, ea = best_W(lam)
        ef = eff(W_calm, lam)
        state = "正常" if ea > 0.1 else "须降级"
        print(f"{name:10}{lam:>11.2e}{lam*C:>8.2f}{ef:>10.4f}{ea:>10.4f}{wa:>12.2f}{state:>8}")
    # 全程积分:平均有效吞吐(时间加权)
    sf = sa = 0.0; collapse_fix = collapse_ad = 0
    for (t, lam) in trace:
        ef = eff(W_calm, lam); _, ea = best_W(lam)
        sf += ef; sa += ea
        if ef < 0.01: collapse_fix += 1
        if ea < 0.01: collapse_ad += 1
    print(f"\n全程({ORBITS}轨):平均有效吞吐  固定={100*sf/n:.1f}%  自适应={100*sa/n:.1f}%")
    print(f"吞吐塌陷(η<1%)步占比  固定={100*collapse_fix/n:.1f}%  自适应={100*collapse_ad/n:.1f}%")
    print("\n读法:")
    print(" 固定间隔:耀斑期 λ(W+C) 爆炸,η 塌到~0 → 程序卡死在重做,这就是'爆炸'")
    print(" 自适应:λ 升→W 自动缩,保持 λW<1,η 救回 → 程序继续推进")
    print(" λC>1 的点(耀斑×SAA):W 再小也没用(存点都比 MTBF 慢)→ 标'须降级',这是硬天花板")
    reactive_demo()

# ---- Chinchilla 式反应自适应:不知道 λ,靠"没推进就减半/连成功就涨"调 W ----
def reactive_demo():
    import random
    random.seed(1)
    T = ORBITS*PERIOD
    probes = [("固定W", "fixed"), ("oracle自适应", "oracle"), ("反应式(不知λ)", "reactive")]
    print(f"\n=== 部署视角:不知 λ 的反应式 vs oracle vs 固定(事件级模拟,{int(T)}s) ===")
    print(f"{'策略':16}{'有用工作完成(s)':>16}{'墙钟(s)':>12}{'有效吞吐%':>12}")
    Wfix, _ = best_W(LAM_Q)
    for name, mode in probes:
        wall = 0.0; useful = 0.0; W = Wfix; succ = 0
        while wall < T:
            lam = lam_at(wall % (ORBITS*PERIOD))
            if mode == "oracle":
                W, _ = best_W(lam)
            # 一次尝试耗时 W+C,成功概率 e^{-λ(W+C)}
            dur = W + C
            p = math.exp(-min(lam*dur*E, 60))
            wall += dur
            if random.random() < p:
                useful += W
                if mode == "reactive":
                    succ += 1
                    if succ >= 4: W = min(W*1.5, Wfix); succ = 0   # 连成功→涨回
            else:
                if mode == "reactive":
                    W = max(W*0.5, 0.02); succ = 0                 # 没推进→减半
        print(f"{name:16}{useful:>16.0f}{wall:>12.0f}{100*useful/wall:>12.1f}")
    print(" 反应式不需要知道 λ,照样把耀斑期吞吐救回(接近 oracle)→ 可部署,不依赖上帝视角。")

if __name__ == "__main__":
    main()
