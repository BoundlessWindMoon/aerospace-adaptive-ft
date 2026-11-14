#!/usr/bin/env python3
"""
轨道剖面生成器（离线一次性运行，产物入库，运行时不依赖本脚本）

目的：消除"速率取自 A 卫星、过境时长取自 B 轨道"的不自洽。本脚本对**同一条
轨道**同时给出轨道周期、SAA 占时比、每次过境驻留时长，再与**同一颗卫星实测
的轨道平均上翻率**配对，于是 Ostler composite MTTF 的 (lambda_i, p_i) 列表
全部来自一处。

配方照 Jacobs, Cieslewski, George, Gordon-Ross, Lam,
ACM TRETS 5(4), Art. 21, Dec 2012：轨道传播 -> 地磁场 -> 分段故障率。
与原文的差别（必须在论文中声明）：
  - 原文第三步用 CREME96 按 McIlwain L 分段查上翻率；本脚本离线无 CREME96，
    改为「用 IGRF 的 |B| 判定是否位于 SAA（SAA 即地磁场强度极小区），
    绝对尺度锚定到该卫星实测的轨道平均率，SAA 内外比值作为扫描参数」。
  - 因此本脚本给出的是**几何量**（占时比、驻留时长、周期），不是物理通量；
    物理尺度来自实测文献。

依赖（只有本脚本需要，且只在重新生成剖面时需要）：
    python3 -m venv /tmp/orbenv && /tmp/orbenv/bin/pip install sgp4 ppigrf numpy pyyaml
用法：
    /tmp/orbenv/bin/python experiments/orbit_rates.py
产物：
    results/rq9_orbit_profile.csv       每条轨道一行的几何摘要
    results/rq9_orbit_timeseries.csv    |B| 与 SAA 标记的时间序列（抽稀后）
"""

import csv
import datetime
import math
import sys
from pathlib import Path

import numpy as np
import ppigrf
import yaml
from sgp4.api import WGS72, Satrec, jday

ROOT_DIR = Path(__file__).resolve().parent.parent
RES_DIR = ROOT_DIR / "results"
CFG_PATH = ROOT_DIR / "experiments" / "config" / "default.yaml"

R_EARTH_KM = 6378.137
WGS84_F = 1.0 / 298.257223563
MU_EARTH = 398600.4418


def gmst_rad(jd, fr):
    """IAU 1982 格林尼治平恒星时。TEME -> ECEF 只差这一个绕 Z 轴的转动。"""
    t = (jd - 2451545.0 + fr) / 36525.0
    s = 67310.54841 + (876600.0 * 3600.0 + 8640184.812866) * t \
        + 0.093104 * t * t - 6.2e-6 * t * t * t
    return math.radians((s % 86400.0) / 240.0)


def eci_to_geodetic(r_km, jd, fr):
    """TEME 位置 -> (经度 deg, 纬度 deg, 高度 km)，WGS84 椭球、迭代解纬度。"""
    th = gmst_rad(jd, fr)
    x = r_km[0] * math.cos(th) + r_km[1] * math.sin(th)
    y = -r_km[0] * math.sin(th) + r_km[1] * math.cos(th)
    z = r_km[2]
    lon = math.degrees(math.atan2(y, x))
    p = math.hypot(x, y)
    e2 = WGS84_F * (2.0 - WGS84_F)
    lat = math.atan2(z, p)
    for _ in range(5):
        n = R_EARTH_KM / math.sqrt(1.0 - e2 * math.sin(lat) ** 2)
        h = p / math.cos(lat) - n
        lat = math.atan2(z, p * (1.0 - e2 * n / (n + h)))
    n = R_EARTH_KM / math.sqrt(1.0 - e2 * math.sin(lat) ** 2)
    h = p / math.cos(lat) - n
    return lon, math.degrees(lat), h


def make_satrec(apogee_km, perigee_km, inc_deg, raan_deg=0.0, argp_deg=0.0):
    """由平均要素构造 SGP4 对象（没有真实 TLE 时的替代，须在论文声明）。"""
    a = R_EARTH_KM + 0.5 * (apogee_km + perigee_km)
    ecc = 0.5 * (apogee_km - perigee_km) / a
    n_rad_min = math.sqrt(MU_EARTH / a ** 3) * 60.0
    s = Satrec()
    s.sgp4init(WGS72, "i", 1, 0.0, 0.0, 0.0, 0.0, ecc, math.radians(argp_deg),
               math.radians(inc_deg), 0.0, n_rad_min, math.radians(raan_deg))
    return s, 2.0 * math.pi / n_rad_min


def sample_orbit(spec, days, step_s, epoch):
    """沿轨道采样 -> [(t_s, lon, lat, alt_km, |B| nT, L)]。

    IGRF 调用一次批量算完（逐点调用慢两个数量级）。L 用倾斜偶极子近似：
    L = (r/Re) / cos^2(geomagnetic latitude)，偶极极点取 IGRF-13 2020 年值
    80.65N / 72.68W。真正的 McIlwain L 需要沿磁力线积分，这里的近似必须在
    论文中声明（内带区域误差约百分之几，足以界定 SAA 的 L 范围）。
    """
    sat, period_min = make_satrec(spec["apogee_km"], spec["perigee_km"],
                                  spec["inc_deg"])
    jd0, fr0 = jday(epoch.year, epoch.month, epoch.day, 0, 0, 0)
    n_steps = int(days * 86400.0 / step_s)
    ts, lons, lats, alts = [], [], [], []
    for i in range(n_steps):
        t_s = i * step_s
        fr = fr0 + t_s / 86400.0
        err, r_km, _ = sat.sgp4(jd0, fr)
        if err != 0:
            raise RuntimeError(f"SGP4 传播失败 err={err} at t={t_s}s")
        lon, lat, alt = eci_to_geodetic(r_km, jd0, fr)
        ts.append(t_s); lons.append(lon); lats.append(lat); alts.append(alt)

    lon_a = np.array(lons); lat_a = np.array(lats); alt_a = np.array(alts)
    be, bn, bu = ppigrf.igrf(lon_a, lat_a, alt_a, epoch)
    b_nt = np.sqrt(np.asarray(be).ravel() ** 2 + np.asarray(bn).ravel() ** 2
                   + np.asarray(bu).ravel() ** 2)

    pole_lat, pole_lon = math.radians(80.65), math.radians(-72.68)
    la, lo = np.radians(lat_a), np.radians(lon_a)
    sin_mlat = (np.sin(la) * math.sin(pole_lat)
                + np.cos(la) * math.cos(pole_lat) * np.cos(lo - pole_lon))
    cos2 = np.maximum(1.0 - sin_mlat ** 2, 1e-9)
    l_shell = (R_EARTH_KM + alt_a) / R_EARTH_KM / cos2
    return list(zip(ts, lons, lats, alts, b_nt.tolist(),
                    l_shell.tolist())), period_min


