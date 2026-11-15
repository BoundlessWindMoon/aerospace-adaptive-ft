//===----------------------------------------------------------------------===//
// Direct Compilation Wrapper for Unmodified Upstream Open-Source Benchmarks
//
// 本翻译单元通过 #include 直接编译 third_party/ 下**未经修改**的上游源文件；
// 本文件自身不含任何自写计算内核，只做参数转接与符号重命名（避免 main /
// init_array 等同名冲突）。各上游来源：
//   1. PolyBench/C 4.2.1          third_party/polybench-c/
//        FP64 : gemm, 2mm, atax, bicg, jacobi-2d, ludcmp, durbin, deriche
//        INT32: floyd-warshall, nussinov
//   2. KissFFT v1.3.1             third_party/kissfft/kiss_fft.c
//   3. PARSEC Black-Scholes       third_party/llvm-loop-perforation/benchmarks/
//                                 blackscholes/blackscholes.c（上游单文件移植版）
//   4. Sobel 图像滤波             third_party/llvm-loop-perforation/benchmarks/
//                                 sobel/sobel.c
//   5. COAST 星载容错测试程序     third_party/coast/tests/crc16/crc16.c
//                                 third_party/coast/tests/quicksort/quicksort.c
//===----------------------------------------------------------------------===//

#include "benchmarks.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// 1. PolyBench/C 4.2.1 —— FP64 内核
// ============================================================================
#define MINI_DATASET
#define POLYBENCH_USE_C99_PROTO
#define DATA_TYPE_IS_DOUBLE
#include "../third_party/polybench-c/utilities/polybench.c"
#define static __attribute__((always_inline)) static

// (a) linear-algebra/blas/gemm
#define init_array  pb_gemm_init_array
#define print_array pb_gemm_print_array
#define main        pb_gemm_unused_main
#include "../third_party/polybench-c/linear-algebra/blas/gemm/gemm.c"
#undef init_array
#undef print_array
#undef main
#undef NI
#undef NJ
#undef NK
#undef _PB_NI
#undef _PB_NJ
#undef _PB_NK

// (b) linear-algebra/kernels/2mm
#define init_array  pb_2mm_init_array
#define print_array pb_2mm_print_array
#define main        pb_2mm_unused_main
#include "../third_party/polybench-c/linear-algebra/kernels/2mm/2mm.c"
#undef init_array
#undef print_array
#undef main
#undef NI
#undef NJ
#undef NK
#undef NL
#undef _PB_NI
#undef _PB_NJ
#undef _PB_NK
#undef _PB_NL

// (c) linear-algebra/kernels/atax
#define init_array  pb_atax_init_array
#define print_array pb_atax_print_array
#define main        pb_atax_unused_main
#include "../third_party/polybench-c/linear-algebra/kernels/atax/atax.c"
#undef init_array
#undef print_array
#undef main
#undef M
#undef N
#undef _PB_M
#undef _PB_N

// (d) linear-algebra/kernels/bicg
#define init_array  pb_bicg_init_array
#define print_array pb_bicg_print_array
#define main        pb_bicg_unused_main
#include "../third_party/polybench-c/linear-algebra/kernels/bicg/bicg.c"
#undef init_array
#undef print_array
#undef main
#undef M
#undef N
#undef _PB_M
#undef _PB_N

// (e) stencils/jacobi-2d
#define init_array  pb_jacobi2d_init_array
#define print_array pb_jacobi2d_print_array
#define main        pb_jacobi2d_unused_main
#include "../third_party/polybench-c/stencils/jacobi-2d/jacobi-2d.c"
#undef init_array
#undef print_array
#undef main
#undef TSTEPS
#undef N
#undef _PB_TSTEPS
#undef _PB_N

// (f) linear-algebra/solvers/ludcmp —— LU 分解 + 前后代入（替代原自写 PID）
#define init_array  pb_ludcmp_init_array
#define print_array pb_ludcmp_print_array
#define main        pb_ludcmp_unused_main
#include "../third_party/polybench-c/linear-algebra/solvers/ludcmp/ludcmp.c"
#undef init_array
#undef print_array
#undef main
#undef N
#undef _PB_N

