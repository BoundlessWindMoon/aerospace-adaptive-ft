#include "benchmarks.h"
#include <math.h>

// ============================================================================
// 类别 1: 数值计算 (BLIS 代表性内核 dgemm 与 dgemv, 三重/双重嵌套循环)
// ============================================================================
__attribute__((noinline))
void bench_dgemm_kernel(int n, const double *A, const double *B, double *C) {
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            double sum = 0.0;
            for (int k = 0; k < n; k++) {
                double prod = A[i * n + k] * B[k * n + j];
                sum = sum + prod;
            }
            C[i * n + j] = sum;
        }
    }
}

__attribute__((noinline))
void bench_dgemv_kernel(int n, const double *A, const double *x, double *y) {
    for (int i = 0; i < n; i++) {
        double acc = 0.0;
        for (int j = 0; j < n; j++) {
            double term = A[i * n + j] * x[j];
            acc = acc + term;
        }
        y[i] = acc;
    }
}

// ============================================================================
// 类别 2: 数字信号处理 (DFT/FFT 旋转因子核 与 CMSIS-DSP 风格 FIR 滤波器)
// ============================================================================
__attribute__((noinline))
void bench_fft_radix2_kernel(int n, const double *in_re, const double *in_im,
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

__attribute__((noinline))
void bench_fir_filter_kernel(int n_samples, int n_taps, const double *signal,
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
// 类别 3: 图像处理 (Sobel 边缘梯度检测 与 5x5 高斯平滑滤波, 参考 llvm-loop-perforation)
// ============================================================================
__attribute__((noinline))
void bench_sobel_kernel(int h, int w, const double *img_in, double *img_out) {
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

__attribute__((noinline))
void bench_gaussian_blur_kernel(int h, int w, const double *img_in, double *img_out) {
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
// 类别 4: 控制算法 (卫星姿态 PID 控制器 与 EKF 扩展卡尔曼滤波器, 强状态递归依赖)
// ============================================================================
__attribute__((noinline))
void bench_pid_controller_kernel(int steps, const double *setpoints, const double *measurements,
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
void bench_ekf_attitude_kernel(int steps, const double *gyro_rates, const double *accel_angles,
                               double dt, double *state_est_out) {
    double angle = 0.0;
    double bias = 0.002;
    double p00 = 1.0, p01 = 0.0, p10 = 0.0, p11 = 1.0;
    const double q_angle = 0.001, q_bias = 0.003, r_measure = 0.03;

    for (int t = 0; t < steps; t++) {
        // 预测步 (State Prediction)
        double rate = gyro_rates[t] - bias;
        angle = angle + dt * rate;

        p00 = p00 + dt * (dt * p11 - p01 - p10 + q_angle);
        p01 = p01 - dt * p11;
        p10 = p10 - dt * p11;
        p11 = p11 + q_bias * dt;

        // 更新步 (Kalman Gain & Measurement Update)
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

// ============================================================================
// 辅助非关键代码段: 简单线性无状态遥测校验和
// ============================================================================
__attribute__((noinline))
double bench_noncritical_telemetry_checksum(int n, const double *buf) {
    double s = 0.0;
    if (n > 0) {
        s = buf[0] + buf[n - 1];
    }
    return s * 0.5;
}
