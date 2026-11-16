#!/usr/bin/env python3
"""
资源价格调度：把自适应的目标从"墙钟时间"换成"能量"，暴露"空闲核 ~1× 不是免费"。

动机：单时间目标下，有空闲核时 coarse_parallel 墙钟 1× 恒最优 → 自适应退化成"处处
1×"。但三副本的计算功耗照付：墙钟 1× 对应能量 2–3×。航天真正受限的是能量/功率
(NASA HPSC：用同等电量处理更多数据)，所以目标应是能量。

复用 atft_core 的菜单/轨迹/约束求解；只加一维 o_energy(= 该档的计算功倍率，与并行
无关)，不改 atft_core 的冻结行为。
"""
import csv
import os

from atft_core import (build_menu, avail, load_continuous_lambda,
                       trace_geo_flare, OPT)

EPS = 1e-4   # 每步残余失效率上限(与 capstone 同口径)


def energy_menu():
    """给每个候选加 o_energy：并行隐藏墙钟但不隐藏功耗。"""
    menu = build_menu()
    for b, cands in menu.items():
        o_serial = next((c["o"] for c in cands if c["policy"] == "coarse_serial"), 2.1)
        for c in cands:
            c["o_energy"] = o_serial if c["policy"] == "coarse_parallel" else c["o"]
    return menu


def _feas(cands, lam):
    cs = avail(cands, True)                      # 并行场景：coarse_parallel 可用
    feas = [c for c in cs if lam * c["e"] <= EPS]
    return cs, feas


def choose_time(cands, lam):
    cs, feas = _feas(cands, lam)
    return min(feas, key=lambda c: c["o"]) if feas else min(cs, key=lambda c: c["e"])


def choose_energy(cands, lam):
    cs, feas = _feas(cands, lam)
    return min(feas, key=lambda c: c["o_energy"]) if feas else min(cs, key=lambda c: c["e"])


def choose_high(cands, lam):
    cs = avail(cands, True)
    emin = min(c["e"] for c in cs)
    return min([c for c in cs if c["e"] <= emin + 1e-12], key=lambda c: c["o_energy"])


def integ(trace, menu, chooser, dt):
    ks = sorted(menu)
    O = E = F = T = 0.0
    for t, lam in trace:
        so = se = sf = 0.0
        for b in ks:
            c = chooser(menu[b], lam)
            so += c["o"]; se += c["o_energy"]; sf += lam * c["e"]
        O += so / len(ks) * dt; E += se / len(ks) * dt; F += sf * dt; T += dt
    return O / T, E / T, F


def main():
    menu = energy_menu()
    orbits = [("LEO-SAMPEX", *load_continuous_lambda("sampex")),
              ("SSO-Alsat1", *load_continuous_lambda("alsat1")),
              ("GEO-flare", trace_geo_flare(), 300.0)]
    rows = []
    for name, trace, dt in orbits:
        ot, et_t, _ = integ(trace, menu, choose_time, dt)       # 时间目标：墙钟 vs 隐藏能量
        oe, ee, _ = integ(trace, menu, choose_energy, dt)        # 能量目标
        oh, eh, _ = integ(trace, menu, choose_high, dt)          # 全程强防护
        rows.append([name, f"{ot:.3f}", f"{et_t:.3f}", f"{oe:.3f}",
                     f"{ee:.3f}", f"{eh:.3f}", f"{(1 - ee / eh) * 100:.1f}"])
    hdr = ["orbit", "时间目标_墙钟x", "时间目标_隐藏能量x",
           "能量目标_墙钟x", "能量目标_能量x", "全程强防护_能量x", "省能量%"]
    out = os.path.join(OPT, "resource_pricing.csv")
    with open(out, "w", newline="") as f:
        w = csv.writer(f); w.writerow(hdr); w.writerows(rows)
    print(",".join(hdr))
    for r in rows:
        print(",".join(r))
    print(f"-> {out}")


if __name__ == "__main__":
    main()
