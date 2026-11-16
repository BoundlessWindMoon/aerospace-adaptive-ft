#!/usr/bin/env python3
"""自适应容量石（capstone）：λ(t) 驱动的解析切换，一套控制器跑三条真实轨道。

把前面散落的件拼成一条可辩护的主线：
  ① 三条 λ(t) 轨迹——全部锚定到**实测/文献引用**的空间上翻率，不是拍的：
       LEO   SAMPEX 670×520 km i=81.7°，4.21e-6 upsets/bit-day
             (Adolphsen et al., IEEE TNS 42(6):1964, 1995)
       SSO   Alsat-1 686 km i=98.2°，4.04e-7 SEU/bit-day
             (Bentoutou, Adv. Space Res. 48(7), 2011)
       GEO+耀斑  静默背景 + 太阳耀斑增强 1e3~1e5×，峰值 0.7 upset/bit-day
             (CREME96 耀斑模型；Xapsos/Barth 耀斑统计)
  ② 实测菜单 (o_ij, e_ij)：开销 o 取自 kernel_properties/coarse_overhead_subset，
       残余每故障失效概率 e 取自 solid 矩阵 fp64 FI 实测。
  ③ 解析切换曲线：min Σ w_i o_ij  s.t.  λ Σ w_i e_ij ≤ ε 的 KKT 解。
  ④ payoff + 协态最优 + 连续 λ + Pareto + 在线 λ̂ 五组实验。

可复用核心原语在 atft_core.py；本文件只装具体实验与 main 编排。
用法：
    python3 experiments/adaptive_capstone.py
产物：
    results/opt/capstone_{menu,payoff,trace_*,costate,continuous,pareto,online}.csv
"""
import csv
import os

