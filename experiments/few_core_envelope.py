#!/usr/bin/env python3
"""少核/满载真实工作点的保护下包络：每核在 K 个可用核下选最便宜的 SDC≈0 方案。
候选：ABFT(可归约核, 核无关)、slack/simd_tmr(核无关)、coarse DMR+重执行(核相关, 用
core_saturation_sweep 的 DMR 实测值)。回答"少核下谁还活着"。
用法: python3 experiments/few_core_envelope.py [suffix]
"""
import csv, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parent.parent
OPT = ROOT / "results" / "opt"
SDC0 = 0.05  # fp64 SDC 视为清零的阈值(%)
# coarse DMR+重执行无故障墙钟（相对 native）随可用核数：来自 core_saturation_sweep.csv
COARSE_DMR = {1: 1.98, 2: 1.04, 4: 1.00}


def load(p):
    with open(p) as f:
        return list(csv.DictReader(f))


def main():
    suf = sys.argv[1] if len(sys.argv) > 1 else "a4_t200_matrix_solid"
    pmu = load(OPT / f"pmu_{suf}.csv")
    fi = load(OPT / f"fi_outcomes_{suf}.csv")
    natc = {r["benchmark"]: float(r["hw_cycles"]) for r in pmu if r["policy"] == "native"}
    cyc = {(r["policy"], r["benchmark"]): float(r["hw_cycles"]) for r in pmu}
    fp64 = {(r["policy"], r["benchmark"]): float(r["sdc_pct"])
            for r in fi if r["fi_target_type"] == "fp64"}

    def ov(pol, b):
        c = cyc.get((pol, b)); return (c / natc[b]) if c and natc.get(b) else None

    ks = sorted(b for b in natc if ("native", b) in fp64)
    print(f"{'kernel':14s}{'ABFT(cov)':>13}{'slack/simd':>12}  | 下包络 @K核(方案)")
    env = {1: [], 2: [], 4: []}
    for b in ks:
        abft_o = ov("abft_auto", b); abft_sdc = fp64.get(("abft_auto", b), 99)
        abft_ok = abft_o is not None and abft_sdc <= SDC0
        simd_o = ov("simd_tmr", b); simd_ok = fp64.get(("simd_tmr", b), 99) <= SDC0
        abft_str = (f"{abft_o:.2f}×✓" if abft_ok else
                    (f"{abft_o:.2f}×✗{abft_sdc:.0f}%" if abft_o else "-"))
        line = f"{b:14s}{abft_str:>13}{(f'{simd_o:.2f}×' if simd_o else '-'):>12}  | "
        cells = []
        for K in (1, 2, 4):
            cands = {"coarse": COARSE_DMR[K]}
            if abft_ok: cands["ABFT"] = abft_o
            if simd_ok and simd_o: cands["slack"] = simd_o
            best = min(cands, key=cands.get)
            env[K].append(cands[best])
            cells.append(f"{cands[best]:.2f}×({best})")
        print(line + "  ".join(f"K{K}:{c}" for K, c in zip((1,2,4), cells)))
    print("\n真实工作点下包络（均值）：", "  ".join(
        f"K={K}核 {sum(env[K])/len(env[K]):.2f}×" for K in (1,2,4)))


if __name__ == "__main__":
    sys.exit(main())
