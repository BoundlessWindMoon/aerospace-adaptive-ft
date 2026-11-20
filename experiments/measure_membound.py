#!/usr/bin/env python3
"""验证假设:访存为主(低 IPC)的 kernel,FP 保护开销应很小(冗余计算填进访存空转周期)。
用 native IPC 当访存/计算判据,看 FP-only(dmr-generic)开销是否随 IPC 升高而升高。"""
import sys, statistics
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import benchmark_harness as H

SSA = H.build_infrastructure()
def pmu(tag, mode, flags, gold):
    p, _ = H.compile_and_evaluate_policy(SSA, tag, mode, budget=1.0, fi_trials=0,
                                         run_fi=False, dump_golden=gold, pass_flags=flags)
    return {r["benchmark"]: r for r in p}

nat = pmu("mb_native", "native", ["-atft-isa-avx512"], True)
fpo = pmu("mb_fp", "approx-simd", ["-atft-dmr-generic", "-atft-no-cse-barrier"], False)

rows = []
for b in nat:
    if b not in fpo: continue
    ni, nc, nt = nat[b]["hw_insts"], nat[b]["hw_cycles"], nat[b]["exec_time_us"]
    ipc = ni / nc if nc else 0
    ov = fpo[b]["exec_time_us"] / nt if nt else 0
    rows.append((b, ipc, ov))

rows.sort(key=lambda x: x[1])  # 按 IPC 升序(访存→计算)
print(f"\n{'benchmark':26}{'native IPC':>12}{'FP保护开销':>12}  类别")
for b, ipc, ov in rows:
    cls = "访存为主" if ipc < 1.0 else ("中间" if ipc < 2.0 else "计算密集")
    print(f"{b:26}{ipc:>12.2f}{ov:>11.2f}x  {cls}")

# 相关性:IPC 越高开销越高?
import math
xs = [r[1] for r in rows]; ys = [r[2] for r in rows]
n = len(xs); mx, my = statistics.mean(xs), statistics.mean(ys)
cov = sum((x-mx)*(y-my) for x,y in zip(xs,ys))/n
sx = statistics.pstdev(xs); sy = statistics.pstdev(ys)
corr = cov/(sx*sy) if sx and sy else 0
lo = [r[2] for r in rows if r[1] < 1.0]; hi = [r[2] for r in rows if r[1] >= 2.0]
print(f"\nPearson(IPC, FP开销) = {corr:+.2f}")
if lo: print(f"访存为主(IPC<1)平均 FP 开销 = {statistics.mean(lo):.2f}x  ({len(lo)} 个)")
if hi: print(f"计算密集(IPC≥2)平均 FP 开销 = {statistics.mean(hi):.2f}x  ({len(hi)} 个)")
print("假设:访存为主开销应明显低于计算密集(冗余计算填空转周期)。")