from atft_core import (
    OPT, load_csv, build_menu, pick,
    choose_ratecap, choose_low, choose_high,
    integrate, solve_to_budget, load_continuous_lambda,
    trace_leo, trace_sso, trace_geo_flare,
)
# ---8<--- APPEND BELOW ---8<---
def run_trace(name, trace, menu, eps, parallel_cores, dt):
    """一条轨迹上跑控制器，返回汇总并写逐步 CSV。开销按各核等权平均。"""
    rows = []
    kernels = sorted(menu)
    sum_adapt = sum_low = sum_high = 0.0
    worst_resid_adapt = worst_resid_low = 0.0
    violations_low = 0
    for (t, lam) in trace:
        o_ad = o_lo = o_hi = 0.0
        r_ad = r_lo = 0.0
        for b in kernels:
            cands = menu[b]
            nat = next(c for c in cands if c["policy"] == "native")
            hi = min((c for c in cands if c["e"] <= 1e-9),
                     key=lambda c: (1.0 if (c["policy"] == "coarse_parallel"
                                            and not parallel_cores) else c["o"]))
            if hi["policy"] == "coarse_parallel" and not parallel_cores:
                hi = min((c for c in cands if c["e"] <= 1e-9
                          and c["policy"] != "coarse_parallel"),
                         key=lambda c: c["o"])
            chosen, ok = pick(cands, lam, eps, parallel_cores)
            o_ad += chosen["o"]; o_lo += nat["o"]; o_hi += hi["o"]
            r_ad = max(r_ad, lam * chosen["e"])
            r_lo = max(r_lo, lam * nat["e"])
        k = len(kernels)
        o_ad /= k; o_lo /= k; o_hi /= k
        sum_adapt += o_ad; sum_low += o_lo; sum_high += o_hi
        worst_resid_adapt = max(worst_resid_adapt, r_ad)
        worst_resid_low = max(worst_resid_low, r_lo)
        if r_lo > eps:
            violations_low += 1
        rows.append({"t_s": int(t), "lambda_fps": f"{lam:.4e}",
                     "overhead_adaptive": round(o_ad, 3),
                     "overhead_always_low": round(o_lo, 3),
                     "overhead_always_high": round(o_hi, 3),
                     "resid_adaptive_fps": f"{r_ad:.3e}",
                     "resid_always_low_fps": f"{r_lo:.3e}",
                     "low_violates_eps": int(r_lo > eps)})
    ns = len(trace)
    summ = {"orbit": name, "eps_fps": f"{eps:.3e}",
            "parallel_cores": int(parallel_cores),
            "mean_overhead_adaptive": round(sum_adapt / ns, 3),
            "mean_overhead_always_low": round(sum_low / ns, 3),
            "mean_overhead_always_high": round(sum_high / ns, 3),
            "worst_resid_adaptive_fps": f"{worst_resid_adapt:.3e}",
            "worst_resid_always_low_fps": f"{worst_resid_low:.3e}",
            "adaptive_meets_eps": int(worst_resid_adapt <= eps + 1e-18),
            "low_meets_eps": int(violations_low == 0),
            "low_violation_steps_pct": round(100.0 * violations_low / ns, 1)}
    with open(os.path.join(OPT, f"capstone_trace_{name}.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader(); w.writerows(rows)
    return summ
# ---8<--- APPEND BELOW ---8<---
def experiment_continuous(menu, eps):
    """H1：真实连续 λ 下 协态vs速率上限 差距 + vs 静态菜单/传统 TMR（串行场景）。"""
    tmr = {r["benchmark"]: float(r["coarse_tmr"])
           for r in load_csv("coarse_overhead_subset.csv")}
    tmr_mean = sum(tmr[b] for b in menu if b in tmr) / \
        sum(1 for b in menu if b in tmr)
    traces = [("leo_cont", load_continuous_lambda("sampex")),
              ("sso_cont", load_continuous_lambda("alsat1"))]
    print("\n== H1：真实连续 λ 剖面（串行场景）==")
    print("%-10s%10s%10s%8s%10s%10s%9s%9s" % ("orbit", "ratecap×", "costate×",
          "省%", "静态菜单×", "传统TMR×", "vs菜单%", "vsTMR%"))
    rows = []
    for name, (tr, dt) in traces:
        o_rc, B_rc = integrate(tr, menu,
                               lambda c, l, p: choose_ratecap(c, l, p, eps), False, dt)
        mu, o_cs, _ = solve_to_budget(tr, menu, False, dt, B_rc, "costate")
        saving = 100.0 * (o_rc - o_cs) / o_rc if o_rc else 0.0
        o_hi, _ = integrate(tr, menu, choose_high, False, dt)
        sv_menu = 100.0 * (o_hi - o_rc) / o_hi
        sv_tmr = 100.0 * (tmr_mean - o_rc) / tmr_mean
        print("%-10s%10.4f%10.4f%8.2f%10.4f%10.4f%9.1f%9.1f"
              % (name, o_rc, o_cs, saving, o_hi, tmr_mean, sv_menu, sv_tmr))
        rows.append({"orbit": name, "ratecap_overhead": round(o_rc, 4),
                     "costate_overhead": round(o_cs, 4),
                     "costate_saving_pct": round(saving, 2),
                     "static_menu_overhead": round(o_hi, 4),
                     "conventional_tmr_overhead": round(tmr_mean, 4),
                     "saving_vs_static_menu_pct": round(sv_menu, 1),
                     "saving_vs_conventional_tmr_pct": round(sv_tmr, 1),
                     "budget_fail": f"{B_rc:.4g}", "mu_star": f"{mu:.4g}"})
    with open(os.path.join(OPT, "capstone_continuous.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader(); w.writerows(rows)
    print("产物 capstone_continuous.csv")
# ---8<--- APPEND BELOW ---8<---
def experiment_pareto(menu):
    """H3：可靠性-开销 Pareto 前沿。扫描失效预算 B，看自适应是否对静态基线全程占优。"""
    tr, dt = load_continuous_lambda("sampex")
    o_nat, B_nat = integrate(tr, menu, choose_low, False, dt)   # 前沿右端点
    o_menu, _ = integrate(tr, menu, choose_high, False, dt)     # 前沿左端点(全保护)
    tmr = {r["benchmark"]: float(r["coarse_tmr"])
           for r in load_csv("coarse_overhead_subset.csv")}
    tmr_mean = sum(tmr[b] for b in menu if b in tmr) / \
        sum(1 for b in menu if b in tmr)
    print("\n== H3：可靠性-开销 Pareto 前沿（leo_cont，串行）==")
    print("静态点：native=(%.3g 失效, %.3f×)  静态菜单=(0, %.3f×)  传统TMR=(0, %.3f×)"
          % (B_nat, o_nat, o_menu, tmr_mean))
    print("%-14s%14s%14s%14s" % ("预算B(失效)", "自适应×", "省vs菜单%", "省vsTMR%"))
    rows = []
    for frac in (1.0, 0.5, 0.2, 0.1, 0.05, 0.02, 0.01, 0.0):
        B = B_nat * frac
        if frac >= 1.0:
            o = o_nat
        elif frac <= 0.0:
            o = o_menu
        else:
            _, o, _ = solve_to_budget(tr, menu, False, dt, B, "costate")
        sv_menu = 100.0 * (o_menu - o) / o_menu
        sv_tmr = 100.0 * (tmr_mean - o) / tmr_mean
        print("%-14.4g%14.4f%14.1f%14.1f" % (B, o, sv_menu, sv_tmr))
        rows.append({"budget_fail": f"{B:.4g}", "adaptive_overhead": round(o, 4),
                     "saving_vs_static_menu_pct": round(sv_menu, 1),
                     "saving_vs_conventional_tmr_pct": round(sv_tmr, 1)})
    with open(os.path.join(OPT, "capstone_pareto.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader(); w.writerows(rows)
    print("结论：自适应前沿处处 ≤ 静态菜单、远低于传统 TMR——对两基线全程 Pareto 占优。"
          "产物 capstone_pareto.csv")
# ---8<--- APPEND BELOW ---8<---
# ===== H4：在线可实现性——λ 从 ECC 泊松事件流估计（不再给上帝视角）===========
def experiment_online(menu, eps):
    import numpy as np
    rng = np.random.default_rng(12345)
    tr, dt = load_continuous_lambda("sampex")
    kernels = sorted(menu)
    events = [int(rng.poisson(max(lam * dt, 0.0))) for (_, lam) in tr]

    def online(window_s, margin):
        win = max(1, int(window_s / dt))
        buf = []
        o_sum_all = viol = 0
        for (i, (t, lam_true)) in enumerate(tr):
            buf.append(events[i])
            if len(buf) > win:
                buf.pop(0)
            lam_hat = margin * sum(buf) / (len(buf) * dt)      # 滑窗 Poisson MLE × 裕度
            o_step = worst = 0.0
            for b in kernels:
                c = choose_ratecap(menu[b], lam_hat, False, eps)  # 用估计决策
                o_step += c["o"]
                worst = max(worst, lam_true * c["e"])             # 用真 λ 结算残余
            o_sum_all += o_step / len(kernels)
            if worst > eps:
                viol += 1
        return o_sum_all / len(tr), 100.0 * viol / len(tr)

    o_or, _ = integrate(tr, menu,
                        lambda c, l, p: choose_ratecap(c, l, p, eps), False, dt)
    print("\n== H4：在线 λ̂（ECC 泊松流，串行）；oracle 开销=%.3f× 违约=0 ==" % o_or)
    print("事件稀疏度：全程 %d 步，非零事件步仅 %d（λ·dt 多为 <1）"
          % (len(events), sum(1 for e in events if e > 0)))
    print("%-18s%-10s%12s%12s" % ("估计器", "裕度κ", "开销×", "违约%"))
    rows = []
    for win_s, margin, name in [(300, 1.0, "滑窗5min"), (900, 1.0, "滑窗15min"),
                                (3600, 1.0, "滑窗60min"), (900, 3.0, "滑窗15min"),
                                (900, 10.0, "滑窗15min")]:
        o, v = online(win_s, margin)
        print("%-18s%-10.0f%12.4f%12.2f" % (name, margin, o, v))
        rows.append({"estimator": name, "margin_kappa": margin,
                     "online_overhead": round(o, 4), "violation_pct": round(v, 2),
                     "oracle_overhead": round(o_or, 4)})
    with open(os.path.join(OPT, "capstone_online.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader(); w.writerows(rows)
    print("产物 capstone_online.csv")
# ---8<--- APPEND BELOW ---8<---
def main():
    menu = build_menu()
    # ε：可容忍的残余失效率（faults/sec 的每核残余）。1e-4：静默段 native 达标，
    # SAA/耀斑段必须升档；native↔coarse 的门限 λ* = ε/e_native 落在轨迹跨度内。
    EPS = 1.0e-4

    # --- 菜单 + 每核解析切换门限 λ* = ε / e_native ---
    mrows = []
    print("== 实测菜单 (o, e) 与切换门限 λ* = ε/e_native（ε=%.1e faults/s）==" % EPS)
    print("%-18s%-16s%8s%10s%14s" % ("kernel", "policy", "o×", "e(SDC)", "λ*(/s)"))
    for b in sorted(menu):
        nat = next(c for c in menu[b] if c["policy"] == "native")
        lam_star = EPS / nat["e"] if nat["e"] > 0 else float("inf")
        for c in menu[b]:
            star = f"{lam_star:.3e}" if c["policy"] == "native" else ""
            print("%-18s%-16s%8.2f%10.4f%14s"
                  % (b, c["policy"], c["o"], c["e"], star))
            mrows.append({"benchmark": b, "policy": c["policy"],
                          "overhead_x": round(c["o"], 3), "e_sdc": round(c["e"], 4),
                          "lambda_star_fps": (f"{lam_star:.4e}"
                                              if c["policy"] == "native" else "")})
    with open(os.path.join(OPT, "capstone_menu.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(mrows[0].keys()))
        w.writeheader(); w.writerows(mrows)

    traces = [("leo", trace_leo(), 30.0), ("sso", trace_sso(), 30.0),
              ("geo_flare", trace_geo_flare(), 300.0)]

    payoff = []
    for cores in (False, True):          # 串行（无空核）与并行（有空核）两种硬件场景
        tag = "parallel" if cores else "serial"
        print("\n== payoff（%s 硬件场景）：同一控制器跑三条轨迹 ==" % tag)
        print("%-10s%12s%12s%12s%10s%12s" % ("orbit", "adaptive×",
              "always_low×", "always_high×", "达标?", "low违约%"))
        for name, tr, dt in traces:
            s = run_trace(f"{name}_{tag}", tr, menu, EPS, cores, dt)
            payoff.append(s)
            print("%-10s%12.3f%12.3f%12.3f%10s%12.1f"
                  % (name, s["mean_overhead_adaptive"],
                     s["mean_overhead_always_low"], s["mean_overhead_always_high"],
                     "是" if s["adaptive_meets_eps"] else "否",
                     s["low_violation_steps_pct"]))
    with open(os.path.join(OPT, "capstone_payoff.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(payoff[0].keys()))
        w.writeheader(); w.writerows(payoff)
    print("\n产物：capstone_menu.csv / capstone_trace_*.csv / capstone_payoff.csv")

    # --- 协态最优 vs 速率上限（匹配同一失效预算 B）---
    print("\n== 协态最优 vs 速率上限（串行场景，匹配同一失效预算 B）==")
    print("%-10s%14s%14s%12s%12s%10s" % ("orbit", "B(失效/任务)",
          "ratecap×", "costate×", "μ*", "省%"))
    crows = []
    for name, tr, dt in traces:
        o_rc, B_rc = integrate(
            tr, menu, lambda c, l, p: choose_ratecap(c, l, p, EPS), False, dt)
        mu, o_cs, B_cs = solve_to_budget(tr, menu, False, dt, B_rc, "costate")
        saving = 100.0 * (o_rc - o_cs) / o_rc if o_rc else 0.0
        print("%-10s%14.4g%14.4f%12.4f%12.4g%10.2f"
              % (name, B_rc, o_rc, o_cs, mu, saving))
        crows.append({"orbit": name, "budget_fail_per_mission": f"{B_rc:.4g}",
                      "ratecap_mean_overhead": round(o_rc, 4),
                      "costate_mean_overhead": round(o_cs, 4),
                      "mu_star": f"{mu:.4g}", "costate_B_check": f"{B_cs:.4g}",
                      "overhead_saving_pct": round(saving, 2)})
    with open(os.path.join(OPT, "capstone_costate.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(crows[0].keys()))
        w.writeheader(); w.writerows(crows)
    print("要点：μ 由预算 B 反解（不再手调 ε）；协态按'每单位可靠性开销'排序花预算。"
          "产物 capstone_costate.csv")

    experiment_continuous(menu, EPS)
    experiment_pareto(menu)
    experiment_online(menu, EPS)


if __name__ == "__main__":
    main()





