#!/usr/bin/env python3
"""
时序层：把条件概率换算成绝对速率指标（后处理，不改注入器）

现有 campaign 报的是 P(结局 | 一个故障落在可注入点上)，是条件概率，
因此 MWTF 这类需要绝对故障率的指标无法成立。本模块补上缺的那一层：

  sigma[upsets/bit/day] x B[bit] -> lambda[1/s]
  a = lambda * t_exec                                  (单次执行期望故障数)
  P_F = sum_{n>=1} P(F|n) * Poisson(n; a)              (Ostler 链条)
  lambda_sys = P_F / t_exec ; MTTF = 1/lambda_sys
  MEBF = MTTF / t_exec ; 每任务失效数 = lambda_sys * T_mission
  MWTF = 1 / (开销倍率 * P_F)

链条出处：Ostler, Caffrey, Gibelyou, Graham, Morgan, Pratt, Quinn, Wirthlin,
IEEE TNS 56(6):3519-3526, Dec 2009。链条里唯一必须由注入实验提供的量是
P(F|n)，它已经由 RQ6 的 Temporal_Burst (k=1/2/4/8) 测出，这里直接复用，
不新增实验。

MWTF 定义式出处：Bohman et al., IEEE TNS 66(1), 2019；
James, Wirthlin, Goeders, IEEE TNS 68(5):1014-1022, 2021。

用法：
    python3 experiments/temporal.py            # 读 results/ 下的 rq1/rq6 CSV
    python3 experiments/temporal.py --json out.json
"""

import argparse
import csv
import json
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from aggregate import load_meta, two_level_weights, weighted_geomean  # noqa: E402
from benchmark_harness import load_config, wilson_ci95  # noqa: E402

ROOT_DIR = Path(__file__).resolve().parent.parent
RES_DIR = ROOT_DIR / "results"
SEC_PER_DAY = 86400.0


def poisson_pmf(n, a):
    if a <= 0:
        return 1.0 if n == 0 else 0.0
    return math.exp(-a + n * math.log(a) - math.lgamma(n + 1))


def load_orbit_profiles(path=None):
    """读 orbit_rates.py 的产物，取每条轨道被选中的那一档。

    周期 / SAA 占时 / 过境驻留 / 实测 sigma_avg 全部来自同一行，
    这是消除"速率取自 A 卫星、过境时长取自 B 轨道"的关键。
    """
    path = Path(path or RES_DIR / "rq9_orbit_profile.csv")
    if not path.exists():
        raise RuntimeError(
            f"缺少轨道剖面 {path}；先离线跑一次 experiments/orbit_rates.py "
            f"（需要 sgp4 + ppigrf，见该脚本头部注释）")
    out = {}
    for r in csv.DictReader(open(path, encoding="utf8")):
        if r.get("selected") != "yes":
            continue
        out[r["orbit"]] = {
            "period_min": float(r["period_min"]),
            "saa_fraction": float(r["saa_fraction"]),
            "dwell_mean_s": float(r["dwell_mean_min"]) * 60.0,
            "passes_per_day": float(r["passes_per_day"]),
            "sigma_avg": float(r["sigma_avg_upsets_per_bit_day"]),
            "box_agreement": float(r["saa_box_agreement"]),
            "source": r.get("source", ""),
        }
    if not out:
        raise RuntimeError(f"{path} 里没有 selected=yes 的行")
    return out


def resolve_environments(cfg, orbits):
    """展开 temporal.environments：sigma 与占时权重都由所属轨道派生。"""
    tcfg = cfg.get("temporal", {})
    ratio = float(tcfg.get("saa_ratio", {}).get("nominal", 200))
    out, by_tag = [], {}
    for e in tcfg.get("environments", []):
        tag = e["tag"]
        orb_tag = e.get("orbit")
        if orb_tag not in orbits:
            raise RuntimeError(f"环境 {tag} 引用了未生成剖面的轨道 {orb_tag}")
        orb = orbits[orb_tag]
        f = orb["saa_fraction"]
        # sigma_avg = f*sigma_in + (1-f)*sigma_out, sigma_in = ratio*sigma_out
        sigma_out = orb["sigma_avg"] / (f * ratio + 1.0 - f)
        sigma = {"avg": orb["sigma_avg"], "saa_out": sigma_out,
                 "saa_in": sigma_out * ratio}[e.get("derive", "avg")]
        sigma *= float(e.get("multiplier", 1.0))
        rec = {"tag": tag, "orbit": orb_tag, "derive": e.get("derive", "avg"),
               "multiplier": float(e.get("multiplier", 1.0)),
               "sigma": sigma, "source": e.get("source", orb["source"])}
        by_tag[tag] = rec
        out.append(rec)
    return out


