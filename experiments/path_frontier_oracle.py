#!/usr/bin/env python3
"""Run a small frontier-level oracle probe using existing static-site FI."""

import argparse
import csv
import json
from pathlib import Path

import benchmark_harness as H

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "results" / "opt"


def policy_flags(profile, selection):
    return [
        "-atft-cmp-tmr",
        "-atft-delta-runtime",
        "-atft-replica-exact",
        "-atft-int-tmr-wire",
        "-atft-int-tmr-i64",
        "-atft-abft-insert",
        "-atft-layer-exclusive",
        "-atft-budget-mode=risk-cost",
        f"-atft-profile={profile}",
        f"-atft-selection={selection}",
    ]


def load_frontiers(path):
    with open(path, newline="", encoding="utf8") as handle:
        return list(csv.DictReader(handle))


def benchmark_name(function):
    return function[len("bench_"):] if function.startswith("bench_") else function


def choose_frontiers(rows, limit, benchmark_names, selected_keys):
    ranked = sorted(
        (
            row for row in rows
            if row.get("profile_complete") == "1"
            and row.get("closure_complete") == "1"
            and benchmark_name(row["function"]) in benchmark_names
        ),
        key=lambda row: (-float(row["benefit"]), int(row["frontier_id"])),
    )
    chosen = []
    seen = set()
    buckets = [
        [row for row in ranked if row["source_site_key"] in selected_keys],
        [row for row in ranked if row["source_site_key"] not in selected_keys],
    ]

    def append_bucket(bucket, quota):
        for row in bucket:
            key = (row["function"], row["sink_kind"])
            if key in seen:
                continue
            seen.add(key)
            chosen.append(row)
            if len(chosen) >= quota:
                break

    selected_quota = max(1, limit // 2)
    append_bucket(buckets[0], selected_quota)
    append_bucket(buckets[1], limit)
    if len(chosen) < limit:
        append_bucket(ranked, limit)
    return chosen[:limit]


def load_selected_keys(path):
    if not path.exists():
        return set()
    with path.open(newline="", encoding="utf8") as handle:
        return {row["site_key"] for row in csv.DictReader(handle) if row.get("selected") == "1"}


def summarize(rows, effective_benchmark):
    effective = [row for row in rows if row["benchmark"] == effective_benchmark]
    trials = sum(int(row["trials"]) for row in effective)
    if trials == 0:
        return {"effective_benchmark": effective_benchmark, "trials": 0,
                "all_trials": sum(int(row["trials"]) for row in rows),
                "sdc": 0.0, "due": 0.0, "det_rec": 0.0, "masked": 0.0}
    return {
        "effective_benchmark": effective_benchmark,
        "trials": trials,
        "all_trials": sum(int(row["trials"]) for row in rows),
        "sdc": sum(float(row["sdc_pct"]) * int(row["trials"]) for row in effective) / trials,
        "due": sum(float(row["due_pct"]) * int(row["trials"]) for row in effective) / trials,
        "det_rec": sum(float(row["det_recovered_pct"]) * int(row["trials"]) for row in effective) / trials,
        "masked": sum(float(row["masked_pct"]) * int(row["trials"]) for row in effective) / trials,
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--frontier-profile", type=Path, required=True)
    parser.add_argument("--site-profile", type=Path, required=True)
    parser.add_argument("--budget", type=float, default=0.6)
    parser.add_argument("--trials", type=int, default=20)
    parser.add_argument("--max-frontiers", type=int, default=8)
    parser.add_argument("--suffix", default="frontier_oracle_b60_t20_20261115")
    args = parser.parse_args()

    OUT.mkdir(parents=True, exist_ok=True)
    ssa_ll = H.build_infrastructure()
    with (ROOT / "build" / "benchmark_meta.csv").open(newline="", encoding="utf8") as handle:
        benchmark_names = {row["name"] for row in csv.DictReader(handle)}
    selection = OUT / f"pgsp_selection_{args.suffix}.csv"
    flags = policy_flags(args.site_profile, selection)

    site_map = []
    H.compile_and_evaluate_policy(
        ssa_ll, f"frontier_map_{args.suffix}", "approx-simd",
        rank="boundary-feedback", budget=args.budget, fi_trials=1,
        dump_golden=False, run_fi=True, fi_target_type="all", arch="x86_64",
        pass_flags=flags, fi_flags=["-fi-origin-only"], fi_site_map_out=site_map,
    )
    fi_ids_by_key = {row.get("site_key", ""): row["site_id"] for row in site_map if row.get("site_key")}
    selected_keys = load_selected_keys(selection)
    frontiers = choose_frontiers(
        load_frontiers(args.frontier_profile), args.max_frontiers,
        benchmark_names, selected_keys
    )

    outcomes = []
    for frontier in frontiers:
        source_key = frontier["source_site_key"]
        fi_site_id = fi_ids_by_key.get(source_key)
        if fi_site_id is None:
            outcome = {**frontier, "fi_site_id": "", "site_map_match": "0",
                       "selector_selected": "1" if source_key in selected_keys else "0",
                       "effective_benchmark": "", "trials": 0, "all_trials": 0,
                       "sdc": 0.0, "due": 0.0, "det_rec": 0.0, "masked": 0.0}
            outcomes.append(outcome)
            continue
        rows = H.compile_and_evaluate_policy(
            ssa_ll, f"frontier_site_{fi_site_id}_{args.suffix}", "approx-simd",
            rank="boundary-feedback", budget=args.budget, fi_trials=args.trials,
            dump_golden=False, run_fi=True, fi_target_type="all", arch="x86_64",
            pass_flags=flags, fi_flags=["-fi-origin-only"],
            env_extra={"ATFT_FI_STATIC_SITE": fi_site_id},
        )[1]
        effective_benchmark = frontier["function"]
        if effective_benchmark.startswith("bench_"):
            effective_benchmark = effective_benchmark[len("bench_"):]
        outcomes.append({
            **frontier,
            "fi_site_id": fi_site_id,
            "site_map_match": "1",
            "selector_selected": "1" if source_key in selected_keys else "0",
            **summarize(rows, effective_benchmark),
        })

    csv_path = OUT / f"pgsp_frontier_oracle_{args.suffix}.csv"
    fields = list(outcomes[0].keys()) if outcomes else ["frontier_id"]
    with csv_path.open("w", newline="", encoding="utf8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        writer.writerows(outcomes)

    metadata = {
        "frontier_profile": str(args.frontier_profile),
        "site_profile": str(args.site_profile),
        "budget": args.budget,
        "trials": args.trials,
        "targeting": "source_site_first_hit_via_ATFT_FI_STATIC_SITE",
        "site_map_count": len(site_map),
        "frontier_candidates": len(frontiers),
        "site_map_matches": sum(row.get("site_map_match") == "1" for row in outcomes),
        "warning": "Small frontier-level oracle probe; not a replacement for the standard aggregate campaign.",
        "csv": str(csv_path),
    }
    json_path = OUT / f"pgsp_frontier_oracle_{args.suffix}.json"
    json_path.write_text(json.dumps({**metadata, "frontiers": outcomes}, indent=2, ensure_ascii=False) + "\n", encoding="utf8")
    print(json.dumps(metadata, ensure_ascii=False))


if __name__ == "__main__":
    main()
