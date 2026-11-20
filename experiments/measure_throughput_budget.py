#!/usr/bin/env python3
"""载荷实时吞吐 vs 现代平台(HPSC / Jetson Orin Nano)——CPU 性能与负载对齐当代。
insts/pixel 实测归一;HPSC=RISC-V RVV 512-bit 向量;Jetson 靠 nvpmodel 调频/关核限功率。
回答:(1)限频=真实 degrade 执行器(Jetson 7W/15W);(2)两平台都有向量单元,
我们的 SIMD 结构保护直接落在 RVV/NEON 上;(3)不用老件。
来源:NASA cFS 2025 / SiFive X280 / NVIDIA L4T nvpmodel / DLR S4Pro。"""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import atft_core as C

IPP = 485626 / (48*48)          # 实测 sobel 指令/像素 ≈ 211(当代应换 CNN 推理核,见末尾)
FLARE_REEXEC = 1.8

def achievable_mpix(dmips, m):
    return dmips * 1e6 / (IPP * m) / 1e6

def main():
    print(f"\n=== 载荷实时吞吐(现代平台, {IPP:.0f} insts/pixel) ===")
    # 平台 + 频率/功率模式 → 标量 DMIPS
    configs = [
        ("HPSC(8×X280@1GHz,全吞吐)", C.platform_dmips("HPSC")),
        ("Jetson 15W(6核@1.5GHz)",   C.platform_dmips("JetsonOrinNano8G", "15W")),
        ("Jetson 7W(限频:4核@960MHz)", C.platform_dmips("JetsonOrinNano8G", "7W")),
    ]
    print(f"{'平台·频率模式':30}{'DMIPS':>9}{'native':>10}{'full(2x)':>10}{'full+耀斑':>11}")
    for name, dmips in configs:
        print(f"{name:30}{dmips:>9}{achievable_mpix(dmips,1.0):>8.0f}  "
              f"{achievable_mpix(dmips,2.0):>8.0f}  {achievable_mpix(dmips,2.0*FLARE_REEXEC):>9.0f}")

    print(f"\n真实传感器需求(MSamples/s) vs HPSC 可处理(coverage):")
    print(f"{'传感器/标准':28}{'需求':>8}{'native':>9}{'full':>8}{'full+耀斑':>11}")
    hpsc = C.platform_dmips("HPSC")
    for name, demand in [("PROBA-V 90Mbps@12b",7.5),("Sentinel-2 490Mbps@12b",40.8),
                         ("CCSDS-122 最低要求",20.0),("CCSDS-IDC(V5QV)",128.0)]:
        cn=min(1,achievable_mpix(hpsc,1.0)/demand); cf=min(1,achievable_mpix(hpsc,2.0)/demand)
        cfl=min(1,achievable_mpix(hpsc,2.0*FLARE_REEXEC)/demand)
        print(f"{name:28}{demand:>8.1f}{100*cn:>8.0f}%{100*cf:>7.0f}%{100*cfl:>10.0f}%")

    print(f"\n限频=功率 degrade 执行器(Jetson,真实 nvpmodel 模式):")
    d15=C.platform_dmips("JetsonOrinNano8G","15W"); d7=C.platform_dmips("JetsonOrinNano8G","7W")
    print(f"  15W(6核@1.5GHz): {achievable_mpix(d15,2.0):.0f} Mpix/s @保护  能耗≈{15/achievable_mpix(d15,2.0):.2f} J/Mpix")
    print(f"   7W(4核@960MHz): {achievable_mpix(d7,2.0):.0f} Mpix/s @保护  能耗≈{7/achievable_mpix(d7,2.0):.2f} J/Mpix")
    print(f"  → 地影/功率紧时降到 7W:吞吐降 {100*(1-d7/d15):.0f}%,但功率减半;这是独立于'降保护档'的另一个 degrade 轴")

    print("\n要点:")
    print(" 1 限频:要。Jetson 7W/15W 就是关核+限频,是真实功率执行器=另一个 degrade 轴(不只降保护档)")
    print(" 2 向量单元:都有。HPSC=RVV1.0 512-bit(可拼4096,FP16/32/64);Jetson=NEON128+GPU")
    print("   → 我们的 SIMD 结构保护(②SIMD 车道冗余打包)直接落在 RVV/NEON 上,契合")
    print(" 3 对齐当代:平台已换 HPSC/Jetson;负载下一步应把 Sobel 换成 CNN 推理核")
    print("   (X280 官方基准就是 MobileNet v1 VLEN=512,向量化 ~24×),才算 CPU+负载都当代")

if __name__ == "__main__":
    main()
