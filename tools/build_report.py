#!/usr/bin/env python3
"""原始实测数据报告：逐格吐出故障注入与 PMU 的真实测量，带 trials 与 95% 置信区间。

不做任何手工聚合/区间化——每一行对应 canonical CSV 里的一条实测记录，可逐条核对。
生成数据分散各源（见各节数据源），本工具只把 raw 渲染成 md。

用法：
    python3 tools/build_report.py              # → tools/reports/full_report.md
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(ROOT, "experiments"))

from atft_core import OPT, load_csv              # noqa: E402

FT = {"fp64": "浮点", "int_arith": "整数", "cast": "类型转换",
      "load": "访存", "addr": "地址", "cmp_branch": "比较分支"}
POL = {"native": "无防护", "swift": "SWIFT 指令双模", "exact-tmr": "指令级精确TMR",
       "simd_tmr": "指令级SIMD-TMR", "abft_auto": "代数校验ABFT",
       "opt_base": "优化基线", "opt_simd_f64": "优化SIMD-f64", "opt_layered": "分层优化"}


def load_json(name):
    with open(os.path.join(OPT, name)) as f:
        return json.load(f)
def sec_fi():
    rows = [r for r in load_csv("fi_outcomes_a4_t200_matrix_solid.csv")
            if r["policy"] != "policy" and float(r["trials"] or 0) > 0]
    tot = sum(int(float(r["trials"])) for r in rows)
    L = ["## A. 故障注入逐格结果（solid 矩阵）", "",
         f"每行 = 一个 (策略 × 负载 × 故障类别) 格，各 200 次单比特注入；"
         f"共 {len(rows)} 格 / {tot:,} 次注入。SDC% 后括号为 95% 置信区间。", "",
         "数据源：`results/opt/fi_outcomes_a4_t200_matrix_solid.csv`", ""]
    by_pol = {}
    for r in rows:
        by_pol.setdefault(r["policy"], []).append(r)
    for pol in ["native", "swift", "exact-tmr", "simd_tmr", "abft_auto",
                "opt_base", "opt_simd_f64", "opt_layered"]:
        if pol not in by_pol:
            continue
        L += [f"### 策略 {pol}（{POL.get(pol, pol)}）",
              "| 负载 | 故障类别 | trials | masked% | 纠正% | SDC% (95%CI) | DUE% |",
              "| --- | --- | --- | --- | --- | --- | --- |"]
        for r in sorted(by_pol[pol], key=lambda x: (x["benchmark"], x["fi_target_type"])):
            ci = f"{float(r['sdc_ci95_lo']):.1f}–{float(r['sdc_ci95_hi']):.1f}"
            L.append(f"| {r['benchmark']} | {FT.get(r['fi_target_type'], r['fi_target_type'])} "
                     f"| {int(float(r['trials']))} | {float(r['masked_pct']):.1f} "
                     f"| {float(r['det_recovered_pct']):.1f} "
                     f"| {float(r['sdc_pct']):.1f} ({ci}) | {float(r['due_pct']):.1f} |")
        L.append("")
    return L


def sec_pmu():
    rows = [r for r in load_csv("pmu_a4_t200_matrix_solid.csv")
            if r["policy"] != "policy"]
    base = {r["benchmark"]: float(r["hw_cycles"])
            for r in rows if r["policy"] == "native" and float(r["hw_cycles"]) > 0}
    L = ["## B. 开销逐格（PMU 实测）", "",
         f"每行 = 一个 (策略 × 负载) 的硬件计数器实测；共 {len(rows)} 格。"
         "开销× = 该格 hw_cycles / 同负载 native hw_cycles。", "",
         "数据源：`results/opt/pmu_a4_t200_matrix_solid.csv`", "",
         "| 负载 | 策略 | 执行(us) | 硬件指令数 | 硬件周期数 | 开销× |",
         "| --- | --- | --- | --- | --- | --- |"]
    for r in sorted(rows, key=lambda x: (x["benchmark"], x["policy"])):
        cyc = float(r["hw_cycles"])
        b = base.get(r["benchmark"], 0.0)
        ovh = f"{cyc / b:.2f}" if b > 0 else "—"
        L.append(f"| {r['benchmark']} | {r['policy']} | {float(r['exec_time_us']):.0f} "
                 f"| {int(float(r['hw_insts']))} | {int(cyc)} | {ovh} |")
    L.append("")
    return L


def sec_coverage():
    sources = [("native", "aggregate_a4_t200.json", "无防护基线"),
               ("coarse", "aggregate_a4_t200_coarse_fi.json", "整内核冗余后"),
               ("fork", "aggregate_a4_t200_coarse_fork.json", "进程隔离冗余后")]
    L = ["## C. 覆盖逐类别原始（带 trials 与 95%CI）", "",
         "每行 = 一个故障类别聚合（跨负载），来自各自的注入活动；SDC/DUE 后为 95%CI 半宽。",
         ""]
    for key, fn, label in sources:
        try:
            bc = load_json(fn)["native"]["by_category"]
        except FileNotFoundError:
            continue
        L += [f"### {label}  数据源：`results/opt/{fn}`",
              "| 故障类别 | trials | SDC% (±95%CI) | DUE% (±95%CI) |",
              "| --- | --- | --- | --- |"]
        for k, lab in FT.items():
            if k not in bc:
                continue
            v = bc[k]
            L.append(f"| {lab} | {int(v['trials'])} | "
                     f"{v['sdc']:.2f} (±{v['sdc_ci95']:.2f}) | "
                     f"{v['due']:.2f} (±{v['due_ci95']:.2f}) |")
        L.append("")
    return L


def main():
    L = ["# 自适应容错框架 — 原始实测数据", "",
         "逐格原始测量，无手工聚合；每行可对回 canonical CSV。"
         "（Freivalds 结果校验暂无逐格 CSV，未纳入，需要再单独跑一轮产出 raw。）", ""]
    L += sec_fi()
    L += sec_pmu()
    L += sec_coverage()
    out = os.path.join(HERE, "reports", "full_report.md")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w") as f:
        f.write("\n".join(L) + "\n")
    n = len([x for x in L if x.startswith("| ") and not x.startswith("| ---")])
    print(f"[写入] {out}  约 {n} 行数据")


if __name__ == "__main__":
    main()


