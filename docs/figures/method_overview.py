#!/usr/bin/env python3
"""
生成方法总体图：docs/figures/method_overview.png

四个模块的数据流，外加每个模块的判定点数与其中拍定的个数——
后者是这张图真正要讲的事：17 个判定点里 15 个没有标定依据。

用法：python3 docs/figures/method_overview.py
"""

from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.patches as mpatches
import matplotlib.pyplot as plt
from matplotlib.patches import FancyArrowPatch, FancyBboxPatch

plt.rcParams["font.sans-serif"] = ["WenQuanYi Micro Hei"]
plt.rcParams["axes.unicode_minus"] = False

OUT = Path(__file__).resolve().parent / "method_overview.png"

C_COMPILE = "#dde8f4"
C_RUNTIME = "#e5efe1"
C_PROVEN = "#cfe8cf"
C_TUNED = "#f6d6d0"
C_EDGE = "#49566b"
C_TEXT = "#1f2b38"
C_WARN = "#b1402c"


def box(ax, x, y, w, h, title, body="", fc="#ffffff", fs=10, bfs=8.5):
    ax.add_patch(FancyBboxPatch(
        (x, y), w, h, boxstyle="round,pad=0.02,rounding_size=0.08",
        linewidth=1.1, edgecolor=C_EDGE, facecolor=fc, zorder=2))
    if body:
        ax.text(x + w / 2, y + h - 0.28, title, ha="center", va="center",
                fontsize=fs, color=C_TEXT, weight="bold", zorder=3)
        ax.text(x + w / 2, y + (h - 0.36) / 2, body, ha="center", va="center",
                fontsize=bfs, color=C_TEXT, linespacing=1.55, zorder=3)
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