def load_pfn(path):
    """RQ6 的 Temporal_Burst 行 -> 三条曲线 (点估计, Wilson 下界, 上界)。

    失效 = SDC + DUE；masked/benign/det_recovered 都算存活（与 RQ6 的
    survive_ok 定义一致）。注入是伯努利抽样，单点率值必须附置信区间
    （Leveugle et al., DATE 2009），这里把区间一路传到 MTTF。
    """
    pt, lo, hi = {}, {}, {}
    for r in csv.DictReader(open(path, encoding="utf8")):
        if r["stress_type"] != "Temporal_Burst":
            continue
        n = int(r["stress_level"].split("=")[1].split()[0])
        trials = int(r["trials"])
        k = round(trials * (float(r["sdc_pct"]) + float(r["due_pct"])) / 100.0)
        p_lo, p_hi, _ = wilson_ci95(k, trials)
        pol = r["policy"]
        pt.setdefault(pol, {})[n] = k / trials
        lo.setdefault(pol, {})[n] = p_lo / 100.0
        hi.setdefault(pol, {})[n] = p_hi / 100.0
    if not pt:
        raise RuntimeError(f"{path} 里没有 Temporal_Burst 行，先跑 RQ6")
    return pt, lo, hi


def pfn_at(curve, n):
    """P(F|n)：实测点之间线性插值，超出最大实测 n 时取端点（偏保守）。"""
    ks = sorted(curve)
    if n <= ks[0]:
        return curve[ks[0]]
    if n >= ks[-1]:
        return curve[ks[-1]]
    for lo, hi in zip(ks, ks[1:]):
        if lo <= n <= hi:
            t = (n - lo) / (hi - lo)
            return curve[lo] + t * (curve[hi] - curve[lo])
    return curve[ks[-1]]


def p_fail(curve, a, n_max_mult=8):
    """P_F = sum_{n>=1} P(F|n) Poisson(n;a)，并返回截断尾部质量。

    截断上界取 max(实测最大 n, a 的若干倍)，保证 a 很大时也收敛。
    """
    k_meas = max(curve)
    upper = max(k_meas, int(math.ceil(a * n_max_mult)) + k_meas)
    total, tail = 0.0, 0.0
    for n in range(1, upper + 1):
        w = poisson_pmf(n, a)
        total += pfn_at(curve, n) * w
        if n > k_meas:
            tail += w
    return total, tail


def exec_profile(pmu_path, meta):
    """{policy: {t_exec_s, inst_ratio}}，负载集合与权重与 RQ6 口径一致。

    RQ6 用 BTAG_REPR 选负载，这里用同一子集；套件内等权、套件间等权。
    """
    rows = list(csv.DictReader(open(pmu_path, encoding="utf8")))
    repr_set = [b for b, m in meta.items() if "repr" in m.get("tags", "")]
    if not repr_set:
        repr_set = sorted(meta)
    weights = two_level_weights(repr_set, meta)
    native_inst = {r["benchmark"]: float(r["hw_insts"])
                   for r in rows if r["policy"] == "native"}
    out = {}
    for r in rows:
        b = r["benchmark"]
        w = weights.get(b, 0.0)
        if w <= 0:
            continue
        d = out.setdefault(r["policy"], {"t_exec_s": 0.0, "_ratio_rows": []})
        d["t_exec_s"] += w * float(r["exec_time_us"]) * 1e-6
        d["_ratio_rows"].append({
            "benchmark": b,
            "ratio": float(r["hw_insts"]) / max(1.0, native_inst.get(b, 1.0)),
        })
    for pol, d in out.items():
        d["inst_ratio"] = weighted_geomean(d.pop("_ratio_rows"), "ratio", weights)
    return out


