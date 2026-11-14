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
    fig, ax = plt.subplots(figsize=(17.3, 10))
    ax.set_xlim(0, 17.3)
    ax.set_ylim(0, 10.1)
    ax.axis("off")

    lane(ax, 5.75, 4.2, "编译期", C_COMPILE)
    lane(ax, 0.75, 4.3, "运行期", C_RUNTIME)

    # ---------------- 编译期 ----------------
    box(ax, 0.6, 8.3, 2.0, 1.1, "LLVM IR", "被测程序")
    box(ax, 3.1, 8.2, 3.1, 1.3, "关键性评分",
        "S = (w1·3)·closeness·opAmp\n+ w2·depth + w3·liveSpan", fc=C_TUNED)
    box(ax, 6.7, 8.3, 2.1, 1.1, "分档",
        "S≥6.5 CRITICAL\nS≥5.5 SUB，其余 NON", fc=C_TUNED)
    box(ax, 9.3, 8.3, 1.9, 1.1, "近似变换", "按档位选")
    box(ax, 11.9, 8.2, 3.8, 1.3, "多版本 IR",
        "每段代码的 LOW / HIGH\n两档受保护版本")

    for x, t, b, fc in (
            (8.1, "FP64 → 2×FP32", "两份近似打包进\n一条 SIMD 指令", C_TUNED),
            (10.35, "超越函数多项式", "sin/cos/exp\n阶数与系数固定", C_TUNED),
            (12.6, "i32 → 2×i16", "仅当\nNumSignBits ≥ 17", C_PROVEN)):
        box(ax, x, 6.25, 2.15, 1.3, t, b, fc=fc, fs=9.5, bfs=8)

    arrow(ax, (2.6, 8.85), (3.1, 8.85))
    arrow(ax, (6.2, 8.85), (6.7, 8.85))
    arrow(ax, (8.8, 8.85), (9.3, 8.85))
    arrow(ax, (11.2, 8.85), (11.9, 8.85))
    for xt in (9.18, 11.43, 13.68):
        arrow(ax, (10.25, 8.3), (xt, 7.55), rad=-0.05)

    # ---------------- 判定点统计带 ----------------
    ax.text(0.42, 5.42, "判定点 / 其中无标定依据", fontsize=11,
            weight="bold", color=C_TEXT)
    stats = [("关键性评分", "4 / 4"), ("近似变换", "2 / 3"),
             ("三路表决", "4 / 5"), ("模式切换", "5 / 5")]
    for i, (name, ratio) in enumerate(stats):
        x = 5.0 + i * 2.1
        ax.text(x, 5.42, name, fontsize=10, color=C_TEXT, va="center")
        ax.text(x + 1.32, 5.42, ratio, fontsize=10.5, color=C_WARN,
                weight="bold", va="center")
    ax.text(13.6, 5.42, "合计 15 / 17 拍定，1 个可证明",
            fontsize=10.5, color=C_WARN, weight="bold", va="center")

    # ---------------- 运行期 ----------------
    for i, t in enumerate(("精确链 R", "近似副本 a1", "近似副本 a2")):
        body = "全精度计算" if i == 0 else "降精度计算"
        box(ax, 0.6, 3.55 - i * 1.0, 2.1, 0.85, t, body, fs=9.5, bfs=8)

    box(ax, 3.3, 1.5, 3.5, 2.9, "三路表决",
        "eff_delta = max(δ·(1+scale), 2e-6)\n\n"
        "快路径    |R−a1| ≤ eff_delta\n"
        "              且 |a1−a2| ≤ 1e-6·(1+scale)\n"
        "冷路径    两近似一致、R 偏离\n"
        "              → 判精确链故障\n"
        "单侧偏离 → 屏蔽该副本\n"
        "三路互不一致 → 不可恢复",
        fs=10.5, bfs=8.5)
    for y in (3.98, 2.98, 1.98):
        arrow(ax, (2.7, y), (3.3, 2.95))

    box(ax, 7.3, 2.45, 2.7, 1.7, "分级恢复",
        "CRITICAL  重执行\nSUB  采纳近似值\nNON  安全回退", fc=C_TUNED,
        fs=10.5, bfs=8.5)
    arrow(ax, (6.8, 2.95), (7.3, 3.3))

    box(ax, 7.3, 1.0, 2.7, 1.05, "输出", "SDC / DUE / 恢复", fs=9.5, bfs=8)
    arrow(ax, (8.65, 2.45), (8.65, 2.05))

    box(ax, 10.6, 1.1, 2.3, 1.2, "ECC 事件计数", "到达间隔\n窗口累计数",
        fs=9.5, bfs=8)
    box(ax, 13.4, 1.0, 2.9, 1.4, "模式切换",
        "指标 A 与 B 同时满足 → HIGH\n连续 M 窗低于水位 → LOW",
        fc=C_TUNED, fs=10.5, bfs=8.5)
    arrow(ax, (12.9, 1.7), (13.4, 1.7))

    # 反馈：模式切换 -> 多版本 IR，走最右侧，避开所有方框
    arrow(ax, (16.3, 1.7), (15.7, 8.85), ls="--",
          cs="angle,angleA=0,angleB=-90,rad=0")
    ax.text(16.15, 3.1, "选哪一档版本", fontsize=9.5, color="#5a6b7c",
            va="center", ha="right")

    legend = [
        mpatches.Patch(facecolor=C_TUNED, edgecolor=C_EDGE, label="含拍定常数"),
        mpatches.Patch(facecolor=C_PROVEN, edgecolor=C_EDGE, label="判据可证明"),
        mpatches.Patch(facecolor="#ffffff", edgecolor=C_EDGE, label="无判定点"),
    ]
    ax.legend(handles=legend, loc="lower left", bbox_to_anchor=(0.015, 0.005),
              fontsize=10, frameon=False, ncol=3, handlelength=1.4,
              columnspacing=1.6)

    fig.subplots_adjust(left=0.004, right=0.996, top=0.996, bottom=0.004)
    fig.savefig(OUT, dpi=160, facecolor="white")
    print(f"-> {OUT}")


if __name__ == "__main__":
    main()
