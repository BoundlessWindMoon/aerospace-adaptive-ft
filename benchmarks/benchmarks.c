#include "benchmarks.h"
#include <math.h>

// ============================================================================
// Suite A: PolyBench/C 4.2.1 Kernels (http://polybench.sourceforge.net)
// ============================================================================

/* 1. PolyBench/C: linear-algebra/blas/gemm (C := alpha*A*B + beta*C) */
__attribute__((noinline))
void bench_pb_gemm(int ni, int nj, int nk, double alpha, double beta,
                   const double *A, const double *B, double *C) {
    for (int i = 0; i < ni; i++) {
        for (int j = 0; j < nj; j++) {
            double c_scaled = C[i * nj + j] * beta;
            double acc = c_scaled;
            for (int k = 0; k < nk; k++) {
                double prod = alpha * A[i * nk + k] * B[k * nj + j];
                acc = acc + prod;
            }
            C[i * nj + j] = acc;
        }
    }
}

/* 2. PolyBench/C: linear-algebra/kernels/2mm (D := alpha*A*B*C + beta*D) */
__attribute__((noinline))
void bench_pb_2mm(int ni, int nj, int nk, int nl, double alpha, double beta,
                  double *tmp, const double *A, const double *B,
                  const double *C, double *D) {
    for (int i = 0; i < ni; i++) {
        for (int j = 0; j < nj; j++) {
            double s = 0.0;
            for (int k = 0; k < nk; ++k) {
                double p = alpha * A[i * nk + k] * B[k * nj + j];
                s = s + p;
            }
            tmp[i * nj + j] = s;
        }
    }
    for (int i = 0; i < ni; i++) {
        for (int j = 0; j < nl; j++) {
            double d_val = D[i * nl + j] * beta;
            for (int k = 0; k < nj; ++k) {
                double p = tmp[i * nj + k] * C[k * nl + j];
                d_val = d_val + p;
            }
            D[i * nl + j] = d_val;
        }
    }
}

/* 3. PolyBench/C: linear-algebra/kernels/atax (y := A^T * (A * x)) */
__attribute__((noinline))
void bench_pb_atax(int m, int n, const double *A, const double *x,
                   double *y, double *tmp) {
    for (int i = 0; i < n; i++) {
        y[i] = 0.0;
    }
    for (int i = 0; i < m; i++) {
        double t = 0.0;
        for (int j = 0; j < n; j++) {
            t = t + A[i * n + j] * x[j];
        }
        tmp[i] = t;
        for (int j = 0; j < n; j++) {
            y[j] = y[j] + A[i * n + j] * t;
        }
    }
}

/* 4. PolyBench/C: linear-algebra/kernels/bicg (BiCGStab subkernel) */
__attribute__((noinline))
void bench_pb_bicg(int m, int n, const double *A, double *s, double *q,
                   const double *p, const double *r) {
    for (int i = 0; i < m; i++) {
        s[i] = 0.0;
    }
    for (int i = 0; i < n; i++) {
        double qi = 0.0;
        double ri = r[i];
        for (int j = 0; j < m; j++) {
            double a_ij = A[i * m + j];
            s[j] = s[j] + ri * a_ij;
            qi = qi + a_ij * p[j];
        }
        q[i] = qi;
    }
}

/* 5. PolyBench/C: stencils/jacobi-2d (2D 5-point stencil PDE solver) */
__attribute__((noinline))
void bench_pb_jacobi2d(int tsteps, int n, double *A, double *B) {
    for (int t = 0; t < tsteps; t++) {
        for (int i = 1; i < n - 1; i++) {
            for (int j = 1; j < n - 1; j++) {
                double sum_a = A[i * n + j] + A[i * n + (j - 1)] + A[i * n + (j + 1)]
                             + A[(i + 1) * n + j] + A[(i - 1) * n + j];
                B[i * n + j] = 0.2 * sum_a;
            }
        }
        for (int i = 1; i < n - 1; i++) {
            for (int j = 1; j < n - 1; j++) {
                double sum_b = B[i * n + j] + B[i * n + (j - 1)] + B[i * n + (j + 1)]
                             + B[(i + 1) * n + j] + B[(i - 1) * n + j];
                A[i * n + j] = 0.2 * sum_b;
            }
        }
    }
}

// ============================================================================
// Suite B: PARSEC & Approximate Computing Benchmarks
// ============================================================================

