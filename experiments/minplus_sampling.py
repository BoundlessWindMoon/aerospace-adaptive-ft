#!/usr/bin/env python3
"""Offline sampling study for min-plus verification.

The experiment deliberately separates two sampling models:

* global_triple: sample (i, j, k) over the whole distance matrix;
* target_pair_k: assume the affected output pair (i, j) is already known and
  sample only k witnesses for that pair.

The second model is optimistic. It is useful as an upper bound on what a
runtime detector could achieve after an independent locator has narrowed the
candidate pair. Neither model changes the compiler or frozen campaigns.
"""

import argparse
import csv
import json
import math
import random
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
INF = 10 ** 9


def build_graph(n):
    weights = [[INF] * n for _ in range(n)]
    for i in range(n):
        weights[i][i] = 0
        for j in range(n):
            if i == j:
                continue
            value = (i * j) % 7 + 1
            if (i + j) % 13 == 0 or (i + j) % 7 == 0 or (i + j) % 11 == 0:
                value = 999
            weights[i][j] = value
    return weights


def floyd_with_predecessor(weights):
    n = len(weights)
    dist = [row[:] for row in weights]
    pred = [[-1] * n for _ in range(n)]
    for i in range(n):
        for j in range(n):
            if i != j and dist[i][j] < INF:
                pred[i][j] = i
    for k in range(n):
        for i in range(n):
            dik = dist[i][k]
            if dik >= INF:
                continue
            for j in range(n):
                candidate = dik + dist[k][j]
                if candidate < dist[i][j]:
                    dist[i][j] = candidate
                    pred[i][j] = pred[k][j]
    return dist, pred


def wilson(successes, total, z=1.959963984540054):
    if total == 0:
        return 0.0, 0.0
    p = successes / total
    denom = 1.0 + z * z / total
    center = (p + z * z / (2.0 * total)) / denom
    half = z * math.sqrt(p * (1.0 - p) / total + z * z / (4.0 * total * total)) / denom
    return max(0.0, center - half), min(1.0, center + half)


def witnesses(dist, i, j):
    return [k for k in range(len(dist))
            if k != i and k != j and dist[i][j] == dist[i][k] + dist[k][j]]


def choose_pairs(dist):
    pairs = []
    n = len(dist)
    for i in range(n):
        for j in range(n):
            if i != j and dist[i][j] < INF and witnesses(dist, i, j):
                pairs.append((i, j))
    return pairs


def sampled_global(rng, n, m, bad, target_i, target_j, mode, pred, weights):
    for _ in range(m):
        i = rng.randrange(n)
        j = rng.randrange(n)
        k = rng.randrange(n)
        if mode == "increase":
            if bad[i][j] > bad[i][k] + bad[k][j]:
                return True
        else:
            if i == target_i and j == target_j and pred[i][j] >= 0:
                p = pred[i][j]
                if bad[i][j] != bad[i][p] + weights[p][j]:
                    return True
    return False


def sampled_target_pair(rng, n, m, bad, target_i, target_j, mode, pred, weights):
    if mode == "increase":
        for _ in range(m):
            k = rng.randrange(n)
            if bad[target_i][target_j] > bad[target_i][k] + bad[k][target_j]:
                return True
        return False
    if pred[target_i][target_j] < 0:
        return False
    p = pred[target_i][target_j]
    return bad[target_i][target_j] != bad[target_i][p] + weights[p][target_j]


def run_point(n, mode, sampler, m, trials, seed):
    weights = build_graph(n)
    dist, pred = floyd_with_predecessor(weights)
    pairs = choose_pairs(dist)
    rng = random.Random(seed)
    detected = 0
    for _ in range(trials):
        i, j = rng.choice(pairs)
        bad = [row[:] for row in dist]
        if mode == "increase":
            bad[i][j] += 1
        else:
            if bad[i][j] <= 1:
                bad[i][j] += 1
            else:
                bad[i][j] -= 1
        if sampler == "global_triple":
            found = sampled_global(rng, n, m, bad, i, j, mode, pred, weights)
        else:
            found = sampled_target_pair(rng, n, m, bad, i, j, mode, pred, weights)
        detected += int(found)
    low, high = wilson(detected, trials)
    miss = trials - detected
    _, miss_high = wilson(miss, trials)
    return {
        "n": n,
        "mode": mode,
        "sampler": sampler,
        "m": m,
        "trials": trials,
        "detected_pct": 100.0 * detected / trials,
        "detected_ci95_low_pct": 100.0 * low,
        "detected_ci95_high_pct": 100.0 * high,
        "miss_pct": 100.0 * miss / trials,
        "miss_ci95_high_pct": 100.0 * miss_high,
        "checks_per_trial": m,
        "pair_count": len(pairs),
    }


def read_native_insts(path, benchmark):
    if not path:
        return None
    with open(path, newline="") as f:
        for row in csv.DictReader(f):
            if row.get("policy") == "native" and row.get("benchmark") == benchmark:
                return int(row["hw_insts"])
    return None


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--sizes", default="24,48")
    parser.add_argument("--m-values", default="1,4,16,64,256,1024,4096")
    parser.add_argument("--trials", type=int, default=1000)
    parser.add_argument("--seed", type=int, default=20261115)
    parser.add_argument("--native-pmu", default="results/opt/pmu_a4_t200_excl.csv")
    parser.add_argument("--out", default="results/opt/minplus_sampling_20261115")
    args = parser.parse_args()

    sizes = [int(x) for x in args.sizes.split(",") if x]
    m_values = [int(x) for x in args.m_values.split(",") if x]
    rows = []
    for size_index, n in enumerate(sizes):
        for mode_index, mode in enumerate(("increase", "decrease")):
            for sampler_index, sampler in enumerate(("global_triple", "target_pair_k")):
                for m_index, m in enumerate(m_values):
                    point_seed = args.seed + size_index * 1000000 + mode_index * 100000 + sampler_index * 10000 + m_index
                    rows.append(run_point(n, mode, sampler, m, args.trials, point_seed))

    pmu_path = ROOT / args.native_pmu
    native_insts = read_native_insts(pmu_path, "pb_floyd_warshall") if pmu_path.exists() else None
    for row in rows:
        row["native_floyd_hw_insts"] = native_insts
        row["check_to_native_inst_ratio"] = (
            row["checks_per_trial"] / native_insts if native_insts else None
        )

    output = ROOT / (args.out if args.out.endswith(".csv") else args.out + ".csv")
    output.parent.mkdir(parents=True, exist_ok=True)
    with open(output, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)

    summary = {
        "sizes": sizes,
        "m_values": m_values,
        "trials": args.trials,
        "seed": args.seed,
        "graph_model": "eval_suite.c floyd-warshall initialization, n=24/48 corresponds to small/default harness scales",
        "native_pmu": args.native_pmu,
        "native_floyd_hw_insts": native_insts,
        "rows": rows,
    }
    json_output = output.with_suffix(".json")
    json_output.write_text(json.dumps(summary, indent=2, ensure_ascii=False))
    print(f"wrote {output}")
    print(f"wrote {json_output}")


if __name__ == "__main__":
    main()
