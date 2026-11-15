#!/usr/bin/env python3
"""代数校验层（等价饱和主线）的回归测试

跑得快、不依赖已跑过的实验产物之外的东西，用来在改动后确认：
  1. e-graph 与规则集本身没坏（四个人工用例的判定与合成式）
  2. 多项式代价模型与准入判据没坏（次数比较）
  3. 前端产物能被正确解析，且判定与实测一致的那几条没变
  4. ABFT 通用校验器在无故障输入上不误报、在注入错值时能检出

用法：
    python3 experiments/test_eqsat.py
退出码非 0 表示有回归。
"""

import csv
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import eqsat_checker as E

ROOT = Path(__file__).resolve().parent.parent
SUMS = ROOT / "results" / "opt" / "sums.csv"

fails = []


def check(cond, what):
    print(("  PASS  " if cond else "  FAIL  ") + what)
    if not cond:
        fails.append(what)


def test_poly():
    print("[1] 多项式代价代数")
    check(E.p_deg([0, 0, 3]) == 2, "deg(3N^2) == 2")
    check(E.p_str(E.p_shift([0, 1])) == "1N^2", "shift(N) == N^2")
    check(E.p_key([0, 0, 1]) > E.p_key([0, 9]), "N^2 比 9N 贵（按次数优先）")
    check(E.p_eval([1, 2, 3], 10) == 1 + 20 + 300, "求值正确")
    km, ka = E.kernel_cost(3, "**")
    check(E.p_deg(E.p_add(km, ka)) == 3, "三层嵌套内核是 N^3 阶")


def test_builtin_cases():
    print("[2] 规则集与自带用例")
    expect = {"gemm 行校验": True, "atax/bicg 全和": False,
              "jacobi2d 常系数 stencil": False,
              "floyd-warshall (min-plus)": False}
    got = {name: admit for name, _expr, admit in E.main()}
    for name, want in expect.items():
        check(got.get(name) == want,
              f"{name} 判定 = {'准入' if want else '回落'}")
    # gemm 必须搜到"把 B 沿 j 求和后再与 A 相乘"的形式
    eg = E.EGraph()
    root = E.case_gemm(eg)
    E.saturate(eg, root)
    expr = E.render(eg, E.extract(eg, root), root)
    check("sum_j(B[k,j])" in expr, "gemm 合成式里出现 sum_j(B[k,j])")
    # min-plus 不应产生任何新等价类（规则集无匹配）
    eg2 = E.EGraph()
    r2 = E.case_minplus(eg2)
    before = sum(1 for c in eg2.classes if eg2.find(c) == c)
    E.saturate(eg2, r2)
    after = sum(1 for c in eg2.classes if eg2.find(c) == c)
    check(before == after, "min-plus 饱和后等价类数不变")


def test_checksum_capability():
    print("[3] 校验式结构性检测能力")
    cap = E.checksum_capability("REDUCTION", ["i", "j"], "j", ["k"])
    check(cap["checksum_rank"] == 1, "行/列和校验秩为 1")
    check(cap["detected_error_subspace_dimension"] == "1",
          "检测误差子空间维度为 1")
    check(cap["undetected_error_subspace_dimension"] == "N_j-1",
          "未检测误差子空间维度为 N_j-1")
    check(cap["single_error_coverage"] == 1.0,
          "精确算术下单个非零输出错误可改变校验和")
    check(cap["multi_error_cancellation"] == "possible",
          "多错误抵消盲点被显式标出")
    check(cap["multi_error_localization"] == "not_supported",
          "单个校验和不声称支持多错误定位")
    check(cap["correction_capability"] == "snapshot_required",
          "归约校验纠错需要原数据快照")


