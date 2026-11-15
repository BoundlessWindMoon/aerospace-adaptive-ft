#!/usr/bin/env python3
"""分析阈值包络从 calibration profile 迁移到 validation profile 的效果。

Profile 文件使用 run_opt_ablation.py 生成的三列 CSV：
    benchmark,env_max_rel_dev,observations

用法：
    python3 experiments/analyze_delta_generalization.py \
        --calibration results/opt/delta_profile.csv \
        --validation results/opt/delta_profile_validation.csv \
        --alpha 4 --kappa 4 \
        --out results/opt/delta_generalization.csv

这个工具只分析 profile，不改变默认阈值表，也不写入冻结的 results/。
validation 文件可以来自不同输入、不同规模或不同初始化方式的独立运行。
"""

import argparse
import csv
import math
from pathlib import Path


U_FP32 = 2.0 ** -24


def load_profile(path):
    rows = {}
    with open(path, newline="") as f:
        for row in csv.DictReader(f):
            name = row["benchmark"].strip()
            if not name:
                continue
            value = float(row["env_max_rel_dev"])
            if not math.isfinite(value) or value < 0.0:
                raise ValueError(f"{path}: invalid envelope for {name}: {value}")
            rows[name] = {
                "env_max_rel_dev": value,
                "observations": int(row.get("observations", 0) or 0),
            }
    if not rows:
        raise ValueError(f"{path}: profile is empty")
    return rows


def analyze(calibration, validation, alpha, kappa):
    missing = sorted(set(calibration) - set(validation))
    extra = sorted(set(validation) - set(calibration))
    if missing or extra:
        raise ValueError(
            "calibration/validation benchmark sets differ: "
            f"missing_in_validation={missing}, extra_in_validation={extra}"
        )

    floor = kappa * U_FP32
    out = []
    for name in sorted(calibration):
        cal = calibration[name]
        val = validation[name]
        threshold = max(alpha * cal["env_max_rel_dev"], floor)
        observed = val["env_max_rel_dev"]
        usable = int(val["observations"] > 0)
        margin = None if observed == 0.0 else threshold / observed
        required_alpha = (
            None
            if cal["env_max_rel_dev"] == 0.0
            else observed / cal["env_max_rel_dev"]
        )
        out.append({
            "benchmark": name,
            "calibration_env_max_rel_dev": cal["env_max_rel_dev"],
            "calibration_observations": cal["observations"],
            "validation_env_max_rel_dev": observed,
            "validation_observations": val["observations"],
            "alpha": alpha,
            "kappa": kappa,
            "floor": floor,
            "threshold": threshold,
            "validation_margin": margin,
            "required_alpha": required_alpha,
            "validation_usable": usable,
            "covered": int(observed <= threshold),
        })
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--calibration", required=True, type=Path)
    ap.add_argument("--validation", required=True, type=Path)
    ap.add_argument("--alpha", type=float, default=4.0)
    ap.add_argument("--kappa", type=float, default=4.0)
    ap.add_argument("--min-observations", type=int, default=1,
                    help="低于该观测数的 profile 不计入有效覆盖率")
    ap.add_argument("--max-alpha", type=float, default=None,
                    help="可接受的最大 required alpha；传入后输出适用性 gate")
    ap.add_argument("--out", required=True, type=Path)
    args = ap.parse_args()
    if args.alpha < 0.0 or args.kappa < 0.0:
        ap.error("alpha and kappa must be non-negative")

    rows = analyze(
        load_profile(args.calibration),
        load_profile(args.validation),
        args.alpha,
        args.kappa,
    )
    for row in rows:
        row["validation_usable"] = int(
            row["validation_observations"] >= args.min_observations
        )
        if not row["validation_usable"]:
            row["applicability"] = "no_validation_observation"
        elif row["covered"]:
            row["applicability"] = "covered"
        elif args.max_alpha is not None and (
                row["required_alpha"] is not None
                and row["required_alpha"] <= args.max_alpha):
            row["applicability"] = "covered_with_larger_alpha"
        else:
            row["applicability"] = "outside_calibration_domain"
    if args.max_alpha is not None and args.max_alpha < 0.0:
        ap.error("max-alpha must be non-negative")
    args.out.parent.mkdir(parents=True, exist_ok=True)
    fields = list(rows[0])
    with open(args.out, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)

    usable_rows = [r for r in rows if r["validation_usable"]]
    covered = sum(r["covered"] for r in usable_rows)
    print(f"wrote {args.out}")
    print(f"benchmarks={len(rows)} usable={len(usable_rows)} "
          f"covered={covered}/{len(usable_rows)} "
          f"coverage={(covered / len(usable_rows) if usable_rows else 0.0):.3f}")
    if args.max_alpha is not None:
        counts = {}
        for row in rows:
            key = row["applicability"]
            counts[key] = counts.get(key, 0) + 1
        print("applicability=" + ",".join(
            f"{key}:{counts[key]}" for key in sorted(counts)
        ))


if __name__ == "__main__":
    main()
