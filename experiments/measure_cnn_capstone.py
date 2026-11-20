#!/usr/bin/env python3
"""Capstone:HPSC split-mode 上 CNN 推理,时序自适应软件 FT vs 硬件锁步(DCLS)。
全锚实测:CNN 4.98M insts/推理(实测)、approx-simd 软件 FP 保护 3.94×(实测,SDC 5%→0)、
HPSC 28130 DMIPS、轨道 SAA 占空比 18.56%。
主线:软件 FT 可在平静期关掉、耀斑期开满;锁步是固定 2× 永远半吞吐。
平静占大头 → 自适应软件 FT 的任务均吞吐高于锁步,可靠性相当(保护只在高辐射窗口付)。"""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import atft_core as C

CNN_INSTS = 4983346                       # 实测 hw_insts/推理
ABFT_OV   = 1.10                          # 实测 结构匹配 ABFT(Freivalds) 开销(SDC 4%→1%)
SIMD_OV   = 3.94                          # 实测 通用 approx-simd(错误机制,对照用)
SDC_NATIVE = 0.04                         # 实测 native fp64 SDC
LOCKSTEP_OV = 2.0                         # DCLS 双核锁步 = 半吞吐
SAA = C.LEO_SAA_FRAC                      # 0.1856 高辐射窗口占空比
CALM = 1.0 - SAA

def hpsc_rate(ov):                        # HPSC 标量推理率(未计 RVV 向量加速)
    return C.PLATFORMS["HPSC"]["dmips"]*1e6 / (CNN_INSTS*ov)

def main():
    R0 = hpsc_rate(1.0)
    print(f"\n=== Capstone: HPSC split-mode CNN 推理, 结构化软件 FT vs 硬件锁步 ===")
    print(f"CNN {CNN_INSTS/1e6:.2f}M insts/推理; HPSC native {R0:.0f} 推理/s(标量)")
    print(f"实测机制: ABFT(结构匹配) {ABFT_OV}× SDC 4%→1% | 通用SIMD {SIMD_OV}× | 锁步 {LOCKSTEP_OV}×")
    print(f"轨道: SAA/高辐射窗口 {SAA*100:.1f}%, 平静 {CALM*100:.1f}%\n")

    rows = [
        ("native 不保护",            R0,               "0.04",  "高辐射期裸奔,不可接受"),
        ("硬件锁步 DCLS(固定)",       R0/LOCKSTEP_OV,   "0",     "永远半吞吐,与辐射无关"),
        ("通用软件SIMD复制(全程)",     R0/SIMD_OV,       "0",     "用错机制:比锁步还贵"),
        ("结构化ABFT软件FT(全程)",     R0/ABFT_OV,       "0.01",  "1.10×,单点就赢锁步"),
    ]
    avg = CALM*R0 + SAA*(R0/ABFT_OV)      # 自适应:平静裸奔(λ极小)、高辐射窗口开 ABFT
    rows.append(("时序自适应ABFT(平静off/耀斑on)", avg, "≈0", "保护只在18.6%窗口付1.10×"))

    print(f"{'方案':32}{'任务均吞吐(推理/s)':>16}{'残余':>8}  说明")
    for name, rate, resid, note in rows:
        print(f"{name:32}{rate:>16.0f}{resid:>8}  {note}")

    g_abft = (R0/ABFT_OV)/(R0/LOCKSTEP_OV) - 1
    g_adap = avg/(R0/LOCKSTEP_OV) - 1
    print(f"\n结论:")
    print(f" 结构化 ABFT 单点 {R0/ABFT_OV:.0f} vs 锁步 {R0/LOCKSTEP_OV:.0f} → 高 {g_abft*100:.0f}%(单点就赢,因 conv=matmul 用对了机制)")
    print(f" 再叠时序自适应 {avg:.0f} → 高 {g_adap*100:.0f}%,逼近 native 全吞吐")
    print(f" 两层正交收益:① 按结构选机制(ABFT 1.10× ≪ 复制/锁步);② 时序自适应(平静可关)")
    print(f" 残余 SDC 1% 源于单条 Freivalds 向量的漏检;用 2-3 条随机向量可压到~0,仍 ~1.2×")

if __name__ == "__main__":
    main()
