#!/usr/bin/env python3
"""效果 / 开销 / 自适应 三节报告，各出必要的表。

效果取自同一注入活动族（无防护基线 + 开保护后重测）；开销取自实测 cyc 比；
自适应现跑 workload_eval。换负载/补实验后重跑刷新。

用法：
    python3 tools/build_report.py              # → tools/reports/full_report.md
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(ROOT, "experiments"))
from atft_core import OPT, build_menu, load_csv        # noqa: E402
from workload_eval import eval_workload                # noqa: E402

FT = [("fp64", "浮点"), ("int_arith", "整数"), ("cast", "类型转换"),
      ("load", "访存"), ("addr", "地址"), ("cmp_branch", "比较分支")]
EFFECT_ROWS = [("无防护", "aggregate_a4_t200.json"),
               ("整内核冗余", "aggregate_a4_t200_coarse_fi.json"),
               ("进程隔离冗余", "aggregate_a4_t200_coarse_fork.json")]


def load_json(name):
    with open(os.path.join(OPT, name)) as f:
        return json.load(f)


def eff_table(metric):
    head = "| 方法 | " + " | ".join(lab for _, lab in FT) + " |"
    sep = "| " + " | ".join(["---"] * (len(FT) + 1)) + " |"
    L = [head, sep]
    for label, fn in EFFECT_ROWS:
        bc = load_json(fn)["native"]["by_category"]
        vals = [f"{bc[k][metric]:.1f}" if k in bc else "—" for k, _ in FT]
        L.append("| " + label + " | " + " | ".join(vals) + " |")
    return L
# ---8<--- APPEND BELOW ---8<---
def sec_effect():
    L = ["## 效果", "", "各方法把 6 类故障的算错率(SDC)与崩溃挂起率(DUE)降到多少，单位 %。", ""]
    L += ["### 算错率 SDC%"] + eff_table("sdc")
    L += ["", "### 崩溃/挂起率 DUE%"] + eff_table("due")
    L.append("")
    return L


def sec_overhead():
    props = {r["benchmark"]: r
             for r in load_csv("kernel_properties_a4_t200_matrix_solid.csv")}
    abft = [float(props[b]["cyc_x_abft"]) for b in ("pb_2mm", "pb_3mm", "pb_gemm")]
    slack = float(props["pb_seidel2d"]["cyc_x_simd_tmr"])
    ov = load_csv("coarse_overhead_subset.csv")
    dmr = [float(r["coarse_dmr_rerun"]) for r in ov]
    tmr = [float(r["coarse_tmr"]) for r in ov]
    L = ["## 开销", "", "各方法相对无防护的运行时间倍数。", "",
         "| 方法 | 适用 | 开销× |", "| --- | --- | --- |",
         f"| 代数校验 ABFT | 矩阵乘法 | {min(abft):.1f}–{max(abft):.1f}× |",
         f"| SIMD-slack 打包 | 标量内核 | {slack:.1f}× |",
         f"| 整内核冗余 | 通用 | {min(dmr):.1f}–{max(dmr):.1f}×（单核）/ ~1×（多核）|",
         f"| 进程隔离冗余 | 通用 | {min(tmr):.1f}–{max(tmr):.1f}×（单核）/ ~1×（多核）|", ""]
    return L


def sec_adaptive(res):
    L = ["## 自适应", "",
         "按轨道辐射动态选方法，满足可靠性前提下压开销。", "",
         "| 轨道 | 自适应× | 全保护× | 不防护× | 省 vs 全保护 | 达标 |",
         "| --- | --- | --- | --- | --- | --- |"]
    meet = {("LEO" if "LEO" in m["orbit"] else "SSO" if "SSO" in m["orbit"] else m["orbit"]):
            m["adaptive_meets_eps"] for m in res["meets_eps"]}
    for c in res["continuous"]:
        key = "LEO" if "LEO" in c["orbit"] else "SSO"
        L.append(f"| {c['orbit']} | {c['adaptive']:.2f} | {c['full_protect']:.2f} | "
                 f"{c['native']:.2f} | {c['save_vs_full_pct']:.0f}% | "
                 f"{'是' if meet.get(key, True) else '否'} |")
    L.append("")
    return L


def main():
    res = eval_workload(build_menu())
    L = ["# 自适应容错框架 — 效果 / 开销 / 自适应", ""]
    L += sec_effect()
    L += sec_overhead()
    L += sec_adaptive(res)
    out = os.path.join(HERE, "reports", "full_report.md")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w") as f:
        f.write("\n".join(L) + "\n")
    print("\n".join(L))
    print(f"\n[写入] {out}")


if __name__ == "__main__":
    main()

