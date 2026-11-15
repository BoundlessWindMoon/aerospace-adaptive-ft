#!/usr/bin/env python3
"""校验式的"检测能力"度量与数值验证（离线，不依赖 LLVM，不跑注入管线）

动机：`eqsat_checker.py` 的提取目标只有代价多项式，能力是 `checksum_capability()`
里写死的一组常量（rank=1）。而冻结数据显示单故障模型下 rank-1 已经饱和
（pb_gemm 检出恢复 68.00% = 100 − masked 32.00%，SDC 清零），所以"能力"这个维度
只可能在三个地方产生差别：

  1. k ≥ 2 的多错（空间 MBU / 时间 burst，rq6 已有 native 侧数据）；
  2. 定位能力 —— 能定位就能按位重算，不需要 O(n^2) 的原数据快照；
  3. 阈值 τ —— 加权校验式把 max|w| 乘进舍入界，抬高检测地板。

本脚本把这三件事做成可计算量，并用真实 fp64 gemm 验证它们是否预测实测检出率。
度量全部由校验矩阵与舍入界导出，不含任何 benchmark 常数。

用法：
    python3 experiments/abft_capability.py                 # 结构度量 + 数值仿真
    python3 experiments/abft_capability.py --n 128 --trials 2000
"""

import argparse
import csv
import itertools
import json
import math
import os

import numpy as np

U_FP64 = 2.0 ** -53


# ------------------------------------------------------------------ 候选校验族
# 一个候选 = 若干个作用在输出矩阵 C (M x N) 上的线性泛函。
# 每个泛函给成 (行掩码 r, 列权重 w)：check = sum_{i,j} r[i] * w[j] * C[i,j]
# 这样既能表达行和/列和，也能表达加权和与分块网格，且不依赖具体内核。


def _eye_rows(m):
    return [np.eye(m)[i] for i in range(m)]


def _hadamard_signs(n, bits):
    """第 b 位的 ±1 模式：w[j] = +1 若 j 的第 b 位为 0，否则 -1。

    max|w| = 1，所以不抬高舍入界；bits 条这样的校验式就能把单错的列下标
    按二进制位逐位判出来（符号模式即下标），这是"定位"而不是"检测"。
    """
    out = []
    for b in range(bits):
        w = np.where((np.arange(n) >> b) & 1 == 0, 1.0, -1.0)
        out.append(w)
    return out


