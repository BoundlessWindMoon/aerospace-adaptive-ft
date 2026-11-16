#!/usr/bin/env python3
"""自适应下包络：每核选"达到 fp64 SDC=0 的最便宜策略"，量化按核自选能压到多低。

候选含指令级策略（从 solid 矩阵取 cyc 开销 + fp64 SDC）与粗粒度整内核冗余
（全类别、整内核重算 → SDC≈0；DMR+重执行串行≈2.0×、3 核并行≈1.0×；常量来自
coarse_rmt 实测）。输出每核胜出策略 + 自适应包络 vs 最优单策略。
用法: python3 experiments/lower_envelope.py <suffix>
"""
import csv
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OPT = ROOT / "results" / "opt"
# 粗粒度整内核冗余：全类别、SDC≈0。开销常量取自 coarse_rmt 实测（gemm 2.0-3.1×/~1×）。
COARSE_SERIAL = 2.0     # DMR + 仅在失配时重执行第 3 份 → 无故障≈2×
COARSE_PARALLEL = 1.0   # N 份放 N 核并行 → 墙钟≈1×
SDC_ZERO = 0.05         # fp64 SDC 视为"清零"的阈值（%）


def load_csv(p):
    with open(p) as f:
        return list(csv.DictReader(f))


def main():
    suffix = sys.argv[1] if len(sys.argv) > 1 else "a4_t200_matrix_solid"
    pmu = load_csv(OPT / f"pmu_{suffix}.csv")
    fi = load_csv(OPT / f"fi_outcomes_{suffix}.csv")
    natc = {r["benchmark"]: float(r["hw_cycles"])
            for r in pmu if r["policy"] == "native"}
    cyc = {(r["policy"], r["benchmark"]): float(r["hw_cycles"]) for r in pmu}
    fp64 = {(r["policy"], r["benchmark"]): float(r["sdc_pct"])
            for r in fi if r["fi_target_type"] == "fp64"}
    instr = ["simd_tmr", "exact-tmr", "opt_simd_f64", "opt_layered"]
    # 每核实测的粗粒度 DMR+重执行开销（coarse_rmt/eval_suite ATFT_COARSE=2）；缺则回退常量
    cov = {}
    cov_path = OPT / "coarse_overhead_subset.csv"
    if cov_path.exists():
        cov = {r["benchmark"]: float(r["coarse_dmr_rerun"]) for r in load_csv(cov_path)}

    kernels = sorted(k for k in natc if ("native", k) in fp64)
    rows = []
    for b in kernels:
        cands = {}
        for p in instr:
            if (p, b) in cyc and fp64.get((p, b), 99) <= SDC_ZERO:
                cands[p] = cyc[(p, b)] / natc[b]
        cands["coarse_serial"] = cov.get(b, COARSE_SERIAL)   # 全核适用、SDC≈0
        best_p = min(cands, key=cands.get)
        rows.append({
            "benchmark": b,
            "best_policy": best_p,
            "best_overhead_serial": round(cands[best_p], 2),
            "best_overhead_parallel": round(COARSE_PARALLEL if best_p == "coarse_serial"
                                            else cands[best_p], 2),
            "simd_tmr": round(cands.get("simd_tmr", float("nan")), 2),
            "coarse": COARSE_SERIAL,
        })
    out = OPT / f"lower_envelope_{suffix}.csv"
    with open(out, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader()
        w.writerows(rows)

    n = len(rows)
    env_s = sum(r["best_overhead_serial"] for r in rows) / n
    env_p = sum(r["best_overhead_parallel"] for r in rows) / n
    flat = {}
    for p in instr + ["abft_auto"]:
        vals = [cyc[(p, b)] / natc[b] for b in kernels if (p, b) in cyc]
        if vals:
            flat[p] = sum(vals) / len(vals)
    print(f"[envelope] {out}  ({n} 个 fp64 内核)")
    print(f"{'kernel':14s}{'best':16s}{'serial×':>9}{'parallel×':>10}{'simd_tmr×':>10}")
    for r in rows:
        print(f"{r['benchmark']:14s}{r['best_policy']:16s}"
              f"{r['best_overhead_serial']:9.2f}{r['best_overhead_parallel']:10.2f}"
              f"{r['simd_tmr']:10.2f}")
    print(f"\n自适应下包络：串行 {env_s:.2f}×  /  并行 {env_p:.2f}×")
    print("最优单策略（全核同一策略，均值）：")
    for p, v in sorted(flat.items(), key=lambda kv: kv[1]):
        print(f"  {p:16s}{v:6.2f}×")


if __name__ == "__main__":
    sys.exit(main())
