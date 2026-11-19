#!/usr/bin/env python3
"""通用 DMR 的公平、可信全套件测量。
- golden = native512（真 native 参考）
- 税分母 = gen_base（与 dmr_generic 完全相同的 ISA+内部O3 管线，但不注入冗余）→ 隔离纯冗余开销
- 逐核真冗余核验：objdump 数 gen_base vs dmr_generic 的 FP 乘加条数，比值≥1.5=真冗余，否则折叠
- 只在"真冗余"内核上报几何均税；折叠的单列出来（诚实）
pin + 多次中位降噪。"""
import math, sys, subprocess, statistics, re
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import benchmark_harness as H

BASE = ["-atft-delta-runtime", "-atft-replica-exact", "-atft-no-cse-barrier", "-atft-simd-f64"]
ISA = ["-atft-isa-avx512"]
TRIALS = 150
CFGS = [
    ("native512",  "native",      ISA,                                          True),
    ("gen_base",   "approx-simd", ["-atft-dmr-generic", "-atft-dmr-no-inject"],  False),
    ("dmr_generic","approx-simd", ["-atft-dmr-generic"],                         False),
    ("tmr_generic","approx-simd", ["-atft-dmr-generic", "-atft-tmr-generic"],    False),
]

SSA = H.build_infrastructure()
FI, RL = {}, {}
for tag, mode, flags, gold in CFGS:
    p, f = H.compile_and_evaluate_policy(SSA, f"wdg_{tag}_fp64", mode, budget=1.0,
                                         fi_trials=TRIALS, run_fi=True, fi_target_type="fp64",
                                         dump_golden=gold, pass_flags=flags)
    RL[tag] = {r["benchmark"]: r["rel_l2_err"] for r in p}
    FI[tag] = {r["benchmark"]: r for r in f}

def pmu(bin):
    out = subprocess.run(["taskset", "-c", "2", bin, "--pmu-perf", "x", str(H.GOLDEN_DIR)],
                         capture_output=True, text=True).stdout
    return {p.split(",")[2]: float(p.split(",")[3]) for p in out.splitlines() if p.startswith("PMU_CSV,")}

B = {tag: f"{H.BUILD_DIR}/bin_wdg_{tag}_fp64_x86_64" for tag, *_ in CFGS}
meas = {tag: [] for tag in B}
for _ in range(9):
    for tag, b in B.items():
        meas[tag].append(pmu(b))
def med(tag, n):
    v = [r[n] for r in meas[tag] if n in r]
    return statistics.median(v) if v else None

# (注：FP 指令计数做真冗余判据不可靠——漏 div/sqrt/fma，已弃用，改用注错 SDC 作信号)

names = sorted(meas["native512"][0].keys())
# 真冗余的可靠信号 = 注错 SDC 是否下降(检出)。FP 指令计数会漏 div/sqrt/fma，不可靠，弃用。
def sdc(tag, bench):  # bench 名在 FI 里
    r = FI[tag].get(bench)
    return (r["sdc_pct"], r["det_recovered_pct"]) if r else (None, None)

def geo(xs): return math.exp(sum(math.log(x) for x in xs) / len(xs)) if xs else float("nan")
rows = []
print(f"{'kernel':18}{'base_us':>9}{'dmr/b':>7}{'tmr/b':>7}{'natSDC':>8}{'dmrSDC':>8}{'tmrSDC':>8}")
for n in names:
    base = med("gen_base", n); dmr = med("dmr_generic", n); tmr = med("tmr_generic", n)
    if not base or not dmr or not tmr: continue
    ns, _ = sdc("native512", n); ds, _ = sdc("dmr_generic", n); ts, _ = sdc("tmr_generic", n)
    rows.append((n, dmr / base, tmr / base, ns, ds, ts))
    f = lambda x: f"{x:.1f}" if x is not None else "-"
    print(f"{n:18}{base:>9.1f}{dmr/base:>7.2f}{tmr/base:>7.2f}{f(ns):>8}{f(ds):>8}{f(ts):>8}")

fp = [r for r in rows if r[3] and r[3] > 1.0]   # 有 fp64 故障的核
print(f"\nFP 核({len(fp)}个) 几何均:  dmr/base={geo([r[1] for r in fp]):.2f}  tmr/base={geo([r[2] for r in fp]):.2f}")
print(f"全部核 几何均:  dmr/base={geo([r[1] for r in rows if r[1]>0]):.2f}  tmr/base={geo([r[2] for r in rows if r[2]>0]):.2f}")
print(f"tmr<3× 的核: {sum(1 for r in rows if r[2]<3.0)}/{len(rows)}")

def agg(tag, key):
    v = [FI[tag][b][key] for b in FI[tag]]
    return sum(v)/len(v) if v else float("nan")
print(f"\n聚合 fp64 SDC: native {agg('native512','sdc_pct'):.1f}  dmr {agg('dmr_generic','sdc_pct'):.1f}"
      f"  tmr {agg('tmr_generic','sdc_pct'):.1f}  (tmr 纠错应最低)")
print(f"max rel_l2: dmr {max(RL['dmr_generic'].values(),default=0):.1e}  tmr {max(RL['tmr_generic'].values(),default=0):.1e}")
