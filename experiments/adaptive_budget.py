#!/usr/bin/env python3
"""自适应层的失效预算：把检测延迟、传感器增益、回落滞后写成闭式

口径与 experiments/temporal.py 一致（Ostler/Poisson 链条）：
  - 每故障失效概率 q = P(F|1)，来自 rq6 的 Temporal_Burst k=1 实测点
  - 速率 lambda 来自 rq9_temporal_mttf.csv 的 sampex_saa_in / sampex_saa_out（4 MiB）
  - 轨道剖面用 rq9 实际在用的那一行（由 orbit_avg = f*lam_in + (1-f)*lam_out 反解确认）

校验：把 LOW=HIGH=native 代进去应当复现冻结报告的"每 90 天 1683 次失效"。

用法：
    python3 experiments/adaptive_budget.py
    python3 experiments/adaptive_budget.py --switch-csv results/opt/switch_sweep.csv
"""

import argparse
import csv
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# --- 轨道与速率（4 MiB 故障空间，sampex，r=200）----------------------------
LAM_SAA_IN = 0.00862015      # 故障/秒
LAM_SAA_OUT = 4.31008e-05
PERIOD_MIN = 96.58
MISSION_DAYS = 90.0


def orbit_from_profile(path=None):
    """用 orbit_avg = f*lam_in + (1-f)*lam_out 反解 rq9 实际用的那一行。

    这一步是必要的：METHOD.md 的占空比段落用的是 b=20000 那一行（4.41% / 8.29 min /
    7.67 次每天），而 rq9 的 MTTF 表用的是 b=26000（18.56% / 13.1 min / 20.3 次每天）。
    两者不能混，闭式必须跟 MTTF 表对齐，否则校验不上 1683。
    """
    path = path or os.path.join(ROOT, "results", "rq9_orbit_profile.csv")
    lam_avg = 0.001635
    f_target = (lam_avg - LAM_SAA_OUT) / (LAM_SAA_IN - LAM_SAA_OUT)
    best = None
    for r in csv.DictReader(open(path)):
        if r["orbit"] != "sampex" or not r["dwell_mean_min"]:
            continue
        try:
            f = float(r["saa_fraction"])
            d = float(r["dwell_mean_min"])
            n = float(r["passes_per_day"])
        except ValueError:
            continue
        if d <= 0 or n <= 0:
            continue
        err = abs(f - f_target)
        if best is None or err < best[0]:
            best = (err, {"saa_fraction": f, "dwell_min": d, "passes_per_day": n,
                          "b_nt": r["b_threshold_nt"]})
    return f_target, best[1]


def load_pfn_k1(path=None):
    """每故障失效概率 q = P(F|1)，直接取 rq6 的 k=1 实测点。

    opt_i64 的 k=1 是零失效样本（0.00%），所以 HIGH 档只能给上界：取 k=2 的
    0.01% 作为保守代入值，并把它标出来 —— 不能当成测到的 0。
    """
    path = path or os.path.join(ROOT, "results", "opt", "rq6_opt_burst_4k.csv")
    out = {}
    for r in csv.DictReader(open(path)):
        if r["stress_type"] != "Temporal_Burst":
            continue
        k = r["stress_level"].split()[0].replace("k=", "")
        out.setdefault(r["policy"], {})[int(k)] = float(r["sdc_pct"]) / 100.0
    q = {}
    for pol, curve in out.items():
        q1 = curve.get(1, 0.0)
        q[pol] = {"q": q1 if q1 > 0 else curve.get(2, 0.0),
                  "is_bound": q1 <= 0.0, "k1": q1, "k2": curve.get(2, 0.0)}
    return q


