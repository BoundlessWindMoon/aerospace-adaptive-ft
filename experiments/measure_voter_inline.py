#!/usr/bin/env python3
"""表决器开销计量变体（不碰冻结数据，只写 results/opt/）。

目的：隔离"插桩开销"与"冗余计算开销"两根杠杆，回答
  exact_tmr / swift 的 15.7× / 7.9× 里有多少是外部函数调用约定 + 统计计数器，
  而不是三份冗余算术本身。

两根杠杆各自独立开关，组成 2×2 矩阵：
  调用方式  call   = 表决器走 libatft_rt 外部调用（冻结路径）
            inline = runtime 编成 IR，llvm-link 进被测模块后 always-inline 展开
  统计计数  count   = total_votes/fault_free_votes 照常自增（冻结路径）
            nocount = -DATFT_STATS_NOCOUNT 关掉这两个纯统计 RMW

关键不变量（保证是干净对照、落入分析自己给的否证条件）：
  - FI 注入点在 benchmark IR 上、在链接/内联表决器之前就已选定，
    所以内联不可能移动 SDC/DUE —— 若 SDC/DUE 变了，就是语义被改，实验作废。
  - SDC 只看表决器返回值，DUE 只看信号；两者都不依赖被关掉的统计计数器。

用法：
  python3 experiments/measure_voter_inline.py --trials 200
  python3 experiments/measure_voter_inline.py --trials 200 --policies exact-tmr,swift
"""

import argparse
import csv
import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import benchmark_harness as H

OUT = H.RES_DIR / "opt"

# 被测策略：tag -> (atft-mode, pass_flags, env_extra)
# exact-tmr / swift 是文献基线（COAST TNS'19 / SWIFT CGO'05），表决器是外部调用；
# opt_i64 / opt_layered 是本方法，整数通路 i32 汇点仍走外部 atft_voter_exact_i32。
POLICIES = {
    "exact-tmr": ("exact-tmr", [], {}),
    "swift":     ("swift", [], {}),
    "opt_i64":   ("approx-simd",
                  ["-atft-cmp-tmr", "-atft-int-tmr-wire", "-atft-int-tmr-i64"], {}),
    "opt_layered": ("approx-simd",
                    ["-atft-cmp-tmr", "-atft-int-tmr-wire", "-atft-int-tmr-i64",
                     "-atft-abft-insert", "-atft-layer-exclusive"],
                    {"ATFT_ABFT": "1"}),
}

CELLS = ["call_count", "call_nocount", "inline_count", "inline_nocount"]

def build_runtime_ir():
    """把 runtime 编成两份 IR：带计数器 / 不带计数器。always-inline 属性已在头文件里。"""
    H.BUILD_DIR.mkdir(parents=True, exist_ok=True)
    out = {}
    for cnt, extra in (("count", []), ("nocount", ["-DATFT_STATS_NOCOUNT"])):
        ll = H.BUILD_DIR / f"atft_rt_{cnt}.ll"
        H.run_cmd([H.CLANG, "-O1", "-emit-llvm", "-S", *extra,
                   *H.inc_flags(["include"]), "runtime/atft_runtime.c", "-o", str(ll)])
        out[cnt] = ll
    return out


def run_pass(ssa_ll, tag, mode, pflags):
    """跑策略层 pass 得到被测 IR；FI pass 得到注错 IR（注错点在内联之前就已固定）。"""
    sut_ll = H.BUILD_DIR / f"vi_{tag}.ll"
    H.run_cmd([H.OPT, f"-load-pass-plugin={H.PASS_POLICY_SO}",
               "-passes=adaptive-ft,verify", f"-atft-mode={mode}",
               "-atft-rank=semantic", "-atft-budget=1.0", "-atft-delta=5e-5",
               *pflags, "-S", str(ssa_ll), "-o", str(sut_ll)])
    return sut_ll


def link_inline(sut_ll, tag, rt_ll):
    """llvm-link 表决器 IR 进被测模块，再 always-inline 展开。返回内联后的 IR。"""
    linked = H.BUILD_DIR / f"vi_{tag}_linked.ll"
    inl = H.BUILD_DIR / f"vi_{tag}_inlined.ll"
    H.run_cmd([f"llvm-link-15", str(sut_ll), str(rt_ll), "-S", "-o", str(linked)])
    H.run_cmd([H.OPT, "-passes=always-inline", "-S", str(linked), "-o", str(inl)])
    return inl


def build_binary(ir_ll, cell, rt_ir, out_bin, fi_target=None):
    """按 cell 组合编译可执行文件。inline 变体不再单独链接 runtime（已在 IR 内）。"""
    inline = cell.startswith("inline")
    nocount = cell.endswith("nocount")
    define = ["-DATFT_STATS_NOCOUNT"] if nocount else []
    cc = [H.CLANG, *H.CFLAGS, *define, "experiments/eval_suite.c"]
    if inline:
        link_ir = link_inline(ir_ll, Path(out_bin).name, rt_ir["nocount" if nocount else "count"])
        cc += [str(link_ir)]
    else:
        cc += [str(ir_ll), "runtime/atft_runtime.c"]
    cc += [str(H.LIB_HARNESS_RT), *H.inc_flags(H.HARNESS_INCLUDE_DIRS), "-lm", "-o", str(out_bin)]
    H.run_cmd(cc)
    return out_bin

