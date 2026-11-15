#!/usr/bin/env python3
"""中期优化的消融驱动（不触碰已冻结的 results/rq1_*.csv）

三条优化改动，各带独立开关，可单独与叠加评估：
  D2  -atft-cmp-tmr      控制平面三模：复制驱动 br/select/switch 的比较并多数决
  D3a -atft-delta-runtime 表决阈值改为逐负载值，来源是无故障包络剖析
  D3b -atft-replica-exact 近似副本一致性判据改为按位相等

流程：
  1. 用 -atft-delta-profile 构建剖析二进制，跑无故障执行拿到每个负载的
     E_f = max |R - A| / (1 + |A|)，写 results/opt/delta_profile.csv
  2. 按 delta_f = max(alpha * E_f, kappa * u_fp32) 生成阈值表（alpha 可扫）
  3. 逐策略跑 RQ1 口径的七类别注错，结果写 results/opt/
  4. 用 experiments/aggregate.py 的同一套口径聚合并打印消融表

用法：
  python3 experiments/run_opt_ablation.py --profile-only
  python3 experiments/run_opt_ablation.py --trials 120
  python3 experiments/run_opt_ablation.py --trials 500 --alpha 4 --seeds 3
"""

import argparse
import csv
import json
import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import benchmark_harness as H
from aggregate import aggregate, format_table, format_category_table, load_meta

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "results" / "opt"
U_FP32 = 2.0 ** -24


