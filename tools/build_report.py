#!/usr/bin/env python3
"""当前方案（x86 粗粒度路线）实验报告：ABFT / 粗粒度DMR / fork-TMR / SIMD-slack / 自适应。

数据源 results/opt：FI 覆盖聚合 JSON、粗粒度逐内核开销、多核扩展、每核属性，
+ 现跑自适应评测。不含已弃用的指令级方案。

用法：python3 tools/build_report.py   → tools/reports/full_report.md
"""
import csv
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
OPT = os.path.join(ROOT, "results", "opt")
sys.path.insert(0, os.path.join(ROOT, "experiments"))
from atft_core import build_menu                         # noqa: E402
from workload_eval import eval_workload                  # noqa: E402

CLASSES = [("fp64", "浮点"), ("int_arith", "整数"), ("cast", "类型转换"),
           ("load", "访存"), ("addr", "地址"), ("cmp_branch", "比较分支")]
KP = "kernel_properties_a4_t200_matrix_solid.csv"


def load_csv(name):
    with open(os.path.join(OPT, name)) as f:
        return list(csv.DictReader(f))


def load_json(name):
    with open(os.path.join(OPT, name)) as f:
        return json.load(f)


def bycat(name):
    return load_json(name)["native"]["by_category"]
def sec_menu():
    kp = {r["benchmark"]: r for r in load_csv(KP)}
    abft = [float(kp[b]["cyc_x_abft"]) for b in ("pb_2mm", "pb_3mm", "pb_gemm")]
    slack = float(kp["pb_seidel2d"]["cyc_x_simd_tmr"])
    ov = load_csv("coarse_overhead_subset.csv")
    dmr = [float(r["coarse_dmr_rerun"]) for r in ov]
    tmr = [float(r["coarse_tmr"]) for r in ov]
    L = ["## 一、方法总览", "",
         "| 方法 | 适用 | 治什么 | 开销× |", "| --- | --- | --- | --- |",
         f"| 代数校验 ABFT | 矩阵乘法 | 算错 | {min(abft):.1f}–{max(abft):.1f}× |",
         f"| SIMD-slack 打包 | 标量内核 | 算错 | {slack:.1f}× |",
         f"| 粗粒度 DMR+重执行 | 通用 | 算错 | "
         f"{min(dmr):.1f}–{max(dmr):.1f}×（单核）/ ~1×（多核）|",
         f"| 进程隔离 fork-TMR | 通用 | 算错 + 崩溃/挂起 | "
         f"{min(tmr):.1f}–{max(tmr):.1f}×（单核）/ ~1×（多核）|", ""]
    return L


def sec_coverage():
    rows_of = {"无防护": bycat("aggregate_a4_t200.json"),
               "粗粒度冗余": bycat("aggregate_a4_t200_coarse_fi.json"),
               "进程隔离冗余": bycat("aggregate_a4_t200_coarse_fork.json")}

    def tbl(metric):
        head = "| 方法 | " + " | ".join(l for _, l in CLASSES) + " |"
        sep = "| " + " | ".join(["---"] * (len(CLASSES) + 1)) + " |"
        out = [head, sep]
        for lab, bc in rows_of.items():
            vals = [f"{bc[k][metric]:.1f}" if k in bc else "—" for k, _ in CLASSES]
            out.append("| " + lab + " | " + " | ".join(vals) + " |")
        return out

    L = ["## 二、覆盖 · 按故障类别（单比特注入，跨负载聚合）", "",
         "算错(SDC)与崩溃挂起(DUE)率，单位 %。粗粒度治算错、治不了崩溃挂起；"
         "进程隔离两者都治。", "",
         "### 算错率 SDC%"] + tbl("sdc")
    L += ["", "### 崩溃/挂起率 DUE%"] + tbl("due") + [""]
    return L