def _row(env_tag, bits, pol, lam, t_exec, ratio, curves):
    pt, lo, hi = curves
    a = lam * t_exec
    pf, tail = p_fail(pt, a)
    pf_lo, _ = p_fail(lo, a)
    pf_hi, _ = p_fail(hi, a)
    lam_sys = pf / t_exec if t_exec > 0 else 0.0
    h = lambda x: (1.0 / (x / t_exec) / 3600.0) if x > 0 else float("inf")
    # 零失效样本：点估计没有信息（0/N），只有置信上界可报。此时整行的点估计
    # 全部换成由 Wilson 上界得到的保守界，避免出现 MTTF=1e9 小时这种假数字。
    zero = pt[min(pt)] <= 0.0
    return {
        "environment": env_tag,
        "fault_space_mib": round(bits / 8 / 1024 / 1024, 1),
        "policy": pol,
        "lambda_per_s": f"{lam:.6g}",
        "exec_time_s": f"{t_exec:.6g}",
        "inst_ratio": f"{ratio:.4g}",
        "faults_per_exec_a": f"{a:.6g}",
        "zero_failure_sample": "yes" if zero else "",
        "p_fail_per_exec": (f"<{pf_hi:.6g}" if zero else f"{pf:.6g}"),
        "p_fail_ci95_lo": "" if zero else f"{pf_lo:.6g}",
        "p_fail_ci95_hi": f"{pf_hi:.6g}",
        "mttf_hours": (f">{h(pf_hi):.6g}" if zero else f"{h(pf):.6g}"),
        "mttf_ci95_lo_hours": f"{h(pf_hi):.6g}",
        "mttf_ci95_hi_hours": "" if zero else f"{h(pf_lo):.6g}",
        # MWTF 的绝对形式：单位有用工作（一次执行）对应的失效数之倒数。
        # 运行时开销已经通过 a = lambda*t_exec 进入 p_fail，
        # 不能再乘 inst_ratio，否则开销被计两次。
        "mwtf_execs_per_failure": (f">{1.0 / pf_hi:.6g}" if zero
                                   else (f"{1.0 / pf:.6g}" if pf > 0 else "inf")),
        "poisson_tail_above_measured_n": f"{tail:.3g}",
        "_lam_sys": (pf_hi if zero else pf) / t_exec if t_exec > 0 else 0.0,
        "_lam_sys_hi": pf_hi / t_exec if t_exec > 0 else 0.0,
        "_pf_hi": pf_hi,
        "_zero": zero,
    }


def mttf_table(cfg, curves, prof, orbits):
    """Ostler, IEEE TNS 56(6), 2009 第 1-4 步逐环境，第 5 步给每条轨道的复合 MTTF。"""
    tcfg = cfg.get("temporal", {})
    mission_s = float(tcfg.get("mission_days", 90)) * SEC_PER_DAY
    envs = resolve_environments(cfg, orbits)
    pols = [p for p in sorted(curves[0]) if p in prof]
    rows = []
    for bits in tcfg.get("fault_space_bits", [33554432]):
        bits = float(bits)
        # 第 5 步的占时权重：每条轨道上 SAA 内 / 外两种条件，占时比来自该轨道剖面
        comp = {o: {p: 0.0 for p in pols} for o in orbits}
        comp_hi = {o: {p: 0.0 for p in pols} for o in orbits}
        for env in envs:
            lam = env["sigma"] * bits / SEC_PER_DAY
            f = orbits[env["orbit"]]["saa_fraction"]
            w = {"saa_in": f, "saa_out": 1.0 - f}.get(env["derive"], 0.0)
            if float(env.get("multiplier", 1.0)) != 1.0:
                w = 0.0  # SPE 档不进常态复合
            for pol in pols:
                r = _row(env["tag"], bits, pol, lam,
                         prof[pol]["t_exec_s"], prof[pol]["inst_ratio"],
                         (curves[0][pol], curves[1][pol], curves[2][pol]))
                comp[env["orbit"]][pol] += w * r.pop("_lam_sys")
                comp_hi[env["orbit"]][pol] += w * r.pop("_lam_sys_hi")
                zero = r.pop("_zero")
                r["_pf_hi"] = r.pop("_pf_hi")
                r["_zero"] = zero
                rows.append(r)
        for orb_tag, per_pol in comp.items():
            for pol in pols:
                ls = per_pol[pol]
                if ls <= 0:
                    continue
                t_exec = prof[pol]["t_exec_s"]
                zero = any(r["_zero"] for r in rows
                           if r["policy"] == pol and r["environment"].startswith(orb_tag))
                pre = ">" if zero else ""
                rows.append({
                    "environment": f"{orb_tag}_composite",
                    "fault_space_mib": round(bits / 8 / 1024 / 1024, 1),
                    "policy": pol,
                    "lambda_per_s": "n/a",
                    "exec_time_s": f"{t_exec:.6g}",
                    "inst_ratio": f"{prof[pol]['inst_ratio']:.4g}",
                    "faults_per_exec_a": "n/a",
                    "zero_failure_sample": "yes" if zero else "",
                    "p_fail_per_exec": f"{'<' if zero else ''}{ls * t_exec:.6g}",
                    "p_fail_ci95_lo": "",
                    "p_fail_ci95_hi": f"{comp_hi[orb_tag][pol] * t_exec:.6g}",
                    "mttf_hours": f"{pre}{1.0 / ls / 3600.0:.6g}",
                    "mttf_ci95_lo_hours": f"{1.0 / comp_hi[orb_tag][pol] / 3600.0:.6g}",
                    "mttf_ci95_hi_hours": "",
                    "mwtf_execs_per_failure": f"{pre}{1.0 / (ls * t_exec):.6g}",
                    "poisson_tail_above_measured_n": "",
                    "_pf_hi": comp_hi[orb_tag][pol] * t_exec,
                    "_zero": zero,
                })
    # 每任务失效数与相对 native 的 MWTF，按 (环境, 故障空间) 分组算。
    # 策略间比较统一用**95% 下界**（即 P_F 的 Wilson 上界），否则会出现
    # "零失效策略的下界" 对 "有失效策略的点估计" 这种不可比的组合。
    groups = {}
    for r in rows:
        groups.setdefault((r["environment"], r["fault_space_mib"]), []).append(r)
    for g in groups.values():
        base_hi = next((r["_pf_hi"] for r in g if r["policy"] == "native"), None)
        for r in g:
            zero = r.pop("_zero")
            pf_hi = r.pop("_pf_hi")
            pf = float(r["p_fail_per_exec"].lstrip("<>"))
            t_exec = float(r["exec_time_s"])
            pre = "<" if zero else ""
            r["failures_per_mission"] = f"{pre}{pf / t_exec * mission_s:.6g}"
            r["mwtf_lb95_vs_native"] = (f"{base_hi / pf_hi:.4g}"
                                        if base_hi and pf_hi > 0 else "n/a")
    return rows


