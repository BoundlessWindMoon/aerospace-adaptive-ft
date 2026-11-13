#!/usr/bin/env python3
"""
聚合口径（单一实现，harness 与看板共用）

惯例来源：
  - 开销等比值类指标用**几何平均**，可靠性等率值用**算术平均**
    SWIFT (CGO'05) 与 SWIFT-R (DSN'06) 的图题即如此区分：
    "The Average is across all benchmarks" / "The GeoMean is across all benchmarks"
  - 负载权重：套件内等权 -> 套件间等权（Relyzer, ASPLOS'12 的 "four each from
    SPLASH-2 / PARSEC / SPEC CPU2006" 精神），避免单一套件按注入点数主导
  - 注错类别：采样时等额，聚合时额外给一份按动态频率加权的口径
    （SASSIFI, ISPASS'17: "weighted with the total fraction of dynamic
    instructions in each group"）
  - 聚合值的置信区间：核查过的同类工作均未报告，这里按独立二项比例的
    加权均值方差给出，属本工作的增量

用法：
    python3 experiments/aggregate.py                     # 读 results/ 下默认 CSV
    python3 experiments/aggregate.py --json out.json
"""

import argparse
import csv
import json
import math
from pathlib import Path

ROOT_DIR = Path(__file__).resolve().parent.parent
RES_DIR = ROOT_DIR / "results"
META_PATH = ROOT_DIR / "build" / "benchmark_meta.csv"

Z95 = 1.959963985


def load_meta(path=META_PATH):
    """负载 -> (suite, kind)。优先读 C 侧注册表导出的文件。"""
    if Path(path).exists():
        out = {}
        for r in csv.DictReader(open(path, encoding="utf8")):
            out[r["name"]] = {"suite": r["suite"], "kind": r["kind"],
                              "tags": r.get("tags", "")}
        if out:
            return out
    raise RuntimeError(
        f"缺少基准注册表 {path}；先跑一次 benchmark_harness.py 生成（它由 "
        f"eval_suite --list-benchmarks 导出，不要在 Python 侧另写一份套件表）")


def geomean(xs):
    xs = [x for x in xs if x > 0]
    if not xs:
        return 0.0
    return math.exp(sum(math.log(x) for x in xs) / len(xs))


def two_level_weights(items, meta):
    """套件内等权、套件间等权：返回 benchmark -> 权重（和为 1）。"""
    by_suite = {}
    for b in items:
        by_suite.setdefault(meta[b]["suite"], []).append(b)
    n_suites = len(by_suite)
    w = {}
    for suite, bs in by_suite.items():
        for b in bs:
            w[b] = 1.0 / (n_suites * len(bs))
    return w


def weighted_rate(rows, field, weights, trials_field="trials"):
    """加权平均率值 + 95% 置信区间（独立二项比例加权均值）。"""
    mean = 0.0
    var = 0.0
    for r in rows:
        b = r["benchmark"]
        w = weights.get(b, 0.0)
        if w <= 0:
            continue
        p = float(r[field]) / 100.0
        n = max(1, int(float(r[trials_field])))
        mean += w * p
        var += (w ** 2) * p * (1.0 - p) / n
    half = Z95 * math.sqrt(var) if var > 0 else 0.0
    return 100.0 * mean, 100.0 * half


def weighted_geomean(rows, field, weights):
    acc = 0.0
    wsum = 0.0
    for r in rows:
        w = weights.get(r["benchmark"], 0.0)
        v = float(r[field])
        if w <= 0 or v <= 0:
            continue
        acc += w * math.log(v)
        wsum += w
    return math.exp(acc / wsum) if wsum > 0 else 0.0