# SAA 的常规地理范围，仅用于**校验** |B| 判据是否真的圈到了 SAA，
# 不参与判定本身。范围取自 SAA 文献的通用描述（经度 -90..+40，纬度 -50..0）。
SAA_BOX = {"lon": (-90.0, 40.0), "lat": (-50.0, 0.0)}


def in_saa_box(lon, lat):
    return (SAA_BOX["lon"][0] <= lon <= SAA_BOX["lon"][1]
            and SAA_BOX["lat"][0] <= lat <= SAA_BOX["lat"][1])


def episodes(samples, b_thresh, l_max, step_s):
    """SAA 判据：|B| 低于阈值**且** L 落在内带范围。

    只用 |B| 会把整条磁赤道低场带一起圈进来（实测与 SAA 地理范围的一致率只有
    0.5），加上 L 上界才能把内带与外区分开。
    """
    inside = [s[4] < b_thresh and s[5] < l_max for s in samples]
    runs, cur = [], 0
    for flag in inside:
        if flag:
            cur += 1
        elif cur:
            runs.append(cur)
            cur = 0
    if cur:
        runs.append(cur)
    n_in = sum(inside)
    total_s = len(samples) * step_s
    hits = [in_saa_box(s[1], s[2]) for s, f in zip(samples, inside) if f]
    return {
        "frac": n_in / len(samples) if samples else 0.0,
        "dwell_mean_s": (sum(runs) / len(runs) * step_s) if runs else 0.0,
        "dwell_max_s": (max(runs) * step_s) if runs else 0.0,
        "passes_per_day": len(runs) / (total_s / 86400.0) if total_s else 0.0,
        "box_agreement": (sum(hits) / len(hits)) if hits else 0.0,
    }


def main():
    cfg = yaml.safe_load(open(CFG_PATH, encoding="utf8")) or {}
    ocfg = cfg.get("temporal", {}).get("orbit_profiles", {})
    specs = ocfg.get("orbits", [])
    if not specs:
        print("配置里没有 temporal.orbit_profiles.orbits，无事可做")
        return 1
    days = float(ocfg.get("sample_days", 1.0))
    step_s = float(ocfg.get("step_s", 10.0))
    sweep = ocfg.get("b_threshold_sweep_nt", [24000, 26000, 28000, 30000])
    l_max = float(ocfg.get("l_shell_max", 2.0))
    epoch = datetime.datetime.fromisoformat(str(ocfg.get("epoch", "2020-01-01")))

    summary, series = [], []
    for spec in specs:
        samples, period_min = sample_orbit(spec, days, step_s, epoch)
        chosen = float(spec.get("b_threshold_nt", ocfg.get("b_threshold_nt", 26000)))
        for b in list(sweep) + ([chosen] if chosen not in sweep else []):
            st = episodes(samples, float(b), l_max, step_s)
            summary.append({
                "orbit": spec["tag"],
                "apogee_km": spec["apogee_km"],
                "perigee_km": spec["perigee_km"],
                "inc_deg": spec["inc_deg"],
                "period_min": f"{period_min:.4g}",
                "b_threshold_nt": int(b),
                "l_shell_max": l_max,
                "selected": "yes" if float(b) == chosen else "",
                "saa_fraction": f"{st['frac']:.4g}",
                "dwell_mean_min": f"{st['dwell_mean_s'] / 60.0:.3g}",
                "dwell_max_min": f"{st['dwell_max_s'] / 60.0:.3g}",
                "passes_per_day": f"{st['passes_per_day']:.3g}",
                "saa_box_agreement": f"{st['box_agreement']:.3g}",
                "sigma_avg_upsets_per_bit_day": spec.get("sigma_avg", ""),
                "source": spec.get("source", ""),
            })
        keep = max(1, int(len(samples) / 4000))
        for i, s in enumerate(samples):
            if i % keep:
                continue
            series.append({"orbit": spec["tag"], "t_s": f"{s[0]:.0f}",
                           "lon_deg": f"{s[1]:.3f}", "lat_deg": f"{s[2]:.3f}",
                           "alt_km": f"{s[3]:.1f}", "b_nt": f"{s[4]:.0f}",
                           "l_shell": f"{s[5]:.3f}",
                           "in_saa": int(s[4] < chosen and s[5] < l_max)})

    for path, rows in ((RES_DIR / "rq9_orbit_profile.csv", summary),
                       (RES_DIR / "rq9_orbit_timeseries.csv", series)):
        with open(path, "w", newline="", encoding="utf8") as f:
            w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
            w.writeheader()
            w.writerows(rows)
        print(f"[orbit] -> {path} ({len(rows)} 行)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
