#!/usr/bin/env python3
"""Conservative static pre-screen for replayable LLVM basic blocks.

This is a feasibility gate for C1. It does not claim that a block is
semantically idempotent: it only removes blocks with obvious externally
visible writes, calls, address formation, or control-flow state. A positive
candidate still needs SSA/memory-dependence validation before any replay code
is emitted.
"""

import argparse
import csv
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FUNC_RE = re.compile(r'^define\s+.*@([^(]+)\(')
LABEL_RE = re.compile(r'^([A-Za-z$._][A-Za-z0-9$._-]*):')


def classify_block(name, lines):
    text = "\n".join(lines)
    return {
        "function": name,
        "block": name,
        "instructions": len([line for line in lines if line.strip() and not line.lstrip().startswith(';')]),
        "has_store": bool(re.search(r'\bstore\b', text)),
        "has_atomic_or_volatile": bool(re.search(r'\b(?:atomic|volatile)\b', text)),
        "has_call": bool(re.search(r'\bcall\b|\binvoke\b', text)),
        "has_gep": bool(re.search(r'\bgetelementptr\b', text)),
        "has_phi": bool(re.search(r'\bphi\b', text)),
        "has_conditional_branch": bool(re.search(r'\bbr\s+i1\b|\bswitch\b', text)),
        "has_return": bool(re.search(r'\bret\b', text)),
    }


def scan(path):
    rows = []
    current_function = None
    current_block = None
    block_lines = []

    def flush():
        nonlocal block_lines
        if current_function is not None and current_block is not None:
            row = classify_block(current_function, block_lines)
            row["block"] = current_block
            row["candidate"] = row["instructions"] >= 2 and not any(row[key] for key in (
                "has_store", "has_atomic_or_volatile", "has_call", "has_gep", "has_conditional_branch"))
            row["rejection"] = "candidate" if row["candidate"] else ",".join(
                key.removeprefix("has_") for key in (
                    "has_store", "has_atomic_or_volatile", "has_call", "has_gep", "has_conditional_branch")
                if row[key])
            rows.append(row)
        block_lines = []

    for raw in path.read_text().splitlines():
        line = raw.strip("\n")
        fn = FUNC_RE.match(line)
        if fn:
            flush()
            current_function = fn.group(1)
            current_block = "entry"
            block_lines = []
            continue
        if line.startswith("}") and current_function is not None:
            flush()
            current_function = None
            current_block = None
            continue
        if current_function is None:
            continue
        label = LABEL_RE.match(line.strip())
        if label and not line.lstrip().startswith(";"):
            flush()
            current_block = label.group(1)
            continue
        block_lines.append(line)
    flush()
    return rows


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--ir", default="build/benchmarks_ssa.ll")
    parser.add_argument("--out", default="results/opt/idempotent_regions_20261115")
    args = parser.parse_args()
    ir = ROOT / args.ir
    rows = scan(ir)
    output = ROOT / (args.out if args.out.endswith(".csv") else args.out + ".csv")
    output.parent.mkdir(parents=True, exist_ok=True)
    with open(output, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)
    by_function = {}
    for row in rows:
        item = by_function.setdefault(row["function"], {"blocks": 0, "candidates": 0})
        item["blocks"] += 1
        item["candidates"] += int(row["candidate"])
    summary = {
        "ir": args.ir,
        "blocks": len(rows),
        "conservative_candidates": sum(int(row["candidate"]) for row in rows),
        "by_function": by_function,
        "interpretation": "Candidates are only a structural pre-screen; no candidate is safe to replay until memory dependence and external state are proven.",
    }
    output.with_suffix(".json").write_text(json.dumps(summary, indent=2, ensure_ascii=False))
    print(f"blocks={len(rows)} candidates={summary['conservative_candidates']}")
    print(f"wrote {output}")


if __name__ == "__main__":
    main()