/* 6. PARSEC 3.0: Black-Scholes Analytical PDE Pricing (third_party/llvm-loop-perforation/benchmarks/blackscholes) */
__attribute__((noinline))
void bench_parsec_blackscholes(int n_options, const double *sptprice,
                               const double *strike, const double *rate,
                               const double *volatility, const double *otime,
                               double *prices_out) {
    const double inv_sqrt_2xPI = 0.39894228040143270286;
    for (int i = 0; i < n_options; i++) {
        double sp = sptprice[i];
        double st = strike[i];
        double r  = rate[i];
        double v  = volatility[i];
        double tm = otime[i];

        double xSqrtTime = sqrt(tm);
        double logTerm = (sp - st) / st - 0.5 * ((sp - st) / st) * ((sp - st) / st);
        double xD1 = (r + 0.5 * v * v) * tm + logTerm;
        double xDen = v * xSqrtTime;
        double d1 = xD1 / xDen;
        double d2 = d1 - xDen;

        double abs_d1 = (d1 < 0.0) ? -d1 : d1;
        double exp_d1 = exp(-0.5 * abs_d1 * abs_d1) * inv_sqrt_2xPI;
        double k1 = 1.0 / (1.0 + 0.2316419 * abs_d1);
        double poly1 = k1 * (0.319381530 + k1 * (-0.356563782 + k1 * (1.781477937 + k1 * (-1.821255978 + k1 * 1.330274429))));
        double cndf_d1 = 1.0 - poly1 * exp_d1;
        if (d1 < 0.0) cndf_d1 = 1.0 - cndf_d1;

        double abs_d2 = (d2 < 0.0) ? -d2 : d2;
        double exp_d2 = exp(-0.5 * abs_d2 * abs_d2) * inv_sqrt_2xPI;
        double k2 = 1.0 / (1.0 + 0.2316419 * abs_d2);
        double poly2 = k2 * (0.319381530 + k2 * (-0.356563782 + k2 * (1.781477937 + k2 * (-1.821255978 + k2 * 1.330274429))));
        double cndf_d2 = 1.0 - poly2 * exp_d2;
        if (d2 < 0.0) cndf_d2 = 1.0 - cndf_d2;

        double futureVal = st * exp(-r * tm);
        prices_out[i] = (sp * cndf_d1) - (futureVal * cndf_d2);
    }
}

/* 7. ACCEPT / AxC Benchmark: Sobel Edge Gradient Detection */
__attribute__((noinline))
void bench_axc_sobel(int h, int w, const double *img_in, double *img_out) {
    for (int r = 1; r < h - 1; r++) {
        for (int c = 1; c < w - 1; c++) {
            double gx = -img_in[(r - 1) * w + (c - 1)] + img_in[(r - 1) * w + (c + 1)]
                        - 2.0 * img_in[r * w + (c - 1)] + 2.0 * img_in[r * w + (c + 1)]
                        - img_in[(r + 1) * w + (c - 1)] + img_in[(r + 1) * w + (c + 1)];
            double gy = -img_in[(r - 1) * w + (c - 1)] - 2.0 * img_in[(r - 1) * w + c] - img_in[(r - 1) * w + (c + 1)]
                        + img_in[(r + 1) * w + (c - 1)] + 2.0 * img_in[(r + 1) * w + c] + img_in[(r + 1) * w + (c + 1)];
            double mag_sq = gx * gx + gy * gy;
            img_out[r * w + c] = sqrt(mag_sq);
        }
    }
}

/* 8. ACCEPT / AxC Benchmark: 5x5 Gaussian Convolution Blur */
__attribute__((noinline))
void bench_axc_gaussian(int h, int w, const double *img_in, double *img_out) {
    static const double k5x5[5][5] = {
        {1.0/273.0,  4.0/273.0,  7.0/273.0,  4.0/273.0, 1.0/273.0},
        {4.0/273.0, 16.0/273.0, 26.0/273.0, 16.0/273.0, 4.0/273.0},
        {7.0/273.0, 26.0/273.0, 41.0/273.0, 26.0/273.0, 7.0/273.0},
        {4.0/273.0, 16.0/273.0, 26.0/273.0, 16.0/273.0, 4.0/273.0},
        {1.0/273.0,  4.0/273.0,  7.0/273.0,  4.0/273.0, 1.0/273.0}
    };
    for (int r = 2; r < h - 2; r++) {
        for (int c = 2; c < w - 2; c++) {
            double acc = 0.0;
            for (int kr = -2; kr <= 2; kr++) {
                for (int kc = -2; kc <= 2; kc++) {
                    double p = img_in[(r + kr) * w + (c + kc)] * k5x5[kr + 2][kc + 2];
                    acc = acc + p;
                }
            }
            img_out[r * w + c] = acc;
        }
    }
}

// ============================================================================
// Suite C: Digital Signal Processing (KissFFT & FIR Filter)
// ============================================================================

/* 9. KissFFT Butterfly & Twiddle Transform Kernel */
__attribute__((noinline))
void bench_dsp_kissfft(int n, const double *in_re, const double *in_im,
                       double *out_re, double *out_im) {
    const double two_pi = 6.28318530717958647692;
    for (int k = 0; k < n; k++) {
        double sum_re = 0.0;
        double sum_im = 0.0;
        for (int t = 0; t < n; t++) {
            double angle = two_pi * (double)(t * k) / (double)n;
            double w_re = cos(angle);
            double w_im = -sin(angle);
            double tre = in_re[t] * w_re - in_im[t] * w_im;
            double tim = in_re[t] * w_im + in_im[t] * w_re;
            sum_re = sum_re + tre;
            sum_im = sum_im + tim;
        }
        out_re[k] = sum_re;
        out_im[k] = sum_im;
    }
}

/* 10. CMSIS-DSP FIR Filter Kernel */
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

// ============================================================================
// Suite D: Spacecraft GNC Attitude & Flight Control
// ============================================================================

/* 11. Closed-Loop Attitude PID Controller */
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

/* 12. Extended Kalman Filter (EKF) Attitude Estimation */
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
