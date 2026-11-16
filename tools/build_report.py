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


def desc(what, metrics, src):
    """实验说明块：做什么 + 指标含义 + 数据源，用缩进列表呈现。"""
    L = [f"- 做什么：{what}", "- 指标："]
    for m in metrics:
        L.append(f"    - {m}")
    L.append(f"- 数据源：`{src}`")
    L.append("")
    return L


def sec_menu(menu):
    L = ["## 1. 保护菜单", ""]
    L += desc("每核在各防护策略下的开销与残余失效概率；此处为并行场景，强防护多为 1×，"
              "串行下的档位差异见第 4 节",
              ["native× — 无防护基准，恒为 1",
               "e(SDC) — native 的残余每故障失效概率，取实测 fp64 SDC",
               "最便宜强防护 / 其× — 使 e≈0 的最便宜策略及其开销"],
              "results/opt/capstone_menu.csv")
    L += ["| 内核 | native× | e(SDC) | 最便宜强防护 | 其× |",
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
        return ["## 2. 策略级覆盖与开销", "", "缺 fi_outcomes matrix_solid，跳过。", ""]
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
    L = ["## 2. 策略级覆盖与开销", ""]
    L += desc("单比特故障注入下各防护策略的覆盖与开销，按内核取均值",
              ["SDC% — 静默数据损坏率，越低越好",
               "DUE% — 检测到但不可恢复（崩溃/挂起）率",
               "动态指令× — 相对 native 的动态指令膨胀",
               "MWTF — 平均无失效工作量，越高越好"],
              "results/opt/fi_outcomes_a4_t200_matrix_solid.csv")
    L += ["| 策略 | SDC% | DUE% | 动态指令× | MWTF |",
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
        return ["## 3. 少核下包络", "", "缺 core_saturation_sweep，跳过。", ""]
    L = ["## 3. 少核下包络", ""]
    L += desc("粗粒度冗余副本并行放不同核数时的墙钟开销；并行回到 ~1× 要求空闲核≥副本数",
              ["DMR 并行× — 双模冗余（2 副本）的墙钟开销",
               "TMR 并行× — 三模冗余（3 副本）的墙钟开销"],
              "results/opt/core_saturation_sweep.csv")
    L += ["| 核数 | DMR 并行× | TMR 并行× |", "| --- | --- | --- |"]
    for r in rows:
        L.append(f"| {r['cores']} | {fnum(r['dmr_parallel_x'],2)} | "
                 f"{fnum(r['tmr_parallel_x'],2)} |")
    L.append("")
    return L


def sec_adaptive(res):
    L = ["## 4. 自适应控制器 payoff", ""]
    L += desc("λ(t) 驱动的控制器在真实轨道上的开销，对照全保护与 native；ε=1e-4 faults/s",
              ["自适应× — 控制器平均开销",
               "协态× — 由失效预算 B 反解乘子 μ 的最优解",
               "全保护× / native× — 两个静态基线（全程强防护 / 全程不防护）",
               "达标 — 自适应最坏残余失效率是否 ≤ ε",
               "违约% — always-native 超 ε 的时间步占比",
               "失效预算 B — 一次任务允许的期望失效次数上限"],
              "现跑 workload_eval.eval_workload(build_menu())")
    L += ["### 连续 λ 真实轨道",
          "| 轨道 | 自适应× | 协态× | 全保护× | native× | 省 vs 全保护 |",
          "| --- | --- | --- | --- | --- | --- |"]
    for c in res["continuous"]:
        L.append(f"| {c['orbit']} | {c['adaptive']:.3f} | {c['costate']:.3f} | "
                 f"{c['full_protect']:.3f} | {c['native']:.3f} | "
                 f"{c['save_vs_full_pct']:.1f}% |")
    L += ["", "### 达标性（方波轨道）",
          "| 轨道 | 自适应× | 达标 | always-native 违约% |",
          "| --- | --- | --- | --- |"]
    for m in res["meets_eps"]:
        L.append(f"| {m['orbit']} | {m['adaptive']:.3f} | "
                 f"{'是' if m['adaptive_meets_eps'] else '否'} | "
                 f"{m['native_violation_pct']:.1f}% |")
    L += ["", "### 可靠性-开销 Pareto 前沿",
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
        return ["## 5. 在线可实现性", "", "缺 capstone_online，跳过。", ""]
    L = ["## 5. 在线可实现性", ""]
    L += desc("λ 不给真值、改从 ECC 泊松事件流在线估计时的开销与违约",
              ["裕度κ — 估计值的安全放大因子",
               "开销× — 在线控制器平均开销",
               "违约% — 真实残余超 ε 的时间步占比"],
              "results/opt/capstone_online.csv")
    L += ["| 估计器 | 裕度κ | 开销× | 违约% |", "| --- | --- | --- | --- |"]
    for r in rows:
        L.append(f"| {r['estimator']} | {fnum(r['margin_kappa'],0)} | "
                 f"{fnum(r['online_overhead'])} | {fnum(r['violation_pct'],2)} |")
    L.append("")
    return L


def main():
    menu = build_menu()
    res = eval_workload(menu)

    L = ["# 自适应容错框架 — 统一评测报告", ""]
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


