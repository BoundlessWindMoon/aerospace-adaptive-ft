#!/usr/bin/env python3
"""代价模型分析：B1 开销 vs 向量化相关；B2 故障率 λ 的策略交叉。

输入（results/opt/ 下，由 run_opt_ablation.py 产出，--suffix 指定）:
  pmu_<suffix>.csv          每 (policy,benchmark) 的 hw_insts/hw_cycles
  fi_outcomes_<suffix>.csv  每 (policy,benchmark,category) 的 sdc/due/det
  kernel_vectorization.csv  每 kernel 的 mean_vec_width / packed_frac / fp 计数

B1: 指令级策略的 cyc 开销比 vs {向量化宽度, 打包比例, FP 指令数} 的 Pearson/Spearman，
    判定"指令级冗余开销是否由向量化 slack 主导"这条假设成立与否（可证伪）。
B2: 用实测 (开销 O_p, 每故障 SDC 概率 s_p) 在 λ 扫描下算期望残余 SDC 与归一开销，
    输出"不同 λ 下最优策略"交叉表——自适应的依据。
用法: python3 experiments/analyze_cost_model.py <suffix>
"""

import csv
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OPT = ROOT / "results" / "opt"


def load_csv(p):
    with open(p) as f:
        return list(csv.DictReader(f))


def pearson(xs, ys):
    n = len(xs)
    if n < 2:
        return float("nan")
    mx, my = sum(xs) / n, sum(ys) / n
    sxy = sum((x - mx) * (y - my) for x, y in zip(xs, ys))
    sxx = sum((x - mx) ** 2 for x in xs)
    syy = sum((y - my) ** 2 for y in ys)
    if sxx == 0 or syy == 0:
        return float("nan")
    return sxy / (sxx * syy) ** 0.5


def spearman(xs, ys):
    def ranks(v):
        order = sorted(range(len(v)), key=lambda i: v[i])
        r = [0.0] * len(v)
        i = 0
        while i < len(v):
            j = i
            while j + 1 < len(v) and v[order[j + 1]] == v[order[i]]:
                j += 1
            avg = (i + j) / 2.0 + 1
            for k in range(i, j + 1):
                r[order[k]] = avg
            i = j + 1
        return r
    return pearson(ranks(xs), ranks(ys))


# 指令级冗余策略（会和向量化抢 lane 的那些）
INSTR_POLICIES = ["exact-tmr", "simd_tmr", "opt_base"]


