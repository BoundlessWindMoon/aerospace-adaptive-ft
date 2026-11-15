#!/usr/bin/env python3
"""Build an auditable per-site cost profile for budgeted protection.

The current harness exposes a module-level dynamic instruction count, not a
stable compiler-site execution count. This script therefore emits an explicit
static loop-frequency proxy. It must not be interpreted as a measured runtime
profile.
"""

import argparse
import csv
import json
import math
from pathlib import Path


VARIANT_SCALE = {
    "default": 1.0,
    "scaled": 1.35,
    "scaled_safe": 1.10,
}


def parse_bool(value):
    return str(value).strip().lower() in {"1", "true", "yes"}


def proxy_cost(row, variant):
    loop_depth = max(0, int(row["loop_depth"]))
    fanout = max(0, int(row["fanout"]))
    cross_bb = max(0, int(row["cross_bb_uses"]))
    boundary = max(0.0, float(row["boundary_sensitivity"]))
    class_name = row["class"]

    # The exponential loop term is a generic static hotness proxy. It is kept
    # visible in the output so it can be replaced by measured counts later.
    loop_weight = 10.0 ** min(loop_depth, 6)
    dataflow_weight = 1.0 + 0.10 * fanout + 0.25 * cross_bb
    boundary_weight = 1.0 + 0.05 * boundary
    class_weight = {
        "fp64": 1.0,
        "integer": 1.0,
        "control": 1.0,
    }.get(class_name, 1.0)
    return max(1.0, loop_weight * dataflow_weight * boundary_weight * class_weight
               * VARIANT_SCALE[variant])


def make_profile(manifest_path, output_path, metadata_path, variant, priors_path=None,
                 source_mode="unknown", source_flags=None):
    with manifest_path.open(newline="") as handle:
        rows = list(csv.DictReader(handle))

    priors = {"fp64": 1.0, "integer": 1.0, "control": 1.0}
    if priors_path is not None:
        priors.update(json.loads(priors_path.read_text()).get("class_priors", {}))

    if not rows:
        raise ValueError("site manifest is empty")

    ir_fingerprints = {row["ir_fingerprint"] for row in rows}
    if len(ir_fingerprints) != 1:
        raise ValueError("site manifest contains multiple IR fingerprints")

    out_rows = []
    rejected = []
    for row in rows:
        try:
            site_id = int(row["site_id"])
            cost = proxy_cost(row, variant)
            if not math.isfinite(cost) or cost <= 0:
                raise ValueError("non-positive or non-finite proxy cost")
        except (KeyError, TypeError, ValueError) as exc:
            rejected.append({"site_id": row.get("site_id", ""), "reason": str(exc)})
            continue

        out_rows.append({
            "site_id": site_id,
            "site_key": row["site_key"],
            "ir_fingerprint": row["ir_fingerprint"],
            "class": row["class"],
            "dynamic_count": "",
            "dynamic_count_proxy": f"{cost:.9f}",
            "loop_depth": row["loop_depth"],
            "fanout": row["fanout"],
            "cross_bb_uses": row["cross_bb_uses"],
            "boundary_sensitivity": row["boundary_sensitivity"],
            "risk_prior": f"{float(priors.get(row['class'], 1.0)):.9f}",
            "profile_kind": "static_loop_frequency_proxy",
            "profile_variant": variant,
            "feeds_branch": row["feeds_branch"],
            "feeds_store": row["feeds_store"],
            "feeds_store_addr": row["feeds_store_addr"],
            "feeds_call": row["feeds_call"],
            "feeds_gep": row["feeds_gep"],
            "recurrent_feedback": row["recurrent_feedback"],
            "match_status": row["match_status"],
        })

    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("w", newline="") as handle:
        fieldnames = list(out_rows[0].keys())
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(out_rows)

    metadata = {
        "manifest": str(manifest_path),
        "source_mode": source_mode,
        "source_flags": source_flags or [],
        "profile": str(output_path),
        "profile_kind": "static_loop_frequency_proxy",
        "measured_dynamic_count_available": False,
        "dynamic_count_column": "",
        "proxy_column": "dynamic_count_proxy",
        "variant": variant,
        "variant_scale": VARIANT_SCALE[variant],
        "class_priors": priors,
        "ir_fingerprints": sorted(ir_fingerprints),
        "input_sites": len(rows),
        "output_sites": len(out_rows),
        "rejected_sites": rejected,
        "missing_site_count": len(rejected),
        "formula": "10**min(loop_depth,6) * (1+0.10*fanout+0.25*cross_bb_uses) * (1+0.05*boundary_sensitivity) * variant_scale",
        "warning": "This is a reproducible static cost proxy, not a measured per-site execution profile.",
    }
    metadata_path.parent.mkdir(parents=True, exist_ok=True)
    metadata_path.write_text(json.dumps(metadata, indent=2, ensure_ascii=False) + "\n")
    return metadata


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--metadata", type=Path, required=True)
    parser.add_argument("--variant", choices=sorted(VARIANT_SCALE), default="default")
    parser.add_argument("--priors", type=Path, default=None)
    parser.add_argument("--source-mode", default="unknown")
    parser.add_argument("--source-flag", action="append", default=[])
    args = parser.parse_args()
    metadata = make_profile(args.manifest, args.output, args.metadata, args.variant,
                            args.priors, args.source_mode, args.source_flag)
    print(json.dumps({
        "profile": str(args.output),
        "sites": metadata["output_sites"],
        "missing": metadata["missing_site_count"],
        "kind": metadata["profile_kind"],
        "variant": metadata["variant"],
    }, ensure_ascii=False))


if __name__ == "__main__":
    main()