def budget(orbit, q_low, q_high, p_det, delay_min, recov_min,
           c_low=1.0, c_high=8.25):
    """一个轨道周期内的失效期望与期望开销。

    LOW 暴露窗口 W = p_det * delay + (1 - p_det) * dwell：检出了就只暴露判决延迟，
    没检出就整个过境都在 LOW。这一项乘的是 SAA 段速率 lambda1 = r * lambda0，
    所以它不随 r 变小 —— 这是"自适应能省多少"的硬上界来源。
    """
    D = orbit["dwell_min"]
    quiet_per_orbit = PERIOD_MIN - D
    # 回落滞后被平静段长度截断：滞后 >= 平静段就等于从不回到 LOW
    high_in_quiet = min(recov_min, quiet_per_orbit) if p_det > 0 else 0.0
    high_in_quiet *= p_det

    W_low = p_det * delay_min + (1.0 - p_det) * D          # SAA 段处于 LOW 的时长
    W_high = D - W_low                                      # SAA 段处于 HIGH 的时长

    f_saa = 60.0 * LAM_SAA_IN
    f_quiet = 60.0 * LAM_SAA_OUT                            # 故障/分钟
    fail_per_orbit = (f_saa * (W_low * q_low + W_high * q_high)
                      + f_quiet * (high_in_quiet * q_high
                                   + (quiet_per_orbit - high_in_quiet) * q_low))

    orbits_per_day = orbit["passes_per_day"]                 # 一次过境算一个周期
    fail_mission = fail_per_orbit * orbits_per_day * MISSION_DAYS

    t_high = W_high + high_in_quiet
    phi_high = t_high / PERIOD_MIN
    cost = phi_high * c_high + (1.0 - phi_high) * c_low
    return {"fail_mission": fail_mission, "phi_high": phi_high, "cost": cost,
            "W_low_min": W_low, "high_in_quiet_min": high_in_quiet,
            "quiet_per_orbit_min": quiet_per_orbit}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="results/opt/adaptive_budget")
    args = ap.parse_args()

    f_target, orb = orbit_from_profile()
    q = load_pfn_k1()
    print("== 轨道剖面（由 orbit_avg 反解）==")
    print("  f_target=%.4f -> b=%s nT, saa_fraction=%.4f, dwell=%.1f min, "
          "passes/day=%.1f" % (f_target, orb["b_nt"], orb["saa_fraction"],
                               orb["dwell_min"], orb["passes_per_day"]))
    print("  注意：METHOD.md 的占空比段落用的是 4.41% / 8.29min / 7.67次，"
          "与此不是同一行")

    print("\n== 每故障失效概率 q = P(F|1)（rq6 k=1）==")
    for pol, v in sorted(q.items()):
        print("  %-14s q=%.5f %s" % (pol, v["q"],
              "(k=1 是零失效样本，这里代入 k=2 的上界 %.5f)" % v["k2"]
              if v["is_bound"] else ""))

    qN = q["native"]["q"]
    qH = q["opt_i64"]["q"]

    # --- 校验：全程 native，p_det=0 -> 应当复现 1683 ---
    chk = budget(orb, qN, qN, 0.0, 0.0, 0.0, c_low=1.0, c_high=1.0)
    print("\n== 校验：全程无保护 ==")
    print("  闭式给出每 %g 天 %.1f 次失效；冻结报告是 1683" %
          (MISSION_DAYS, chk["fail_mission"]))

    always_high = budget(orb, qH, qH, 1.0, 0.0, PERIOD_MIN,
                         c_low=8.25, c_high=8.25)
    print("  全程保护（q 用 k=2 上界）：%.2f 次 —— 冻结报告是 <1.53"
          % always_high["fail_mission"])

    # --- 自适应：传感器增益扫描，检测器参数来自蒙特卡洛实测 ---
    print("\n== 自适应（LOW=无保护，HIGH=全开+i64，r=100 的 SPRT α=β=0.01）==")
    rows = []
    for g, kappa, p_det, delay, recov in [
            (1.000, "-",    0.9588, 3.563, 108.556),
            (0.9925, "0.1", 0.9578, 3.581, 107.906),
            (0.7875, "0.25", 0.8880, 4.120, 133.807),
            (0.680, "32",   0.8288, 4.477, 154.048)]:
        b = budget(orb, qN, qH, p_det, delay, recov)
        ratio_fail = b["fail_mission"] / always_high["fail_mission"]
        ratio_cost = always_high["cost"] / b["cost"]
        rows.append({"sensor_gain_g": g, "kappa": kappa, "p_detect": p_det,
                     "delay_min": delay, "recovery_lag_min": recov,
                     "quiet_per_orbit_min": round(b["quiet_per_orbit_min"], 2),
                     "high_in_quiet_min": round(b["high_in_quiet_min"], 2),
                     "low_exposure_min": round(b["W_low_min"], 3),
                     "phi_high": round(b["phi_high"], 4),
                     "exp_cost_x": round(b["cost"], 3),
                     "fail_per_90d": round(b["fail_mission"], 2),
                     "fail_vs_always_high": round(ratio_fail, 1),
                     "cost_saving_x": round(ratio_cost, 3),
                     "mwtf_ratio_vs_always_high": round(ratio_cost / ratio_fail, 4)})
        print("  g=%-6.4f (κ=%-4s) 回落滞后 %.1f min vs 平静段 %.1f min -> "
              "HIGH 占空 %.1f%%，期望开销 %.2f×，90 天失效 %.1f（全程保护的 %.0f 倍），"
              "MWTF 比 %.3f"
              % (g, kappa, recov, b["quiet_per_orbit_min"], 100 * b["phi_high"],
                 b["cost"], b["fail_mission"], ratio_fail,
                 ratio_cost / ratio_fail))

    # --- 两个独立杠杆：回落速度 与 LOW 档的可靠性 ---
    print("\n== 杠杆一：回落滞后决定占空收益的上限（LOW 可靠性暂设与 HIGH 相同）==")
    lever = []
    for recov, name in [(154.0, "实测 κ=32"), (108.6, "实测 g=1"),
                        (40.0, "半个平静段"), (10.0, "时间驱动降档"),
                        (0.0, "星历驱动（出 SAA 即降档）")]:
        b = budget(orb, qH, qH, 0.9588, 3.563, recov)
        lever.append({"recovery_lag_min": recov, "scenario": name,
                      "phi_high": round(b["phi_high"], 4),
                      "exp_cost_x": round(b["cost"], 3),
                      "saving_vs_always_high_x": round(always_high["cost"] / b["cost"], 3)})
        print("  回落 %6.1f min（%s）-> HIGH 占空 %5.1f%%，期望开销 %.2f×，"
              "相对全程保护省 %.2f×"
              % (recov, name, 100 * b["phi_high"], b["cost"],
                 always_high["cost"] / b["cost"]))

    print("\n== 杠杆二：LOW 档的每故障失效概率要低到多少，自适应才不亏 ==")
    brk = []
    for recov, name in [(154.0, "实测 κ=32"), (10.0, "时间驱动降档"),
                        (0.0, "星历驱动")]:
        lo, hi = qH, 1.0
        for _ in range(200):
            mid = 0.5 * (lo + hi)
            b = budget(orb, mid, qH, 0.9588, 3.563, recov)
            m = (always_high["cost"] / b["cost"]) / (b["fail_mission"]
                                                     / always_high["fail_mission"])
            if m > 1.0:
                lo = mid
            else:
                hi = mid
        b = budget(orb, lo, qH, 0.9588, 3.563, recov)
        brk.append({"scenario": name, "recovery_lag_min": recov,
                    "q_low_breakeven": lo, "ratio_vs_q_high": lo / qH,
                    "exp_cost_x": round(b["cost"], 3)})
        print("  %s：q_low 必须 <= %.3g（= q_high 的 %.1f 倍），"
              "此时期望开销 %.2f×"
              % (name, lo, lo / qH, b["cost"]))
    print("  对照：无保护 q=%.4f，所以无保护档差了 %.0f~%.0f 倍"
          % (qN, qN / brk[0]["q_low_breakeven"], qN / brk[-1]["q_low_breakeven"]))

    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    with open(args.out + ".csv", "w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=list(rows[0].keys()))
        w.writeheader()
        w.writerows(rows)
    with open(args.out + ".json", "w") as fh:
        json.dump({"orbit": orb, "q": q, "check_unprotected_90d":
                   chk["fail_mission"], "always_high_90d":
                   always_high["fail_mission"], "sweep": rows,
                   "lever_recovery": lever, "lever_breakeven": brk}, fh,
                  indent=2, ensure_ascii=False)
    print("\n写入 %s.csv / %s.json" % (args.out, args.out))


if __name__ == "__main__":
    main()