def aggregate(fi_rows, pmu_rows, meta):
    """返回 {policy: {"by_category": {...}, "overall": {...}}}。

    overall 有两个口径：
      equal_category  —— 七类等权（比较机制优劣用这个）
      freq_weighted   —— 按各类别动态注入点数加权（估计真实程序级风险）
    """
    native_inst = {r["benchmark"]: float(r["hw_insts"])
                   for r in pmu_rows if r["policy"] == "native"}

    # 只保留真的有注入点的行；dyn=0 表示该类别下无插桩点，不能当成"没损坏"
    rows = [r for r in fi_rows if float(r.get("dyn_target_insts", 0)) > 0]

    policies = []
    for r in rows:
        if r["policy"] not in policies:
            policies.append(r["policy"])

    # 指令倍率按负载取（与注错类别无关），几何平均
    ratio_rows = {}
    for r in pmu_rows:
        ratio_rows.setdefault(r["policy"], []).append({
            "benchmark": r["benchmark"],
            "ratio": float(r["hw_insts"]) / max(1.0, native_inst.get(r["benchmark"], 1.0)),
        })

    # 各类别的动态注入点总数（用 native 的分布作为频率权重）
    cat_sites = {}
    for r in rows:
        if r["policy"] != "native":
            continue
        cat_sites[r["fi_target_type"]] = cat_sites.get(r["fi_target_type"], 0.0) \
            + float(r["dyn_target_insts"])

    # 基线（native）的类别集合定义了"原程序的故障空间"
    base_category_set = [r["fi_target_type"] for r in rows if r["policy"] == "native"]
    base_category_set = list(dict.fromkeys(base_category_set))
    if not base_category_set:
        base_category_set = list(dict.fromkeys(r["fi_target_type"] for r in rows))

    out = {}
    for pol in policies:
        prows = [r for r in rows if r["policy"] == pol]
        cats = []
        for r in prows:
            if r["fi_target_type"] not in cats:
                cats.append(r["fi_target_type"])

        by_cat = {}
        for c in cats:
            crows = [r for r in prows if r["fi_target_type"] == c]
            bms = [r["benchmark"] for r in crows]
            w = two_level_weights(bms, meta)
            entry = {"n_benchmarks": len(bms),
                     "n_suites": len({meta[b]["suite"] for b in bms}),
                     "trials": sum(int(float(r["trials"])) for r in crows)}
            for field, key in [("sdc_pct", "sdc"), ("due_pct", "due"),
                               ("det_recovered_pct", "det_rec"),
                               ("masked_pct", "masked"), ("benign_pct", "benign")]:
                m, h = weighted_rate(crows, field, w)
                entry[key] = m
                entry[key + "_ci95"] = h
            by_cat[c] = entry

        # 开销：几何平均（比值类指标）
        rr = ratio_rows.get(pol, [])
        rw = two_level_weights([x["benchmark"] for x in rr], meta) if rr else {}
        inst_ratio = weighted_geomean(rr, "ratio", rw) if rr else float("nan")

        def combine(keys_weight):
            agg = {}
            for key in ["sdc", "due", "det_rec", "masked", "benign"]:
                m = sum(keys_weight[c] * by_cat[c][key] for c in cats)
                # 方差按权重平方合成
                v = sum((keys_weight[c] ** 2) * (by_cat[c][key + "_ci95"] / Z95) ** 2
                        for c in cats)
                agg[key] = m
                agg[key + "_ci95"] = Z95 * math.sqrt(v) if v > 0 else 0.0
            return agg

        # 等权聚合只覆盖**基线程序本身存在的**故障类别。
        # 保护方案会引入新的故障点（例如 approx 副本带来的 fp32 运算），
        # 那些点不属于原程序的故障空间，混进主口径会让不同策略的
        # 类别集合不一致、失去可比性；它们单列在 extra_categories 里。
        base_cats = [c for c in cats if c in base_category_set] or cats
        extra_cats = [c for c in cats if c not in base_category_set]

        eq_w = {c: 1.0 / len(base_cats) for c in base_cats}
        tot_sites = sum(cat_sites.get(c, 0.0) for c in base_cats) or 1.0
        fq_w = {c: cat_sites.get(c, 0.0) / tot_sites for c in base_cats}
        cats = base_cats

        equal_cat = combine(eq_w)
        freq_w = combine(fq_w)
        for d in (equal_cat, freq_w):
            fail = (d["sdc"] + d["due"]) / 100.0
            fail = max(fail, 1e-6)
            d["inst_ratio"] = inst_ratio
            d["mwtf_total"] = 1.0 / (inst_ratio * fail) if inst_ratio > 0 else 0.0

        out[pol] = {"by_category": by_cat,
                    "equal_category": equal_cat,
                    "freq_weighted": freq_w,
                    "base_categories": base_cats,
                    "extra_categories": extra_cats,
                    "inst_ratio": inst_ratio}
    return out


def format_table(agg, key="equal_category"):
    lines = []
    lines.append(f"{'policy':14s}{'倍率':>7s}{'SDC%':>8s}{'±':>6s}"
                 f"{'DUE%':>8s}{'±':>6s}{'检出恢复%':>10s}{'MWTF':>9s}")
    for pol, d in agg.items():
        e = d[key]
        lines.append(f"{pol:14s}{e['inst_ratio']:7.2f}{e['sdc']:8.2f}{e['sdc_ci95']:6.2f}"
                     f"{e['due']:8.2f}{e['due_ci95']:6.2f}{e['det_rec']:10.2f}"
                     f"{e['mwtf_total']:9.1f}")
    return "\n".join(lines)


def format_category_table(agg, policy):
    d = agg[policy]["by_category"]
    lines = [f"[{policy}] 分类别明细"]
    lines.append(f"  {'category':12s}{'SDC%':>8s}{'±':>6s}{'DUE%':>8s}"
                 f"{'检出恢复%':>10s}{'负载':>5s}{'试验':>7s}")
    for c, e in d.items():
        lines.append(f"  {c:12s}{e['sdc']:8.2f}{e['sdc_ci95']:6.2f}{e['due']:8.2f}"
                     f"{e['det_rec']:10.2f}{e['n_benchmarks']:5d}{e['trials']:7d}")
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser(description="按既定口径聚合故障注入结果")
    ap.add_argument("--fi", default=str(RES_DIR / "rq1_fault_injection_outcomes.csv"))
    ap.add_argument("--pmu", default=str(RES_DIR / "rq1_pmu_performance_qor.csv"))
    ap.add_argument("--meta", default=str(META_PATH))
    ap.add_argument("--json", default=None, help="同时写出 JSON")
    ap.add_argument("--policy-detail", default=None, help="打印该策略的分类别明细")
    args = ap.parse_args()

    meta = load_meta(args.meta)
    fi_rows = list(csv.DictReader(open(args.fi, encoding="utf8")))
    pmu_rows = list(csv.DictReader(open(args.pmu, encoding="utf8")))
    agg = aggregate(fi_rows, pmu_rows, meta)

    print("=== 七类等权（比较机制优劣）===")
    print(format_table(agg, "equal_category"))
    print("\n=== 按动态频率加权（估计真实程序级风险）===")
    print(format_table(agg, "freq_weighted"))
    if args.policy_detail:
        print()
        print(format_category_table(agg, args.policy_detail))
    if args.json:
        Path(args.json).write_text(json.dumps(agg, indent=2, ensure_ascii=False))
        print(f"\n-> {args.json}")


if __name__ == "__main__":
    main()
