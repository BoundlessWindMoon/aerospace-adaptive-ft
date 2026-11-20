#!/usr/bin/env python3
"""A5: λ(t) 闭环注故障（主张二唯一的实测路径，仿真注入那一半）。

参照 Essex 自适应 SEU 缓解的"仿真注入 + 辐照趋势对照"做法——做仿真那一半。
沿真实轨道 λ(t)（复用 rq9_orbit_timeseries.csv 的 in_saa 分类）生成**一条固定的故障事件序列**，
三条策略(always_low / always_high / adaptive)跑**同一条轨迹、同一组事件**（同种子，公平），
每个事件按该策略在该相位的保护强度以实测 P(SDC|fault) 判定是否静默失效。
量：端到端静默失效数 + 累计开销(墙钟) + 截止违约数。

输入口径（全部来自已测，不新造）：
- λ 相位：atft_core.LAM_LEO_SAA / LAM_LEO_QUIET；相位分类取 rq9_orbit_timeseries.csv 的 in_saa。
- P(SDC|fault)：results/e2e_struct.csv 的等权聚合 SDC（native vs struct，已含 P0-4 修正）。
- 每周期开销：e2e_struct.csv 的 struct overhead（相对 native）。
事件总数由一个 calibrated SCALE 定（为统计分辨率，不等于绝对 FIT——绝对率是 A-2 的事）；
三策略共用同一事件序列，故 SDC 比较与 SCALE 无关。
"""
import csv
import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import atft_core as C

ROOT = Path(__file__).resolve().parent.parent
TS = ROOT / "results" / "rq9_orbit_timeseries.csv"
E2E = ROOT / "results" / "e2e_struct.csv"

GNC_HZ = C.GNC_LOOP_HZ          # 10 Hz
DEADLINE_S = C.GNC_DEADLINE_S   # 0.1 s
STEP_S = 60.0                   # rq9 时序步长
SCALE = 2.0e4                   # 事件率标定(统计分辨率用;三策略共用,不影响比较)
GNC_BASE_S = 3.0e-5             # GNC 单周期基准墙钟(kalman,量级);开销乘在其上
FLARE_MULT = 50.0               # GEO 耀斑窗口 λ 放大
SEEDS = [1, 2, 3]


def agg_sdc():
    """返回 native / struct 的等权聚合 SDC 比例。"""
    buck = {}
    with open(E2E) as f:
        for r in csv.DictReader(f):
            buck.setdefault(r["config"], {"s": [], "ov": float(r["overhead"])})
            buck[r["config"]]["s"].append(float(r["sdc_pct"]) / 100.0)
    out = {}
    for c, v in buck.items():
        out[c] = (sum(v["s"]) / len(v["s"]), v["ov"])
    return out


def load_traj(orbit="sampex", max_steps=400):
    """取一条轨道的 (in_saa) 序列;末尾追加一段 GEO 耀斑窗口。"""
    steps = []
    with open(TS) as f:
        for r in csv.DictReader(f):
            if r["orbit"] != orbit:
                continue
            steps.append(int(r["in_saa"]))
            if len(steps) >= max_steps:
                break
    phases = [("saa" if s else "calm") for s in steps]
    phases += ["flare"] * 20          # GEO 一次耀斑
    return phases


def lam_of(phase):
    if phase == "saa":
        return C.LAM_LEO_SAA
    if phase == "flare":
        return C.LAM_LEO_SAA * FLARE_MULT
    return C.LAM_LEO_QUIET


def gen_events(phases, seed):
    """沿轨迹生成固定故障事件 [(step_idx, phase), ...];三策略共用。"""
    rng = random.Random(seed)
    cycles_per_step = STEP_S * GNC_HZ
    events = []
    for i, ph in enumerate(phases):
        exp = lam_of(ph) * STEP_S * SCALE          # 该步期望事件数
        # 泊松采样
        n, p = 0, 2.718281828 ** (-exp)
        cdf, k = p, 0
        u = rng.random()
        while u > cdf and k < 10000:
            k += 1
            p *= exp / k
            cdf += p
        n = k
        for _ in range(n):
            events.append((i, ph))
    return events