def main():
    fig, ax = plt.subplots(figsize=(13.9, 9.9))
    ax.set_xlim(0, 13.9)
    ax.set_ylim(0, 9.9)
    ax.axis("off")

    lane(ax, 5.35, 4.3, "编译期", C_COMPILE)
    lane(ax, 0.6, 4.3, "运行期", C_RUNTIME)

    # ---------------- 编译期 ----------------
    box(ax, 0.45, 7.9, 2.0, 1.15, "LLVM IR", "被测程序")
    box(ax, 2.85, 7.85, 3.1, 1.25, "关键性评分",
        "S = (w1·3)·closeness·opAmp\n+ w2·depth + w3·liveSpan", fc=C_TUNED)
    box(ax, 6.45, 7.9, 2.2, 1.15, "分档",
        "S≥6.5 CRITICAL\nS≥5.5 SUB，其余 NON", fc=C_TUNED)
    box(ax, 9.65, 7.85, 3.3, 1.25, "多版本 IR",
        "每段代码的 LOW / HIGH\n两档受保护版本")
    arrow(ax, (2.45, 8.48), (2.85, 8.48))
    arrow(ax, (5.95, 8.48), (6.45, 8.48))

    ax.text(0.45, 6.35, "近似变换", fontsize=11, weight="bold", color=C_TEXT)
    ax.text(0.45, 6.02, "按档位选", fontsize=9, color="#5a6b7c")
    for x, t_, b_, fc in (
            (3.0, "FP64 → 2×FP32", "两份近似打包进\n一条 SIMD 指令", C_TUNED),
            (6.0, "超越函数多项式", "sin/cos/exp\n阶数与系数固定", C_TUNED),
            (9.0, "i32 → 2×i16", "仅当 NumSignBits ≥ 17", C_PROVEN)):
        box(ax, x, 5.65, 2.7, 1.3, t_, b_, fc=fc, fs=9.5, bfs=8)
    for xt in (4.35, 7.35, 10.35):
        arrow(ax, (7.55, 7.9), (xt, 6.95), rad=-0.05)
    arrow(ax, (11.7, 6.95), (11.9, 7.85), rad=-0.1)

    # ---------------- 判定点统计带 ----------------
    ax.text(0.45, 5.08, "判定点 / 其中无标定依据", fontsize=10.5,
            weight="bold", color=C_TEXT, va="center")
    for i, (name, ratio) in enumerate(
            (("关键性评分", "4 / 4"), ("近似变换", "2 / 3"),
             ("三路表决", "4 / 5"), ("模式切换", "5 / 5"))):
        x = 4.25 + i * 1.85
        ax.text(x, 5.08, name, fontsize=9.5, color=C_TEXT, va="center")
        ax.text(x + 1.18, 5.08, ratio, fontsize=10, color=C_WARN,
                weight="bold", va="center")
    ax.text(11.7, 5.08, "合计 15 / 17", fontsize=10, color=C_WARN,
            weight="bold", va="center")

    # ---------------- 运行期 ----------------
    for i, t_ in enumerate(("精确链 R", "近似副本 a1", "近似副本 a2")):
        box(ax, 0.45, 3.45 - i * 0.95, 2.0, 0.8, t_,
            "全精度计算" if i == 0 else "降精度计算", fs=9.5, bfs=8)

    box(ax, 2.95, 1.35, 3.4, 2.9, "三路表决",
        "eff_delta = max(δ·(1+scale), 2e-6)\n\n"
        "快路径    |R−a1| ≤ eff_delta\n"
        "              且 |a1−a2| ≤ 1e-6·(1+scale)\n"
        "冷路径    两近似一致、R 偏离\n"
        "              → 判精确链故障\n"
        "单侧偏离 → 屏蔽该副本\n"
        "三路互不一致 → 不可恢复", fs=10.5, bfs=8.5)
    for y in (3.85, 2.9, 1.95):
        arrow(ax, (2.45, y), (2.95, 2.8))

    box(ax, 6.85, 2.5, 2.5, 1.7, "分级恢复",
        "CRITICAL  重执行\nSUB  采纳近似值\nNON  安全回退", fc=C_TUNED,
        fs=10.5, bfs=8.5)
    arrow(ax, (6.35, 2.8), (6.85, 3.2))
    box(ax, 6.85, 1.1, 2.5, 1.05, "输出", "SDC / DUE / 恢复", fs=9.5, bfs=8)
    arrow(ax, (8.1, 2.5), (8.1, 2.15))

    box(ax, 9.9, 2.95, 3.0, 1.25, "ECC 事件计数", "到达间隔 / 窗口累计数",
        fs=9.5, bfs=8)
    box(ax, 9.9, 1.2, 3.0, 1.35, "模式切换",
        "A 与 B 同时满足 → HIGH\n连续 M 窗低于水位 → LOW", fc=C_TUNED,
        fs=10.5, bfs=8.5)
    arrow(ax, (11.4, 2.95), (11.4, 2.55))
    arrow(ax, (13.5, 1.88), (12.95, 8.48), ls="--",
          cs="angle,angleA=0,angleB=-90,rad=0")
    ax.text(12.9, 4.55, "切换结果决定选哪一档版本", fontsize=9,
            color="#5a6b7c", va="center", ha="right")

    legend = [
        mpatches.Patch(facecolor=C_TUNED, edgecolor=C_EDGE, label="含拍定常数"),
        mpatches.Patch(facecolor=C_PROVEN, edgecolor=C_EDGE, label="判据可证明"),
        mpatches.Patch(facecolor="#ffffff", edgecolor=C_EDGE, label="无判定点"),
    ]
    ax.legend(handles=legend, loc="lower left", bbox_to_anchor=(0.018, 0.005),
              fontsize=9.5, frameon=False, ncol=3, handlelength=1.4,
              columnspacing=1.6)

    fig.subplots_adjust(left=0.005, right=0.995, top=0.995, bottom=0.005)
    fig.savefig(OUT, dpi=170, facecolor="white")
    print(f"-> {OUT}")


if __name__ == "__main__":
    main()