def test_frontend_rows():
    print("[3] 前端产物解析与判定")
    if not SUMS.exists():
        check(False, f"缺少 {SUMS}（先跑一次 -atft-dump-sums）")
        return
    rows = E.from_sums(str(SUMS))
    by_fn = {}
    for r in rows:
        by_fn.setdefault(r["function"], []).append(r)
    check(len(rows) >= 10, f"解析到 {len(rows)} 条描述（期望 >= 10）")
    # 与实测对齐的三条硬断言
    check(any(r["admit"] for r in by_fn.get("bench_pb_gemm", [])),
          "gemm 准入（实测 1.21x / SDC -> 0）")
    check(all(not r["admit"] for r in by_fn.get("bench_pb_atax", [])),
          "atax 回落（实测校验器 4.3x，与内核同阶）")
    check(all(not r["admit"] for r in by_fn.get("bench_pb_bicg", [])),
          "bicg 回落（实测校验器 5.2x）")
    check(all(r["n_stage"] >= 1 for r in by_fn.get("bench_pb_jacobi2d", [])),
          "jacobi2d 识别出阶段循环（时间步）")
    for r in rows:
        check("@" not in r["acc"] and all("@" not in x for x in r["red"].split(",")),
              f"{r['function']} 的下标里不含偏移标记（解析正确）")
        break


def test_abft_runtime():
    print("[4] ABFT 通用校验器（C 侧）")
    src = r'''
#include <stdio.h>
#include <stdlib.h>
#include "atft_runtime.h"
#define N 32
static double A[N*N], B[N*N], C[N*N], Cold[N*N], rs[N];
int main(void) {
    for (int i = 0; i < N*N; i++) {
        A[i] = (double)((i * 37) % 11) - 5.0;
        B[i] = (double)((i * 17) % 7) - 3.0;
        C[i] = (double)((i * 5) % 3);
    }
    atft_abft_set(1, 1);
    const double alpha = 1.5, beta = 1.2;
    atft_abft_pre_rowsum(C, N, N, N, rs);
    atft_abft_pre_copy(C, N, N, N, Cold);
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++) {
            double s = 0.0;
            for (int k = 0; k < N; k++) s += A[i*N+k] * B[k*N+j];
            C[i*N+j] = beta * C[i*N+j] + alpha * s;
        }
    atft_abft_post_gemm(C, A, B, N, N, N, N, N, N, alpha, beta, rs, Cold);
    printf("clean_flagged=%llu\n", (unsigned long long)g_atft_abft.flagged);
    double saved = C[3*N+7];
    C[3*N+7] += 1e3;
    atft_abft_post_gemm(C, A, B, N, N, N, N, N, N, alpha, beta, rs, Cold);
    printf("after_fault_flagged=%llu repaired=%llu restored=%d\n",
           (unsigned long long)g_atft_abft.flagged,
           (unsigned long long)g_atft_abft.repaired,
           (C[3*N+7] - saved) * (C[3*N+7] - saved) < 1e-12);
    return 0;
}
'''
    tmp = ROOT / "build" / "test_abft.c"
    tmp.write_text(src)
    bin_path = ROOT / "build" / "test_abft"
    r = subprocess.run(["clang-15", "-O2", str(tmp), str(ROOT / "build" / "libatft_rt.a"),
                        "-I", str(ROOT / "include"), "-lm", "-o", str(bin_path)],
                       capture_output=True, text=True)
    if r.returncode != 0:
        check(False, "测试程序编译失败: " + r.stderr[-300:])
        return
    out = subprocess.run([str(bin_path)], capture_output=True, text=True).stdout
    check("clean_flagged=0" in out, "无故障输入不误报（" + out.splitlines()[0] + "）")
    line = [l for l in out.splitlines() if "after_fault" in l]
    check(bool(line) and "flagged=1 " in line[0].replace("after_fault_", ""),
          "注入错值被检出 1 行")
    check(bool(line) and "restored=1" in line[0], "重算把该行修回正确值")


