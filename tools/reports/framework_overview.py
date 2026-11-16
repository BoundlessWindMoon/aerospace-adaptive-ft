#!/usr/bin/env python3
"""
生成框架全貌图：tools/reports/framework_overview.png

一张图讲清"按结构付费 + 辐射驱动分层切换"：
  - 中间：输入 → 自动分层判据 → 三层保护阶梯（开销随可用结构升高）
  - 顶部：辐射强度 λ(t) 驱动层间切换，决定启用哪一档
  - 底部：任务级可靠性账本

用法：python3 tools/reports/framework_overview.py
"""

from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.patches as mpatches
import matplotlib.pyplot as plt
from matplotlib.patches import FancyArrowPatch, FancyBboxPatch

plt.rcParams["font.sans-serif"] = ["WenQuanYi Micro Hei"]
plt.rcParams["axes.unicode_minus"] = False

OUT = Path(__file__).resolve().parent / "framework_overview.png"

C_ALG = "#cfe8cf"
C_CTRL = "#d7e5f3"
C_INST = "#f6e0c8"
C_SWITCH = "#e7dcf0"
C_REL = "#eef3da"
C_IO = "#ffffff"
C_EDGE = "#49566b"
C_TEXT = "#1f2b38"
C_MUTE = "#5a6b7c"


def box(ax, x, y, w, h, title, body="", fc="#ffffff", fs=11, bfs=9):
    ax.add_patch(FancyBboxPatch(
        (x, y), w, h, boxstyle="round,pad=0.02,rounding_size=0.08",
        linewidth=1.1, edgecolor=C_EDGE, facecolor=fc, zorder=3))
    if body:
        ax.text(x + w / 2, y + h - 0.30, title, ha="center", va="center",
                fontsize=fs, color=C_TEXT, weight="bold", zorder=4)
        ax.text(x + w / 2, y + (h - 0.40) / 2, body, ha="center", va="center",
                fontsize=bfs, color=C_TEXT, linespacing=1.5, zorder=4)
    else:
        ax.text(x + w / 2, y + h / 2, title, ha="center", va="center",
                fontsize=fs, color=C_TEXT, weight="bold", zorder=4)


def arrow(ax, p0, p1, ls="-", rad=0.0):
    ax.add_patch(FancyArrowPatch(
        p0, p1, arrowstyle="-|>", mutation_scale=14, linewidth=1.3,
        color=C_EDGE, linestyle=ls, zorder=2,
        connectionstyle=f"arc3,rad={rad}"))