// (g) linear-algebra/solvers/durbin —— Levinson-Durbin 线性预测（替代原自写 EKF）
#define init_array  pb_durbin_init_array
#define print_array pb_durbin_print_array
#define main        pb_durbin_unused_main
#include "../third_party/polybench-c/linear-algebra/solvers/durbin/durbin.c"
#undef init_array
#undef print_array
#undef main
#undef N
#undef _PB_N

// (h) medley/deriche —— Deriche 递归图像滤波（替代原自写 5x5 高斯卷积）
#define init_array  pb_deriche_init_array
#define print_array pb_deriche_print_array
#define main        pb_deriche_unused_main
#include "../third_party/polybench-c/medley/deriche/deriche.c"
#undef init_array
#undef print_array
#undef main
#undef W
#undef H
#undef _PB_W
#undef _PB_H

// (k) datamining/correlation
#define init_array  pb_correlation_init_array
#define print_array pb_correlation_print_array
#define main        pb_correlation_unused_main
#include "../third_party/polybench-c/datamining/correlation/correlation.c"
#undef init_array
#undef print_array
#undef main
#undef M
#undef N
#undef _PB_M
#undef _PB_N

// (l) datamining/covariance
#define init_array  pb_covariance_init_array
#define print_array pb_covariance_print_array
#define main        pb_covariance_unused_main
#include "../third_party/polybench-c/datamining/covariance/covariance.c"
#undef init_array
#undef print_array
#undef main
#undef M
#undef N
#undef _PB_M
#undef _PB_N

// (m) linear-algebra/blas/gemver
#define init_array  pb_gemver_init_array
#define print_array pb_gemver_print_array
#define main        pb_gemver_unused_main
#include "../third_party/polybench-c/linear-algebra/blas/gemver/gemver.c"
#undef init_array
#undef print_array
#undef main
#undef N
#undef _PB_N

// (n) linear-algebra/blas/gesummv
#define init_array  pb_gesummv_init_array
#define print_array pb_gesummv_print_array
#define main        pb_gesummv_unused_main
#include "../third_party/polybench-c/linear-algebra/blas/gesummv/gesummv.c"
#undef init_array
#undef print_array
#undef main
#undef N
#undef _PB_N


// (o) linear-algebra/blas/symm
#define init_array  pb_symm_init_array
#define print_array pb_symm_print_array
#define main        pb_symm_unused_main
#include "../third_party/polybench-c/linear-algebra/blas/symm/symm.c"
#undef init_array
#undef print_array
#undef main
#undef M
#undef N
#undef _PB_M
#undef _PB_N

// (p) linear-algebra/blas/syr2k
#define init_array  pb_syr2k_init_array
#define print_array pb_syr2k_print_array
#define main        pb_syr2k_unused_main
#include "../third_party/polybench-c/linear-algebra/blas/syr2k/syr2k.c"
#undef init_array
#undef print_array
#undef main
#undef M
#undef N
#undef _PB_M
#undef _PB_N

// (q) linear-algebra/blas/syrk
#define init_array  pb_syrk_init_array
#define print_array pb_syrk_print_array
#define main        pb_syrk_unused_main
#include "../third_party/polybench-c/linear-algebra/blas/syrk/syrk.c"
#undef init_array
#undef print_array
#undef main
#undef M
#undef N
#undef _PB_M
#undef _PB_N

// (r) linear-algebra/blas/trmm
#define init_array  pb_trmm_init_array
#define print_array pb_trmm_print_array
#define main        pb_trmm_unused_main
#include "../third_party/polybench-c/linear-algebra/blas/trmm/trmm.c"
#undef init_array
#undef print_array
#undef main
#undef M
#undef N
#undef _PB_M
#undef _PB_N


// (s) linear-algebra/kernels/3mm
#define init_array  pb_3mm_init_array
#define print_array pb_3mm_print_array
#define main        pb_3mm_unused_main
#include "../third_party/polybench-c/linear-algebra/kernels/3mm/3mm.c"
#undef init_array
#undef print_array
#undef main
#undef NI
#undef NJ
#undef NK
#undef NL
#undef NM
#undef _PB_NI
#undef _PB_NJ
#undef _PB_NK
#undef _PB_NL
#undef _PB_NM

