#!/usr/bin/env python3
"""系统框架评估（对齐 CLEAR/COAST/August 口径）。
所有配置同一 host ISA；注错同口径；指标 = SDC/DUE ± Wilson95%CI + MWTF(可靠性×性能)。
产出：逐配置表 + (开销, MWTF) Pareto 前沿。基线用我们已移植的 COAST/SWIFT 类 pass。
能量轴不在此脚本（需干净节点跑 RAPL）；完成率/UUniFast 为后续。"""
import math, sys, csv
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import benchmark_harness as H

ISA = ["-atft-isa-avx512"]
APPROX = ["-atft-delta-runtime", "-atft-replica-exact", "-atft-no-cse-barrier", "-atft-simd-f64"]
TRIALS = 400
# (tag, mode, extra_flags, is_golden)
CFGS = [
    ("native",     "native",      ISA,                                     True),
    ("eddi",       "eddi",        ISA,                                     False),  # 基线: 全复制+检错
    ("swift",      "swift",       ISA,                                     False),  # 基线: SWIFT 式
    ("exact_tmr",  "exact-tmr",   ISA,                                     False),  # 基线: 全 TMR(COAST 类)
    ("inlane",     "approx-simd", APPROX + ISA,                            False),  # 我们旧的 in-lane
    ("dmr_gen",    "approx-simd", ["-atft-dmr-generic"],                   False),  # 通用 DMR(检错)
    ("tmr_gen",    "approx-simd", ["-atft-dmr-generic", "-atft-tmr-generic"], False), # 通用 TMR(纠错)
]

SSA = H.build_infrastructure()
rows = {}
for tag, mode, flags, gold in CFGS:
    pmu, fi = H.compile_and_evaluate_policy(SSA, f"fw_{tag}", mode, budget=1.0,
                                            fi_trials=TRIALS, run_fi=True, fi_target_type="fp64",
                                            dump_golden=gold, pass_flags=flags)
    t = {r["benchmark"]: r["exec_time_us"] for r in pmu}
    f = {r["benchmark"]: r for r in fi}
    rows[tag] = (t, f)

B = list(rows["native"][0].keys())
tnat = rows["native"][0]
def geo(xs):
    xs = [x for x in xs if x > 0]
    return math.exp(sum(map(math.log, xs)) / len(xs)) if xs else float("nan")

def agg(tag):
    t, f = rows[tag]
    ov = geo([t[b] / tnat[b] for b in B if b in t and tnat.get(b, 0) > 0])
    ks = sum(round(f[b]["sdc_pct"] / 100 * f[b]["trials"]) for b in B if b in f)
    kd = sum(round(f[b]["due_pct"] / 100 * f[b]["trials"]) for b in B if b in f)
    n = sum(f[b]["trials"] for b in B if b in f)
    _, _, shalf = H.wilson_ci95(ks, n)            # 已是百分比
    _, _, fhalf = H.wilson_ci95(ks + kd, n)       # SDC+DUE 合并故障的 CI
    return ov, ks / n * 100, shalf, kd / n * 100, (ks + kd) / n * 100, fhalf, n

A = {tag: agg(tag) for tag, *_ in CFGS}
# 故障 = SDC + DUE（任何未恢复错误；对"检错即崩溃"的基线才公平）。MWTF ∝ 1/(overhead·fail)
nat_fail = A["native"][4] / 100
def mwtf_ratio(tag):
    ov, sdc, shalf, due, fail, fhalf, n = A[tag]
    fail_frac = fail / 100 if fail > 0 else fhalf / 100   # 0 故障→用 CI 上界(得 MWTF 下界)
    base = nat_fail if nat_fail > 0 else 1e-9
    return base / (ov * max(fail_frac, 1e-9))

print(f"\n{'config':10}{'overhead':>9}{'SDC%':>7}{'±':>5}{'DUE%':>7}{'MWTF↑vsNat':>11}")
out = []
for tag, *_ in CFGS:
    ov, sdc, shalf, due, fail, fhalf, n = A[tag]
    m = mwtf_ratio(tag)
    out.append((tag, ov, fail, m))
    print(f"{tag:10}{ov:>9.2f}{sdc:>7.1f}{shalf:>5.1f}{due:>7.1f}{m:>11.1f}")
print(f"\n(trials/bench={TRIALS}, n_total≈{A['native'][6]}; 故障=SDC+DUE; 同 host ISA; MWTF 相对 native 越大越好)")

def dominated(a, others):
    _, ova, _, ma = a
    return any((ovb <= ova and mb >= ma and (ovb < ova or mb > ma)) for _, ovb, _, mb in others)
cand = [o for o in out if o[0] != "native"]
front = [r for r in cand if not dominated(r, cand)]
print("Pareto 前沿(开销–MWTF):", ", ".join(sorted(t for t, *_ in front)))

RES = H.RES_DIR / "framework_eval.csv"
with open(RES, "w", newline="") as fp:
    w = csv.writer(fp); w.writerow(["config", "overhead", "sdc_pct", "sdc_ci95", "due_pct", "fail_pct", "mwtf_vs_native"])
    for tag, *_ in CFGS:
        ov, sdc, shalf, due, fail, fhalf, n = A[tag]
        w.writerow([tag, round(ov, 3), round(sdc, 2), round(shalf, 2), round(due, 2), round(fail, 2), round(mwtf_ratio(tag), 2)])
print("CSV ->", RES)