def profile_envelope():
    """构建剖析二进制并取每个负载的无故障近似误差包络。"""
    ssa_ll = H.build_infrastructure()
    tag = "deltaprof"
    sut_ll = H.BUILD_DIR / f"sut_{tag}.ll"
    sut_bin = H.BUILD_DIR / f"bin_{tag}_x86_64"
    H.run_cmd([
        H.OPT, f"-load-pass-plugin={H.PASS_POLICY_SO}",
        "-passes=adaptive-ft,verify",
        "-atft-mode=approx-simd", "-atft-rank=semantic",
        "-atft-budget=1.0", "-atft-delta=5e-5",
        "-atft-delta-profile",
        "-S", str(ssa_ll), "-o", str(sut_ll)
    ])
    H.run_cmd([
        H.CLANG, *H.CFLAGS, "experiments/eval_suite.c", str(sut_ll),
        str(H.LIB_ATFT_RT), str(H.LIB_HARNESS_RT),
        *H.inc_flags(H.HARNESS_INCLUDE_DIRS), "-lm", "-o", str(sut_bin)
    ])
    out = H.run_cmd([str(sut_bin), "--delta-profile"])
    rows = []
    for line in out.strip().splitlines():
        parts = line.split(",")
        if len(parts) == 3 and parts[0] != "benchmark":
            rows.append({"benchmark": parts[0],
                         "env_max_rel_dev": float(parts[1]),
                         "observations": int(parts[2])})
    OUT.mkdir(parents=True, exist_ok=True)
    with open(OUT / "delta_profile.csv", "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=["benchmark", "env_max_rel_dev", "observations"])
        w.writeheader()
        w.writerows(rows)
    return ssa_ll, rows


def write_delta_table(rows, alpha, kappa):
    """delta_f = max(alpha * E_f, kappa * u_fp32)。

    地板量取 fp32 单位舍入的 kappa 倍：精确 fp64 链与 f32 副本之间的分歧不可能
    低于这个量级，所以它是由数制而非调参决定的下界。
    """
    path = OUT / f"delta_table_a{alpha:g}_k{kappa:g}.csv"
    with open(path, "w") as f:
        for r in rows:
            d = max(alpha * r["env_max_rel_dev"], kappa * U_FP32)
            f.write(f"{r['benchmark']},{d:.9e}\n")
    return path


# 消融矩阵：tag -> (pass_flags, env)
def ablation_matrix(delta_table, seed=None):
    base = ["-atft-cmp-tmr"]
    dflags = ["-atft-delta-runtime", "-atft-replica-exact"]
    denv = {"ATFT_DELTA_TABLE": str(delta_table), "ATFT_REPLICA_EXACT": "1"}
    seed_env = {"ATFT_FI_SEED": str(seed)} if seed is not None else {}
    mk = lambda e: {**e, **seed_env}
    return [
        # tag,                 mode,           pass_flags,       env
        ("native",             "native",       [],               mk({})),
        ("opt_base",           "approx-simd",  [],               mk({})),
        ("opt_cmptmr",         "approx-simd",  base,             mk({})),
        ("opt_delta",          "approx-simd",  dflags,           mk(denv)),
        ("opt_cmptmr_delta",   "approx-simd",  base + dflags,    mk(denv)),
        ("native_cmptmr",      "native",       base,             mk({})),
        ("opt_intwire",        "approx-simd",  ["-atft-int-tmr-wire"], mk({})),
        ("opt_all",            "approx-simd",  base + dflags + ["-atft-int-tmr-wire"], mk(denv)),
        ("opt_bnd",            "approx-simd",  base + dflags + ["-atft-int-tmr-boundary"], mk(denv)),
        ("opt_bnd_only",       "approx-simd",  ["-atft-int-tmr-boundary"], mk({})),
        # 代数校验层：零指令冗余，只跑等价饱和合成出的 O(n^2) 校验式
        ("abft",               "native",       [],               mk({"ATFT_ABFT": "1",
                                                                     "ATFT_ABFT_HARNESS": "1"})),
        ("abft_norepair",      "native",       [],               mk({"ATFT_ABFT": "1",
                                                                     "ATFT_ABFT_REPAIR": "0",
                                                                     "ATFT_ABFT_HARNESS": "1"})),
        # 编译期自动插桩（无 harness 接线）：粒度由归约区域推导、准入由代价判据判定
        ("abft_auto",          "native",       ["-atft-abft-insert"], mk({"ATFT_ABFT": "1"})),
        # D4 整数三模覆盖面扩到 i64（i32->i64 的下标扩展占 cast 注入点的 88%）
        ("opt_i64",            "approx-simd",  base + dflags + ["-atft-int-tmr-wire",
                                                                "-atft-int-tmr-i64"], mk(denv)),
        ("opt_i64_split",      "approx-simd",  base + dflags + ["-atft-int-tmr-wire",
                                                                "-atft-int-tmr-i64",
                                                                "-atft-int-budget=1.0"], mk(denv)),
        # 整数通路双模 + 冷路径重执行（替掉常驻第三份副本）
        ("opt_dmr",            "approx-simd",  base + dflags + ["-atft-int-dmr",
                                                                "-atft-int-tmr-i64"], mk(denv)),
        # D10 按通路分预算：浮点近似通路跟随 --budget，整数通路恒为全量
        ("opt_split",          "approx-simd",  base + dflags + ["-atft-int-tmr-wire",
                                                                "-atft-int-budget=1.0"], mk(denv)),
    ]


def run_campaign(ssa_ll, matrix, categories, trials, suffix, budget=1.0, fi_flags=None):
    pmu_all, fi_all = [], []
    native_inst = {}
    for tag, mode, pflags, penv in matrix:
        first = True
        for cat in categories:
            print(f"  -> [{tag}/{cat}] flags={' '.join(pflags) or '-'} trials={trials}")
            pmu_rows, fi_rows = H.compile_and_evaluate_policy(
                ssa_ll, tag, mode, rank="semantic",
                budget=(1.0 if tag == "native" else budget), delta=5e-5,
                fi_trials=trials, dump_golden=False, run_fi=True,
                fi_target_type=cat, arch="x86_64",
                pass_flags=pflags, env_extra=penv, fi_flags=fi_flags)
            if first:
                if tag == "native":
                    for r in pmu_rows:
                        native_inst[r["benchmark"]] = r["hw_insts"]
                pmu_all.extend(pmu_rows)
                first = False
            fi_all.extend(fi_rows)

    pmu_lookup = {(r["policy"], r["benchmark"]): r for r in pmu_all}
    for fr in fi_all:
        ratio = pmu_lookup[(fr["policy"], fr["benchmark"])]["hw_insts"] \
                / max(1, native_inst.get(fr["benchmark"], 1))
        fail = max(0.5 / (fr["trials"] + 1.0), (fr["sdc_pct"] + fr["due_pct"]) / 100.0)
        sdc = max(0.5 / (fr["trials"] + 1.0), fr["sdc_pct"] / 100.0)
        fr["norm_hw_inst_ratio"] = round(ratio, 3)
        fr["mwtf_sdc"] = round(1.0 / (ratio * sdc), 2)
        fr["mwtf_total"] = round(1.0 / (ratio * fail), 2)

    OUT.mkdir(parents=True, exist_ok=True)
    fi_path = OUT / f"fi_outcomes_{suffix}.csv"
    pmu_path = OUT / f"pmu_{suffix}.csv"
    with open(fi_path, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(fi_all[0].keys()))
        w.writeheader()
        w.writerows(fi_all)
    with open(pmu_path, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(pmu_all[0].keys()))
        w.writeheader()
        w.writerows(pmu_all)
    return fi_all, pmu_all, fi_path, pmu_path


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--trials", type=int, default=120, help="每负载每类别的注入次数")
    ap.add_argument("--alpha", type=float, default=4.0, help="包络安全系数")
    ap.add_argument("--kappa", type=float, default=4.0, help="地板系数（单位舍入的倍数）")
    ap.add_argument("--categories", default="fp64,int_arith,cast,load,addr,cmp_branch")
    ap.add_argument("--seeds", type=int, default=1, help="重复的注错种子数（方差评估）")
    ap.add_argument("--profile-only", action="store_true")
    ap.add_argument("--policies", default=None,
                    help="只跑这些 tag（逗号分隔），用于小范围的方差/敏感性实验")
    ap.add_argument("--origin-only", action="store_true",
                    help="配对注入：只往原程序本来就有的故障点注入，跳过冗余副本")
    ap.add_argument("--budget", type=float, default=1.0,
                    help="保护预算；压开销的 Pareto 前沿用它扫")
    ap.add_argument("--seed-start", type=int, default=0,
                    help="种子编号起点；0 号种子与历史口径一致")
    args = ap.parse_args()

    cfg = H.load_config(None)
    H.apply_threshold_env(cfg)

    print("=== [1] 无故障近似误差包络剖析 ===")
    ssa_ll, rows = profile_envelope()
    for r in rows:
        print(f"  {r['benchmark']:22s} E_f={r['env_max_rel_dev']:.3e}  obs={r['observations']}")
    if args.profile_only:
        return

    table = write_delta_table(rows, args.alpha, args.kappa)
    print(f"  -> 阈值表 {table}")

    categories = [c.strip() for c in args.categories.split(",") if c.strip()]
    meta = load_meta()
    want = set(x.strip() for x in args.policies.split(",")) if args.policies else None
    for s in range(args.seed_start, args.seed_start + args.seeds):
        seed = None if s == 0 else ((0x123456789abcdef0 + 0x9e3779b97f4a7c15 * s) & ((1 << 64) - 1))
        suffix = (f"a{args.alpha:g}_t{args.trials}"
                  + ("_paired" if args.origin_only else "")
                  + (f"_b{int(args.budget * 100)}" if args.budget < 0.999 else "")
                  + ("" if s == 0 else f"_s{s}"))
        print(f"=== [2] 消融 campaign (seed#{s}) ===")
        fi_rows, pmu_rows, fi_path, pmu_path = run_campaign(
            ssa_ll, [m for m in ablation_matrix(table, seed)
                     if want is None or m[0] in want or m[0] == "native"],
            categories, args.trials, suffix, budget=args.budget,
            fi_flags=(["-fi-origin-only"] if args.origin_only else None))
        agg = aggregate([{k: str(v) for k, v in r.items()} for r in fi_rows],
                        [{k: str(v) for k, v in r.items()} for r in pmu_rows], meta)
        print("\n=== 等权聚合 ===")
        print(format_table(agg, "equal_category"))
        for pol in agg:
            print()
            print(format_category_table(agg, pol))
        (OUT / f"aggregate_{suffix}.json").write_text(
            json.dumps(agg, indent=2, ensure_ascii=False))
        print(f"\n-> {fi_path}\n-> {OUT / f'aggregate_{suffix}.json'}")


if __name__ == "__main__":
    main()