// (t) linear-algebra/kernels/doitgen
#define init_array  pb_doitgen_init_array
#define print_array pb_doitgen_print_array
#define main        pb_doitgen_unused_main
#include "../third_party/polybench-c/linear-algebra/kernels/doitgen/doitgen.c"
#undef init_array
#undef print_array
#undef main
#undef NQ
#undef NR
#undef NP
#undef _PB_NQ
#undef _PB_NR
#undef _PB_NP

// (u) linear-algebra/kernels/mvt
#define init_array  pb_mvt_init_array
#define print_array pb_mvt_print_array
#define main        pb_mvt_unused_main
#include "../third_party/polybench-c/linear-algebra/kernels/mvt/mvt.c"
#undef init_array
#undef print_array
#undef main
#undef N
#undef _PB_N


// (v) linear-algebra/solvers/cholesky（上游 init 已构造正定矩阵，保留其 init）
#define init_array  pb_cholesky_init_array
#define print_array pb_cholesky_print_array
#define main        pb_cholesky_unused_main
#include "../third_party/polybench-c/linear-algebra/solvers/cholesky/cholesky.c"
#undef init_array
#undef print_array
#undef main
#undef N
#undef _PB_N

// (w) linear-algebra/solvers/gramschmidt
#define init_array  pb_gramschmidt_init_array
#define print_array pb_gramschmidt_print_array
#define main        pb_gramschmidt_unused_main
#include "../third_party/polybench-c/linear-algebra/solvers/gramschmidt/gramschmidt.c"
#undef init_array
#undef print_array
#undef main
#undef M
#undef N
#undef _PB_M
#undef _PB_N

// (x) linear-algebra/solvers/lu
#define init_array  pb_lu_init_array
#define print_array pb_lu_print_array
#define main        pb_lu_unused_main
#include "../third_party/polybench-c/linear-algebra/solvers/lu/lu.c"
#undef init_array
#undef print_array
#undef main
#undef N
#undef _PB_N

// (y) linear-algebra/solvers/trisolv
#define init_array  pb_trisolv_init_array
#define print_array pb_trisolv_print_array
#define main        pb_trisolv_unused_main
#include "../third_party/polybench-c/linear-algebra/solvers/trisolv/trisolv.c"
#undef init_array
#undef print_array
#undef main
#undef N
#undef _PB_N


// (z) stencils/adi
#define init_array  pb_adi_init_array
#define print_array pb_adi_print_array
#define main        pb_adi_unused_main
#include "../third_party/polybench-c/stencils/adi/adi.c"
#undef init_array
#undef print_array
#undef main
#undef TSTEPS
#undef N
#undef _PB_TSTEPS
#undef _PB_N

// (aa) stencils/fdtd-2d
#define init_array  pb_fdtd2d_init_array
#define print_array pb_fdtd2d_print_array
#define main        pb_fdtd2d_unused_main
#include "../third_party/polybench-c/stencils/fdtd-2d/fdtd-2d.c"
#undef init_array
#undef print_array
#undef main
#undef TMAX
#undef NX
#undef NY
#undef _PB_TMAX
#undef _PB_NX
#undef _PB_NY

// (ab) stencils/heat-3d
#define init_array  pb_heat3d_init_array
#define print_array pb_heat3d_print_array
#define main        pb_heat3d_unused_main
#include "../third_party/polybench-c/stencils/heat-3d/heat-3d.c"
#undef init_array
#undef print_array
#undef main
#undef TSTEPS
#undef N
#undef _PB_TSTEPS
#undef _PB_N

// (ac) stencils/jacobi-1d
#define init_array  pb_jacobi1d_init_array
#define print_array pb_jacobi1d_print_array
#define main        pb_jacobi1d_unused_main
#include "../third_party/polybench-c/stencils/jacobi-1d/jacobi-1d.c"
#undef init_array
#undef print_array
#undef main
#undef TSTEPS
#undef N
#undef _PB_TSTEPS
#undef _PB_N