# 策略：相位 -> 保护("native"|"struct")
STRATS = {
    "always_low":  lambda ph: "native",
    "always_high": lambda ph: "struct",
    "adaptive":    lambda ph: "native" if ph == "calm" else "struct",
}


def run_strategy(name, events, sdc, seed):
    rng = random.Random(seed ^ 0xA5)
    pol = STRATS[name]
    p_nat, _ = sdc["native"]
    p_str, ov = sdc["struct_full"]
    n_sdc = 0
    for (i, ph) in events:
        prot = pol(ph)
        p = p_nat if prot == "native" else p_str
        if rng.random() < p:
            n_sdc += 1
    return n_sdc


def main():
    sdc = agg_sdc()
    p_nat = sdc["native"][0]
    p_str, ov = sdc["struct_full"]
    phases = load_traj()
    nstep = len(phases)
    n_saa = sum(1 for p in phases if p == "saa")
    n_flare = sum(1 for p in phases if p == "flare")
    n_calm = nstep - n_saa - n_flare
    cyc_per_step = STEP_S * GNC_HZ

    print(f"轨迹: {nstep} 步 x {STEP_S:.0f}s (calm {n_calm}/saa {n_saa}/flare {n_flare}); "
          f"GNC {GNC_HZ:.0f}Hz; P_sdc native={p_nat*100:.2f}% struct={p_str*100:.2f}%; struct开销 {ov:.2f}x")

    # 累计开销(与种子无关,按相位周期占比):每周期墙钟 = base * overhead(该相位该策略)
    def cum_time(name):
        pol = STRATS[name]
        t = 0.0
        for ph, nstp in (("calm", n_calm), ("saa", n_saa), ("flare", n_flare)):
            prot = pol(ph)
            o = 1.0 if prot == "native" else ov
            t += nstp * cyc_per_step * GNC_BASE_S * o
        return t
    t_low = cum_time("always_low")

    rows = []
    for name in ["always_low", "always_high", "adaptive"]:
        sdcs = []
        for s in SEEDS:
            ev = gen_events(phases, s)
            sdcs.append(run_strategy(name, ev, sdc, s))
        mean_sdc = sum(sdcs) / len(sdcs)
        t = cum_time(name)
        # 截止违约：单周期墙钟 vs 100ms（小核永不违约,如实报）
        per_cycle_max = GNC_BASE_S * (ov if name != "always_low" else 1.0)
        dl_viol = per_cycle_max > DEADLINE_S
        rows.append({"strategy": name, "mean_sdc_events": round(mean_sdc, 1),
                     "sdc_seeds": "/".join(str(x) for x in sdcs),
                     "cum_time_rel": round(t / t_low, 3),
                     "per_cycle_s": f"{per_cycle_max:.2e}",
                     "deadline_violated": int(dl_viol)})
    print(f"\n{'strategy':14}{'均SDC事件':>10}{'SDC(3种子)':>14}{'累计开销(rel)':>14}{'截止违约':>9}")
    for r in rows:
        print(f"{r['strategy']:14}{r['mean_sdc_events']:>10}{r['sdc_seeds']:>14}"
              f"{r['cum_time_rel']:>14}{r['deadline_violated']:>9}")
    outp = ROOT / "results" / "adaptive_closed_loop.csv"
    with open(outp, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader(); w.writerows(rows)
    print(f"\nCSV -> {outp}")
    low = next(r for r in rows if r["strategy"] == "always_low")
    hi = next(r for r in rows if r["strategy"] == "always_high")
    ad = next(r for r in rows if r["strategy"] == "adaptive")
    print(f"读法: always_low SDC {low['mean_sdc_events']} (裸奔); always_high {hi['mean_sdc_events']} "
          f"(全程保护,开销 {hi['cum_time_rel']}x); adaptive {ad['mean_sdc_events']} "
          f"(开销 {ad['cum_time_rel']}x)。")
    print("若 adaptive 的 SDC 远高于 always_high,说明平静期全关的残余来自 calm 裸奔(呼应 MWTF 结论)。")


if __name__ == "__main__":
    main()
