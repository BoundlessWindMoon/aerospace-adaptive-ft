#!/usr/bin/env python3
"""离线探针：只读 results/ 下已冻结的数据，不重跑任何实验。

三个问题，各自对应一个优化方向的前置判决：
  A (D3)  逐负载阈值的收益上界：用 rq3 扫描数据算 oracle-δ，与全局 5e-5 比。
  B (D5)  关键性评分与实测 SDC 的秩相关：评分排序到底有没有信号。
  C (D6)  变点检测在过境窗口内是否统计可行：SPRT 判决所需事件数 vs 窗口期望事件数。

产物写入 results/offline/，不覆盖任何已有文件。
"""

import csv
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
RES = ROOT / "results"
OUT = RES / "offline"


def read(name):
    with open(RES / name) as f:
        return list(csv.DictReader(f))


# ---------------------------------------------------------------- A (D3)
def probe_d3(fa_cap=1.0):
    """每个负载在 δ 扫描网格上选 oracle：误报 ≤ fa_cap% 前提下恢复率最大。"""
    rows = read("rq3_delta_threshold_sensitivity.csv")
    by_bench = {}
    for r in rows:
        by_bench.setdefault(r["benchmark"], []).append(
            {
                "delta": float(r["delta"]),
                "fa": float(r["false_alarm_pct_no_fault"]),
                "err": float(r["no_fault_rel_l2_err"]),
                "rec": float(r["fi_det_recovered_pct"]),
                "sdc": float(r["fi_sdc_pct"]),
            }
        )
    recs, out = [], []
    for b, rs in sorted(by_bench.items()):
        glob = next(r for r in rs if abs(r["delta"] - 5e-5) < 1e-12)
        feas = [r for r in rs if r["fa"] <= fa_cap]
        best = max(feas, key=lambda r: (r["rec"], -r["delta"])) if feas else None
        # 固有近似误差的下界估计：最大的「仍然报误报」的 δ 说明 |R-A| 超过它
        fa_deltas = [r["delta"] for r in rs if r["fa"] > 0]
        e_lb = max(fa_deltas) if fa_deltas else 0.0
        e_obs = max(r["err"] for r in rs)
        out.append(
            {
                "benchmark": b,
                "global_delta_fa": glob["fa"],
                "global_delta_rec": glob["rec"],
                "global_delta_sdc": glob["sdc"],
                "oracle_delta": best["delta"] if best else None,
                "oracle_fa": best["fa"] if best else None,
                "oracle_rec": best["rec"] if best else None,
                "oracle_sdc": best["sdc"] if best else None,
                "inherent_err_lb": e_lb,
                "inherent_err_obs": e_obs,
                "alpha_implied": (best["delta"] / e_obs) if (best and e_obs > 0) else None,
            }
        )
        recs.append((glob, best))
    agg = {
        "fa_cap_pct": fa_cap,
        "n_bench": len(recs),
        "global_fa": sum(g["fa"] for g, _ in recs) / len(recs),
        "global_rec": sum(g["rec"] for g, _ in recs) / len(recs),
        "global_sdc": sum(g["sdc"] for g, _ in recs) / len(recs),
        "oracle_fa": sum(b["fa"] for _, b in recs if b) / len(recs),
        "oracle_rec": sum(b["rec"] for _, b in recs if b) / len(recs),
        "oracle_sdc": sum(b["sdc"] for _, b in recs if b) / len(recs),
        "n_bench_delta_differs": sum(1 for g, b in recs if b and b["delta"] != g["delta"]),
    }
    return out, agg


# ---------------------------------------------------------------- B (D5)
def _spearman(xs, ys):
    def rank(v):
        order = sorted(range(len(v)), key=lambda i: v[i])
        r = [0.0] * len(v)
        i = 0
        while i < len(order):
            j = i
            while j + 1 < len(order) and v[order[j + 1]] == v[order[i]]:
                j += 1
            avg = (i + j) / 2.0 + 1
            for k in range(i, j + 1):
                r[order[k]] = avg
            i = j + 1
        return r

    rx, ry = rank(xs), rank(ys)
    n = len(xs)
    mx, my = sum(rx) / n, sum(ry) / n
    num = sum((a - mx) * (b - my) for a, b in zip(rx, ry))
    den = math.sqrt(sum((a - mx) ** 2 for a in rx) * sum((b - my) ** 2 for b in ry))
    if den == 0:
        return None, None
    rho = num / den
    t = rho * math.sqrt(max(n - 2, 1) / max(1e-12, 1 - rho * rho))
    return rho, t


