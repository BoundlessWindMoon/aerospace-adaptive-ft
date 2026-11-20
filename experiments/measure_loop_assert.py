#!/usr/bin/env python3
"""层① 分支循环索引断言 vs 层② cmp 复制。看结构断言能否覆盖判决翻转、且更便宜,
同时核对 fault-free 不误报(断言在正确执行下必成立)。"""
import sys, csv, statistics
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import benchmark_harness as H

TRIALS = 200
CATS = ["fp64", "cmp_branch"]
BAR = ["-atft-no-cse-barrier"]
CFGS = [
    ("native",     "native",      ["-atft-isa-avx512"]),
    ("cmp_dmr",    "approx-simd", ["-atft-dmr-generic", "-atft-cmp-dmr"] + BAR),
    ("loop_assert","approx-simd", ["-atft-dmr-generic", "-atft-loop-assert"] + BAR),
]
SSA = H.build_infrastructure()
def pct(k, n): return 100.0 * k / n if n else 0.0

res = {}; etime = {}
for tag, mode, flags in CFGS:
    for cat in CATS:
        p, f = H.compile_and_evaluate_policy(
            SSA, f"lpa_{tag}_{cat}", mode, budget=1.0, fi_trials=TRIALS,
            run_fi=True, fi_target_type=cat, dump_golden=True, pass_flags=flags)
        ks = sum(round(r["sdc_pct"] / 100 * r["trials"]) for r in f)
        kd = sum(round(r["due_pct"] / 100 * r["trials"]) for r in f)
        krec = sum(round(r["det_recovered_pct"] / 100 * r["trials"]) for r in f)
        km = sum(round(r["masked_pct"] / 100 * r["trials"]) for r in f)
        n = sum(r["trials"] for r in f)
        fa = sum(r.get("false_alarms", 0) for r in p)  # PMU 行里的误报计数
        res[(tag, cat)] = (ks, kd, krec, km, n, fa)
        if cat == "fp64":
            etime[tag] = {r["benchmark"]: r["exec_time_us"] for r in p}

def overhead(tag):
    bt = etime["native"]; pt = etime[tag]
    rs = [pt[b] / bt[b] for b in bt if b in pt and bt[b] > 0]
    return statistics.median(rs) if rs else float("nan")

print("\n=== cmp_branch 结局分解 + 开销 + 误报 ===")
print(f"{'config':12}{'SDC':>8}{'DUE':>8}{'det_rec':>9}{'masked':>8}{'overhead':>10}{'false_al':>9}")
for tag, *_ in CFGS:
    ks, kd, krec, km, n, _ = res[(tag, "cmp_branch")]
    fa = res[(tag, "fp64")][5] + res[(tag, "cmp_branch")][5]
    print(f"{tag:12}{pct(ks,n):8.1f}{pct(kd,n):8.1f}{pct(krec,n):9.1f}{pct(km,n):8.1f}"
          f"{overhead(tag):9.2f}x{fa:9d}")

OUT = H.RES_DIR / "loop_assert.csv"
with open(OUT, "w", newline="") as fp:
    w = csv.writer(fp); w.writerow(["config","sdc_pct","due_pct","det_rec_pct","n","overhead","false_alarms"])
    for tag, *_ in CFGS:
        ks, kd, krec, km, n, _ = res[(tag, "cmp_branch")]
        fa = res[(tag, "fp64")][5] + res[(tag, "cmp_branch")][5]
        w.writerow([tag, round(pct(ks,n),2), round(pct(kd,n),2), round(pct(krec,n),2), n,
                    round(overhead(tag),3), fa])
print("CSV ->", OUT)
