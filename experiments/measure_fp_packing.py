#!/usr/bin/env python3
"""定位 FP 数据通路那 5.9× 税到底卡在哪：对比 标量近似 / <2xfloat> / <2xdouble> 三种打包，
量 时间税 + fp64 覆盖 + SIMD bookkeeping(pack/unpack/转换)指令占比(RELATED_WORK 的 20% 否证线)。"""
import math, sys, csv, re
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import benchmark_harness as H

BASE = ["-atft-delta-runtime", "-atft-replica-exact", "-atft-no-cse-barrier"]
TRIALS = 150
CFGS = [
    ("native", "native",       []),
    ("scalar", "approx-scalar", BASE),
    ("simd32", "approx-simd",   BASE),
    ("simd64", "approx-simd",   BASE + ["-atft-simd-f64"]),
]

def bookkeep_frac(ll_path):
    """pack/unpack + fp 转换 指令 占全部指令的比例。"""
    try:
        txt = Path(ll_path).read_text()
    except OSError:
        return float("nan")
    bk = len(re.findall(r"\b(extractelement|insertelement|shufflevector|fptrunc|fpext)\b", txt))
    total = len(re.findall(r"^\s+%?\S+\s*=\s*\w+|^\s+(store|br|ret|call)\b", txt, re.M))
    return bk / total if total else float("nan")

SSA = H.build_infrastructure()
T, S, BK = {}, {}, {}
for tag, mode, flags in CFGS:
    T[tag] = {}
    p, f = H.compile_and_evaluate_policy(
        SSA, f"pk_{tag}", mode, budget=1.0, fi_trials=TRIALS,
        run_fi=True, fi_target_type="fp64", pass_flags=flags)
    for r in p:
        T[tag][r["benchmark"]] = r["exec_time_us"]
    S[tag] = {r["benchmark"]: r["sdc_pct"] for r in f}
    BK[tag] = bookkeep_frac(H.BUILD_DIR / f"sut_pk_{tag}.ll")

def geomean(xs):
    xs = [x for x in xs if x > 0]
    return math.exp(sum(map(math.log, xs))/len(xs)) if xs else float("nan")

benches = list(T["native"].keys())
print(f"\n{'config':10} {'t_tax':>7} {'fp64_sdc mean/max':>18} {'bookkeep%':>10}")
rows = []
for tag, *_ in CFGS:
    sv = [S[tag][b] for b in benches if b in S[tag]]
    sm, sx = (sum(sv)/len(sv), max(sv)) if sv else (float("nan"), float("nan"))
    if tag == "native":
        print(f"{tag:10} {'1.00':>7} {sm:>8.2f}/{sx:<8.2f} {BK[tag]*100:>9.1f}")
    else:
        tt = geomean([T[tag][b]/T['native'][b] for b in benches if b in T[tag]])
        print(f"{tag:10} {tt:>7.2f} {sm:>8.2f}/{sx:<8.2f} {BK[tag]*100:>9.1f}")
        rows.append({"config": tag, "time_tax_geo": round(tt,3),
                     "fp64_sdc_mean": round(sm,3), "fp64_sdc_max": round(sx,3),
                     "bookkeep_frac": round(BK[tag],4)})
csv_path = Path(H.ROOT_DIR) / "results" / "opt" / "fp_packing.csv"
with open(csv_path, "w", newline="") as fh:
    w = csv.DictWriter(fh, fieldnames=["config","time_tax_geo","fp64_sdc_mean","fp64_sdc_max","bookkeep_frac"])
    w.writeheader(); w.writerows(rows)
print(f"\nwrote {csv_path}")