// (ad) stencils/seidel-2d
#define init_array  pb_seidel2d_init_array
#define print_array pb_seidel2d_print_array
#define main        pb_seidel2d_unused_main
#include "../third_party/polybench-c/stencils/seidel-2d/seidel-2d.c"
#undef init_array
#undef print_array
#undef main
#undef TSTEPS
#undef N
#undef _PB_TSTEPS
#undef _PB_N


// ============================================================================
// 2. PolyBench/C 4.2.1 —— INT32 内核（不规则控制流 / 整数数据通路）
//    切换 DATA_TYPE 到 int，需先清掉 FP64 一组宏定义
// ============================================================================
#undef DATA_TYPE_IS_DOUBLE
#undef DATA_TYPE
#undef DATA_PRINTF_MODIFIER
#undef SCALAR_VAL
#undef SQRT_FUN
#undef EXP_FUN
#undef POW_FUN
#define DATA_TYPE_IS_INT

// (i) medley/floyd-warshall —— 全源最短路，整数 min/加法 + 三重循环依赖
#define init_array  pb_fw_init_array
#define print_array pb_fw_print_array
#define main        pb_fw_unused_main
#include "../third_party/polybench-c/medley/floyd-warshall/floyd-warshall.c"
#undef init_array
#undef print_array
#undef main
#undef N
#undef _PB_N

// (j) medley/nussinov —— RNA 折叠动态规划，分支密集 + 三角形迭代空间
#define init_array  pb_nussinov_init_array
#define print_array pb_nussinov_print_array
#define main        pb_nussinov_unused_main
#include "../third_party/polybench-c/medley/nussinov/nussinov.c"
#undef init_array
#undef print_array
#undef main
#undef N
#undef _PB_N
#undef match
#undef max_score

#undef static

// ============================================================================
// 3. KissFFT v1.3.1 原版源文件（third_party/kissfft/kiss_fft.c）
// ============================================================================
#define kiss_fft_scalar double
#include "../third_party/kissfft/kiss_fft.c"

// ============================================================================
// 4. PARSEC Black-Scholes（llvm-loop-perforation 上游单文件移植版）
// ============================================================================
#define float double
double __attribute__((always_inline)) CNDF(double InputX);
double __attribute__((always_inline)) BlkSchlsEqEuroNoDiv(double sptprice, double strike, double rate, double volatility, double time, int otype, double timet);
#define main parsec_blackscholes_unused_main
#include "../third_party/llvm-loop-perforation/benchmarks/blackscholes/blackscholes.c"
#undef main
#undef float

// ============================================================================
// 5. Sobel 图像滤波（llvm-loop-perforation 上游 benchmarks/sobel/sobel.c）
//    上游内核通过 mypgm.h 中的全局图像缓冲 image1/image2 传参，并在计算过程中
//    向 stdout 打印进度；评测 harness 以 stdout 传递 CSV，故此处仅用宏把上游的
//    printf 重定向到空实现，**计算代码未作任何改动**。
// ============================================================================
static int atft_sobel_mute_printf(const char *fmt, ...) { (void)fmt; return 0; }
#define printf atft_sobel_mute_printf
#define main   sobel_unused_main
#include "../third_party/llvm-loop-perforation/benchmarks/sobel/sobel.c"
#undef main
#undef printf

// ============================================================================
// 6. COAST 星载容错测试程序（BYU/LANL，IEEE TNS'19 中子束实测负载）
// ============================================================================
#define main crc16_unused_main
#include "../third_party/coast/tests/crc16/crc16.c"
#undef main

#define main   quicksort_unused_main
#define printf atft_sobel_mute_printf
#include "../third_party/coast/tests/quicksort/quicksort.c"
#undef printf
#undef main

