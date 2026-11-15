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


def profile_envelope(profile_scale="default", profile_variant="default", output_path=None,
                     rank="semantic"):
    """构建剖析二进制并取每个负载的无故障近似误差包络。"""
    ssa_ll = H.build_infrastructure()
    tag = "deltaprof"
    sut_ll = H.BUILD_DIR / f"sut_{tag}.ll"
    sut_bin = H.BUILD_DIR / f"bin_{tag}_x86_64"
    H.run_cmd([
        H.OPT, f"-load-pass-plugin={H.PASS_POLICY_SO}",
        "-passes=adaptive-ft,verify",
        "-atft-mode=approx-simd", f"-atft-rank={rank}",
        "-atft-budget=1.0", "-atft-delta=5e-5",
        "-atft-delta-profile",
        "-S", str(ssa_ll), "-o", str(sut_ll)
    ])
    H.run_cmd([
        H.CLANG, *H.CFLAGS, "experiments/eval_suite.c", str(sut_ll),
        str(H.LIB_ATFT_RT), str(H.LIB_HARNESS_RT),
        *H.inc_flags(H.HARNESS_INCLUDE_DIRS), "-lm", "-o", str(sut_bin)
    ])
    profile_env = os.environ.copy()
    if profile_scale == "small":
        profile_env["ATFT_PROFILE_SMALL"] = "1"
    if profile_variant != "default":
        profile_env["ATFT_PROFILE_VARIANT"] = profile_variant
    out = H.run_cmd([str(sut_bin), "--delta-profile"], env=profile_env)
    rows = []
    for line in out.strip().splitlines():
        parts = line.split(",")
        if len(parts) == 3 and parts[0] != "benchmark":
            rows.append({"benchmark": parts[0],
                         "env_max_rel_dev": float(parts[1]),
                         "observations": int(parts[2])})
    OUT.mkdir(parents=True, exist_ok=True)
    output_path = Path(output_path) if output_path else OUT / "delta_profile.csv"
    with open(output_path, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=["benchmark", "env_max_rel_dev", "observations"])
        w.writeheader()
        w.writerows(rows)
    return ssa_ll, rows


def write_delta_table(rows, alpha, kappa, output_suffix=""):
    """delta_f = max(alpha * E_f, kappa * u_fp32)。

    地板量取 fp32 单位舍入的 kappa 倍：精确 fp64 链与 f32 近似副本之间可能出现的分歧不可能
    低于这个量级，所以它是由数制而非调参决定的下界。输出后缀用于隔离 profile/validation
    和不同参数网格的结果，避免覆盖已有校准表。
    """
    path = OUT / f"delta_table_a{alpha:g}_k{kappa:g}{output_suffix}.csv"
    with open(path, "w") as f:
        for r in rows:
            d = max(alpha * r["env_max_rel_dev"], kappa * U_FP32)
            f.write(f"{r['benchmark']},{d:.9e}\n")
    return path