def test_abft_rowcol():
    print("[5] 行列双向校验：不留快照也能纠单错（C 侧）")
    src = r'''
#include <stdio.h>
#include <stdlib.h>
#include "atft_runtime.h"
#define N 32
static double A[N*N], B[N*N], C[N*N], rs[N], cs[N];
int main(void) {
    for (int i = 0; i < N*N; i++) {
        A[i] = (double)((i * 37) % 11) - 5.0;
        B[i] = (double)((i * 17) % 7) - 3.0;
        C[i] = (double)((i * 5) % 3);
    }
    setenv("ATFT_ABFT_COLCHECK", "1", 1);
    atft_abft_set(1, 1);
    const double alpha = 1.5, beta = 1.2;
    // 只拍 O(n) 的行和与列和，没有 pre_copy
    atft_abft_pre_rowsum(C, N, N, N, rs);
    atft_abft_pre_colsum(C, N, N, N, cs);
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++) {
            double s = 0.0;
            for (int k = 0; k < N; k++) s += A[i*N+k] * B[k*N+j];
            C[i*N+j] = beta * C[i*N+j] + alpha * s;
        }
    atft_abft_post_gemm_rc(C, A, B, N, N, N, N, N, N, alpha, beta, rs, cs);
    printf("clean_flagged=%llu\n", (unsigned long long)g_atft_abft.flagged);
    double saved = C[3*N+7];
    C[3*N+7] += 1e3;
    atft_abft_post_gemm_rc(C, A, B, N, N, N, N, N, N, alpha, beta, rs, cs);
    double d = C[3*N+7] - saved;
    printf("after_fault flagged=%llu repaired=%llu restored=%d relerr=%.3g\n",
           (unsigned long long)g_atft_abft.flagged,
           (unsigned long long)g_atft_abft.repaired,
           d * d < 1e-12, d / (saved == 0.0 ? 1.0 : saved));
    // 两处错误：应当检出但拒绝修（乱改比不改危险）
    unsigned long long rep0 = g_atft_abft.repaired;
    C[5*N+9] += 1e3;
    C[11*N+2] -= 1e3;
    atft_abft_post_gemm_rc(C, A, B, N, N, N, N, N, N, alpha, beta, rs, cs);
    printf("two_faults flagged=%llu new_repairs=%llu\n",
           (unsigned long long)g_atft_abft.flagged,
           (unsigned long long)(g_atft_abft.repaired - rep0));
    return 0;
}
'''
    tmp = ROOT / "build" / "test_abft_rc.c"
    tmp.write_text(src)
    bin_path = ROOT / "build" / "test_abft_rc"
    r = subprocess.run(["clang-15", "-O2", str(tmp), str(ROOT / "build" / "libatft_rt.a"),
                        "-I", str(ROOT / "include"), "-lm", "-o", str(bin_path)],
                       capture_output=True, text=True)
    if r.returncode != 0:
        check(False, "测试程序编译失败: " + r.stderr[-300:])
        return
    out = subprocess.run([str(bin_path)], capture_output=True, text=True).stdout
    check("clean_flagged=0" in out, "无故障输入不误报（" + out.splitlines()[0] + "）")
    one = next((l for l in out.splitlines() if l.startswith("after_fault")), "")
    check("flagged=1 " in one, "单错被检出（" + one + "）")
    check("repaired=1 " in one, "单错被定位并修复")
    check("restored=1" in one, "syndrome 直接给出修正值（未用任何原数据副本）")
    two = next((l for l in out.splitlines() if l.startswith("two_faults")), "")
    check("flagged=2 " in two, "双错被检出（" + two + "）")
    check("new_repairs=0" in two, "双错定位不出来时拒绝修复")


def main():
    test_poly()
    test_builtin_cases()
    test_checksum_capability()
    test_frontend_rows()
    test_abft_runtime()
    test_abft_rowcol()
    print()
    if fails:
        print(f"{len(fails)} 项回归：")
        for f in fails:
            print("  - " + f)
        return 1
    print("全部通过")
    return 0


if __name__ == "__main__":
    sys.exit(main())
