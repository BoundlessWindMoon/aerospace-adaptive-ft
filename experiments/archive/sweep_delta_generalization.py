#!/usr/bin/env python3
"""扫描阈值安全系数 alpha 在独立 profile 上的覆盖曲线。

用法：
    python3 experiments/sweep_delta_generalization.py \
        --calibration results/opt/delta_profile.csv \
        --validation results/opt/delta_profile_scaled.csv \
        --alphas 1,2,4,8,16,32,64 \
        --kappa 4 \
        --out results/opt/delta_alpha_sweep_scaled.csv

输出同时区分：
  - valid coverage：有实际 validation observation 且被阈值覆盖的负载；
  - failed benchmarks：验证包络超过阈值的负载；
  - required alpha：每个负载单独覆盖所需的最小比例系数。
"""

import argparse
import csv
from pathlib import Path

from analyze_delta_generalization import analyze, load_profile


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--calibration", required=True, type=Path)
    ap.add_argument("--validation", required=True, type=Path)
    ap.add_argument("--alphas", required=True,
                    help="comma-separated alpha values")
    ap.add_argument("--kappa", type=float, default=4.0)
    ap.add_argument("--min-observations", type=int, default=1)
    ap.add_argument("--out", required=True, type=Path)
    args = ap.parse_args()

    alphas = [float(x) for x in args.alphas.split(",") if x.strip()]
    if not alphas or any(x < 0.0 for x in alphas):
        ap.error("alphas must contain non-negative numbers")

    calibration = load_profile(args.calibration)
    validation = load_profile(args.validation)
    rows = []
    for alpha in alphas:
        detail = analyze(calibration, validation, alpha, args.kappa)
        usable = [
            r for r in detail
            if r["validation_observations"] >= args.min_observations
        ]
        failed = [r["benchmark"] for r in usable if not r["covered"]]
        rows.append({
            "alpha": alpha,
            "kappa": args.kappa,
            "usable_benchmarks": len(usable),
            "covered_benchmarks": len(usable) - len(failed),
            "coverage": (
                (len(usable) - len(failed)) / len(usable)
                if usable else 0.0
            ),
            "failed_benchmarks": ",".join(failed),
        })

    args.out.parent.mkdir(parents=True, exist_ok=True)
    with open(args.out, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    print(f"wrote {args.out}")
    for row in rows:
        print(f"alpha={row['alpha']:g} coverage="
              f"{row['covered_benchmarks']}/{row['usable_benchmarks']} "
              f"({row['coverage']:.3f}) failed={row['failed_benchmarks'] or '-'}")


if __name__ == "__main__":
    main()