def sec_overhead():
    ov = load_csv("coarse_overhead_subset.csv")
    sat = load_csv("core_saturation_sweep.csv")
    kp = {r["benchmark"]: r for r in load_csv(KP)}
    L = ["## 三、开销明细", "", "### 3.1 粗粒度 / 进程隔离 · 逐内核（单核串行）",
         "| 内核 | 粗粒度 DMR× | 进程隔离 TMR× |", "| --- | --- | --- |"]
    for r in ov:
        L.append(f"| {r['benchmark']} | {float(r['coarse_dmr_rerun']):.2f} "
                 f"| {float(r['coarse_tmr']):.2f} |")
    L += ["", "### 3.2 多核并行标度（副本摊到多核，墙钟开销）",
          "| 核数 | DMR 并行× | TMR 并行× |", "| --- | --- | --- |"]
    for r in sat:
        L.append(f"| {r['cores']} | {float(r['dmr_parallel_x']):.2f} "
                 f"| {float(r['tmr_parallel_x']):.2f} |")
    L += ["", "### 3.3 ABFT / SIMD-slack · 逐内核",
          "| 内核 | 方法 | 开销× |", "| --- | --- | --- |"]
    for b in ("pb_2mm", "pb_3mm", "pb_gemm"):
        L.append(f"| {b} | 代数校验 ABFT | {float(kp[b]['cyc_x_abft']):.2f} |")
    L.append(f"| pb_seidel2d | SIMD-slack | {float(kp['pb_seidel2d']['cyc_x_simd_tmr']):.2f} |")
    L.append("")
    return L


def sec_abft():
    kp = {r["benchmark"]: r for r in load_csv(KP)}
    L = ["## 四、代数校验 ABFT 明细（矩阵乘法族）", "",
         "| 内核 | 无防护错误% | ABFT后错误% | 开销× |", "| --- | --- | --- | --- |"]
    for b in ("pb_2mm", "pb_3mm", "pb_gemm"):
        r = kp[b]
        L.append(f"| {b} | {float(r['fp64_sdc_native']):.1f} | "
                 f"{float(r['fp64_sdc_abft']):.1f} | {float(r['cyc_x_abft']):.2f} |")
    L.append("")
    return L


def sec_adaptive(res):
    L = ["## 五、自适应调度（SAMPEX / Alsat-1 轨道）", "",
         "按轨道辐射动态选方法，满足可靠性预算前提下压开销。", "",
         "| 轨道 | 自适应× | 全保护× | 不防护× | 省 vs 全保护 | 达标 |",
         "| --- | --- | --- | --- | --- | --- |"]
    meet = {}
    for m in res["meets_eps"]:
        k = "LEO" if "LEO" in m["orbit"] else ("SSO" if "SSO" in m["orbit"] else m["orbit"])
        meet[k] = m["adaptive_meets_eps"]
    for c in res["continuous"]:
        k = "LEO" if "LEO" in c["orbit"] else "SSO"
        L.append(f"| {c['orbit']} | {c['adaptive']:.2f} | {c['full_protect']:.2f} | "
                 f"{c['native']:.2f} | {c['save_vs_full_pct']:.0f}% | "
                 f"{'是' if meet.get(k, True) else '否'} |")
    L.append("")
    return L


def sec_todo():
    return ["## 六、已知局限", "",
            "- 粗粒度在进程内治不了崩溃/挂起(DUE)，需进程隔离 fork-TMR 才一并清零。",
            "- ABFT 仅覆盖矩阵乘法族；其余结构交结果校验 / 粗粒度。",
            "- 并行 ~1× 要求空闲核 ≥ 副本数；少核场景回落到 2–3×。",
            "- 全程 x86 实测；RISC-V 仅做过静态+QEMU 旁路评估，未纳入本报告。", ""]


def main():
    res = eval_workload(build_menu())
    L = ["# 自适应容错框架 — 实验报告（x86 粗粒度路线）", ""]
    for part in (sec_menu(), sec_coverage(), sec_overhead(),
                 sec_abft(), sec_adaptive(res), sec_todo()):
        L += part
    out = os.path.join(HERE, "reports", "full_report.md")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w") as f:
        f.write("\n".join(L) + "\n")
    print(f"[写入] {out}")


if __name__ == "__main__":
    main()


