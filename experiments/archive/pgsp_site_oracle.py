#!/usr/bin/env python3
"""Measure a small, auditable set of protected compiler sites.

This is an oracle probe, not part of the standard aggregate campaign. Static
site targeting is enabled only when ATFT_FI_STATIC_SITE is set in the child
process environment; ordinary FI runs keep their dynamic-step semantics.
"""

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


def load_selected_keys(path):
    if not path.exists():
        return set()
    with path.open(newline="") as handle:
        return {
            row["site_key"]
            for row in csv.DictReader(handle)
            if row.get("selected") == "1"
        }


def choose_sites(rows, limit):
    ranked = sorted(
        rows,
        key=lambda row: (
            -int(row["feeds_phi"]),
            -int(row["feeds_store_addr"]),
            -int(row["feeds_gep"]),
            -int(row["feeds_branch"]),
            -int(row["feeds_store"]),
            row["function"],
            int(row["site_id"]),
        ),
    )
    chosen = []
    seen = set()
    for row in ranked:
        key = (row["function"], row["category"])
        if key in seen:
            continue
        seen.add(key)
        chosen.append(row)
        if len(chosen) >= limit:
            break
    return chosen


def summarize(rows, effective_benchmark):
    effective = [row for row in rows if row["benchmark"] == effective_benchmark]
    total_trials = sum(int(row["trials"]) for row in effective)
    if total_trials == 0:
        return {
            "effective_benchmark": effective_benchmark,
            "trials": 0,
            "all_trials": sum(int(row["trials"]) for row in rows),
            "sdc": 0.0,
            "due": 0.0,
            "det_rec": 0.0,
        }
    return {
        "effective_benchmark": effective_benchmark,
        "trials": total_trials,
        "all_trials": sum(int(row["trials"]) for row in rows),
        "sdc": sum(float(row["sdc_pct"]) * int(row["trials"]) for row in effective) / total_trials,
        "due": sum(float(row["due_pct"]) * int(row["trials"]) for row in effective) / total_trials,
        "det_rec": sum(float(row["det_recovered_pct"]) * int(row["trials"]) for row in effective) / total_trials,
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--profile", type=Path, required=True)
    parser.add_argument("--budget", type=float, default=0.6)
    parser.add_argument("--trials", type=int, default=20)
    parser.add_argument("--max-sites", type=int, default=8)
    parser.add_argument("--suffix", default="boundary_feedback_20261115")
    args = parser.parse_args()

    ssa_ll = H.build_infrastructure()
    selection = OUT / f"pgsp_selection_oracle_{args.suffix}.csv"
    base_flags = policy_flags(args.profile, selection)
    site_map = []
    H.compile_and_evaluate_policy(
        ssa_ll,
        f"oracle_map_{args.suffix}",
        "approx-simd",
        rank="boundary-feedback",
        budget=args.budget,
        delta=5e-5,
        fi_trials=1,
        dump_golden=False,
        run_fi=True,
        fi_target_type="all",
        arch="x86_64",
        pass_flags=base_flags,
        fi_flags=["-fi-origin-only"],
        fi_site_map_out=site_map,
    )

    chosen = choose_sites(site_map, args.max_sites)
    selected_keys = load_selected_keys(selection)
    outcomes = []
    for site in chosen:
        site_id = site["site_id"]
        rows = H.compile_and_evaluate_policy(
            ssa_ll,
            f"oracle_site_{site_id}_{args.suffix}",
            "approx-simd",
            rank="boundary-feedback",
            budget=args.budget,
            delta=5e-5,
            fi_trials=args.trials,
            dump_golden=False,
            run_fi=True,
            fi_target_type="all",
            arch="x86_64",
            pass_flags=base_flags,
            fi_flags=["-fi-origin-only"],
            env_extra={"ATFT_FI_STATIC_SITE": site_id},
        )[1]
        effective_benchmark = site["function"]
        if effective_benchmark.startswith("bench_"):
            effective_benchmark = effective_benchmark[len("bench_"):]
        outcomes.append({
            **site,
            "selector_selected": site.get("site_key", "") in selected_keys,
            **summarize(rows, effective_benchmark),
        })

    OUT.mkdir(parents=True, exist_ok=True)
    csv_path = OUT / f"pgsp_site_oracle_{args.suffix}.csv"
    with csv_path.open("w", newline="") as handle:
        fields = list(outcomes[0].keys()) if outcomes else ["site_id"]
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        writer.writerows(outcomes)

    json_path = OUT / f"pgsp_site_oracle_{args.suffix}.json"
    json_path.write_text(json.dumps({
        "profile": str(args.profile),
        "budget": args.budget,
        "trials": args.trials,
        "rank": "boundary-feedback",
        "targeting": "ATFT_FI_STATIC_SITE_first_hit",
        "site_map_count": len(site_map),
        "selected_sites": outcomes,
        "warning": "Small site-level oracle probe; not a replacement for the standard aggregate campaign.",
    }, indent=2, ensure_ascii=False) + "\n")
    print(json.dumps({
        "site_map_count": len(site_map),
        "selected_sites": len(outcomes),
        "csv": str(csv_path),
        "json": str(json_path),
    }, ensure_ascii=False))


if __name__ == "__main__":
    main()
