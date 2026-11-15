#ifndef ATFT_BENCHMARKS_H
#define ATFT_BENCHMARKS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// 标准开源基准测试套件接口 (17 Benchmarks, 5 Upstream Suites, 0 Self-Written)
//
//   本头文件中的每一个 bench_* 包装函数都只负责参数转接，真正的计算内核
//   全部来自 third_party/ 下未经修改的上游源文件（见 benchmarks.c 的 #include）。
//
//   Suite A: PolyBench/C 4.2.1 (Pouchet & Yuki, OSU/Inria) —— 10 个内核
//     [FP64 规整线性代数]
//       1. pb_gemm            (linear-algebra/blas/gemm)
//       2. pb_2mm             (linear-algebra/kernels/2mm)
//       3. pb_atax            (linear-algebra/kernels/atax)
//       4. pb_bicg            (linear-algebra/kernels/bicg)
//     [FP64 Stencil]
//       5. pb_jacobi2d        (stencils/jacobi-2d)
//     [FP64 线性方程求解 / 线性预测——替代原自写 PID/EKF 控制内核]
//       6. pb_ludcmp          (linear-algebra/solvers/ludcmp)
//       7. pb_durbin          (linear-algebra/solvers/durbin, Levinson-Durbin)
//     [FP64 图像递归滤波——替代原自写高斯卷积]
//       8. pb_deriche         (medley/deriche, Deriche 边缘/平滑滤波)
//     [INT32 不规则控制流与数据依赖]
//       9. pb_floyd_warshall  (medley/floyd-warshall, 全源最短路)
//      10. pb_nussinov        (medley/nussinov, RNA 折叠动态规划)
//
//   Suite B: PARSEC Black-Scholes (经 llvm-loop-perforation 移植的单文件版本)
//      11. parsec_blackscholes (超越函数密集的期权定价 PDE)
//
//   Suite C: KissFFT v1.3.1 (Mark Borgerding)
//      12. dsp_kissfft        (混合基 FFT，kiss_fft.c 原版蝶形核)
//
//   Suite D: llvm-loop-perforation 图像基准 (Cornell CS6120)
//      13. axc_sobel          (benchmarks/sobel/sobel.c 原版 Sobel 滤波)
//
//   Suite E: COAST 星载容错测试程序 (BYU / LANL, IEEE TNS'19 实测负载)
//      14. coast_crc16        (tests/crc16/crc16.c, CCITT CRC-16 整数位运算)
//      15. coast_quicksort    (tests/quicksort/quicksort.c, LANL 递归快排)
//
//   Suite F: OBPMark v0.3 (ESA + Barcelona Supercomputing Center) —— 空天域负载
//      16. obpmark_image_prep (1.1 影像校正与定标：偏置校正 + 坏像元替换)
//      17. obpmark_image_proc (1.1 影像校正与定标：辐射擦洗 + 增益校正
//                              + 2x2 空间合并 + 多帧累加)
//      这是 ESA 官方发布的星载数据处理基准，直接对应卫星成像仪在轨的校正流水线。
//      引用：D. Steenari 等，OBPMark (On-Board Processing Benchmarks)，ESA + BSC，
//            OBDP2021；许可：ESA Public License (ESA-PL) Strong Copyleft v2.3
//
//   QoR 判据分两类：
//     - FP64 负载：相对 L2 误差 / PSNR，可容忍近似分支的固有误差；
//     - INT32 / uint16 负载（9/10/14/15/16/17）：按位精确匹配，任何非零偏差即判为 SDC。
// ============================================================================

// ---- Suite A: PolyBench/C 4.2.1, FP64 ----
void bench_pb_gemm(int ni, int nj, int nk, double alpha, double beta,
                   const double *A, const double *B, double *C);

void bench_pb_2mm(int ni, int nj, int nk, int nl, double alpha, double beta,
                  double *tmp, const double *A, const double *B,
                  const double *C, double *D);

void bench_pb_atax(int m, int n, const double *A, const double *x,
                   double *y, double *tmp);

void bench_pb_bicg(int m, int n, const double *A, double *s, double *q,
                   const double *p, const double *r);

void bench_pb_jacobi2d(int tsteps, int n, double *A, double *B);

