#!/usr/bin/env python3
"""
Inno 4 探底（纯仿真，不碰 Pass）：min-plus（Floyd-Warshall 全源最短路）的结果
能否用"不等式校验"检出单比特错。半环无加法逆元、校验和无定义，但最短路满足
三角不等式 D[i,j] <= D[i,k] + D[k,j]，单侧违反即确定的错误。

量两件事：
  1. 全量三角检查（遍历所有 k，O(n^3)）的单比特检出率 —— 检测能力上界。
  2. 只用 t 个随机 pivot k 的 O(t*n^2) 抽样检查 —— 可降阶的实际版本。
按比特范围拆（低位 0-11 = 现实幅度扰动，vs 全 0-31），门槛：单比特检出 >= 40%。

用法：python3 experiments/probe_semiring_check.py
"""
import numpy as np

INF = 1 << 30


def make_graph(n, rng, wmax=50, density=0.55):
    W = rng.integers(1, wmax, size=(n, n)).astype(np.int64)
    miss = rng.random((n, n)) >= density
    W[miss] = INF
    np.fill_diagonal(W, 0)
    return W


def floyd(W):
    D = W.copy()
    n = len(D)
    for k in range(n):
        D = np.minimum(D, D[:, k:k + 1] + D[k:k + 1, :])
    return D


def violates_full(D):
    n = len(D)
    for k in range(n):
        if np.any(D > D[:, k:k + 1] + D[k:k + 1, :]):
            return True
    return False


def violates_pivots(D, ks):
    for k in ks:
        if np.any(D > D[:, k:k + 1] + D[k:k + 1, :]):
            return True
    return False


def run(n=64, trials=4000, seed=0):
    rng = np.random.default_rng(seed)
    D0 = floyd(make_graph(n, rng))
    assert not violates_full(D0), "gold 应满足三角不等式"
    fin = np.argwhere(D0 < INF)                 # 只注入可达（有限）距离
    pivot_sets = {t: None for t in (1, 2, 4, 8)}
    # 统计容器：[low-bits 0-11, all-bits 0-31]
    cats = {"low(0-11)": range(0, 12), "all(0-31)": range(0, 32)}
    res = {c: {"sdc": 0, "full": 0, **{f"piv{t}": 0 for t in pivot_sets}} for c in cats}
    for t in range(trials):
        idx = fin[rng.integers(len(fin))]
        i, j = int(idx[0]), int(idx[1])
        for cname, brange in cats.items():
            b = int(rng.choice(list(brange)))
            D = D0.copy()
            D[i, j] = int(D0[i, j]) ^ (1 << b)
            if D[i, j] == D0[i, j]:
                continue
            res[cname]["sdc"] += 1
            if violates_full(D):
                res[cname]["full"] += 1
            for tp in pivot_sets:
                ks = rng.choice(n, size=min(tp, n), replace=False)
                if violates_pivots(D, ks):
                    res[cname][f"piv{tp}"] += 1
    print(f"Floyd-Warshall APSP, n={n}, trials={trials}, seed={seed}")
    print(f"{'bit-range':12} {'SDC':>6} {'full%':>7} "
          + " ".join(f"piv{t}%".rjust(7) for t in pivot_sets))
    for c in cats:
        s = res[c]["sdc"] or 1
        row = [f"{res[c]['full']/s*100:7.1f}"] + [f"{res[c][f'piv{t}']/s*100:7.1f}" for t in pivot_sets]
        print(f"{c:12} {res[c]['sdc']:>6} " + " ".join(row))


if __name__ == "__main__":
    # 误报：无故障的正确 APSP 结果上，三角检查是否触发（应恒为 0）
    rng = np.random.default_rng(99)
    fa = sum(1 for _ in range(200) if violates_full(floyd(make_graph(64, rng))))
    print(f"false-alarm（200 个无故障随机图，全量三角检查触发次数）：{fa}\n")
    for sd in (0, 1, 2):
        run(seed=sd)
        print()
