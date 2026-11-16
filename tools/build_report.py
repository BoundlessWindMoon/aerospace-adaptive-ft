#!/usr/bin/env python3
"""原始实测数据报告：按故障类别逐行吐出 harness 自己的聚合（带 trials 与 95%CI）。

数据全部取自 canonical 的 aggregate JSON（harness 跨负载聚合的结果）：不逐负载展开、
不手工再算、不臆造。每个故障类别一行。Freivalds 暂无逐格数据，未纳入。

用法：
    python3 tools/build_report.py              # → tools/reports/full_report.md
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(ROOT, "experiments"))
from atft_core import OPT                        # noqa: E402

FT = [("fp64", "浮点"), ("int_arith", "整数"), ("cast", "类型转换"),
      ("load", "访存"), ("addr", "地址"), ("cmp_branch", "比较分支")]
POL = {"native": "无防护", "abft_auto": "代数校验ABFT", "simd_tmr": "指令级SIMD-TMR",
       "exact-tmr": "指令级精确TMR", "swift": "SWIFT指令双模",
       "opt_base": "优化基线", "opt_simd_f64": "优化SIMD-f64", "opt_layered": "分层优化"}
POL_ORDER = list(POL)
SOLID = "aggregate_a4_t200_matrix_solid.json"


def load_json(name):
    with open(os.path.join(OPT, name)) as f:
        return json.load(f)


def cat_table(bc):
    L = ["| 故障类别 | trials | 纠正% | SDC% (±95%CI) | DUE% (±95%CI) |",
         "| --- | --- | --- | --- | --- |"]
    for k, lab in FT:
        if k not in bc:
            continue
        v = bc[k]
        L.append(f"| {lab} | {int(v['trials'])} | {v.get('det_rec', 0):.1f} | "
                 f"{v['sdc']:.2f} (±{v['sdc_ci95']:.2f}) | "
                 f"{v['due']:.2f} (±{v['due_ci95']:.2f}) |")
    return L
# ---8<--- APPEND BELOW ---8<---
def sec_fi():
    d = load_json(SOLID)
    L = ["## A. 故障注入 · 按策略 × 故障类别", "",
         "每行 = 一个策略在一类故障上的聚合（跨 12 负载、每格 200 次注入）；"
         "trials 为该类别总注入数，SDC/DUE 后为 95%CI 半宽。", "",
         f"数据源：`results/opt/{SOLID}`", ""]
    for pol in POL_ORDER:
        bc = d.get(pol, {}).get("by_category")
        if not bc:
            continue
        L.append(f"### {pol}（{POL.get(pol, pol)}）")
        L += cat_table(bc)
        L.append("")
    return L


def sec_overhead():
    d = load_json(SOLID)
    L = ["## B. 开销 · 按策略", "",
         "动态指令× = 该策略相对无防护的动态指令数倍数（harness inst_ratio）。", "",
         f"数据源：`results/opt/{SOLID}`", "",
         "| 策略 | 动态指令× |", "| --- | --- |"]
    for pol in POL_ORDER:
        if pol in d and "inst_ratio" in d[pol]:
            L.append(f"| {pol}（{POL.get(pol, pol)}） | "
                     f"{float(d[pol]['inst_ratio']):.2f} |")
    L.append("")
    return L


def sec_coverage():
    sources = [("aggregate_a4_t200.json", "无防护基线"),
               ("aggregate_a4_t200_coarse_fi.json", "整内核冗余后"),
               ("aggregate_a4_t200_coarse_fork.json", "进程隔离冗余后")]
    L = ["## C. 整内核 / 进程隔离冗余 · 覆盖", "",
         "无防护基线与开启对应保护后的重测，各按故障类别聚合。", ""]
    for fn, label in sources:
        try:
            bc = load_json(fn)["native"]["by_category"]
        except FileNotFoundError:
            continue
        L.append(f"### {label}  `results/opt/{fn}`")
        L += cat_table(bc)
        L.append("")
    return L


def main():
    L = ["# 自适应容错框架 — 原始实测数据（按故障类别）", "",
         "全部取自 harness 的 canonical 聚合 JSON（跨负载聚合、带 trials 与 95%CI），"
         "不逐负载展开、不手工再算、不臆造。Freivalds 暂无逐格数据，未纳入。", ""]
    L += sec_fi()
    L += sec_overhead()
    L += sec_coverage()
    out = os.path.join(HERE, "reports", "full_report.md")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w") as f:
        f.write("\n".join(L) + "\n")
    print("\n".join(L))
    print(f"\n[写入] {out}")


if __name__ == "__main__":
    main()