def candidates(m, n):
    """返回 [(名字, [(行掩码, 列权重), ...], 说明)]。"""
    ones_r = np.ones(m)
    ones_c = np.ones(n)
    bits = max(1, int(math.ceil(math.log2(n))))
    cs = []

    # 当前方法：逐行一个无权校验（沿检查方向求和）
    cs.append(("row_sum", [(r, ones_c) for r in _eye_rows(m)], "当前 eqsat 产物"))
    # 列校验
    cs.append(("col_sum", [(ones_r, c) for c in _eye_rows(n)], "转置方向"))
    # Huang & Abraham 1984 的行列双校验
    cs.append(("row+col", [(r, ones_c) for r in _eye_rows(m)]
               + [(ones_r, c) for c in _eye_rows(n)], "行列双校验和"))
    # 经典加权校验和（Jou & Abraham）：w(j) = j+1，可定位但 max|w| = n
    cs.append(("row+weighted_idx",
               [(r, ones_c) for r in _eye_rows(m)]
               + [(r, np.arange(1.0, n + 1.0)) for r in _eye_rows(m)],
               "加权校验和 w=j+1"))
    # 本脚本要检验的替代：±1 符号模式，log2(n) 条，max|w| = 1
    cs.append(("row+sign_bits",
               [(r, ones_c) for r in _eye_rows(m)]
               + [(r, w) for r in _eye_rows(m) for w in _hadamard_signs(n, bits)],
               "符号位定位校验"))
    # 分块网格（2x2 块），对标 2025 年手工设计的 grid-like 码
    half_r = [np.where(np.arange(m) < m // 2, 1.0, 0.0),
              np.where(np.arange(m) >= m // 2, 1.0, 0.0)]
    half_c = [np.where(np.arange(n) < n // 2, 1.0, 0.0),
              np.where(np.arange(n) >= n // 2, 1.0, 0.0)]
    cs.append(("grid_2x2",
               [(r, c) for r in _eye_rows(m) for c in half_c]
               + [(r, c) for r in half_r for c in _eye_rows(n)],
               "行列各切两块的网格码"))
    return cs


# ------------------------------------------------------------------ 结构度量
def check_matrix(checks, m, n):
    """展平成 W (k x m*n)：第 t 行是第 t 个泛函对每个输出元素的系数。"""
    return np.stack([np.outer(r, w).reshape(-1) for r, w in checks])


def min_dependent_size(W, kmax=4):
    """最小线性相关列数 d；保证可检出的错误个数 = d - 1。

    对小规模实例穷举（列数 m*n 在 8x8 下是 64，C(64,4)=635k，秒级）。
    d 只取决于校验矩阵的结构，与规模的关系由构造本身给出，所以用小实例定出
    d 再按构造外推是正确的，不是抽样估计。
    """
    cols = [W[:, j] for j in range(W.shape[1])]
    nz = [j for j, c in enumerate(cols) if np.any(c != 0)]
    if len(nz) < W.shape[1]:
        return 1          # 有全零列：该位置的错误完全不可见
    for k in range(2, kmax + 1):
        for comb in itertools.combinations(nz, k):
            sub = np.stack([cols[j] for j in comb], axis=1)
            if np.linalg.matrix_rank(sub, tol=1e-9) < k:
                return k
    return kmax + 1       # 下界


def localizable(W):
    """单错是否可唯一定位：各列两两不成比例（含不等零）。"""
    cols = W.T
    norms = np.linalg.norm(cols, axis=1, keepdims=True)
    if np.any(norms == 0):
        return False
    unit = cols / norms
    g = unit @ unit.T
    off = g - np.eye(g.shape[0])
    return bool(np.max(np.abs(off)) < 1 - 1e-9)


def structural_metrics(name, checks, m, n, kmax=4):
    W = check_matrix(checks, m, n)
    k = W.shape[0]
    d = min_dependent_size(W, kmax=kmax)
    maxw = max(float(np.max(np.abs(w))) for _, w in checks)
    return {
        "candidate": name,
        "n_checks_per_output_row": round(k / m, 4),
        "checks_total_at_m_n": k,
        "rank": int(np.linalg.matrix_rank(W, tol=1e-9)),
        "min_dependent_cols": d,
        "guaranteed_detect_k": d - 1,
        "single_error_localizable": localizable(W),
        "tau_amplification_maxw": maxw,
        "correction_memory": "O(1) recompute" if localizable(W) else "O(m*n) snapshot",
    }


def cost_families(checks, m, n):
    """代价以"校验族"计：一个族 = 一个权重向量在所有行（或所有列）上复用一次。

    每个族的代价是 K*n（预先把 B 沿权重压掉）+ m*K（压后的矩阵再乘 A）+ m*n
    （输出侧求和），即 O(n^2)；内核是 O(n^3)。所以开销倍率 ≈ families * c / n，
    families 就是代价的领头系数，可以直接比。
    """
    row_fams, col_fams = set(), set()
    for r, w in checks:
        if np.count_nonzero(r) == 1:
            row_fams.add(np.array2string(w, precision=6))
        elif np.count_nonzero(w) == 1:
            col_fams.add(np.array2string(r, precision=6))
        else:
            row_fams.add(np.array2string(w, precision=6) + "|" + np.array2string(r))
    return len(row_fams) + len(col_fams)


# ------------------------------------------------------------------ 数值仿真
def lowered_check(A, B, r, w):
    """校验式的降阶形式（eqsat 搜出来的那条路径）：r^T (A (B w))。

    rhs 走输入通路，lhs 走输出通路，两者的舍入差就是检测地板的来源。
    """
    return float(r @ (A @ (B @ w)))


def flip_random_bit(x, rng, low_bit=0):
    b = int(rng.integers(low_bit, 64))
    u = np.array([x], dtype=np.float64).view(np.uint64)
    u[0] ^= np.uint64(1) << np.uint64(b)
    return float(u.view(np.float64)[0]), b


def simulate(cands, n, trials, kfaults, kappa, rng, low_bit=0):
    """真实 fp64 gemm + 输出位翻转，统计每个候选的检出率/定位率/修复正确率。"""
    m = n
    A = rng.standard_normal((m, n))
    B = rng.standard_normal((n, n))
    C = A @ B
    nrmA = float(np.max(np.abs(A).sum(axis=1)))
    nrmB = float(np.max(np.abs(B).sum(axis=1)))

    prepared = []
    for name, checks, note in cands:
        W = check_matrix(checks, m, n)
        rhs = np.array([lowered_check(A, B, r, w) for r, w in checks])
        lhs0 = W @ C.reshape(-1)
        tau = np.array([kappa * U_FP64 * n * float(np.max(np.abs(w))) * nrmA * nrmB
                        for _, w in checks])
        prepared.append([name, checks, W, rhs, lhs0, tau, note,
                         {"det": 0, "loc": 0, "fix": 0, "fa": 0,
                          "resid_max": float(np.max(np.abs(lhs0 - rhs)))}])

    flat = C.reshape(-1)
    for _ in range(trials):
        pos = rng.choice(flat.size, size=kfaults, replace=False)
        err = np.zeros(flat.size)
        for p in pos:
            bad, _ = flip_random_bit(flat[p], rng, low_bit)
            if not np.isfinite(bad):
                bad = flat[p]          # NaN/Inf 另有判据接管，这里不计
            err[p] = bad - flat[p]
        if not np.any(err != 0.0):
            continue
        for st in prepared:
            _, _, W, rhs, lhs0, tau, _, acc = st
            with np.errstate(over="ignore", invalid="ignore"):
                s = (lhs0 + W @ err) - rhs
            if not np.any(np.abs(s) > tau):
                continue
            acc["det"] += 1
            if kfaults != 1:
                continue
            j, alpha = locate(W, s)
            if j == int(pos[0]):
                acc["loc"] += 1
                if abs(alpha - err[pos[0]]) <= 1e-6 * max(1.0, abs(err[pos[0]])):
                    acc["fix"] += 1
    return prepared, trials


def locate(W, s):
    """通用单错定位器：把 syndrome 向最像的那一列投影。

    不含任何按候选定制的分支 —— 行列校验、加权校验、符号位校验都走同一段代码，
    定位能力完全由 W 的列结构决定，这正是要度量的东西。
    """
    if not np.all(np.isfinite(s)):
        return -1, 0.0       # 指数位翻转造成的溢出，由范围判据接管，不进定位统计
    cols = W
    norms = np.linalg.norm(cols, axis=0)
    norms[norms == 0] = np.inf
    with np.errstate(over="ignore", invalid="ignore"):
        proj = (cols.T @ s) / (norms ** 2)
        resid = np.linalg.norm(s[None, :] - proj[:, None] * cols.T, axis=1)
    if not np.all(np.isfinite(resid)):
        return -1, 0.0
    j = int(np.argmin(resid))
    return j, float(proj[j])


def false_alarm(cands, n, trials, kappa, rng):
    """无故障误报：每次换一组输入，看 |lhs-rhs| 是否越过 τ。"""
    out = {}
    m = n
    for name, checks, _ in cands:
        out[name] = {"fa": 0, "ratio_max": 0.0}
    for _ in range(trials):
        A = rng.standard_normal((m, n))
        B = rng.standard_normal((n, n))
        C = A @ B
        nrmA = float(np.max(np.abs(A).sum(axis=1)))
        nrmB = float(np.max(np.abs(B).sum(axis=1)))
        for name, checks, _ in cands:
            W = check_matrix(checks, m, n)
            rhs = np.array([lowered_check(A, B, r, w) for r, w in checks])
            s = np.abs(W @ C.reshape(-1) - rhs)
            tau = np.array([kappa * U_FP64 * n * float(np.max(np.abs(w))) * nrmA * nrmB
                            for _, w in checks])
            out[name]["fa"] += int(np.any(s > tau))
            out[name]["ratio_max"] = max(out[name]["ratio_max"],
                                         float(np.max(s / (tau / kappa))))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--n", type=int, default=64, help="仿真用的矩阵规模")
    ap.add_argument("--struct-n", type=int, default=8, help="结构度量穷举用的小规模")
    ap.add_argument("--trials", type=int, default=1000)
    ap.add_argument("--fa-trials", type=int, default=200)
    ap.add_argument("--kappa", type=float, default=32.0, help="当前写死的阈值常数")
    ap.add_argument("--kfaults", type=int, nargs="+", default=[1, 2, 3, 4])
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--low-bit", type=int, default=0,
                    help="只翻转 >= 该位的比特，用来定位检测地板的来源")
    ap.add_argument("--out", default="results/opt/abft_capability")
    args = ap.parse_args()

    rng = np.random.default_rng(args.seed)
    sm = args.struct_n
    struct = []
    for name, checks, note in candidates(sm, sm):
        row = structural_metrics(name, checks, sm, sm)
        row["cost_families"] = cost_families(checks, sm, sm)
        row["note"] = note
        struct.append(row)

    print("== 结构度量（穷举于 %dx%d，d 由构造外推）==" % (sm, sm))
    hdr = ("candidate", "cost_families", "guaranteed_detect_k", "rank",
           "single_error_localizable", "tau_amplification_maxw", "correction_memory")
    print(" | ".join(hdr))
    for r in struct:
        print(" | ".join(str(r[h]) for h in hdr))

    cands = candidates(args.n, args.n)
    fa = false_alarm(cands, args.n, args.fa_trials, args.kappa, rng)
    print("\n== 无故障误报与经验 κ（%d 组输入，n=%d）==" % (args.fa_trials, args.n))
    for name in fa:
        print("%-18s 误报 %d/%d  实测所需 κ >= %.3g"
              % (name, fa[name]["fa"], args.fa_trials, fa[name]["ratio_max"]))

    sim_rows = []
    for k in args.kfaults:
        prepared, t = simulate(cands, args.n, args.trials, k, args.kappa, rng,
                               low_bit=args.low_bit)
        print("\n== 数值仿真 k=%d（%d 次，n=%d，low_bit=%d）=="
              % (k, t, args.n, args.low_bit))
        for name, _, _, _, _, _, _, acc in prepared:
            det = 100.0 * acc["det"] / t
            loc = 100.0 * acc["loc"] / t
            fix = 100.0 * acc["fix"] / t
            loc_of_det = 100.0 * acc["loc"] / acc["det"] if acc["det"] else 0.0
            print("%-18s 检出 %6.2f%%  定位 %6.2f%%  修复 %6.2f%%  检出中可定位 %6.2f%%"
                  % (name, det, loc, fix, loc_of_det))
            sim_rows.append({"candidate": name, "n": args.n, "kfaults": k,
                             "low_bit": args.low_bit,
                             "trials": t, "detect_pct": round(det, 3),
                             "locate_pct": round(loc, 3), "repair_pct": round(fix, 3),
                             "locate_of_detected_pct": round(loc_of_det, 3),
                             "faultfree_resid_max": acc["resid_max"]})

    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    with open(args.out + "_structural.json", "w") as f:
        json.dump({"structural": struct, "false_alarm": fa}, f, indent=2,
                  ensure_ascii=False)
    with open(args.out + "_sim.csv", "w", newline="") as f:
        wtr = csv.DictWriter(f, fieldnames=list(sim_rows[0].keys()))
        wtr.writeheader()
        wtr.writerows(sim_rows)
    print("\n写入 %s_structural.json / %s_sim.csv" % (args.out, args.out))


if __name__ == "__main__":
    main()
