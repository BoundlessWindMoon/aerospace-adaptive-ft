#!/usr/bin/env python3
"""每内核属性+结果汇总表（独立脚本，避开 Edit 保存桥问题）。

合并 向量化(meanW/packed) + 可归约性(abft 是否降 fp64 SDC) + 各策略 cyc 开销
+ fp64 覆盖，输出 results/opt/kernel_properties_<suffix>.csv。
用法: python3 experiments/build_property_table.py <suffix>
"""
import csv
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OPT = ROOT / "results" / "opt"


def load_csv(p):
    with open(p) as f:
        return list(csv.DictReader(f))


def main():
    suffix = sys.argv[1] if len(sys.argv) > 1 else "a4_t200_matrix_solid"
    pmu = load_csv(OPT / f"pmu_{suffix}.csv")
    fi = load_csv(OPT / f"fi_outcomes_{suffix}.csv")
    vec = {r["benchmark"]: r for r in load_csv(OPT / "kernel_vectorization.csv")}
    natc = {r["benchmark"]: float(r["hw_cycles"])
            for r in pmu if r["policy"] == "native"}
    cyc = {(r["policy"], r["benchmark"]): float(r["hw_cycles"]) for r in pmu}
    fp64 = {(r["policy"], r["benchmark"]): float(r["sdc_pct"])
            for r in fi if r["fi_target_type"] == "fp64"}

    def ratio(pol, b):
        c = cyc.get((pol, b))
        return round(c / natc[b], 2) if c and natc.get(b) else ""

    rows = []
    for b in sorted(natc):
        if b not in vec:
            continue
        ns = fp64.get(("native", b))
        asdc = fp64.get(("abft_auto", b))
        reducible = int(ns is not None and asdc is not None and asdc < ns - 0.5)
        rows.append({
            "benchmark": b,
            "mean_vec_width": vec[b]["mean_vec_width"],
            "packed_frac": vec[b]["packed_frac"],
            "reducible": reducible,
            "cyc_x_exact_tmr": ratio("exact-tmr", b),
            "cyc_x_simd_tmr": ratio("simd_tmr", b),
            "cyc_x_abft": ratio("abft_auto", b),
            "cyc_x_layered": ratio("opt_layered", b),
            "fp64_sdc_native": ns if ns is not None else "",
            "fp64_sdc_abft": asdc if asdc is not None else "",
            "fp64_sdc_layered": fp64.get(("opt_layered", b), ""),
        })
    out = OPT / f"kernel_properties_{suffix}.csv"
    with open(out, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader()
        w.writerows(rows)
    print(f"[A2] -> {out}  ({len(rows)} kernels)")
    print(f"{'kernel':18s}{'meanW':>6}{'red':>4}{'exact':>7}{'simd':>6}"
          f"{'abft':>6}{'layer':>6}{'SDCnat':>8}{'SDClay':>8}")
    for r in rows:
        print(f"{r['benchmark']:18s}{float(r['mean_vec_width']):6.2f}{r['reducible']:4d}"
              f"{str(r['cyc_x_exact_tmr']):>7}{str(r['cyc_x_simd_tmr']):>6}"
              f"{str(r['cyc_x_abft']):>6}{str(r['cyc_x_layered']):>6}"
              f"{str(r['fp64_sdc_native']):>8}{str(r['fp64_sdc_layered']):>8}")


if __name__ == "__main__":
    sys.exit(main())
