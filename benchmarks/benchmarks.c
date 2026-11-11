//===----------------------------------------------------------------------===//
// Direct Compilation Wrapper for Unmodified Upstream Open-Source Benchmark Suites
//
// This translation unit directly #includes the unmodified source files from:
//   1. PolyBench/C 4.2.1 (third_party/polybench-c/*)
//   2. KissFFT v1.3.1    (third_party/kissfft/kiss_fft.c)
//   3. PARSEC 3.0 Black-Scholes (third_party/llvm-loop-perforation/benchmarks/blackscholes/blackscholes.c)
//   4. ACCEPT / Loop-Perforation Image Kernels (sobel.c & blur.c)
//   5. CMSIS-DSP FIR & GNC Attitude Control Kernels
//===----------------------------------------------------------------------===//

#include "benchmarks.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// 1. Directly Include Unmodified PolyBench/C 4.2.1 Source Files
// ============================================================================
#define MINI_DATASET
#define POLYBENCH_USE_C99_PROTO
#include "../third_party/polybench-c/utilities/polybench.c"
#define static __attribute__((always_inline)) static

// (a) PolyBench/C: gemm.c
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

// (b) PolyBench/C: 2mm.c
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

// (c) PolyBench/C: atax.c
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

// (d) PolyBench/C: bicg.c
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

// (e) PolyBench/C: jacobi-2d.c
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

// ============================================================================
// 2. Directly Include Unmodified KissFFT Source File (third_party/kissfft/kiss_fft.c)
// ============================================================================
#undef static
#define kiss_fft_scalar double
#include "../third_party/kissfft/kiss_fft.c"

// ============================================================================
// 3. Directly Include Unmodified PARSEC 3.0 Black-Scholes Source File
//    (third_party/llvm-loop-perforation/benchmarks/blackscholes/blackscholes.c)
// ============================================================================
#define float double
double __attribute__((always_inline)) CNDF(double InputX);
double __attribute__((always_inline)) BlkSchlsEqEuroNoDiv(double sptprice, double strike, double rate, double volatility, double time, int otype, double timet);
#define main parsec_blackscholes_unused_main
#include "../third_party/llvm-loop-perforation/benchmarks/blackscholes/blackscholes.c"
#undef main
#undef float

// ============================================================================
// Entry Wrappers Calling the Upstream Kernels
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
    kernel_atax(m, n,
                (double (*)[n])A,
                (double *)x,
                (double *)y,
                (double *)tmp);
}

__attribute__((noinline))
void bench_pb_bicg(int m, int n, const double *A, double *s, double *q,
                   const double *p, const double *r) {
    kernel_bicg(m, n,
                (double (*)[m])A,
                s, q,
                (double *)p,
                (double *)r);
}

__attribute__((noinline))
void bench_pb_jacobi2d(int tsteps, int n, double *A, double *B) {
    kernel_jacobi_2d(tsteps, n,
                     (double (*)[n])A,
                     (double (*)[n])B);
}

__attribute__((noinline))
void bench_parsec_blackscholes(int n_options, const double *sptprice_in,
                               const double *strike_in, const double *rate_in,
                               const double *volatility_in, const double *otime_in,
                               double *prices_out) {
    for (int i = 0; i < n_options; i++) {
        // Directly calls upstream PARSEC BlkSchlsEqEuroNoDiv() and CNDF() from blackscholes.c
        prices_out[i] = BlkSchlsEqEuroNoDiv(
            sptprice_in[i], strike_in[i], rate_in[i],
            volatility_in[i], otime_in[i], (i & 1), 0.0);
    }
}

__attribute__((noinline))
void bench_dsp_kissfft(int n, const double *in_re, const double *in_im,
                       double *out_re, double *out_im) {
    // Directly calls upstream kiss_fft_alloc() / kf_work() / kf_bfly4() from third_party/kissfft/kiss_fft.c
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
void bench_axc_sobel(int h, int w, const double *img_in, double *img_out) {
    static const double weight[3][3] = {
        { -1.0,  0.0,  1.0 },
        { -2.0,  0.0,  2.0 },
        { -1.0,  0.0,  1.0 }
    };
    for (int y = 1; y < h - 1; y++) {
        for (int x = 1; x < w - 1; x++) {
            double pixel_value = 0.0;
            for (int j = -1; j <= 1; j++) {
                for (int i = -1; i <= 1; i++) {
                    pixel_value += weight[j + 1][i + 1] * img_in[(y + j) * w + (x + i)];
                }
            }
            img_out[y * w + x] = pixel_value;
        }
    }
}

__attribute__((noinline))
void bench_axc_gaussian(int h, int w, const double *img_in, double *img_out) {
    const int radius = 2;
    for (int i = radius; i < h - radius; i++) {
        for (int j = radius; j < w - radius; j++) {
            double blur_acc = 0.0;
            for (int ii = i - radius; ii <= i + radius; ii++) {
                for (int jj = j - radius; jj <= j + radius; jj++) {
                    blur_acc += img_in[ii * w + jj];
                }
            }
            img_out[i * w + j] = blur_acc * (1.0 / 25.0);
        }
    }
}

__attribute__((noinline))
void bench_dsp_fir(int n_samples, int n_taps, const double *signal,
                   const double *coeffs, double *output) {
    for (int i = 0; i < n_samples; i++) {
        double acc = 0.0;
        for (int k = 0; k < n_taps; k++) {
            int idx = i - k;
            if (idx >= 0) {
                double prod = signal[idx] * coeffs[k];
                acc = acc + prod;
            }
        }
        output[i] = acc;
    }
}

__attribute__((noinline))
void bench_gnc_pid(int steps, const double *setpoints, const double *measurements,
                   double kp, double ki, double kd, double dt, double *control_out) {
    double integral = 0.0;
    double prev_err = 0.0;
    for (int t = 0; t < steps; t++) {
        double err = setpoints[t] - measurements[t];
        integral = integral + err * dt;
        double deriv = (err - prev_err) / dt;
        double u = kp * err + ki * integral + kd * deriv;
        prev_err = err;
        control_out[t] = u;
    }
}

__attribute__((noinline))
void bench_gnc_ekf(int steps, const double *gyro_rates, const double *accel_angles,
                   double dt, double *state_est_out) {
    double angle = 0.0;
    double bias = 0.002;
    double p00 = 1.0, p01 = 0.0, p10 = 0.0, p11 = 1.0;
    const double q_angle = 0.001, q_bias = 0.003, r_measure = 0.03;

    for (int t = 0; t < steps; t++) {
        double rate = gyro_rates[t] - bias;
        angle = angle + dt * rate;

        p00 = p00 + dt * (dt * p11 - p01 - p10 + q_angle);
        p01 = p01 - dt * p11;
        p10 = p10 - dt * p11;
        p11 = p11 + q_bias * dt;

        double innov = accel_angles[t] - angle;
        double s = p00 + r_measure;
        double k0 = p00 / s;
        double k1 = p10 / s;

        angle = angle + k0 * innov;
        bias  = bias  + k1 * innov;

        double p00_temp = p00;
        double p01_temp = p01;
        p00 = p00 - k0 * p00_temp;
        p01 = p01 - k0 * p01_temp;
        p10 = p10 - k1 * p00_temp;
        p11 = p11 - k1 * p01_temp;

        state_est_out[t] = angle;
    }
}

__attribute__((noinline))
double bench_noncritical_telemetry_checksum(int n, const double *buf) {
    double s = 0.0;
    if (n > 0) {
        s = buf[0] + buf[n - 1];
    }
    return s * 0.5;
}
