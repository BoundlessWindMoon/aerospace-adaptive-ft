#!/usr/bin/env python3
"""
生成方法图：tools/reports/method_improved.png

只画最新数据（opt_i64 / 全开＋D4）真正跑的那套方法：
  - 浮点：一份精确 f64 链 ＋ 两份 f32 近似副本，阈值表决（δ_f 逐负载）
  - 整数 / i64 下标 / 比较：各三份精确副本，多数决接回数据流
  - 全量保护（budget=1.0），不做关键性选择、不做多版本、不做在线模式切换
底部给四项改动的改进前后数字。

用法：python3 tools/reports/method_improved.py
"""

from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.patches as mpatches
import matplotlib.pyplot as plt
from matplotlib.patches import FancyArrowPatch, FancyBboxPatch

plt.rcParams["font.sans-serif"] = ["WenQuanYi Micro Hei"]
plt.rcParams["axes.unicode_minus"] = False

OUT = Path(__file__).resolve().parent / "method_improved.png"

C_COMPILE = "#dde8f4"
C_RUNTIME = "#e5efe1"
C_APPROX = "#f6e0c8"
C_EXACT = "#cfe0ea"
C_IMPROVE = "#eef3da"
C_EDGE = "#49566b"
C_TEXT = "#1f2b38"
C_OK = "#2f7d4f"


def box(ax, x, y, w, h, title, body="", fc="#ffffff", fs=10, bfs=8.5):
    ax.add_patch(FancyBboxPatch(
        (x, y), w, h, boxstyle="round,pad=0.02,rounding_size=0.08",
        linewidth=1.1, edgecolor=C_EDGE, facecolor=fc, zorder=2))
    if body:
        ax.text(x + w / 2, y + h - 0.26, title, ha="center", va="center",
                fontsize=fs, color=C_TEXT, weight="bold", zorder=3)
        ax.text(x + w / 2, y + (h - 0.34) / 2, body, ha="center", va="center",
                fontsize=bfs, color=C_TEXT, linespacing=1.5, zorder=3)
    else:
        ax.text(x + w / 2, y + h / 2, title, ha="center", va="center",
                fontsize=fs, color=C_TEXT, weight="bold", zorder=3)
def arrow(ax, p0, p1, rad=0.0, ls="-", cs=None):
    ax.add_patch(FancyArrowPatch(
        p0, p1, arrowstyle="-|>", mutation_scale=13, linewidth=1.2,
        color=C_EDGE, linestyle=ls, zorder=4,
        connectionstyle=cs or f"arc3,rad={rad}"))


def lane(ax, y, h, label, fc):
    ax.add_patch(mpatches.Rectangle((0.2, y), 16.1, h, facecolor=fc,
                                    edgecolor="none", zorder=0))
    ax.text(0.42, y + h - 0.3, label, fontsize=12, weight="bold",
            color="#5a6b7c", va="center", zorder=1)


def chip(ax, x, w, tag, before_after, fc=C_IMPROVE):
    ax.add_patch(FancyBboxPatch(
        (x, -1.75), w, 1.12, boxstyle="round,pad=0.02,rounding_size=0.08",
        linewidth=1.1, edgecolor=C_EDGE, facecolor=fc, zorder=2))
    ax.text(x + w / 2, -0.93, tag, ha="center", va="center",
            fontsize=9.5, color=C_TEXT, weight="bold", zorder=3)
    ax.text(x + w / 2, -1.42, before_after, ha="center", va="center",
            fontsize=9, color=C_OK, weight="bold", zorder=3)


