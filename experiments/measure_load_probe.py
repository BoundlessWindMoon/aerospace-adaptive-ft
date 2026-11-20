#!/usr/bin/env python3
"""诊断 load 穿越口:载入故障的覆盖到底卡在哪。
假设:FP-DMR 只 shadow FP load,整数 load(地址/下标)没覆盖 → 残余 SDC 来自整数 load。
若加 int-TMR(它 shadow 整数 load)后 load SDC 大降 → 假设成立,中间档=纳入整数穿越口。"""
import sys, csv
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import benchmark_harness as H

TRIALS = 200
CFGS = [
    ("vec_dmr",         "approx-simd", ["-atft-dmr-generic"]),
    ("vec_dmr_inttmr",  "approx-simd", ["-atft-dmr-generic", "-atft-int-tmr-wire", "-atft-int-tmr-i64"]),
    ("eddi_ref",        "eddi",        []),
]
SSA = H.build_infrastructure()
def pct(k, n): return 100.0 * k / n if n else 0.0

print("\n=== load 类别:故障结局分解(对自身 golden) ===")
print(f"{'config':18}{'masked':>9}{'det_rec':>9}{'benign':>9}{'SDC':>9}{'DUE':>9}")
rows = []
for tag, mode, flags in CFGS:
    _, f = H.compile_and_evaluate_policy(
        SSA, f"ldp_{tag}", mode, budget=1.0, fi_trials=TRIALS,
        run_fi=True, fi_target_type="load", dump_golden=True, pass_flags=flags)
    n = sum(r["trials"] for r in f)
    agg = {}
    for key in ["masked_pct", "det_recovered_pct", "benign_pct", "sdc_pct", "due_pct"]:
        agg[key] = sum(r[key] / 100 * r["trials"] for r in f) / n * 100 if n else 0
    print(f"{tag:18}{agg['masked_pct']:9.1f}{agg['det_recovered_pct']:9.1f}"
          f"{agg['benign_pct']:9.1f}{agg['sdc_pct']:9.1f}{agg['due_pct']:9.1f}")
    _, _, h = H.wilson_ci95(round(agg['sdc_pct'] / 100 * n), n)
    rows.append((tag, round(agg['sdc_pct'], 2), round(h, 2), round(agg['due_pct'], 2), n))

print("\n=== 判定 ===")
base = next(r for r in rows if r[0] == "vec_dmr")
intr = next(r for r in rows if r[0] == "vec_dmr_inttmr")
print(f"  vec_dmr load SDC {base[1]}±{base[2]}  →  +int-TMR {intr[1]}±{intr[2]}")
if intr[1] < base[1] - (base[2] + intr[2]):
    print("  → 显著下降:残余 load SDC 确来自整数 load,中间档=纳入整数穿越口(无需新机制)")
else:
    print("  → 未显著下降:存在整数 load 之外的 load 洞(可能共模持久故障),需要真正的中间档")

OUT = H.RES_DIR / "load_probe.csv"
with open(OUT, "w", newline="") as fp:
    w = csv.writer(fp); w.writerow(["config", "load_sdc_pct", "sdc_ci95_half", "load_due_pct", "n"])
    w.writerows(rows)
print("CSV ->", OUT)