// ============================================================================
// 7. OBPMark 1.1 影像校正与定标（ESA + BSC 官方星载处理基准）
//    上游 ports/cpu/processing.c 原封不动编译。上游的 include/device.h 会经
//    util_prints.h -> benchmark.h 引入 <string> 等 C++ 头并声明重载 init()，
//    无法用 C 编译；因此用 benchmarks/obpmark_shim/device.h 作最小替身，
//    只复刻其"顺序 C 版本"分支的数据类型，计算内核源码未作任何改动。
// ============================================================================
#include "../third_party/obpmark/src/1.1-image/src/ports/cpu/processing.c"

// ============================================================================
// Entry Wrappers —— 仅做参数转接，直接调用上游内核
// ============================================================================

__attribute__((noinline))
void bench_pb_gemm(int ni, int nj, int nk, double alpha, double beta,
                   const double *A, const double *B, double *C) {
    kernel_gemm(ni, nj, nk, alpha, beta,
                (double (*)[nj])C,
                (double (*)[nk])A,
                (double (*)[nj])B);
}

__attribute__((noinline))
void bench_pb_2mm(int ni, int nj, int nk, int nl, double alpha, double beta,
                  double *tmp, const double *A, const double *B,
                  const double *C, double *D) {
    kernel_2mm(ni, nj, nk, nl, alpha, beta,
               (double (*)[nj])tmp,
               (double (*)[nk])A,
               (double (*)[nj])B,
               (double (*)[nl])C,
               (double (*)[nl])D);
}

__attribute__((noinline))
void bench_pb_atax(int m, int n, const double *A, const double *x,
                   double *y, double *tmp) {
    kernel_atax(m, n, (double (*)[n])A, (double *)x, (double *)y, (double *)tmp);
}

__attribute__((noinline))
void bench_pb_bicg(int m, int n, const double *A, double *s, double *q,
                   const double *p, const double *r) {
    kernel_bicg(m, n, (double (*)[m])A, s, q, (double *)p, (double *)r);
}

__attribute__((noinline))
void bench_pb_jacobi2d(int tsteps, int n, double *A, double *B) {
    kernel_jacobi_2d(tsteps, n, (double (*)[n])A, (double (*)[n])B);
}

__attribute__((noinline))
void bench_pb_ludcmp(int n, double *A, double *b, double *x, double *y) {
    kernel_ludcmp(n, (double (*)[n])A, b, x, y);
}

__attribute__((noinline))
void bench_pb_durbin(int n, const double *r, double *y) {
    kernel_durbin(n, (double *)r, y);
}

__attribute__((noinline))
void bench_pb_deriche(int w, int h, double alpha, const double *imgIn,
                      double *imgOut, double *y1, double *y2) {
    kernel_deriche(w, h, alpha,
                   (double (*)[h])imgIn,
                   (double (*)[h])imgOut,
                   (double (*)[h])y1,
                   (double (*)[h])y2);
}

__attribute__((noinline))
void bench_pb_correlation(int m, int n, double float_n,
                          double *data, double *corr, double *mean, double *stddev) {
    kernel_correlation(m, n, float_n, (double (*)[m])data, (double (*)[m])corr, mean, stddev);
}

__attribute__((noinline))
void bench_pb_covariance(int m, int n, double float_n,
                         double *data, double *cov, double *mean) {
    kernel_covariance(m, n, float_n, (double (*)[m])data, (double (*)[m])cov, mean);
}

__attribute__((noinline))
void bench_pb_gemver(int n, double alpha, double beta, double *A,
                     double *u1, double *v1, double *u2, double *v2,
                     double *w, double *x, double *y, double *z) {
    kernel_gemver(n, alpha, beta, (double (*)[n])A, u1, v1, u2, v2, w, x, y, z);
}

__attribute__((noinline))
void bench_pb_gesummv(int n, double alpha, double beta,
                      double *A, double *B, double *tmp, double *x, double *y) {
    kernel_gesummv(n, alpha, beta, (double (*)[n])A, (double (*)[n])B, tmp, x, y);
}

__attribute__((noinline))
void bench_pb_symm(int m, int n, double alpha, double beta,
                   double *C, double *A, double *B) {
    kernel_symm(m, n, alpha, beta, (double (*)[n])C, (double (*)[m])A, (double (*)[n])B);
}