void bench_pb_ludcmp(int n, double *A, double *b, double *x, double *y);

void bench_pb_durbin(int n, const double *r, double *y);

void bench_pb_deriche(int w, int h, double alpha, const double *imgIn,
                      double *imgOut, double *y1, double *y2);

// ---- Suite A (扩展): PolyBench/C 4.2.1 完整 30 内核的其余 20 个 FP64 内核 ----
void bench_pb_correlation(int m, int n, double float_n,
                          double *data, double *corr, double *mean, double *stddev);
void bench_pb_covariance(int m, int n, double float_n,
                         double *data, double *cov, double *mean);
void bench_pb_gemver(int n, double alpha, double beta, double *A,
                     double *u1, double *v1, double *u2, double *v2,
                     double *w, double *x, double *y, double *z);
void bench_pb_gesummv(int n, double alpha, double beta,
                      double *A, double *B, double *tmp, double *x, double *y);
void bench_pb_symm(int m, int n, double alpha, double beta,
                   double *C, double *A, double *B);
void bench_pb_syr2k(int n, int m, double alpha, double beta,
                    double *C, double *A, double *B);
void bench_pb_syrk(int n, int m, double alpha, double beta, double *C, double *A);
void bench_pb_trmm(int m, int n, double alpha, double *A, double *B);
void bench_pb_3mm(int ni, int nj, int nk, int nl, int nm,
                  double *E, double *A, double *B, double *F,
                  double *C, double *D, double *G);
void bench_pb_doitgen(int nr, int nq, int np, double *A, double *C4, double *sum);
void bench_pb_mvt(int n, double *x1, double *x2, double *y1, double *y2, double *A);
void bench_pb_cholesky(int n, double *A);
void bench_pb_gramschmidt(int m, int n, double *A, double *R, double *Q);
void bench_pb_lu(int n, double *A);
void bench_pb_trisolv(int n, double *L, double *x, double *b);
void bench_pb_adi(int tsteps, int n, double *u, double *v, double *p, double *q);
void bench_pb_fdtd2d(int tmax, int nx, int ny,
                     double *ex, double *ey, double *hz, double *fict);
void bench_pb_heat3d(int tsteps, int n, double *A, double *B);
void bench_pb_jacobi1d(int tsteps, int n, double *A, double *B);
void bench_pb_seidel2d(int tsteps, int n, double *A);

// ---- Suite A: PolyBench/C 4.2.1, INT32 (不规则控制流) ----
void bench_pb_floyd_warshall(int n, int32_t *path);

void bench_pb_nussinov(int n, const char *seq, int32_t *table);

// ---- Suite B: PARSEC Black-Scholes ----
void bench_parsec_blackscholes(int n_options, const double *sptprice,
                               const double *strike, const double *rate,
                               const double *volatility, const double *otime,
                               double *prices_out);

// ---- Suite C: KissFFT ----
void bench_dsp_kissfft(int n, const double *in_re, const double *in_im,
                       double *out_re, double *out_im);

// ---- Suite D: llvm-loop-perforation Sobel (上游 uint8 图像接口) ----
void bench_axc_sobel(int h, int w, const unsigned char *img_in,
                     unsigned char *img_out);

// ---- Suite F: OBPMark 1.1 影像校正与定标 (ESA + BSC, 空天域负载) ----
//   上游：third_party/obpmark/src/1.1-image/src/ports/cpu/processing.c
//   两个入口对应上游 prepare_image_frame / proc_image_frame 的两阶段划分
void bench_obpmark_image_prep(int w, int h, uint16_t *frame,
                              uint16_t *offsets, uint8_t *bad_pixels);

void bench_obpmark_image_proc(int w, int h, int num_frames, int frame_i,
                              uint16_t *frame, uint16_t *all_frames,
                              uint16_t *gains,
                              uint32_t *binned_frame, uint32_t *image_output);

// ---- Suite E: COAST 星载测试程序, INT32 ----
uint16_t bench_coast_crc16(const unsigned char *data, unsigned char length);

void bench_coast_quicksort(int32_t *arr, int n);

#ifdef __cplusplus
}
#endif

#endif // ATFT_BENCHMARKS_H
