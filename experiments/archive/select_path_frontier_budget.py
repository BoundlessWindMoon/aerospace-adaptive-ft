#!/usr/bin/env python3
"""Select complete frontier groups under a de-duplicated member-cost budget."""

import argparse
import csv
import json
from pathlib import Path


def load_rows(path):
    with open(path, newline="", encoding="utf8") as handle:
        return list(csv.DictReader(handle))


def member_ids(row):
    return sorted({int(value) for value in row.get("member_site_ids", "").split(";") if value})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--frontier-profile", required=True, type=Path)
    parser.add_argument("--budget", required=True, type=float)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--metadata-out", required=True, type=Path)
    args = parser.parse_args()
    if not 0.0 <= args.budget <= 1.0:
        raise ValueError("budget must be in [0, 1]")

    rows = load_rows(args.frontier_profile)
    costs = {}
    for row in rows:
        if row.get("profile_complete") != "1":
            continue
        for site_id in member_ids(row):
            costs.setdefault(site_id, 0.0)
        for site_id in member_ids(row):
            # The frontier profile already de-duplicates member cost. Use the
            # average per member only when a group has multiple members.
            costs[site_id] = max(costs[site_id], float(row["member_dynamic_cost"]) / max(1, len(member_ids(row))))
    total_cost = sum(costs.values())
    capacity = args.budget * total_cost

    eligible = [
        row for row in rows
        if row.get("profile_complete") == "1" and row.get("closure_complete") == "1"
    ]
    eligible.sort(key=lambda row: (-float(row["benefit"]), int(row["frontier_id"])))
    selected_frontiers = set()
    selected_sites = set()
    incremental_by_frontier = {}
    selected_cost = 0.0
    for row in eligible:
        ids = member_ids(row)
        incremental = sum(costs[site_id] for site_id in ids if site_id not in selected_sites)
        incremental_by_frontier[row["frontier_id"]] = incremental
        if incremental <= 0.0:
            selected_frontiers.add(row["frontier_id"])
            continue
        if selected_cost + incremental <= capacity + 1e-12:
            selected_frontiers.add(row["frontier_id"])
            selected_sites.update(ids)
            selected_cost += incremental

    output_fields = [
        "frontier_id", "ir_fingerprint", "function", "sink_kind", "member_site_ids",
        "member_dynamic_cost", "benefit", "selected", "selection_budget",
        "selection_incremental_cost",
    ]
    with args.out.open("w", newline="", encoding="utf8") as handle:
        writer = csv.DictWriter(handle, fieldnames=output_fields)
        writer.writeheader()
        for row in rows:
            ids = member_ids(row)
            incremental = sum(costs.get(site_id, 0.0) for site_id in ids if site_id not in selected_sites)
            writer.writerow({
                "frontier_id": row["frontier_id"],
                "ir_fingerprint": row["ir_fingerprint"],
                "function": row["function"],
                "sink_kind": row["sink_kind"],
                "member_site_ids": row.get("member_site_ids", ""),
                "member_dynamic_cost": row.get("member_dynamic_cost", ""),
                "benefit": row.get("benefit", ""),
                "selected": "1" if row["frontier_id"] in selected_frontiers else "0",
                "selection_budget": f"{args.budget:.9f}",
                "selection_incremental_cost": f"{incremental_by_frontier.get(row['frontier_id'], 0.0):.9f}",
            })

    metadata = {
        "frontier_profile": str(args.frontier_profile),
        "budget": args.budget,
        "frontier_count": len(rows),
        "eligible_frontiers": len(eligible),
        "selected_frontiers": len(selected_frontiers),
        "selected_sites": len(selected_sites),
        "total_unique_site_cost": total_cost,
        "selected_site_cost": selected_cost,
        "capacity": capacity,
        "selection_kind": "benefit_sorted_frontier_groups_with_deduplicated_member_cost",
    }
    args.metadata_out.write_text(json.dumps(metadata, indent=2, ensure_ascii=False) + "\n", encoding="utf8")
    print(json.dumps(metadata, ensure_ascii=False))


if __name__ == "__main__":
    main()