__attribute__((noinline))
void bench_pb_syr2k(int n, int m, double alpha, double beta,
                    double *C, double *A, double *B) {
    kernel_syr2k(n, m, alpha, beta, (double (*)[n])C, (double (*)[m])A, (double (*)[m])B);
}

__attribute__((noinline))
void bench_pb_syrk(int n, int m, double alpha, double beta, double *C, double *A) {
    kernel_syrk(n, m, alpha, beta, (double (*)[n])C, (double (*)[m])A);
}

__attribute__((noinline))
void bench_pb_trmm(int m, int n, double alpha, double *A, double *B) {
    kernel_trmm(m, n, alpha, (double (*)[m])A, (double (*)[n])B);
}

__attribute__((noinline))
void bench_pb_3mm(int ni, int nj, int nk, int nl, int nm,
                  double *E, double *A, double *B, double *F,
                  double *C, double *D, double *G) {
    kernel_3mm(ni, nj, nk, nl, nm,
               (double (*)[nj])E, (double (*)[nk])A, (double (*)[nj])B,
               (double (*)[nl])F, (double (*)[nm])C, (double (*)[nl])D,
               (double (*)[nl])G);
}

__attribute__((noinline))
void bench_pb_doitgen(int nr, int nq, int np, double *A, double *C4, double *sum) {
    kernel_doitgen(nr, nq, np, (double (*)[nq][np])A, (double (*)[np])C4, sum);
}

__attribute__((noinline))
void bench_pb_mvt(int n, double *x1, double *x2, double *y1, double *y2, double *A) {
    kernel_mvt(n, x1, x2, y1, y2, (double (*)[n])A);
}

__attribute__((noinline))
void bench_pb_cholesky(int n, double *A) {
    kernel_cholesky(n, (double (*)[n])A);
}

__attribute__((noinline))
void bench_pb_gramschmidt(int m, int n, double *A, double *R, double *Q) {
    kernel_gramschmidt(m, n, (double (*)[n])A, (double (*)[n])R, (double (*)[n])Q);
}

__attribute__((noinline))
void bench_pb_lu(int n, double *A) {
    kernel_lu(n, (double (*)[n])A);
}

__attribute__((noinline))
void bench_pb_trisolv(int n, double *L, double *x, double *b) {
    kernel_trisolv(n, (double (*)[n])L, x, b);
}

__attribute__((noinline))
void bench_pb_adi(int tsteps, int n, double *u, double *v, double *p, double *q) {
    kernel_adi(tsteps, n, (double (*)[n])u, (double (*)[n])v,
               (double (*)[n])p, (double (*)[n])q);
}

__attribute__((noinline))
void bench_pb_fdtd2d(int tmax, int nx, int ny,
                     double *ex, double *ey, double *hz, double *fict) {
    kernel_fdtd_2d(tmax, nx, ny, (double (*)[ny])ex, (double (*)[ny])ey,
                   (double (*)[ny])hz, fict);
}

__attribute__((noinline))
void bench_pb_heat3d(int tsteps, int n, double *A, double *B) {
    kernel_heat_3d(tsteps, n, (double (*)[n][n])A, (double (*)[n][n])B);
}

__attribute__((noinline))
void bench_pb_jacobi1d(int tsteps, int n, double *A, double *B) {
    kernel_jacobi_1d(tsteps, n, A, B);
}

__attribute__((noinline))
void bench_pb_seidel2d(int tsteps, int n, double *A) {
    kernel_seidel_2d(tsteps, n, (double (*)[n])A);
}

__attribute__((noinline))
void bench_pb_floyd_warshall(int n, int32_t *path) {
    kernel_floyd_warshall(n, (int (*)[n])path);
}

__attribute__((noinline))
void bench_pb_nussinov(int n, const char *seq, int32_t *table) {
    kernel_nussinov(n, (char *)seq, (int (*)[n])table);
}

__attribute__((noinline))
void bench_parsec_blackscholes(int n_options, const double *sptprice_in,
                               const double *strike_in, const double *rate_in,
                               const double *volatility_in, const double *otime_in,
                               double *prices_out) {
    for (int i = 0; i < n_options; i++) {
        // 直接调用上游 blackscholes.c 的 BlkSchlsEqEuroNoDiv() / CNDF()
        prices_out[i] = BlkSchlsEqEuroNoDiv(
            sptprice_in[i], strike_in[i], rate_in[i],
            volatility_in[i], otime_in[i], (i & 1), 0.0);
    }
}

