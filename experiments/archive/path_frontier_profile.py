#!/usr/bin/env python3
"""Build an auditable cost/risk table for path frontier groups."""

import argparse
import csv
import json
from pathlib import Path

DEFAULT_SINK_WEIGHTS = {
    "branch": 4.0,
    "address": 5.0,
    "store": 4.0,
    "call": 3.0,
    "return": 3.0,
    "feedback": 2.0,
}


def parse_sink_weights(values):
    weights = dict(DEFAULT_SINK_WEIGHTS)
    for value in values:
        kind, sep, raw = value.partition("=")
        if not sep or kind not in weights:
            raise ValueError(f"invalid --sink-weight {value!r}")
        weights[kind] = float(raw)
    return weights


def load_profile(path, cost_column):
    profile = {}
    with open(path, newline="", encoding="utf8") as handle:
        for row in csv.DictReader(handle):
            site_id = int(row["site_id"])
            if site_id in profile:
                raise ValueError(f"duplicate profile site_id={site_id}")
            raw_cost = row.get(cost_column, "")
            if raw_cost in (None, ""):
                raise ValueError(f"missing {cost_column} for site_id={site_id}")
            cost = float(raw_cost)
            if cost <= 0.0:
                raise ValueError(f"non-positive {cost_column} for site_id={site_id}")
            profile[site_id] = {
                "cost": cost,
                "risk_prior": float(row.get("risk_prior", "1.0") or 1.0),
                "class": row.get("class", "unknown"),
                "match_status": row.get("match_status", "ok"),
            }
    return profile


def build_rows(frontier_path, profile_path, cost_column, sink_weights):
    profile = load_profile(profile_path, cost_column)
    rows = []
    missing_sites = 0
    incomplete_frontiers = 0
    with open(frontier_path, newline="", encoding="utf8") as handle:
        for frontier in csv.DictReader(handle):
            member_ids = [
                int(value)
                for value in frontier.get("member_site_ids", "").split(";")
                if value
            ]
            unique_ids = sorted(set(member_ids))
            member_cost = 0.0
            member_risk = 0.0
            missing = []
            classes = set()
            for site_id in unique_ids:
                entry = profile.get(site_id)
                if entry is None:
                    missing.append(site_id)
                    continue
                member_cost += entry["cost"]
                member_risk += entry["risk_prior"]
                classes.add(entry["class"])
            missing_sites += len(missing)
            profile_complete = not missing
            if not profile_complete:
                incomplete_frontiers += 1
            sink_kind = frontier["sink_kind"]
            sink_weight = sink_weights.get(sink_kind, 1.0)
            risk_score = sink_weight * member_risk
            if member_cost <= 0.0:
                benefit = 0.0
            else:
                benefit = risk_score / member_cost
            row = dict(frontier)
            row.update({
                "member_dynamic_cost": f"{member_cost:.9f}",
                "member_risk_prior_sum": f"{member_risk:.9f}",
                "sink_weight": f"{sink_weight:.9f}",
                "risk_score": f"{risk_score:.9f}",
                "benefit": f"{benefit:.9f}",
                "profile_complete": "1" if profile_complete else "0",
                "missing_profile_sites": ";".join(str(x) for x in missing),
                "member_classes": ";".join(sorted(classes)),
                "cost_column": cost_column,
                "selected": "0",
            })
            rows.append(row)
    rows.sort(key=lambda row: (-float(row["benefit"]), int(row["frontier_id"])))
    metadata = {
        "frontier_manifest": str(frontier_path),
        "profile": str(profile_path),
        "profile_kind": "measured_dynamic_count" if cost_column == "dynamic_count" else "static_loop_frequency_proxy",
        "cost_column": cost_column,
        "sink_weights": sink_weights,
        "frontier_count": len(rows),
        "profile_site_count": len(profile),
        "missing_profile_sites": missing_sites,
        "incomplete_frontiers": incomplete_frontiers,
        "selection_status": "candidate ranking only; no frontier selected",
    }
    return rows, metadata


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--frontier-manifest", required=True)
    parser.add_argument("--profile", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--metadata-out", required=True)
    parser.add_argument("--cost-column", choices=["dynamic_count", "dynamic_count_proxy"], default="dynamic_count_proxy")
    parser.add_argument("--sink-weight", action="append", default=[])
    args = parser.parse_args()

    sink_weights = parse_sink_weights(args.sink_weight)
    rows, metadata = build_rows(
        Path(args.frontier_manifest), Path(args.profile), args.cost_column, sink_weights
    )
    fieldnames = list(rows[0].keys()) if rows else []
    with open(args.out, "w", newline="", encoding="utf8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)
    Path(args.metadata_out).write_text(json.dumps(metadata, indent=2, ensure_ascii=False) + "\n", encoding="utf8")
    print(json.dumps(metadata, ensure_ascii=False))


if __name__ == "__main__":
    main()
