#!/usr/bin/env python3
"""执行线 Step 1: 锁死测量地基 —— 固定口径主矩阵 + Wilson 95% CI。

折入本会话所有注入器修正(P0-4 空注入、addr 下标定义处注入、det_rec/SDC 列序),
固定: 代表性 BTAG_REPR + 空天子集、4 配置、各类别、trials=400、Wilson CI。
load 标 ECC 域(SWIFT SoR 之外),单独列但不进"保护声明"聚合。
产出 results/main_matrix.csv —— 后续 P1/P2' 及论文主表的唯一可信口径。
"""
import sys, csv
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import benchmark_harness as H

TRIALS = 400
CATS = ["fp64", "int_arith", "cast", "cmp_branch", "addr", "load"]
ECC = {"load"}   # load/store 数据属 ECC;不进保护聚合
BAR = ["-atft-no-cse-barrier"]
STRUCT = ["-atft-dmr-generic", "-atft-addr-dmr", "-atft-int-residue", "-atft-cmp-dmr",
          "-atft-abft-insert", "-atft-layer-exclusive", "-atft-simd-f64",
          "-atft-replica-exact"] + BAR
GENERIC = ["-atft-dmr-generic", "-atft-int-tmr-wire", "-atft-int-tmr-i64", "-atft-cmp-dmr"] + BAR
# (tag, mode, flags, env)
CFGS = [
    ("native",      "native",      ["-atft-isa-avx512"], None),
    ("generic_tmr", "approx-simd", GENERIC, None),
    ("exact_tmr",   "exact-tmr",   [], None),
    ("struct_full", "approx-simd", STRUCT, {"ATFT_ABFT": "1"}),
]
# 代表性子集:覆盖 roofline 两侧(算力受限 gemm/2mm;访存受限 stencil)+整数图像+空天闭/开环
SUBSET = ("pb_gemm,pb_2mm,pb_cholesky,pb_jacobi2d,pb_heat3d,pb_seidel2d,"
          "obpmark_image_proc,gnc_kalman,orbit_rk4,cnn_cls_decision")
import os
os.environ["ATFT_BENCH_SUBSET"] = SUBSET


def pct(k, n):
    return 100.0 * k / n if n else 0.0


SSA = H.build_infrastructure()
rows = []
for tag, mode, flags, env in CFGS:
    for cat in CATS:
        _, f = H.compile_and_evaluate_policy(
            SSA, f"mm_{tag}_{cat}", mode, budget=1.0, fi_trials=TRIALS,
            run_fi=True, fi_target_type=cat, dump_golden=True,
            pass_flags=flags, env_extra=env)
        ks = sum(round(r["sdc_pct"] / 100 * r["trials"]) for r in f)
        n = sum(r["trials"] for r in f)
        lo, hi, half = H.wilson_ci95(ks, n) if n else (0, 0, 0)
        rows.append({"config": tag, "category": cat, "ecc_domain": int(cat in ECC),
                     "sdc_pct": round(pct(ks, n), 2), "n": n,
                     "sdc_ci95_lo": round(lo * 100, 2), "sdc_ci95_hi": round(hi * 100, 2),
                     "sdc_ci95_half": round(half * 100, 2)})

OUT = H.RES_DIR / "main_matrix.csv"
with open(OUT, "w", newline="") as fp:
    w = csv.DictWriter(fp, fieldnames=list(rows[0].keys()))
    w.writeheader(); w.writerows(rows)

print(f"\n=== 主矩阵 SDC%±CI (trials={TRIALS}/类, 子集 10 核; load=ECC 域) ===")
print(f"{'config':13}" + "".join(f"{c:>13}" for c in CATS))
for tag, *_ in CFGS:
    cells = []
    for c in CATS:
        r = next(x for x in rows if x["config"] == tag and x["category"] == c)
        mark = "*" if c in ECC else " "
        cells.append(f"{r['sdc_pct']:>5.1f}±{r['sdc_ci95_half']:<4.1f}{mark}")
    print(f"{tag:13}" + "".join(f"{c:>13}" for c in cells))
print("\n* = ECC 域(不进保护聚合)。CSV ->", OUT)
print("保护聚合口径: 对非 ECC 类别等权。这是后续 P1/P2' 与论文主表的唯一可信口径。")