# 消融矩阵：tag -> (pass_flags, env)
def ablation_matrix(delta_table, seed=None, pgsp_profile=None, pgsp_control_budget=None,
                    pgsp_budget_mode="risk-cost", pgsp_frontier_selection=None):
    base = ["-atft-cmp-tmr"]
    dflags = ["-atft-delta-runtime", "-atft-replica-exact"]
    denv = {"ATFT_DELTA_TABLE": str(delta_table), "ATFT_REPLICA_EXACT": "1"}
    seed_env = {"ATFT_FI_SEED": str(seed)} if seed is not None else {}
    mk = lambda e: {**e, **seed_env}
    matrix = [
        # tag,                 mode,           pass_flags,       env
        ("native",             "native",       [],               mk({})),
        # 标准 HPC 容错基线（供论文横比）：SWIFT 纯软冗余、精确 TMR、SIMD 双精确副本
        ("swift",              "swift",        [],               mk({})),
        ("exact-tmr",          "exact-tmr",    [],               mk({})),
        ("simd_tmr",           "approx-simd",  ["-atft-simd-f64", "-atft-replica-exact"], mk({})),
        ("opt_base",           "approx-simd",  [],               mk({})),
        ("opt_cmptmr",         "approx-simd",  base,             mk({})),
        ("opt_delta",          "approx-simd",  dflags,           mk(denv)),
        ("opt_cmptmr_delta",   "approx-simd",  base + dflags,    mk(denv)),
        ("native_cmptmr",      "native",       base,             mk({})),
        ("opt_intwire",        "approx-simd",  ["-atft-int-tmr-wire"], mk({})),
        ("opt_all",            "approx-simd",  base + dflags + ["-atft-int-tmr-wire"], mk(denv)),
        # C4：保留 FP64 精度，把两份精确副本放入 <2 x double>；不与近似向量路径混淆。
        ("opt_simd_f64",       "approx-simd",  base + dflags + ["-atft-simd-f64"], mk(denv)),
        ("opt_simd_f64_layered", "approx-simd", base + dflags + ["-atft-simd-f64",
                                                                  "-atft-int-tmr-wire",
                                                                  "-atft-int-tmr-i64",
                                                                  "-atft-abft-insert",
                                                                  "-atft-layer-exclusive"],
                                                mk({**denv, "ATFT_ABFT": "1"})),
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
        # 行列双向校验：加一个校验族换单错可定位，修复改成按 syndrome 直接修正，
        # 于此同时去掉 O(n^2) 原数据快照（见 docs/HANDOFF.md 第九节）
        ("abft_auto_rc",       "native",       ["-atft-abft-insert",
                                                "-atft-abft-colcheck"],
                                                mk({"ATFT_ABFT": "1",
                                                    "ATFT_ABFT_COLCHECK": "1"})),
        # 分层口径的实测：代数校验层（清 SDC）+ 整数通路三模（压 DUE），
        # 不带浮点近似三模。用来量"该走哪层就走哪层"的真实期望开销。
        ("abft_auto_int",      "native",       ["-atft-abft-insert",
                                                "-atft-int-tmr-wire",
                                                "-atft-int-tmr-i64"],
                                                mk({"ATFT_ABFT": "1"})),
        # 真·分层：代数校验层与指令冗余层互斥（判据本来就这么说，实现以前是叠加的）
        ("opt_layered",        "approx-simd",  base + dflags + ["-atft-int-tmr-wire",
                                                                "-atft-int-tmr-i64",
                                                                "-atft-abft-insert",
                                                                "-atft-layer-exclusive"],
                                                mk({**denv, "ATFT_ABFT": "1"})),
        # 近似副本在 x86 上是净亏损：gemm 里 10 条 fptrunc + 12 条 insertelement
        # 去喂 3 条 <2 x float> fmul，而 <2 x float> 乘法与标量 double 同吞吐。
        # 所以拿精确 fp64 副本做同样的分层，看哪个便宜。
        ("opt_layered_exact",  "exact-tmr",    base + ["-atft-int-tmr-wire",
                                                      "-atft-int-tmr-i64",
                                                      "-atft-abft-insert",
                                                      "-atft-layer-exclusive"],
                                                mk({"ATFT_ABFT": "1"})),
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
    if pgsp_profile or pgsp_frontier_selection:
        pgsp_flags = base + dflags + ["-atft-int-tmr-wire",
                                      "-atft-int-tmr-i64",
                                      "-atft-abft-insert",
                                      "-atft-layer-exclusive",
                                      f"-atft-budget-mode={pgsp_budget_mode}"]
        if pgsp_budget_mode == "frontier":
            if not pgsp_frontier_selection:
                raise ValueError("frontier budget mode requires --pgsp-frontier-selection")
            pgsp_flags.append(f"-atft-frontier-selection={pgsp_frontier_selection}")
        else:
            if not pgsp_profile:
                raise ValueError("non-frontier PGSP mode requires --pgsp-profile")
            pgsp_flags.append(f"-atft-profile={pgsp_profile}")
        if pgsp_control_budget is not None:
            pgsp_flags.append(f"-atft-control-budget={pgsp_control_budget:g}")
        matrix.append(("opt_pgsp_risk", "approx-simd", pgsp_flags,
                       mk({**denv, "ATFT_ABFT": "1"})))
    return matrix


def run_campaign(ssa_ll, matrix, categories, trials, suffix, budget=1.0,
                 fi_flags=None, rank="semantic"):
    pmu_all, fi_all = [], []
    native_inst = {}
    for tag, mode, pflags, penv in matrix:
        first = True
        for cat in categories:
            print(f"  -> [{tag}/{cat}] flags={' '.join(pflags) or '-'} trials={trials}")
            pmu_rows, fi_rows = H.compile_and_evaluate_policy(
                ssa_ll, tag, mode, rank=rank,
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
    ap.add_argument("--profile-scale", choices=["default", "small"], default="default",
                    help="profile 工作集规模；small 用于独立 validation profile")
    ap.add_argument("--profile-variant",
                    choices=["default", "scaled", "scaled_safe"],
                    default="default",
                    help="profile 输入变体；scaled 是压力测试，scaled_safe 保持 Durbin 稳定域")
    ap.add_argument("--rank", default="semantic",
                    choices=["semantic", "semantic-boundary", "boundary-branch",
                             "boundary-address", "boundary-store", "boundary-feedback",
                             "hotpath", "shoestring", "sdctune", "random"],
                    help="instruction ranking method for explicit selector experiments")
    ap.add_argument("--profile-out", default=None,
                    help="profile-only 的输出 CSV；默认写 results/opt/delta_profile.csv")
    ap.add_argument("--policies", default=None,
                    help="只跑这些 tag（逗号分隔），用于小范围的方差/敏感性实验")
    ap.add_argument("--origin-only", action="store_true",
                    help="配对注入：只往原程序本来就有的故障点注入，跳过冗余副本")
    ap.add_argument("--budget", type=float, default=1.0,
                    help="保护预算；压开销的 Pareto 前沿用它扫")
    ap.add_argument("--seed-start", type=int, default=0,
                    help="种子编号起点；0 号种子与历史口径一致")
    ap.add_argument("--out-suffix", default="",
                    help="追加到输出文件名后缀，避免覆盖已有的同参数结果")
    ap.add_argument("--pgsp-profile", default=None,
                    help="profile CSV for the explicit risk-cost selective policy")
    ap.add_argument("--pgsp-control-budget", type=float, default=None,
                    help="optional independent control-flow budget for the risk-cost policy")
    ap.add_argument("--pgsp-budget-mode", choices=["count", "dynamic-cost", "risk-cost", "frontier"],
                    default="risk-cost",
                    help="budget mode for the explicit PGSP policy")
    ap.add_argument("--pgsp-frontier-selection", default=None,
                    help="selected frontier CSV for frontier budget mode")
    args = ap.parse_args()

    cfg = H.load_config(None)
    H.apply_threshold_env(cfg)
    # 规范 12 内核子集：与 benchmark_harness 口径一致，运行循环按名字过滤。
    os.environ.setdefault("ATFT_BENCH_SUBSET", H.ATFT_CANONICAL_SUBSET)
    # full campaign 使用大规模 FI 工作集（scale_small=0），与 PMU/profile 口径一致。
    os.environ.setdefault("ATFT_FI_LARGE", "1")

    print("=== [1] 无故障近似误差包络剖析 ===")
    ssa_ll, rows = profile_envelope(
        args.profile_scale, args.profile_variant, args.profile_out, args.rank)
    for r in rows:
        print(f"  {r['benchmark']:22s} E_f={r['env_max_rel_dev']:.3e}  obs={r['observations']}")
    if args.profile_only:
        return

    table = write_delta_table(rows, args.alpha, args.kappa, args.out_suffix)
    print(f"  -> 阈值表 {table}")

    categories = [c.strip() for c in args.categories.split(",") if c.strip()]
    meta = load_meta()
    want = set(x.strip() for x in args.policies.split(",")) if args.policies else None
    for s in range(args.seed_start, args.seed_start + args.seeds):
        seed = None if s == 0 else ((0x123456789abcdef0 + 0x9e3779b97f4a7c15 * s) & ((1 << 64) - 1))
        suffix = (f"a{args.alpha:g}_t{args.trials}"
                  + ("_paired" if args.origin_only else "")
                  + (f"_b{int(args.budget * 100)}" if args.budget < 0.999 else "")
                  + ("" if s == 0 else f"_s{s}")
                  + args.out_suffix)
        print(f"=== [2] 消融 campaign (seed#{s}) ===")
        fi_rows, pmu_rows, fi_path, pmu_path = run_campaign(
            ssa_ll, [m for m in ablation_matrix(
                         table, seed, args.pgsp_profile, args.pgsp_control_budget,
                         args.pgsp_budget_mode, args.pgsp_frontier_selection)
                     if want is None or m[0] in want or m[0] == "native"],
            categories, args.trials, suffix, budget=args.budget,
            fi_flags=(["-fi-origin-only"] if args.origin_only else None),
            rank=args.rank)
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
