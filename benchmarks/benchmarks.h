#ifndef ATFT_BENCHMARKS_H
#define ATFT_BENCHMARKS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// 标准开源基准测试套件接口 (12 Benchmarks across 4 Standard Domains)
//   Suite A: PolyBench/C 4.2.1 (Pouchet & Yuki, OSU/Inria)
//     1. pb_gemm      (linear-algebra/blas/gemm)
//     2. pb_2mm       (linear-algebra/kernels/2mm)
//     3. pb_atax      (linear-algebra/kernels/atax)
//     4. pb_bicg      (linear-algebra/kernels/bicg)
//     5. pb_jacobi2d  (stencils/jacobi-2d)
//   Suite B: PARSEC & ACCEPT Approximate Computing Benchmark Suite
//     6. parsec_blackscholes (PARSEC 3.0 / Intel Black-Scholes PDE pricing)
//     7. axc_sobel           (ACCEPT / loop-perforation Sobel gradient filter)
//     8. axc_gaussian        (ACCEPT / 5x5 Gaussian image convolution)
//   Suite C: Digital Signal Processing (KissFFT & CMSIS-DSP)
//     9. dsp_kissfft         (KissFFT mixed-radix spectral transform)
//    10. dsp_fir             (Time-domain FIR filter kernel)
//   Suite D: GNC Attitude & Flight Control (Guidance, Navigation, and Control)
//    11. gnc_pid             (Closed-loop PID attitude controller)
//    12. gnc_ekf             (2-state Extended Kalman Filter with covariance update)
// ============================================================================

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

void bench_parsec_blackscholes(int n_options, const double *sptprice,
                               const double *strike, const double *rate,
                               const double *volatility, const double *otime,
                               double *prices_out);

void bench_axc_sobel(int h, int w, const double *img_in, double *img_out);

void bench_axc_gaussian(int h, int w, const double *img_in, double *img_out);

void bench_dsp_kissfft(int n, const double *in_re, const double *in_im,
                       double *out_re, double *out_im);

void bench_dsp_fir(int n_samples, int n_taps, const double *signal,
                   const double *coeffs, double *output);

void bench_gnc_pid(int steps, const double *setpoints, const double *measurements,
                   double kp, double ki, double kd, double dt, double *control_out);

void bench_gnc_ekf(int steps, const double *gyro_rates, const double *accel_angles,
                   double dt, double *state_est_out);

double bench_noncritical_telemetry_checksum(int n, const double *buf);

#ifdef __cplusplus
}
#endif

#endif // ATFT_BENCHMARKS_H
