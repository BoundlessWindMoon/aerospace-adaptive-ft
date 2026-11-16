#!/usr/bin/env python3
"""
粗粒度冗余的开销标度实测（可复现，修 Bug1/2/4）。

编译 experiments/coarse_scaling.c，在 taskset 限定的核数下扫 cores，
每个量取中位数 + p10/p90，写 results/opt/core_saturation_sweep.csv。

用法：python3 experiments/measure_coarse_scaling.py [N] [reps]
"""
import csv
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BIN = ROOT / "build" / "coarse_scaling"
OUT = ROOT / "results" / "opt" / "core_saturation_sweep.csv"

INC = ["include", "benchmarks/obpmark_shim", "third_party/polybench-c/utilities",
       "third_party/kissfft", "third_party/obpmark/src/common",
       "third_party/obpmark/src/1.1-image/src/ports/cpu", "benchmarks"]
CORES = [1, 2, 3, 4, 6, 8]


def build():
    cmd = ["gcc", "-O3", "-march=native", "-w"] + [f"-I{d}" for d in INC] + [
        "experiments/coarse_scaling.c", "benchmarks/benchmarks.c",
        "-o", str(BIN), "-lpthread", "-lm"]
    subprocess.run(cmd, cwd=ROOT, check=True)


def run(cores, n, reps):
    cpul = f"0-{cores - 1}" if cores > 1 else "0"
    r = subprocess.run(["taskset", "-c", cpul, str(BIN), str(n), str(reps)],
                       cwd=ROOT, capture_output=True, text=True, check=True)
    return [line.split(",") for line in r.stdout.strip().splitlines()]


def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 512
    reps = int(sys.argv[2]) if len(sys.argv) > 2 else 15
    build()
    rows = []
    for c in CORES:
        for rec in run(c, n, reps):
            rows.append(rec)
    with open(OUT, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["method", "N", "cores", "reps", "ms_median",
                    "ratio_median", "ratio_p10", "ratio_p90"])
        w.writerows(rows)
    print(f"-> {OUT} ({len(rows)} rows, N={n}, reps={reps}, cores={CORES})")


if __name__ == "__main__":
    main()
