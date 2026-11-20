#!/usr/bin/env python3
"""P3 收口:把 int-TMR 接入向量化路径后,实测它能否覆盖 O3 产生的向量 cast。
验证 sphere_selection demo 里唯一的乐观假设(A 覆盖可迁移到向量化后的 cast)。"""
import sys, csv
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import benchmark_harness as H

TRIALS = 200
CATS = ["cast", "int_arith"]
CFGS = [
    ("vec_dmr",        "approx-simd", ["-atft-dmr-generic"]),
    ("vec_dmr_inttmr", "approx-simd", ["-atft-dmr-generic", "-atft-int-tmr-wire", "-atft-int-tmr-i64"]),
]
SSA = H.build_infrastructure()
def pct(k, n): return 100.0 * k / n if n else 0.0
res = {}
for tag, mode, flags in CFGS:
    for cat in CATS:
        _, f = H.compile_and_evaluate_policy(
            SSA, f"xfer_{tag}_{cat}", mode, budget=1.0, fi_trials=TRIALS,
            run_fi=True, fi_target_type=cat, dump_golden=True, pass_flags=flags)
        k = sum(round(r["sdc_pct"] / 100 * r["trials"]) for r in f)
        n = sum(r["trials"] for r in f)
        res[(tag, cat)] = (k, n)

print("\n=== cast/int SDC:int-TMR 接入向量化路径前后 (对自身 golden, Wilson CI) ===")
print(f"{'config':18}" + "".join(f"{c:>16}" for c in CATS))
for tag, *_ in CFGS:
    row = f"{tag:18}"
    for c in CATS:
        k, n = res[(tag, c)]; _, _, h = H.wilson_ci95(k, n)
        row += f"{pct(k,n):9.1f}±{h:4.1f} "
    print(row)

print("\n=== 迁移判定(cast) ===")
for c in CATS:
    kb, nb = res[("vec_dmr", c)]; kp, np_ = res[("vec_dmr_inttmr", c)]
    lo_b, hi_b, _ = H.wilson_ci95(kb, nb); lo_p, hi_p, _ = H.wilson_ci95(kp, np_)
    disjoint = (lo_b > hi_p) or (lo_p > hi_b)
    verdict = ("显著↓:int-TMR 覆盖到向量 cast" if (disjoint and pct(kp,np_) < pct(kb,nb))
               else "无显著下降:int-TMR 覆盖不到向量 cast(乐观假设被否)")
    print(f"  {c:10} vec_dmr {pct(kb,nb):5.1f}% [{lo_b:.1f},{hi_b:.1f}] → "
          f"+int-TMR {pct(kp,np_):5.1f}% [{lo_p:.1f},{hi_p:.1f}] : {verdict}")

OUT = H.RES_DIR / "transfer.csv"
with open(OUT, "w", newline="") as fp:
    w = csv.writer(fp); w.writerow(["config", "category", "sdc_pct", "sdc_ci95_half", "n"])
    for tag, *_ in CFGS:
        for c in CATS:
            k, n = res[(tag, c)]; _, _, h = H.wilson_ci95(k, n)
            w.writerow([tag, c, round(pct(k, n), 2), round(h, 2), n])
print("CSV ->", OUT)
