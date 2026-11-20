#!/usr/bin/env python3
"""坐实分支=②资源层(ILP 空槽):量 cmp-DMR 相对纯 FP-DMR 的 marginal 开销。
若 marginal≈1(复制比较骑 ILP 几乎免费),则分支与浮点同属②资源层得到实测支撑。"""
import sys, csv, statistics
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import benchmark_harness as H

TRIALS = 200
CATS = ["fp64", "cmp_branch"]
BAR = ["-atft-no-cse-barrier"]
CFGS = [
    ("native",  "native",      ["-atft-isa-avx512"]),
    ("fp_only", "approx-simd", ["-atft-dmr-generic"] + BAR),
    ("fp_cmp",  "approx-simd", ["-atft-dmr-generic", "-atft-cmp-dmr"] + BAR),
]
SSA = H.build_infrastructure()
def pct(k, n): return 100.0 * k / n if n else 0.0

res = {}; etime = {}
for tag, mode, flags in CFGS:
    for cat in CATS:
        p, f = H.compile_and_evaluate_policy(
            SSA, f"mrg_{tag}_{cat}", mode, budget=1.0, fi_trials=TRIALS,
            run_fi=True, fi_target_type=cat, dump_golden=True, pass_flags=flags)
        ks = sum(round(r["sdc_pct"] / 100 * r["trials"]) for r in f)
        n = sum(r["trials"] for r in f)
        res[(tag, cat)] = (ks, n)
        if cat == "fp64":
            etime[tag] = {r["benchmark"]: r["exec_time_us"] for r in p}

def ov(tag, base="native"):
    bt = etime[base]; pt = etime[tag]
    rs = [pt[b] / bt[b] for b in bt if b in pt and bt[b] > 0]
    return statistics.median(rs) if rs else float("nan")

print("\n=== 分支=②资源层(ILP 空槽)坐实 ===")
print(f"{'config':9}{'fp64_SDC':>10}{'cmp_SDC':>10}{'overhead/native':>17}")
for tag, *_ in CFGS:
    f_sdc = pct(*res[(tag, "fp64")][::-1][::-1]) if False else pct(res[(tag,"fp64")][0], res[(tag,"fp64")][1])
    c_sdc = pct(res[(tag, "cmp_branch")][0], res[(tag, "cmp_branch")][1])
    print(f"{tag:9}{f_sdc:>10.1f}{c_sdc:>10.1f}{ov(tag):>16.2f}x")

marg = ov("fp_cmp", base="fp_only")
print(f"\ncmp-DMR 相对纯 FP-DMR 的 marginal 开销 = {marg:.3f}x")
print(f"  (纯 FP-DMR {ov('fp_only'):.2f}× → 加分支保护 {ov('fp_cmp'):.2f}×)")
print("  读法:marginal≈1 → 复制比较骑 ILP 空槽近免费 → 分支确属②资源层(同浮点 SIMD)。")

OUT = H.RES_DIR / "cmp_marginal.csv"
with open(OUT, "w", newline="") as fp:
    w = csv.writer(fp)
    w.writerow(["config", "fp64_sdc", "cmp_branch_sdc", "overhead_vs_native"])
    for tag, *_ in CFGS:
        w.writerow([tag, round(pct(res[(tag,"fp64")][0], res[(tag,"fp64")][1]), 2),
                    round(pct(res[(tag,"cmp_branch")][0], res[(tag,"cmp_branch")][1]), 2),
                    round(ov(tag), 3)])
    w.writerow(["marginal_cmp_over_fp", "", "", round(marg, 3)])
print("CSV ->", OUT)
