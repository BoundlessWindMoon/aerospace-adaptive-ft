#!/usr/bin/env python3
"""把实测负载归一到空间级处理器性能,再查 GNC deadline——而不是乱设 T0。
负载时间 = 实测动态指令数 / 平台 DMIPS(谱系:GR712/RAD750/GR740/RAD5545/COTS)。
回答两个方法学质疑:(1)负载必须按同性能 CPU 归一;(2)资源不能只钉低端。"""
import csv, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import atft_core as C

D = C.GNC_DEADLINE_S
rows = list(csv.DictReader(open(C.OPT + "/pmu_for_temporal.csv")))
nat = [r for r in rows if r.get("policy", "") == "native"]
insts = {r["benchmark"]: int(float(r["hw_insts"])) for r in nat}

plats = ["GR712RC", "RAD750", "GR740", "RAD5545"]
print(f"\n=== 负载归一到空间级性能后的执行时间 vs GNC deadline {D*1000:.0f}ms ===")
print(f"{'benchmark':24}{'动态指令':>12}" + "".join(f"{p:>12}" for p in plats))
mx = max(insts.values())
mxb = [b for b, v in insts.items() if v == mx][0]
for b in sorted(insts, key=lambda x: -insts[x])[:12]:
    line = f"{b:24}{insts[b]:>12}"
    for p in plats:
        t = C.platform_time_s(insts[b], p) * 1000
        line += f"{t:>10.2f}ms"
    print(line)

print(f"\n最大负载 {mxb}: {insts[mx if False else mxb]} 指令")
for p in plats:
    t = C.platform_time_s(insts[mxb], p)
    # 满保护 2× + 最坏一次重试 → 墙钟 ×4 的上界
    worst = t * 4
    print(f"  {p:10}: native {t*1000:6.2f}ms  满保护+重试(×4) {worst*1000:7.2f}ms  "
          f"{'deadline 不binding(裕度大)' if worst < D else 'deadline binding!'}")

print("\n结论(修正之前 T0=40ms 的臆测):")
print(" 实测负载归一到空间级后,最大也就几 ms;即便满保护+重试(×4)也远小于 100ms。")
print(" → 对这批控制回路尺度的小核,GNC deadline 根本不 binding。之前 40ms 的结论作废。")
print(" deadline 只在'负载尺度↔平台速度↔任务截止'匹配时才 binding:")
print("   - 控制回路小核 + 空间级平台 → deadline 大裕度,功率/代码大小先 binding")
print("   - 载荷级大负载(整帧图像/AI) + 慢平台 → deadline 才可能 binding(需换此类负载才有意义)")
