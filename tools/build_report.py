#!/usr/bin/env python3
"""统一报告装配器：把所有层面的测试结果汇成一个 Markdown，多张分层表格。

走这一套的边界：数据**生成**仍分散在各源（FI/PMU C 实测、少核扫描、自适应 Python
评测），各自落一份 canonical CSV；本工具只做**汇总消费**——读 canonical CSV + 现跑
自适应层，渲染成单一 md。换负载/补实验后重跑本工具即可刷新整份报告。

层次：
  1. 保护菜单 (o,e)         ← results/opt/capstone_menu.csv
  2. 策略级覆盖与开销        ← results/opt/fi_outcomes_a4_t200_matrix_solid.csv
  3. 少核下包络             ← results/opt/core_saturation_sweep.csv
  4. 自适应控制器 payoff     ← 现跑 workload_eval.eval_workload(build_menu())
  5. 在线可实现性 λ̂         ← results/opt/capstone_online.csv

用法：
    python3 tools/build_report.py              # → tools/reports/full_report.md
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, "experiments"))

from atft_core import build_menu, load_csv          # noqa: E402
from workload_eval import eval_workload              # noqa: E402


def fnum(x, d=3):
    try:
        return f"{float(x):.{d}f}"
    except (ValueError, TypeError):
        return str(x)


def sec_menu(menu):
    L = ["## 1. 保护菜单 (o, e)  — results/opt/capstone_menu.csv", "",
         "每核：native 开销/残余 + 最便宜的 e≈0 强防护策略。", "",
         "| 内核 | native× | native e(SDC) | 最便宜强防护 | 其× |",
         "| --- | --- | --- | --- | --- |"]
    for b in sorted(menu):
        nat = next(c for c in menu[b] if c["policy"] == "native")
        prot = [c for c in menu[b] if c["e"] <= 1e-9]
        best = min(prot, key=lambda c: c["o"]) if prot else nat
        L.append(f"| {b} | {nat['o']:.2f} | {nat['e']:.3f} | "
                 f"{best['policy']} | {best['o']:.2f} |")
    L.append("")
    return L


def sec_fi():
    try:
        rows = [r for r in load_csv("fi_outcomes_a4_t200_matrix_solid.csv")
                if r["policy"] != "policy" and float(r["trials"] or 0) > 0]
    except FileNotFoundError:
        return ["## 2. 策略级覆盖与开销  — (缺 fi_outcomes matrix_solid，跳过)", ""]
    agg = {}
    for r in rows:
        p = r["policy"]
        a = agg.setdefault(p, {"sdc": 0.0, "due": 0.0, "inst": 0.0,
                               "mwtf": 0.0, "n": 0})
        a["sdc"] += float(r["sdc_pct"]); a["due"] += float(r["due_pct"])
        a["inst"] += float(r["norm_hw_inst_ratio"])
        a["mwtf"] += float(r["mwtf_total"]); a["n"] += 1
    order = ["native", "swift", "abft_auto", "simd_tmr", "opt_simd_f64",
             "opt_layered", "exact-tmr"]
    L = ["## 2. 策略级覆盖与开销（solid FI 矩阵，均值over内核）"
         "  — results/opt/fi_outcomes_a4_t200_matrix_solid.csv", "",
         "| 策略 | 平均 SDC% | 平均 DUE% | 动态指令× | 平均 MWTF |",
         "| --- | --- | --- | --- | --- |"]
    for p in order + [q for q in agg if q not in order]:
        if p not in agg:
            continue
        a = agg[p]; n = a["n"]
        L.append(f"| {p} | {a['sdc']/n:.2f} | {a['due']/n:.2f} | "
                 f"{a['inst']/n:.2f} | {a['mwtf']/n:.1f} |")
    L.append("")
    return L


def sec_fewcore():
    try:
        rows = load_csv("core_saturation_sweep.csv")
    except FileNotFoundError:
        return ["## 3. 少核下包络  — (缺 core_saturation_sweep，跳过)", ""]
    L = ["## 3. 少核下包络（并行冗余需空闲核）  — results/opt/core_saturation_sweep.csv",
         "", "| 核数 | DMR 并行× | TMR 并行× |", "| --- | --- | --- |"]
    for r in rows:
        L.append(f"| {r['cores']} | {fnum(r['dmr_parallel_x'],2)} | "
                 f"{fnum(r['tmr_parallel_x'],2)} |")
    L.append("")
    return L


def sec_adaptive(res):
    L = ["## 4. 自适应控制器 payoff（现跑，连续 λ 真实轨道）", ""]
    L.append("| 轨道 | 自适应× | 协态× | 全保护× | native× | 省 vs 全保护 |")
    L.append("| --- | --- | --- | --- | --- | --- |")
    for c in res["continuous"]:
        L.append(f"| {c['orbit']} | {c['adaptive']:.3f} | {c['costate']:.3f} | "
                 f"{c['full_protect']:.3f} | {c['native']:.3f} | "
                 f"{c['save_vs_full_pct']:.1f}% |")
    L += ["", "达标性（方波轨道）：", "",
          "| 轨道 | 自适应× | 达标 | always-native 违约% |",
          "| --- | --- | --- | --- |"]
    for m in res["meets_eps"]:
        L.append(f"| {m['orbit']} | {m['adaptive']:.3f} | "
                 f"{'是' if m['adaptive_meets_eps'] else '否'} | "
                 f"{m['native_violation_pct']:.1f}% |")
    L += ["", "可靠性-开销 Pareto 前沿（LEO 连续剖面）：", "",
          "| 失效预算 B | 自适应× | 省 vs 全保护 |", "| --- | --- | --- |"]
    for p in res["pareto"]:
        L.append(f"| {p['budget']:.4g} | {p['adaptive']:.3f} | "
                 f"{p['save_vs_full_pct']:.1f}% |")
    L.append("")
    return L


def sec_online():
    try:
        rows = load_csv("capstone_online.csv")
    except FileNotFoundError:
        return ["## 5. 在线可实现性 λ̂  — (缺 capstone_online，跳过)", ""]
    L = ["## 5. 在线可实现性：λ 从 ECC 泊松流估计  — results/opt/capstone_online.csv",
         "", "| 估计器 | 裕度κ | 开销× | 违约% |", "| --- | --- | --- | --- |"]
    for r in rows:
        L.append(f"| {r['estimator']} | {fnum(r['margin_kappa'],0)} | "
                 f"{fnum(r['online_overhead'])} | {fnum(r['violation_pct'],2)} |")
    L.append("")
    return L


def main():
    menu = build_menu()
    res = eval_workload(menu)
    c0 = res["continuous"][0]
    allmeet = all(m["adaptive_meets_eps"] for m in res["meets_eps"])

    L = ["# 自适应容错框架 — 统一评测报告", "",
         "> 自动生成（tools/build_report.py）。生成数据分散在各源（见各节标题的 CSV），"
         "本报告为统一汇总；换负载/补实验后重跑本工具刷新。", "",
         "## 要点", "",
         f"- 连续 λ 真实轨道下自适应 {c0['adaptive']:.3f}×，比全保护省 "
         f"{c0['save_vs_full_pct']:.0f}%（{c0['orbit']}）。",
         f"- 三条方波轨道自适应{'全部达标' if allmeet else '存在未达标'}；"
         "always-native 在峰值段违约。",
         "- 并行冗余 ~1× 仅当空闲核≥副本数；少核回落到 2–3×（见第 3 节）。", ""]
    L += sec_menu(menu)
    L += sec_fi()
    L += sec_fewcore()
    L += sec_adaptive(res)
    L += sec_online()

    out = os.path.join(HERE, "reports", "full_report.md")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w") as f:
        f.write("\n".join(L) + "\n")
    print("\n".join(L))
    print(f"\n[写入] {out}")


if __name__ == "__main__":
    main()