def main():
    fig, ax = plt.subplots(figsize=(14.0, 9.2))
    ax.set_xlim(0, 14)
    ax.set_ylim(0.3, 9.4)
    ax.axis("off")

    ax.text(0.5, 9.1, "按程序结构付费的分层容错框架",
            fontsize=15, weight="bold", color=C_TEXT)

    # ---------------- 顶部：辐射驱动切换 ----------------
    ax.text(0.6, 8.78, "辐射强度 λ(t)", fontsize=10.5, weight="bold", color=C_MUTE)
    xs = [0.7, 1.1, 1.5, 1.9, 2.1, 2.25, 2.4, 2.6, 3.0, 3.5, 3.9, 4.05, 4.2,
          4.45, 4.9, 5.3]
    ys = [7.6, 7.58, 7.62, 7.6, 7.9, 8.55, 7.95, 7.6, 7.58, 7.62, 7.8, 8.45,
          7.85, 7.6, 7.6, 7.6]
    ax.plot(xs, ys, color="#b1402c", linewidth=1.6, zorder=3)
    ax.text(2.35, 8.72, "SAA", fontsize=8.5, color="#b1402c", ha="center")
    ax.text(4.08, 8.62, "耀斑", fontsize=8.5, color="#b1402c", ha="center")
    ax.text(0.7, 7.35, "平静段低　·　异常区 / 耀斑高几个数量级",
            fontsize=9, color=C_MUTE)

    box(ax, 6.0, 7.35, 3.7, 1.25, "层间切换 · 泊松 SPRT",
        "按 λ 在线判决启用哪一档\n误报率 / 检测延迟由 α,β 指定", fc=C_SWITCH,
        fs=11, bfs=9)
    arrow(ax, (5.35, 7.95), (6.0, 7.95))
    arrow(ax, (9.1, 7.35), (10.7, 6.75), ls="--")
    ax.text(10.75, 7.05, "决定启用哪一档", fontsize=9, color=C_MUTE, ha="left")

    # ---------------- 中部：输入 + 判据 + 三层阶梯 ----------------
    box(ax, 0.5, 4.75, 1.85, 1.0, "被测程序", "")
    box(ax, 2.75, 4.55, 2.35, 1.35, "自动分层判据",
        "能否搜到降阶的\n等价校验式？", fc=C_IO, fs=11, bfs=9)
    arrow(ax, (2.35, 5.25), (2.75, 5.22))

    box(ax, 5.7, 5.75, 7.4, 0.9,
        "代数校验层 · 规整数值内核（归约 / 线性映射） · 1.04×", fc=C_ALG, fs=11)
    box(ax, 5.7, 4.70, 7.4, 0.9,
        "控制平面层 · 驱动分支的比较 · 2.35×", fc=C_CTRL, fs=11)
    box(ax, 5.7, 3.65, 7.4, 0.9,
        "粗粒度冗余层 · 通用代码 · 单核 2–3× / 核 ≥ 3 墙钟 ≈1×", fc=C_INST, fs=11)
    arrow(ax, (5.10, 5.35), (5.70, 6.05))
    ax.text(5.28, 5.95, "搜到结构", fontsize=8.5, color=C_MUTE)
    arrow(ax, (5.10, 5.05), (5.70, 4.10))
    ax.text(5.20, 4.30, "搜不到 → 回落", fontsize=8.5, color=C_MUTE)
    ax.text(9.4, 3.42, "分支密集时叠加于上", fontsize=8.5, color=C_MUTE,
            ha="center")

    # 结构/开销梯度注记（阶梯右侧）
    ax.add_patch(FancyArrowPatch((13.35, 6.3), (13.35, 3.9),
                 arrowstyle="-|>", mutation_scale=12, linewidth=1.2,
                 color=C_MUTE, zorder=2))
    ax.text(13.62, 5.1, "可用结构 多→少　开销 低→高", fontsize=9,
            color=C_MUTE, rotation=90, va="center", ha="center")

    # ---------------- 底部：可靠性账本 ----------------
    arrow(ax, (9.4, 3.65), (9.4, 2.45))
    box(ax, 3.1, 1.05, 7.7, 1.3, "可靠性账本 · SAMPEX 轨道平均",
        "整数通路每 90 天失效 7977 → <1.53　｜　MWTF ≥112× 无保护"
        "　｜　轨道违约率 ~20% → 0", fc=C_REL, fs=11, bfs=9)

    legend = [
        mpatches.Patch(facecolor=C_ALG, edgecolor=C_EDGE, label="代数校验层"),
        mpatches.Patch(facecolor=C_CTRL, edgecolor=C_EDGE, label="控制平面层"),
        mpatches.Patch(facecolor=C_INST, edgecolor=C_EDGE, label="粗粒度冗余层"),
        mpatches.Patch(facecolor=C_SWITCH, edgecolor=C_EDGE, label="辐射驱动切换"),
    ]
    ax.legend(handles=legend, loc="lower center", bbox_to_anchor=(0.5, -0.01),
              fontsize=9.5, frameon=False, ncol=4, handlelength=1.4,
              columnspacing=2.0)

    fig.subplots_adjust(left=0.01, right=0.99, top=0.99, bottom=0.04)
    fig.savefig(OUT, dpi=170, facecolor="white")
    print(f"-> {OUT}")


if __name__ == "__main__":
    main()

