#!/usr/bin/env python3
"""在线 λ̂ 违约率的解析预测（无需重跑事件仿真）。

模型：事件流 N(t) ~ Poisson(μ_w(t))，μ_w(t)=滑窗内 λ_true·dt 之和（轨迹给定后是确定量）。
λ̂ = κ·N/T_w_eff → 逐核 choose_ratecap → 违约 iff max_k λ_true·e(c_k) > ε。
对每个 N 枚举泊松 pmf 加权，得逐步违约概率 P_viol(t)，全程平均即期望违约率。

对账对象：results/opt/capstone_online.csv（事件仿真实测）。
产物：results/opt/predict_online_violation.csv
"""
import csv
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from atft_core import load_continuous_lambda, choose_ratecap  # noqa: E402
from adaptive_capstone import build_menu  # noqa: E402

OPT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results", "opt")
EPS = 1.0e-4


def poisson_pmf(mu, n):
    if mu <= 0:
        return 1.0 if n == 0 else 0.0
    return math.exp(-mu + n * math.log(mu) - math.lgamma(n + 1))


def predict(menu, tr, dt, window_s, margin):
    win = max(1, int(window_s / dt))
    kernels = sorted(menu)
    p_viol_sum = 0.0
    o_sum = 0.0
    for i, (_, lam_true) in enumerate(tr):
        lo = max(0, i - win + 1)
        mu_w = sum(l for (_, l) in tr[lo:i + 1]) * dt
        t_eff = (i - lo + 1) * dt
        # 违约指示只随 λ̂=κN/t_eff 变化；λ̂ 大到一个界后选择不再变（全部升档），截断枚举
        p_step = 0.0
        o_step = 0.0
        p_cum = 0.0
        n = 0
        n_max = max(50, int(mu_w + 12 * math.sqrt(max(mu_w, 1.0))))
        worst_at_big_n = None
        while n <= n_max:
            p = poisson_pmf(mu_w, n)
            lam_hat = margin * n / t_eff
            worst = 0.0
            o_k = 0.0
            for b in kernels:
                c = choose_ratecap(menu[b], lam_hat, False, EPS)
                worst = max(worst, lam_true * c["e"])
                o_k += c["o"]
            p_step += p * (1.0 if worst > EPS else 0.0)
            o_step += p * o_k / len(kernels)
            p_cum += p
            worst_at_big_n = worst
            n += 1
        tail = max(0.0, 1.0 - p_cum)          # 截断尾部：按 N>n_max 的选择（已全升档）处理
        p_step += tail * (1.0 if worst_at_big_n > EPS else 0.0)
        o_step += tail * o_k / len(kernels)
        p_viol_sum += p_step
        o_sum += o_step
    return o_sum / len(tr), 100.0 * p_viol_sum / len(tr)


def main():
    menu = build_menu()
    tr, dt = load_continuous_lambda("sampex")
    print("%-14s%-6s%14s%14s" % ("滑窗", "κ", "预测开销×", "预测违约%"))
    rows = []
    for win_s, margin, name in [(300, 1.0, "滑窗5min"), (900, 1.0, "滑窗15min"),
                                (3600, 1.0, "滑窗60min"), (900, 3.0, "滑窗15min"),
                                (900, 10.0, "滑窗15min")]:
        o, v = predict(menu, tr, dt, win_s, margin)
        print("%-14s%-6.0f%14.4f%14.2f" % (name, margin, o, v))
        rows.append({"estimator": name, "margin_kappa": margin,
                     "pred_overhead": round(o, 4), "pred_violation_pct": round(v, 2)})
    out = os.path.join(OPT, "predict_online_violation.csv")
    with open(out, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader()
        w.writerows(rows)
    print("产物 %s（与 capstone_online.csv 实测对账）" % out)


if __name__ == "__main__":
    main()