def main():
    fig, ax = plt.subplots(figsize=(13.9, 11.4))
    ax.set_xlim(0, 13.9)
    ax.set_ylim(-2.3, 9.0)
    ax.axis("off")

    lane(ax, 4.75, 4.1, "编译期", C_COMPILE)
    lane(ax, 0.3, 4.3, "运行期", C_RUNTIME)

    # ---------------- 编译期 ----------------
    box(ax, 0.45, 6.75, 1.95, 1.15, "LLVM IR", "被测程序")
    box(ax, 2.75, 6.75, 2.15, 1.15, "插桩复制", "全量\nbudget = 1.0")
    box(ax, 5.25, 7.3, 3.7, 1.05, "近似冗余 · 浮点",
        "精确 f64 链 ＋ 两份 f32 近似副本（SIMD <2×f32>）", fc=C_APPROX,
        fs=9.5, bfs=8)
    box(ax, 5.25, 6.0, 3.7, 1.05, "精确冗余 · 整数 / 比较",
        "各三份精确副本（含 i64 下标扩展）", fc=C_EXACT, fs=9.5, bfs=8)
    box(ax, 9.3, 6.75, 3.1, 1.15, "受保护 IR", "")
    arrow(ax, (2.40, 7.32), (2.75, 7.32))
    arrow(ax, (4.90, 7.32), (5.25, 7.82), rad=0.0)
    arrow(ax, (4.90, 7.32), (5.25, 6.52), rad=0.0)
    arrow(ax, (8.95, 7.82), (9.30, 7.40), rad=0.0)
    arrow(ax, (8.95, 6.52), (9.30, 7.24), rad=0.0)
    # ---------------- 运行期：浮点阈值表决 ----------------
    box(ax, 0.45, 3.55, 2.0, 0.82, "精确链 R", "全精度 f64", fs=9.5, bfs=8)
    box(ax, 0.45, 2.55, 2.0, 0.82, "近似副本 a1 / a2", "降精度 f32", fs=9.5, bfs=8)
    box(ax, 2.85, 1.9, 3.6, 2.6, "浮点阈值表决",
        "|R − a1| ≤ δ_f·(1+|a1|)\nδ_f 逐负载剖析定值\na1 = a2 按位相等\n\n"
        "R 正常 → 输出 R\nR 偏离且两近似一致 → 输出 a1", fc=C_APPROX,
        fs=10.5, bfs=8.5)
    arrow(ax, (2.45, 3.96), (2.85, 3.4))
    arrow(ax, (2.45, 2.96), (2.85, 3.0))

    # ---------------- 运行期：整数 / 比较多数决 ----------------
    box(ax, 6.95, 3.55, 3.5, 0.9, "整数 / 比较 · 三份精确副本",
        "逐指令复制，保持可注错", fs=9.5, bfs=8)
    box(ax, 6.95, 1.5, 3.5, 1.75, "多数决 · 接回数据流",
        "store / 地址 / i64 下标前\n分支前（br / select / switch）\n→ 输出多数值",
        fc=C_EXACT, fs=10.5, bfs=8.5)
    arrow(ax, (8.7, 3.55), (8.7, 3.25))

    box(ax, 11.0, 2.3, 2.55, 1.35, "输出", "SDC / DUE", fs=10, bfs=8.5)
    arrow(ax, (6.45, 4.15), (11.0, 3.45), rad=0.0)
    arrow(ax, (10.45, 2.4), (11.0, 2.75), rad=0.0)

    # ---------------- 本轮改进带 ----------------
    ax.add_patch(mpatches.Rectangle((0.2, -2.0), 16.1, 1.95, facecolor=C_IMPROVE,
                                    edgecolor="none", zorder=0))
    ax.text(0.42, -0.33, "本轮改进（已落地）", fontsize=12, weight="bold",
            color="#5a6b7c", va="center", zorder=1)
    ax.text(5.6, -0.33, "等权 SDC 27.5 → 2.2，DUE 14.3 → 0.02，开销 9.3×",
            fontsize=10, color=C_OK, weight="bold", va="center", zorder=1)
    chip(ax, 0.45, 3.0, "D2 控制平面三模", "cmp_branch 63.8 → 0.0")
    chip(ax, 3.70, 3.0, "D3 逐负载阈值", "误报 1340 → 0")
    chip(ax, 6.95, 3.0, "D8 整数接回数据流", "int_arith 44.1 → 0.04")
    chip(ax, 10.20, 3.0, "D4 cast i64 扩面", "cast DUE 57.7 → 0.0")

    legend = [
        mpatches.Patch(facecolor=C_APPROX, edgecolor=C_EDGE, label="近似冗余（浮点）"),
        mpatches.Patch(facecolor=C_EXACT, edgecolor=C_EDGE, label="精确冗余（整数/比较）"),
        mpatches.Patch(facecolor="#ffffff", edgecolor=C_EDGE, label="无冗余"),
    ]
    ax.legend(handles=legend, loc="upper center", bbox_to_anchor=(0.5, 1.01),
              fontsize=9.5, frameon=False, ncol=3, handlelength=1.4,
              columnspacing=1.8)

    fig.subplots_adjust(left=0.005, right=0.995, top=0.985, bottom=0.005)
    fig.savefig(OUT, dpi=170, facecolor="white")
    print(f"-> {OUT}")


if __name__ == "__main__":
    main()


