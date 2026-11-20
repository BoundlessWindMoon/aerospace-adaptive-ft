#!/usr/bin/env python3
"""P2 不可分离因果实验:向量化 value 子切片是否把脆弱性挤到 cast/addr 子切片。

单一自变量 = value 计算是否向量化(native 标量 vs native 向量 vs 向量+FP冗余)。
因变量 = cast / addr 两类注入的 SDC/DUE。中介变量 = IR 里的向量 cast 指令数。
每个配置用自己的 fault-free 输出当 golden(dump_golden=True),判据用 Wilson 95% CI。
复用现有 harness,不新建注入器。"""
import sys, csv, re
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import benchmark_harness as H

TRIALS = 200
CATS = ["cast", "addr"]
# (tag, mode, pass_flags)  —— 受控分解:native 不走 O3 向量化,故用 dmr-no-inject
# 当"只向量化不注入"的中间臂,把"向量化"与"FP 注入"两个因子拆开。
CFGS = [
    ("scalar_native",   "native",      []),                                          # 标量,无向量化无注入
    ("vector_noinject", "approx-simd", ["-atft-dmr-generic", "-atft-dmr-no-inject"]),# 向量化,不注入冗余
    ("vector_dmr",      "approx-simd", ["-atft-dmr-generic"]),                        # 向量化 + FP 注入
]

VEC_CAST = re.compile(r'=\s*(sext|zext|trunc|sitofp|uitofp|fptosi|fptoui|fptrunc|fpext)\s+<\d+\s*x')
ALL_CAST = re.compile(r'=\s*(sext|zext|trunc|sitofp|uitofp|fptosi|fptoui|fptrunc|fpext)\s')

SSA = H.build_infrastructure()

res = {}       # res[(tag,cat)] = (k_sdc, k_due, n)
veccast = {}   # veccast[tag] = (n_vector_cast, n_all_cast)
for tag, mode, flags in CFGS:
    for cat in CATS:
        _, f = H.compile_and_evaluate_policy(
            SSA, f"{tag}_{cat}", mode, budget=1.0, fi_trials=TRIALS,
            run_fi=True, fi_target_type=cat, dump_golden=True, pass_flags=flags)
        k_sdc = sum(round(r["sdc_pct"] / 100 * r["trials"]) for r in f)
        k_due = sum(round(r["due_pct"] / 100 * r["trials"]) for r in f)
        n = sum(r["trials"] for r in f)
        res[(tag, cat)] = (k_sdc, k_due, n)
    ll = (H.BUILD_DIR / f"sut_{tag}_cast.ll").read_text().splitlines()
    veccast[tag] = (sum(1 for l in ll if VEC_CAST.search(l)),
                    sum(1 for l in ll if ALL_CAST.search(l)))

def pct(k, n): return 100.0 * k / n if n else 0.0

print("\n=== 中介变量:IR 里的 cast 指令(向量 / 全部) ===")
for tag, *_ in CFGS:
    v, a = veccast[tag]
    print(f"  {tag:14} 向量 cast {v:5d}   全部 cast {a:6d}")

print("\n=== SDC% / DUE% (每配置对自身 golden,Wilson 95%CI) ===")
print(f"{'config':14}" + "".join(f"{c+'_SDC':>18}{c+'_DUE':>12}" for c in CATS))
for tag, *_ in CFGS:
    cells = ""
    for c in CATS:
        ks, kd, n = res[(tag, c)]
        lo, hi, half = H.wilson_ci95(ks, n)
        cells += f"{pct(ks,n):8.1f}±{half:4.1f}  " + f"{pct(kd,n):10.1f}"
    print(f"{tag:14}" + cells)

# 因果判据:两个受控对比,把"向量化"与"FP 注入"两个因子拆开
print("\n=== 因果判据(cast 子切片,两步受控对比) ===")
def contrast(a, b, c):
    ks_a, _, n_a = res[(a, c)]
    ks_b, _, n_b = res[(b, c)]
    lo_a, hi_a, _ = H.wilson_ci95(ks_a, n_a)
    lo_b, hi_b, _ = H.wilson_ci95(ks_b, n_b)
    disjoint = (lo_b > hi_a) or (lo_a > hi_b)
    direction = "↑" if pct(ks_b, n_b) > pct(ks_a, n_a) else "↓"
    verdict = ("显著" + direction) if disjoint else "不显著(CI 重叠)"
    print(f"  {c:6} {a:15} {pct(ks_a,n_a):5.1f}% [{lo_a:.1f},{hi_a:.1f}]  →  "
          f"{b:15} {pct(ks_b,n_b):5.1f}% [{lo_b:.1f},{hi_b:.1f}]  : {verdict}")

for c in CATS:
    contrast("scalar_native", "vector_noinject", c)   # 因子1:纯向量化
    contrast("vector_noinject", "vector_dmr", c)      # 因子2:在向量化之上加 FP 注入

print("\n  中介(向量 cast 数):"
      + "  ".join(f"{t}={veccast[t][0]}" for t, *_ in CFGS))
print("  读法:")
print("    若因子1显著↑ → 向量化本身就把脆弱性挤到 cast 子切片(圈不可分离,强结论)。")
print("    若仅因子2显著↑ → 是 FP 注入的向量布局造成的集中(仍是耦合,弱结论)。")
print("    若都不显著 → cast 洞与向量化无因果,之前 -233% 是别的原因。")

RES = H.RES_DIR / "nonseparability.csv"
with open(RES, "w", newline="") as fp:
    w = csv.writer(fp)
    w.writerow(["config", "category", "sdc_pct", "sdc_ci95_half", "due_pct", "n",
                "n_vector_cast", "n_all_cast"])
    for tag, *_ in CFGS:
        for c in CATS:
            ks, kd, n = res[(tag, c)]
            _, _, half = H.wilson_ci95(ks, n)
            vcz, acz = veccast[tag]
            w.writerow([tag, c, round(pct(ks, n), 2), round(half, 2),
                        round(pct(kd, n), 2), n, vcz, acz])
print("CSV ->", RES)