def probe_d5():
    """逐负载：关键性评分（bench_* 函数）vs 无保护实测 SDC。"""
    crit = {r["function"]: r for r in read("criticality_analysis.csv")}
    out_rows = read("rq1_fault_injection_outcomes.csv")
    sdc, dyn = {}, {}
    for r in out_rows:
        if r["policy"] != "native":
            continue
        b = r["benchmark"]
        n = float(r["dyn_target_insts"])
        sdc[b] = sdc.get(b, 0.0) + float(r["sdc_pct"]) * n
        dyn[b] = dyn.get(b, 0.0) + n
    pairs = []
    for b in sorted(sdc):
        fn = crit.get("bench_" + b)
        if not fn or dyn[b] == 0:
            continue
        pairs.append(
            {
                "benchmark": b,
                "score": float(fn["score"]),
                "level": fn["criticality_level"],
                "native_sdc_dynweighted": sdc[b] / dyn[b],
                "loop_depth": int(fn["max_loop_depth"]),
                "out_closeness": float(fn["avg_output_closeness"]),
                "liveness": float(fn["avg_liveness"]),
            }
        )
    res = {"n": len(pairs)}
    for key in ("score", "loop_depth", "out_closeness", "liveness"):
        rho, t = _spearman([p[key] for p in pairs], [p["native_sdc_dynweighted"] for p in pairs])
        res[key] = {"spearman": rho, "t_stat": t}
    return pairs, res


# ---------------------------------------------------------------- C (D6)
def probe_d6(expected_events_in_window=9.0, dwell_min=8.29):
    """Poisson SPRT：λ0 -> λ1 = r·λ0，α=β 给定时判决所需事件数。

    log Λ = n·log r − (λ1−λ0)·t；在真实速率 λ1 下 t ≈ n/λ1，于是
    每个事件贡献 log r − (r−1)/r，n_needed = log((1−β)/α) / 该增量。
    """
    rows = []
    for alpha in (0.01, 0.05, 0.1):
        bound = math.log((1 - alpha) / alpha)
        for r in (2.0, 4.0, 8.0, 16.0, 25.0, 100.0, 200.0, 450.0):
            gain = math.log(r) - (r - 1) / r
            n_need = bound / gain
            rows.append(
                {
                    "alpha_beta": alpha,
                    "rate_ratio": r,
                    "per_event_llr": gain,
                    "events_to_decide": n_need,
                    "events_available_in_window": expected_events_in_window,
                    "feasible": n_need <= expected_events_in_window,
                    "detect_delay_min_frac_of_dwell": n_need / expected_events_in_window,
                    "detect_delay_min": dwell_min * n_need / expected_events_in_window,
                }
            )
    return rows


def dump_csv(name, rows):
    if not rows:
        return
    with open(OUT / name, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader()
        w.writerows(rows)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    d3_rows, d3_agg = probe_d3(fa_cap=1.0)
    d3_rows_strict, d3_agg_strict = probe_d3(fa_cap=0.0)
    d5_pairs, d5_res = probe_d5()
    d6_rows = probe_d6()
    dump_csv("d3_oracle_delta.csv", d3_rows)
    dump_csv("d5_score_vs_sdc.csv", d5_pairs)
    dump_csv("d6_sprt_feasibility.csv", d6_rows)
    summary = {
        "d3_oracle_fa_cap_1pct": d3_agg,
        "d3_oracle_fa_cap_0pct": d3_agg_strict,
        "d5_rank_correlation": d5_res,
        "d6_min_feasible": [r for r in d6_rows if r["feasible"]][:3],
    }
    with open(OUT / "summary.json", "w") as f:
        json.dump(summary, f, indent=2, ensure_ascii=False)
    print(json.dumps(summary, indent=2, ensure_ascii=False))


if __name__ == "__main__":
    main()
