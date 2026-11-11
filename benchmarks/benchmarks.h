#ifndef ATFT_BENCHMARKS_H
#define ATFT_BENCHMARKS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// 1. 数值计算: dgemm (双精度矩阵乘 C = A * B) 与 dgemv (双精度矩阵向量乘 y = A * x)
void bench_dgemm_kernel(int n, const double *A, const double *B, double *C);
void bench_dgemv_kernel(int n, const double *A, const double *x, double *y);

// 2. 数字信号处理: FFT (蝶形运算+旋转因子计算) 与 FIR 有限冲激响应滤波
void bench_fft_radix2_kernel(int n, const double *in_re, const double *in_im,
                             double *out_re, double *out_im);
void bench_fir_filter_kernel(int n_samples, int n_taps, const double *signal,
                             const double *coeffs, double *output);

// 3. 图像处理: Sobel 边缘检测 与 Gaussian 5x5 高斯平滑滤波
void bench_sobel_kernel(int h, int w, const double *img_in, double *img_out);
void bench_gaussian_blur_kernel(int h, int w, const double *img_in, double *img_out);

// 4. 控制算法: PID 姿态控制器 与 EKF 扩展卡尔曼滤波状态更新
void bench_pid_controller_kernel(int steps, const double *setpoints, const double *measurements,
                                 double kp, double ki, double kd, double dt, double *control_out);
void bench_ekf_attitude_kernel(int steps, const double *gyro_rates, const double *accel_angles,
                               double dt, double *state_est_out);

// 辅助函数: 非关键遥测日志统计函数 (用于测试编译期关键性分级: NON_CRITICAL)
double bench_noncritical_telemetry_checksum(int n, const double *buf);

#ifdef __cplusplus
}
#endif

#endif // ATFT_BENCHMARKS_H
