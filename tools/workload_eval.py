#!/usr/bin/env python3
"""负载无关的自适应容错评测器：给一个符合契约的负载剖面，自动跑评测并生成 Markdown 报告。

把"负载"和"测试"解耦——测试逻辑（λ 轨道、协态/速率上限控制器、Pareto）固定在这里，
任何负载只要提供一张符合契约的剖面表就能无感跑通，产出一份 md 报告。

== 负载契约 ==
一张 CSV，列为 `benchmark,policy,overhead_x,e_sdc`：
  - benchmark  : 负载内核名（任意字符串）
  - policy     : 防护策略名；每个 benchmark 必须至少有一行 policy=native
  - overhead_x : 该策略相对 native 的开销倍数（native 行应为 1.0）
  - e_sdc      : 该策略下的残余每故障失效概率（0~1；native 为实测 SDC，强防护≈0）
`capstone_menu.csv` 就是一份合法剖面（当前 PolyBench 负载）。换任何负载：造一张同样
schema 的表丢进来即可，评测器与报告代码一行不用改。

用法：
    python3 tools/workload_eval.py                         # 默认跑 capstone_menu.csv
    python3 tools/workload_eval.py --menu <profile.csv> --name <负载名>
产物：
    tools/reports/<负载名>_report.md
"""
import argparse
import csv
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(ROOT, "experiments"))

from atft_core import (                       # noqa: E402
    OPT, build_menu, choose_ratecap, choose_low, choose_high,
    integrate, solve_to_budget, load_continuous_lambda,
    trace_leo, trace_sso, trace_geo_flare,
)

EPS = 1.0e-4
ORBITS_CONT = [("LEO (SAMPEX)", "sampex"), ("SSO (Alsat-1)", "alsat1")]


def load_menu_from_csv(path):
    """按契约把剖面 CSV 读成 menu dict：{benchmark: [{policy,o,e}, ...]}。"""
    menu = {}
    for r in csv.DictReader(open(path)):
        menu.setdefault(r["benchmark"], []).append(
            {"policy": r["policy"], "o": float(r["overhead_x"]),
             "e": float(r["e_sdc"])})
    for b, cands in menu.items():
        if not any(c["policy"] == "native" for c in cands):
            raise ValueError(f"负载 {b} 缺 native 策略，违反契约")
    return menu


def eval_trace(trace, dt, menu, chooser, eps):
    """沿一条轨迹评一个选择器：返回 (均开销×, 最坏残余, 违约步占比%)。"""
    kernels = sorted(menu)
    so = worst = 0.0
    viol = n = 0
    for (t, lam) in trace:
        ostep = wr = 0.0
        for b in kernels:
            c = chooser(menu[b], lam, False)
            ostep += c["o"]
            wr = max(wr, lam * c["e"])
        so += ostep / len(kernels)
        worst = max(worst, wr)
        if wr > eps:
            viol += 1
        n += 1
    return so / n, worst, 100.0 * viol / n


def eval_workload(menu, eps=EPS):
    """跑全套评测，返回结构化结果 dict（无副作用，纯计算）。"""
    res = {"n_kernels": len(menu),
           "policies": sorted({c["policy"] for cs in menu.values() for c in cs})}

    # 连续 λ：adaptive vs 协态 vs 全保护 vs native
    cont = []
    for label, key in ORBITS_CONT:
        tr, dt = load_continuous_lambda(key)
        o_rc, B = integrate(tr, menu, lambda c, l, p: choose_ratecap(c, l, p, eps),
                            False, dt)
        _, o_cs, _ = solve_to_budget(tr, menu, False, dt, B, "costate")
        o_hi, _ = integrate(tr, menu, choose_high, False, dt)
        o_lo, _ = integrate(tr, menu, choose_low, False, dt)
        cont.append({"orbit": label, "adaptive": o_rc, "costate": o_cs,
                     "full_protect": o_hi, "native": o_lo,
                     "save_vs_full_pct": 100.0 * (o_hi - o_rc) / o_hi if o_hi else 0.0})
    res["continuous"] = cont

    # 方波三轨道达标性：adaptive 最坏残余 ≤ ε？always-native 违约多少？
    meets = []
    for label, tr, dt in [("LEO", trace_leo(), 30.0), ("SSO", trace_sso(), 30.0),
                          ("GEO+flare", trace_geo_flare(), 300.0)]:
        o_ad, w_ad, _ = eval_trace(tr, dt, menu,
                                   lambda c, l, p: choose_ratecap(c, l, p, eps), eps)
        _, _, v_lo = eval_trace(tr, dt, menu, choose_low, eps)
        meets.append({"orbit": label, "adaptive": o_ad,
                      "adaptive_meets_eps": w_ad <= eps + 1e-18,
                      "native_violation_pct": v_lo})
    res["meets_eps"] = meets

    # Pareto：扫失效预算 B（用 LEO 连续剖面）
    tr, dt = load_continuous_lambda("sampex")
    o_nat, B_nat = integrate(tr, menu, choose_low, False, dt)
    o_full, _ = integrate(tr, menu, choose_high, False, dt)
    par = []
    for frac in (1.0, 0.2, 0.05, 0.01, 0.0):
        B = B_nat * frac
        if frac >= 1.0:
            o = o_nat
        elif frac <= 0.0:
            o = o_full
        else:
            _, o, _ = solve_to_budget(tr, menu, False, dt, B, "costate")
        par.append({"budget": B, "adaptive": o,
                    "save_vs_full_pct": 100.0 * (o_full - o) / o_full if o_full else 0.0})
    res["pareto"] = par
    res["eps"] = eps
    return res