__attribute__((noinline))
void bench_dsp_kissfft(int n, const double *in_re, const double *in_im,
                       double *out_re, double *out_im) {
    // 直接调用上游 kiss_fft_alloc() / kf_work() / kf_bfly4()
    static struct kiss_fft_state *cached_cfg = NULL;
    static int cached_n = 0;
    if (!cached_cfg || cached_n != n) {
        if (cached_cfg) KISS_FFT_FREE(cached_cfg);
        cached_cfg = kiss_fft_alloc(n, 0, NULL, NULL);
        cached_n = n;
    }
    kiss_fft_cpx fin[256];
    kiss_fft_cpx fout[256];
    for (int i = 0; i < n; i++) {
        fin[i].r = in_re[i];
        fin[i].i = in_im[i];
    }
    kiss_fft(cached_cfg, fin, fout);
    for (int i = 0; i < n; i++) {
        out_re[i] = fout[i].r;
        out_im[i] = fout[i].i;
    }
}

__attribute__((noinline))
void bench_axc_sobel(int h, int w, const unsigned char *img_in,
                     unsigned char *img_out) {
    // 上游 sobel.c 通过 mypgm.h 的全局缓冲传参
    x_size1 = w;
    y_size1 = h;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            image1[y][x] = img_in[y * w + x];

    sobel_filtering();

    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            img_out[y * w + x] = image2[y][x];
}

// OBPMark 1.1 第一阶段：对应上游 prepare_image_frame()
//   [I] 偏置校正 f_offset -> [II] 坏像元替换 f_mask_replace
__attribute__((noinline))
void bench_obpmark_image_prep(int w, int h, uint16_t *frame,
                              uint16_t *offsets, uint8_t *bad_pixels) {
    frame16_t f  = { frame,   (unsigned int)w, (unsigned int)h };
    frame16_t off = { offsets, (unsigned int)w, (unsigned int)h };
    frame8_t  bad = { bad_pixels, (unsigned int)w, (unsigned int)h };
    f_offset(&f, &off);
    f_mask_replace(&f, &bad);
}

// OBPMark 1.1 第二阶段：对应上游 proc_image_frame()
//   [III] 辐射擦洗 f_scrub -> [IV] 增益校正 f_gain
//   -> [V] 2x2 空间合并 f_2x2_bin -> [VI] 多帧累加 f_coadd
__attribute__((noinline))
void bench_obpmark_image_proc(int w, int h, int num_frames, int frame_i,
                              uint16_t *frame, uint16_t *all_frames,
                              uint16_t *gains,
                              uint32_t *binned_frame, uint32_t *image_output) {
    frame16_t f = { frame, (unsigned int)w, (unsigned int)h };
    frame16_t fs[8];
    int nf = num_frames > 8 ? 8 : num_frames;
    for (int i = 0; i < nf; i++) {
        fs[i].f = all_frames + (size_t)i * (size_t)w * (size_t)h;
        fs[i].w = (unsigned int)w;
        fs[i].h = (unsigned int)h;
    }
    frame16_t g  = { gains, (unsigned int)w, (unsigned int)h };
    frame32_t bf = { binned_frame, (unsigned int)(w / 2), (unsigned int)(h / 2) };
    frame32_t out = { image_output, (unsigned int)(w / 2), (unsigned int)(h / 2) };

    f_scrub(&f, fs, (unsigned int)frame_i);
    f_gain(&f, &g);
    f_2x2_bin(&f, &bf);
    f_coadd(&out, &bf);
}

__attribute__((noinline))
uint16_t bench_coast_crc16(const unsigned char *data, unsigned char length) {
    return (uint16_t)crc16(data, length);
}

__attribute__((noinline))
void bench_coast_quicksort(int32_t *arr, int n) {
    quick_sort((int *)arr, n);
}
