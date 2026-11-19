#!/usr/bin/env python3
"""wide-DMR A/B（ISA 对齐）：native512 / simd64_512 / wide_dmr 都在 AVX-512 同一 ISA 下比。
native512 产 golden，其余比 rel_l2_err 看数值正确性；看 wide-DMR 把税压到多少、SDC 是否仍 0。
重点看 wide-DMR 真正转换掉的内核（jacobi1d 等）。"""
import math, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import benchmark_harness as H

ISA = ["-atft-isa-avx512"]
BASE = ["-atft-delta-runtime", "-atft-replica-exact", "-atft-no-cse-barrier", "-atft-simd-f64"]
TRIALS = 150
TARGETS = ["fp64"]
CFGS = [
    ("native512",  "native",      ISA,                                  True),
    ("simd64_512", "approx-simd", BASE + ISA,                           False),
    ("wide_dmr",   "approx-simd", BASE + ISA + ["-atft-wide-dmr"],      False),
    ("dmr_generic","approx-simd", ["-atft-dmr-generic"],                False),
]

SSA = H.build_infrastructure()
T, FI, RL = {}, {}, {}
for tag, mode, flags, gold in CFGS:
    T[tag], FI[tag], RL[tag] = {}, {}, {}
    for tgt in TARGETS:
        p, f = H.compile_and_evaluate_policy(SSA, f"wdmr_{tag}_{tgt}", mode, budget=1.0,
                                             fi_trials=TRIALS, run_fi=True, fi_target_type=tgt,
                                             dump_golden=gold, pass_flags=flags)
        for r in p:
            T[tag][r["benchmark"]] = r["exec_time_us"]
            RL[tag][r["benchmark"]] = r["rel_l2_err"]
        FI[tag][tgt] = {r["benchmark"]: r for r in f}

def geo(xs):
    xs = [x for x in xs if x > 0]
    return math.exp(sum(map(math.log, xs)) / len(xs)) if xs else float("nan")

B = list(T["native512"].keys())
def agg(tag, tgt, key):
    v = [FI[tag][tgt][b][key] for b in B if b in FI[tag][tgt]]
    return sum(v) / len(v) if v else float("nan")

print(f"\n{'config':12}{'t_tax':>7}{'fp64 sdc/det':>14}{'max rel_l2':>12}")
for tag, *_ in CFGS:
    tt = 1.0 if tag == "native512" else geo([T[tag][b]/T["native512"][b] for b in B if b in T[tag]])
    maxrl = max((RL[tag].get(b, 0.0) for b in B), default=0.0)
    print(f"{tag:12}{tt:>7.2f}{agg(tag,'fp64','sdc_pct'):>7.1f}/{agg(tag,'fp64','det_recovered_pct'):<6.1f}{maxrl:>12.2e}")

print("\n--- 逐核 time vs native512（全部内核，关注 wide-DMR 转换掉的 jacobi1d）---")
print(f"{'benchmark':22}{'wide/nat':>9}{'simd/nat':>9}{'rel_l2(wide)':>14}")
for b in sorted(B):
    if b not in T["wide_dmr"]:
        continue
    wn = T["wide_dmr"][b]/T["native512"][b]
    sn = T["simd64_512"][b]/T["native512"][b] if b in T["simd64_512"] else float("nan")
    print(f"{b:22}{wn:>9.2f}{sn:>9.2f}{RL['wide_dmr'].get(b,0.0):>14.2e}")