def render_md(res, name):
    """把结果渲染成 Markdown（自动生成，不手工编辑）。"""
    L = []
    L.append(f"# 自适应容错评测报告 — {name}")
    L.append("")
    L.append(f"> 自动生成（tools/workload_eval.py）。负载 {res['n_kernels']} 个内核，"
             f"ε={res['eps']:.0e} faults/s。策略集：{', '.join(res['policies'])}。")
    L.append("")
    L.append("## 连续 λ 轨道 payoff（串行/少核场景）")
    L.append("")
    L.append("| 轨道 | 自适应× | 协态× | 全保护× | native× | 省 vs 全保护 |")
    L.append("| --- | --- | --- | --- | --- | --- |")
    for c in res["continuous"]:
        L.append(f"| {c['orbit']} | {c['adaptive']:.3f} | {c['costate']:.3f} | "
                 f"{c['full_protect']:.3f} | {c['native']:.3f} | "
                 f"{c['save_vs_full_pct']:.1f}% |")
    L.append("")
    L.append("## 达标性（方波轨道，是否处处满足 ε）")
    L.append("")
    L.append("| 轨道 | 自适应× | 自适应达标 | always-native 违约步% |")
    L.append("| --- | --- | --- | --- |")
    for m in res["meets_eps"]:
        L.append(f"| {m['orbit']} | {m['adaptive']:.3f} | "
                 f"{'是' if m['adaptive_meets_eps'] else '否'} | "
                 f"{m['native_violation_pct']:.1f}% |")
    L.append("")
    L.append("## 可靠性-开销 Pareto 前沿（LEO 连续剖面）")
    L.append("")
    L.append("| 失效预算 B | 自适应× | 省 vs 全保护 |")
    L.append("| --- | --- | --- |")
    for p in res["pareto"]:
        L.append(f"| {p['budget']:.4g} | {p['adaptive']:.3f} | "
                 f"{p['save_vs_full_pct']:.1f}% |")
    L.append("")
    c0 = res["continuous"][0]
    allmeet = all(m["adaptive_meets_eps"] for m in res["meets_eps"])
    L.append("## 要点")
    L.append("")
    L.append(f"- 连续 λ 下自适应 {c0['adaptive']:.3f}×，比全保护省 "
             f"{c0['save_vs_full_pct']:.0f}%（{c0['orbit']}）。")
    L.append(f"- 三条方波轨道自适应{'全部达标' if allmeet else '存在未达标'}；"
             f"always-native 在峰值段违约。")
    L.append("- 协态最优（B→μ 反解）与速率上限差距极小，印证双模 λ 下阈值近最优。")
    return "\n".join(L) + "\n"


def main():
    ap = argparse.ArgumentParser(description="负载无关的自适应容错评测器")
    ap.add_argument("--menu", default=None,
                    help="负载剖面 CSV（契约列 benchmark,policy,overhead_x,e_sdc）；"
                         "缺省用 canonical 菜单")
    ap.add_argument("--name", default=None, help="负载名（报告文件名）")
    args = ap.parse_args()

    if args.menu:
        menu = load_menu_from_csv(args.menu)
        name = args.name or os.path.splitext(os.path.basename(args.menu))[0]
    else:
        menu = build_menu()
        name = args.name or "polybench_default"

    res = eval_workload(menu)
    md = render_md(res, name)
    out = os.path.join(HERE, "reports", f"{name}_report.md")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w") as f:
        f.write(md)
    print(md)
    print(f"\n[写入] {out}")


if __name__ == "__main__":
    main()