def detection_table(cfg, orbits):
    """控制器检测预算：攒到第 k 个事件的平均等待时间 vs SAA 过境窗口。

    等待时间是齐次 Poisson 下第 k 个事件的期望到达时刻 k/lambda（Erlang 均值），
    与阈值怎么取、用 CUSUM 还是贝叶斯后验无关 —— 它是任何基于事件计数的
    反应式控制器的延迟下界。窗口长度与占时比来自该轨道自己的剖面。
    """
    tcfg = cfg.get("temporal", {})
    rows = []
    for orb_tag, orb in orbits.items():
        f, tau = orb["saa_fraction"], orb["dwell_mean_s"]
        for ratio in tcfg.get("saa_ratio", {}).get("sweep", [200]):
            ratio = float(ratio)
            sigma_out = orb["sigma_avg"] / (f * ratio + 1.0 - f)
            for bits in tcfg.get("fault_space_bits", [33554432]):
                bits = float(bits)
                lam_in = sigma_out * ratio * bits / SEC_PER_DAY
                lam_out = sigma_out * bits / SEC_PER_DAY
                for k in tcfg.get("detection_events_k", [1, 2, 3]):
                    wait = k / lam_in if lam_in > 0 else float("inf")
                    rows.append({
                        "orbit": orb_tag,
                        "period_min": f"{orb['period_min']:.4g}",
                        "saa_dwell_min": f"{tau / 60.0:.3g}",
                        "saa_fraction": f"{f:.4g}",
                        "ratio_in_out": int(ratio),
                        "fault_space_mib": round(bits / 8 / 1024 / 1024, 1),
                        "lambda_in_per_s": f"{lam_in:.4g}",
                        "mean_gap_in_s": f"{(1.0 / lam_in):.4g}" if lam_in > 0 else "inf",
                        "mean_gap_out_h": f"{(1.0 / lam_out / 3600.0):.4g}" if lam_out > 0 else "inf",
                        "events_per_saa_pass": f"{(lam_in * tau):.3g}",
                        "detect_events_k": k,
                        "mean_detect_delay_s": f"{wait:.4g}",
                        "delay_frac_of_pass": f"{(wait / tau):.3g}",
                    })
    return rows


def write_csv(path, rows):
    if not rows:
        return
    with open(path, "w", newline="", encoding="utf8") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader()
        w.writerows(rows)
    print(f"[temporal] -> {path} ({len(rows)} 行)")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--config", default=None)
    ap.add_argument("--json", default=None)
    args = ap.parse_args()

    cfg = load_config(args.config)
    meta = load_meta()
    orbits = load_orbit_profiles()
    curves = load_pfn(RES_DIR / "rq6_mbu_burst_stress.csv")
    prof = exec_profile(RES_DIR / "rq1_pmu_performance_qor.csv", meta)

    mt = mttf_table(cfg, curves, prof, orbits)
    det = detection_table(cfg, orbits)
    write_csv(RES_DIR / "rq9_temporal_mttf.csv", mt)
    write_csv(RES_DIR / "rq9_detection_budget.csv", det)

    if args.json:
        Path(args.json).write_text(json.dumps(
            {"mttf": mt, "detection": det, "orbits": orbits,
             "p_fail_given_n": {p: {str(k): v for k, v in c.items()}
                                for p, c in curves[0].items()},
             "exec_profile": prof},
            indent=1, ensure_ascii=False), encoding="utf8")
        print(f"[temporal] -> {args.json}")


if __name__ == "__main__":
    main()