def b1_overhead_vs_vec(suffix):
    pmu = load_csv(OPT / f"pmu_{suffix}.csv")
    vec = {r["benchmark"]: r for r in load_csv(OPT / "kernel_vectorization.csv")}
    nat = {r["benchmark"]: (float(r["hw_insts"]), float(r["hw_cycles"]))
           for r in pmu if r["policy"] == "native"}
    rows = []
    for r in pmu:
        p, b = r["policy"], r["benchmark"]
        if p not in INSTR_POLICIES or b not in nat or b not in vec:
            continue
        ni, nc = nat[b]
        v = vec[b]
        rows.append({
            "policy": p, "benchmark": b,
            "cyc_ratio": round(float(r["hw_cycles"]) / nc, 3),
            "inst_ratio": round(float(r["hw_insts"]) / ni, 3),
            "mean_vec_width": float(v["mean_vec_width"]),
            "packed_frac": float(v["packed_frac"]),
            "total_fp": int(v["total_fp"]),
            "packed_fp": int(v["packed_fp"]),
        })
    out = OPT / f"kernel_overhead_vs_vec_{suffix}.csv"
    with open(out, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader()
        w.writerows(rows)
    print(f"[B1] -> {out}  ({len(rows)} points)")
    print(f"{'predictor':16s}{'policy':12s}{'Pearson':>9}{'Spearman':>10}  n")
    for pred in ["mean_vec_width", "packed_frac", "total_fp", "packed_fp"]:
        for p in INSTR_POLICIES:
            sub = [x for x in rows if x["policy"] == p]
            if len(sub) < 3:
                continue
            xs = [x[pred] for x in sub]
            ys = [x["cyc_ratio"] for x in sub]
            print(f"{pred:16s}{p:12s}{pearson(xs, ys):9.3f}{spearman(xs, ys):10.3f}  {len(sub)}")
        # 合并全部指令级策略点
        sub = rows
        xs = [x[pred] for x in sub]
        ys = [x["cyc_ratio"] for x in sub]
        print(f"{pred:16s}{'ALL':12s}{pearson(xs, ys):9.3f}{spearman(xs, ys):10.3f}  {len(sub)}")
    return rows


def agg_by_policy(suffix):
    """把 fi_outcomes 按 policy 等权聚合出 每故障 SDC/DUE 概率，并取 PMU 开销。"""
    fi = load_csv(OPT / f"fi_outcomes_{suffix}.csv")
    pmu = load_csv(OPT / f"pmu_{suffix}.csv")
    nat_ins = {r["benchmark"]: float(r["hw_insts"])
               for r in pmu if r["policy"] == "native"}
    # 每 policy 的平均 cyc 开销（相对 native，跨 kernel 等权）
    natc = {r["benchmark"]: float(r["hw_cycles"])
            for r in pmu if r["policy"] == "native"}
    over = {}
    for r in pmu:
        b = r["benchmark"]
        if b in natc and natc[b] > 0:
            over.setdefault(r["policy"], []).append(float(r["hw_cycles"]) / natc[b])
    pol = {}
    for r in fi:
        p = r["policy"]
        pol.setdefault(p, {"sdc": [], "due": []})
        pol[p]["sdc"].append(float(r["sdc_pct"]) / 100.0)
        pol[p]["due"].append(float(r["due_pct"]) / 100.0)
    res = {}
    for p, d in pol.items():
        res[p] = {
            "overhead": sum(over.get(p, [1.0])) / max(1, len(over.get(p, [1.0]))),
            "sdc_per_fault": sum(d["sdc"]) / len(d["sdc"]),
            "due_per_fault": sum(d["due"]) / len(d["due"]),
        }
    avg_ins = sum(nat_ins.values()) / max(1, len(nat_ins))
    return res, avg_ins


def b2_lambda_crossover(suffix):
    res, avg_ins = agg_by_policy(suffix)
    # λ = 每条动态指令的翻转概率；期望每任务残余 SDC = λ·R·s_p（R≈native 动态指令数）
    # 选择准则：在"残余 SDC ≤ 目标"约束下取 overhead 最小；都不达标时取残余 SDC 最低、
    # 再以 overhead 破平。目标做一个小扫描，让"该用哪层"的交叉显式出现在表里。
    lambdas = [10 ** e for e in range(-12, -2)]
    targets = [1e-9, 1e-6, 1e-3]
    policies = sorted(res)
    out = OPT / f"lambda_crossover_{suffix}.csv"
    with open(out, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["target_sdc", "lambda", "best_policy", "best_overhead"] +
                    [f"sdc_{p}" for p in policies])
        for target in targets:
            print(f"\n[B2] 残余 SDC 目标={target:g}，R≈{avg_ins:.0f} 动态指令/任务")
            print(f"{'lambda':>10}  {'best':16s}{'overhead':>9}")
            for lam in lambdas:
                sdcs = {p: lam * avg_ins * res[p]["sdc_per_fault"] for p in policies}
                feasible = [(res[p]["overhead"], p) for p in policies
                            if sdcs[p] <= target]
                if feasible:
                    best_o, best_p = min(feasible)
                else:
                    best_p = min(policies, key=lambda q: (sdcs[q], res[q]["overhead"]))
                    best_o = res[best_p]["overhead"]
                w.writerow([f"{target:g}", f"{lam:g}", best_p, f"{best_o:.2f}"] +
                           [f"{sdcs[p]:.2e}" for p in policies])
                print(f"{lam:10.0e}  {best_p:16s}{best_o:9.2f}")
    print(f"[B2] -> {out}")
    # 附实测 MWTF（fi_outcomes 已算），给策略排序一个直接读数
    fi = load_csv(OPT / f"fi_outcomes_{suffix}.csv")
    mwtf = {}
    for r in fi:
        try:
            mwtf.setdefault(r["policy"], []).append(float(r["mwtf_sdc"]))
        except (KeyError, ValueError):
            pass
    print("\n各策略实测 (overhead, sdc/fault, due/fault, 均值MWTF_sdc):")
    for p in policies:
        mv = sum(mwtf.get(p, [0])) / max(1, len(mwtf.get(p, [0])))
        print(f"  {p:16s} O={res[p]['overhead']:6.2f}  "
              f"s={res[p]['sdc_per_fault']:.4f}  d={res[p]['due_per_fault']:.4f}"
              f"  MWTF={mv:8.2f}")


def build_property_table(suffix):
    """每内核属性+结果汇总：向量化、可归约性、各策略开销与 fp64 覆盖。"""
    pmu = load_csv(OPT / f"pmu_{suffix}.csv")
    fi = load_csv(OPT / f"fi_outcomes_{suffix}.csv")
    vec = {r["benchmark"]: r for r in load_csv(OPT / "kernel_vectorization.csv")}
    natc = {r["benchmark"]: float(r["hw_cycles"])
            for r in pmu if r["policy"] == "native"}
    cyc = {(r["policy"], r["benchmark"]): float(r["hw_cycles"]) for r in pmu}
    fp64sdc = {(r["policy"], r["benchmark"]): float(r["sdc_pct"])
               for r in fi if r["fi_target_type"] == "fp64"}

    def ratio(pol, b):
        c = cyc.get((pol, b))
        return round(c / natc[b], 2) if c and natc.get(b) else ""

    rows = []
    for b in sorted(natc):
        if b not in vec:
            continue
        ns = fp64sdc.get(("native", b))
        asdc = fp64sdc.get(("abft_auto", b))
        reducible = int(ns is not None and asdc is not None and asdc < ns - 0.5)
        rows.append({
            "benchmark": b,
            "mean_vec_width": vec[b]["mean_vec_width"],
            "packed_frac": vec[b]["packed_frac"],
            "reducible": reducible,
            "cyc_x_exact_tmr": ratio("exact-tmr", b),
            "cyc_x_simd_tmr": ratio("simd_tmr", b),
            "cyc_x_abft": ratio("abft_auto", b),
            "cyc_x_layered": ratio("opt_layered", b),
            "fp64_sdc_native": ns if ns is not None else "",
            "fp64_sdc_abft": asdc if asdc is not None else "",
            "fp64_sdc_layered": fp64sdc.get(("opt_layered", b), ""),
        })
    out = OPT / f"kernel_properties_{suffix}.csv"
    with open(out, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader()
        w.writerows(rows)
    print(f"\n[A2] -> {out}  ({len(rows)} kernels)")
    print(f"{'kernel':18s}{'meanW':>6}{'red':>4}{'exact':>7}{'simd':>6}"
          f"{'abft':>6}{'layer':>6}{'SDCnat':>8}{'SDClay':>8}")
    for r in rows:
        print(f"{r['benchmark']:18s}{float(r['mean_vec_width']):6.2f}{r['reducible']:4d}"
              f"{str(r['cyc_x_exact_tmr']):>7}{str(r['cyc_x_simd_tmr']):>6}"
              f"{str(r['cyc_x_abft']):>6}{str(r['cyc_x_layered']):>6}"
              f"{str(r['fp64_sdc_native']):>8}{str(r['fp64_sdc_layered']):>8}")


def main():
    suffix = sys.argv[1] if len(sys.argv) > 1 else "a4_t200_matrix_solid"
    b1_overhead_vs_vec(suffix)
    b2_lambda_crossover(suffix)
    build_property_table(suffix)


if __name__ == "__main__":
    sys.exit(main())
