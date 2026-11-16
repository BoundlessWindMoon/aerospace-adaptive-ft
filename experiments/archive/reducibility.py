#!/usr/bin/env python3
"""可约性判据：对全部负载给出"进不进代数校验层"及其理由

判据由两个独立条件组成，都从程序结构导出，不含按内核名的分支：

  C1 代价条件：max_array_dims + n_stage < nest_depth
      校验式把输出沿一维压掉之后，右端的代价由**单个被读数组的维数**决定
      （gemm 里 A、B 各是 2 维，所以校验是 O(n^2)，内核是 O(n^3)）。
      n_stage 是不参与寻址的外层数（时间步），校验必须每步跑一次，所以它
      直接加在校验的次数上 —— jacobi2d 就是被这一项挡住的。

  C2 代数条件：归约运算落在环上（有加法逆元）
      min-plus / max-plus 半环上没有减法，"校验和之差"无定义，所以
      floyd-warshall、nussinov 这类动态规划用不了校验和。

输入是 `-atft-dump-nests` 的产物与 native 的 PMU（给工作量权重）。

用法：
    opt-15 -load-pass-plugin=build/libAdaptiveFTPass.so -passes=adaptive-ft \
      -atft-mode=native -atft-dump-nests=results/opt/nests.csv \
      -S build/benchmarks_ssa.ll -o /dev/null
    python3 experiments/reducibility.py
"""

import argparse
import csv
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def load_weights(path):
    """工作量权重 = native 的动态指令数占比。覆盖面必须按工作量报，
    按负载计数会把它低报 4 倍（2/17 = 11.8% 对 48.7%）。"""
    rows = [r for r in csv.DictReader(open(path)) if r["policy"] == "native"]
    tot = sum(int(r["hw_insts"]) for r in rows)
    return {r["benchmark"]: int(r["hw_insts"]) / tot for r in rows}, tot


def classify(nest):
    """对一条巢描述给出 (可约?, 理由)。只用结构字段，不看函数名。"""
    nd = int(nest["nest_depth"])
    od = int(nest["out_dims"])
    mad = int(nest["max_array_dims"])
    out_idx = {x for x in nest["out_idx"].split(";") if x}
    read_idx = {x for x in nest["read_idx"].split(";") if x}
    addressed = out_idx | read_idx
    n_stage = nd - len(addressed)

    if mad == 0 and not read_idx:
        return None, "无数组读：不是可识别的内核形态"
    if nd <= od:
        return False, f"每个输出只有 O(1) 工作量（巢深 {nd} = 输出维数 {od}）"
    if not (mad + n_stage < nd):
        why = f"校验代价不降阶：max_array_dims {mad} + 阶段层 {n_stage} >= 巢深 {nd}"
        if n_stage > 0:
            why += "（校验要每个阶段跑一次）"
        return False, why
    if int(nest["phi_outside_header"]) > 0:
        # 非循环头上的 phi = 值由控制流选出来（min/max/条件更新）。
        # 这个信号在全集上会过度标记（durbin、blackscholes 也有），但它们已经被
        # C1 排除，所以在"通过 C1 的那一组"里它是精确的：只命中 floyd-warshall。
        # dump 里另外两列（cond_self_update / cmp_branch）保留为原始证据但不参与
        # 判定 —— 循环边界检查本身就是条件分支，那两列几乎恒真。
        return "semiring", (f"代价可降阶（{mad}+{n_stage} < {nd}）但归约是条件更新："
                            "min/max 型半环没有加法逆元，校验和无定义")
    return True, f"代价可降阶（{mad}+{n_stage} < {nd}）且归约在环上"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--nests", default="results/opt/nests.csv")
    ap.add_argument("--pmu", default="results/opt/pmu_a4_t200.csv")
    ap.add_argument("--inserted", default="pb_gemm,pb_2mm")
    ap.add_argument("--out", default="results/opt/reducibility")
    args = ap.parse_args()

    w, tot = load_weights(os.path.join(ROOT, args.pmu))
    nests = [r for r in csv.DictReader(open(os.path.join(ROOT, args.nests)))
             if r["function"].startswith("bench_")]

    rank = {True: 3, "semiring": 2, False: 1, None: 0}
    best = {}
    for n in nests:
        b = n["function"][len("bench_"):]
        v, why = classify(n)
        if b not in best or rank[v] > rank[best[b][0]]:
            best[b] = (v, why, n)

    inserted = set(args.inserted.split(","))
    label = {True: "reducible", "semiring": "semiring_blocked",
             False: "not_reducible", None: "unrecognized"}
    rows, acc = [], {True: 0.0, "semiring": 0.0, False: 0.0, None: 0.0}
    for b, (v, why, n) in sorted(best.items(), key=lambda kv: -w.get(kv[0], 0)):
        ww = w.get(b, 0.0)
        acc[v] += ww
        rows.append({"benchmark": b, "work_share_pct": round(100 * ww, 2),
                     "nest_depth": n["nest_depth"], "out_dims": n["out_dims"],
                     "max_array_dims": n["max_array_dims"],
                     "verdict": label[v], "inserted": b in inserted,
                     "reason": why})
    for b in sorted(set(w) - set(best), key=lambda x: -w[x]):
        rows.append({"benchmark": b, "work_share_pct": round(100 * w[b], 2),
                     "nest_depth": "", "out_dims": "", "max_array_dims": "",
                     "verdict": "undumped", "inserted": False,
                     "reason": "前端没有抽出巢描述（非仿射寻址 / 循环携带依赖）"})
        acc[None] += w[b]

    print("%-22s %7s %-17s %-9s %s" % ("benchmark", "work%", "verdict",
                                       "inserted", "reason"))
    for r in rows:
        print("%-22s %6.2f%% %-17s %-9s %s"
              % (r["benchmark"], r["work_share_pct"], r["verdict"],
                 "yes" if r["inserted"] else "", r["reason"][:64]))

    ins = sum(w.get(b, 0) for b in inserted)
    print("\n== 按动态指令加权 ==")
    print("  已插桩并实测清零        %5.1f%%" % (100 * ins))
    print("  判据判为可约           %5.1f%%" % (100 * acc[True]))
    print("  代价可降阶但卡在半环    %5.1f%%" % (100 * acc["semiring"]))
    print("  数学上不可约           %5.1f%%" % (100 * acc[False]))
    print("  前端未识别 / 退化      %5.1f%%" % (100 * acc[None]))

    with open(os.path.join(ROOT, args.out + ".csv"), "w", newline="") as f:
        wtr = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        wtr.writeheader()
        wtr.writerows(rows)
    with open(os.path.join(ROOT, args.out + ".json"), "w") as f:
        json.dump({"total_native_insts": tot,
                   "share": {str(k): acc[k] for k in acc},
                   "inserted_share": ins, "rows": rows}, f, indent=2,
                  ensure_ascii=False)
    print("\n写入 %s.csv / %s.json" % (args.out, args.out))


if __name__ == "__main__":
    main()