def parse_pmu(out, tag, cell):
    rows = {}
    for line in out.strip().splitlines():
        if line.startswith("PMU_CSV,"):
            p = line.split(",")
            rows[p[2]] = {"policy": tag, "cell": cell, "benchmark": p[2],
                          "hw_insts": int(p[4]), "hw_cycles": int(p[5])}
    return rows


def parse_fi(out, tag, cell, cat):
    rows = {}
    for line in out.strip().splitlines():
        if line.startswith("FI5_CSV,"):
            p = line.split(",")
            rows[p[2]] = {"policy": tag, "cell": cell, "benchmark": p[2], "category": cat,
                          "trials": int(p[4]), "sdc_pct": float(p[8]), "due_pct": float(p[9])}
    return rows


def write_outputs(pmu_all, fi_all, args):
    """落盘到 results/opt/，并打印 2×2 对照摘要（倍率 + SDC/DUE 不变性核验）。"""
    pmu_path = OUT / "voter_inline_pmu.csv"
    fi_path = OUT / "voter_inline_fi.csv"
    if pmu_all:
        with open(pmu_path, "w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=list(pmu_all[0].keys()))
            w.writeheader(); w.writerows(pmu_all)
    if fi_all:
        with open(fi_path, "w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=list(fi_all[0].keys()))
            w.writeheader(); w.writerows(fi_all)
    print(f"\n-> {pmu_path}\n-> {fi_path}")

    # 工作量加权（按 native 指令占比）聚合每个 policy×cell 的周期倍率，便于与 HANDOFF 对齐
    print("\n=== 周期倍率（几何平均 over benchmarks）与 SDC/DUE 不变性 ===")
    import math
    by = {}
    for r in pmu_all:
        by.setdefault((r["policy"], r["cell"]), []).append(r["cycle_ratio"])
    fiby = {}
    for r in fi_all:
        fiby.setdefault((r["policy"], r["cell"]), (r["sdc_pct"], r["due_pct"]))
    for pol in [p.strip() for p in args.policies.split(",") if p.strip()]:
        print(f"\n[{pol}]")
        for cell in CELLS:
            rs = by.get((pol, cell))
            if not rs:
                continue
            gm = math.exp(sum(math.log(max(x, 1e-9)) for x in rs) / len(rs))
            sdc, due = fiby.get((pol, cell), (float("nan"), float("nan")))
            print(f"  {cell:16s} cycle×(gm)={gm:6.2f}  SDC={sdc:6.2f}%  DUE={due:6.2f}%")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--trials", type=int, default=200)
    ap.add_argument("--policies", default="exact-tmr,swift,opt_i64,opt_layered")
    ap.add_argument("--fi-category", default="fp64",
                    help="SDC/DUE 不变性核验用的注错类别；integer 通路可用 int_arith")
    args = ap.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)

    print("=== [0] 基础设施 + golden + native 基线 ===")
    ssa_ll = H.build_infrastructure()
    rt_ir = build_runtime_ir()
    npmu, _ = H.compile_and_evaluate_policy(ssa_ll, "native", "native", budget=1.0,
                                            fi_trials=0, dump_golden=True, run_fi=False)
    native_inst = {r["benchmark"]: r["hw_insts"] for r in npmu}
    native_cyc = {r["benchmark"]: r["hw_cycles"] for r in npmu}

    pmu_all, fi_all = [], []
    want = [p.strip() for p in args.policies.split(",") if p.strip()]
    for tag in want:
        mode, pflags, env = POLICIES[tag]
        run_env = {**os.environ, **{k: str(v) for k, v in env.items()}}
        sut_ll = run_pass(ssa_ll, tag, mode, pflags)
        # FI IR（注错点在内联前固定）
        fi_ll = H.BUILD_DIR / f"vi_{tag}_fi_{args.fi_category}.ll"
        H.run_cmd([H.OPT, f"-load-pass-plugin={H.PASS_FI_SO}", "-passes=standalone-fi,verify",
                   f"-fi-target-type={args.fi_category}", "-S", str(sut_ll), "-o", str(fi_ll)])
        for cell in CELLS:
            print(f"  -> [{tag}/{cell}]")
            pbin = H.BUILD_DIR / f"vi_{tag}_{cell}_pmu"
            build_binary(sut_ll, cell, rt_ir, pbin)
            pout = H.run_cmd([str(pbin), "--pmu-perf", f"{tag}_{cell}", str(H.GOLDEN_DIR)], env=run_env)
            pm = parse_pmu(pout, tag, cell)
            fbin = H.BUILD_DIR / f"vi_{tag}_{cell}_fi"
            build_binary(fi_ll, cell, rt_ir, fbin, fi_target=args.fi_category)
            fout = H.run_cmd([str(fbin), "--fi-one", f"{tag}_{cell}", str(H.GOLDEN_DIR),
                              str(args.trials)], allow_nonzero=True, timeout_s=600, env=run_env)
            fi = parse_fi(fout, tag, cell, args.fi_category)
            for b, r in pm.items():
                r["inst_ratio"] = round(r["hw_insts"] / max(1, native_inst.get(b, 1)), 3)
                r["cycle_ratio"] = round(r["hw_cycles"] / max(1, native_cyc.get(b, 1)), 3)
                pmu_all.append(r)
            fi_all.extend(fi.values())
    write_outputs(pmu_all, fi_all, args)


if __name__ == "__main__":
    main()


