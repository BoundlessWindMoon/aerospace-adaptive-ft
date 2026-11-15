#!/usr/bin/env python3
"""Aggregate an independent FI campaign into conservative category priors."""

import argparse
import csv
import json
from collections import defaultdict
from pathlib import Path

CATEGORY_CLASS = {
    "fp64": "fp64",
    "int_arith": "integer",
    "cast": "integer",
    "load": "integer",
    "addr": "integer",
    "cmp_branch": "control",
}


def aggregate(rows, policy):
    totals = defaultdict(lambda: {"trials": 0, "sdc": 0.0, "due": 0.0, "det": 0.0})
    for row in rows:
        if row["policy"] != policy:
            continue
        category = row["fi_target_type"]
        trials = int(row["trials"])
        bucket = totals[category]
        bucket["trials"] += trials
        bucket["sdc"] += float(row["sdc_pct"]) * trials / 100.0
        bucket["due"] += float(row["due_pct"]) * trials / 100.0
        bucket["det"] += float(row["det_recovered_pct"]) * trials / 100.0

    result = {}
    for category, bucket in sorted(totals.items()):
        trials = max(bucket["trials"], 1)
        result[category] = {
            "class": CATEGORY_CLASS.get(category, "unknown"),
            "trials": bucket["trials"],
            "sdc_rate": bucket["sdc"] / trials,
            "due_rate": bucket["due"] / trials,
            "det_recovered_rate": bucket["det"] / trials,
        }
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--fi", type=Path, required=True)
    parser.add_argument("--native-policy", default="native")
    parser.add_argument("--protected-policy", default="opt_layered")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    with args.fi.open(newline="") as handle:
        rows = list(csv.DictReader(handle))

    native = aggregate(rows, args.native_policy)
    protected = aggregate(rows, args.protected_policy)
    class_values = defaultdict(list)
    evidence = {}
    for category, nrow in native.items():
        prow = protected.get(category)
        if prow is None:
            continue
        native_fail = nrow["sdc_rate"] + nrow["due_rate"]
        protected_fail = prow["sdc_rate"] + prow["due_rate"]
        reduction = max(native_fail - protected_fail, 0.0)
        class_name = nrow["class"]
        class_values[class_name].append(reduction)
        evidence[category] = {
            "class": class_name,
            "native": nrow,
            "protected": prow,
            "native_fail_rate": native_fail,
            "protected_fail_rate": protected_fail,
            "estimated_risk_reduction": reduction,
        }

    class_priors = {}
    for class_name, values in class_values.items():
        # Keep priors numerically stable and centered at one. The smallest
        # positive class reduction is the reference; no benchmark is used.
        class_priors[class_name] = max(sum(values) / len(values), 1e-6)
    positive = [value for value in class_priors.values() if value > 0]
    reference = min(positive) if positive else 1.0
    class_priors = {
        name: round(max(value / reference, 1e-3), 6)
        for name, value in class_priors.items()
    }

    output = {
        "fi_input": str(args.fi),
        "native_policy": args.native_policy,
        "protected_policy": args.protected_policy,
        "calibration_kind": "category_level_weighted_fi",
        "per_site_outcomes_available": False,
        "class_priors": class_priors,
        "evidence": evidence,
        "warning": "Priors are category-level calibration weights, not per-site probabilities.",
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(output, indent=2, ensure_ascii=False) + "\n")
    print(json.dumps({"output": str(args.output), "class_priors": class_priors}, ensure_ascii=False))


if __name__ == "__main__":
    main()
