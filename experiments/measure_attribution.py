#!/usr/bin/env python3
"""归因:逐机制累加,量每层对 1.99× 的边际贡献 + 每 benchmark 成本分布(只测 PMU 计时)。"""
import sys, csv, statistics
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import benchmark_harness as H

# 累加顺序:FP → +分支 → +地址 → +整数(= struct_full)
STEPS = [
    ("native", "native",      ["-atft-isa-avx512"]),
    ("+FP",    "approx-simd", ["-atft-dmr-generic", "-atft-no-cse-barrier"]),
    ("+branch","approx-simd", ["-atft-dmr-generic", "-atft-cmp-dmr", "-atft-no-cse-barrier"]),
    ("+addr",  "approx-simd", ["-atft-dmr-generic", "-atft-cmp-dmr", "-atft-addr-dmr", "-atft-no-cse-barrier"]),
    ("+int",   "approx-simd", ["-atft-dmr-generic", "-atft-cmp-dmr", "-atft-addr-dmr", "-atft-int-residue", "-atft-no-cse-barrier"]),
]
SSA = H.build_infrastructure()
etime = {}
for tag, mode, flags in STEPS:
    p, _ = H.compile_and_evaluate_policy(
        SSA, f"attr_{tag.strip('+')}", mode, budget=1.0, fi_trials=0,
        run_fi=False, dump_golden=(tag == "native"), pass_flags=flags)
    etime[tag] = {r["benchmark"]: r["exec_time_us"] for r in p}

bt = etime["native"]
def ov(tag):
    pt = etime[tag]
    return statistics.median([pt[b] / bt[b] for b in bt if b in pt and bt[b] > 0])

print("\n=== 逐机制累加开销(vs native 中位) ===")
prev = 1.0
for tag, *_ in STEPS:
    if tag == "native":
        print(f"  {tag:9} {1.00:.2f}x"); continue
    o = ov(tag)
    print(f"  {tag:9} 累计 {o:.2f}x   边际 +{o - prev:+.2f}")
    prev = o

# 每 benchmark:native→struct_full 的开销,找成本集中处
print("\n=== 成本分布:struct_full(+int)各 benchmark 开销,降序前 12 ===")
pt = etime["+int"]
rows = sorted([(b, pt[b] / bt[b]) for b in bt if b in pt and bt[b] > 0],
              key=lambda x: -x[1])
for b, o in rows[:12]:
    print(f"  {b:26} {o:6.2f}x")
print(f"  ... 共 {len(rows)} 个;中位 {statistics.median([o for _,o in rows]):.2f}x  "
      f"均值 {statistics.mean([o for _,o in rows]):.2f}x")

OUT = H.RES_DIR / "attribution.csv"
with open(OUT, "w", newline="") as fp:
    w = csv.writer(fp); w.writerow(["benchmark"] + [t for t,*_ in STEPS])
    for b in sorted(bt):
        w.writerow([b] + [round(etime[t].get(b, 0) / bt[b], 3) if bt[b] else "" for t,*_ in STEPS])
print("CSV ->", OUT)
