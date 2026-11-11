#include "atft_runtime.h"
#include "benchmarks.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

// 计算数组与黄金标准之间的相对 L2 误差与最大相对误差
static double compute_rel_l2_error(int n, const double *golden, const double *test, int *has_nan_or_inf) {
    double num = 0.0;
    double den = 0.0;
    *has_nan_or_inf = 0;
    for (int i = 0; i < n; i++) {
        if (!isfinite(test[i])) {
            *has_nan_or_inf = 1;
            return 1e9;
        }
        double d = test[i] - golden[i];
        num += d * d;
        den += golden[i] * golden[i];
    }
    if (den < 1e-20) return sqrt(num);
    return sqrt(num / den);
}

// 初始化可复现的测试输入数据
static void init_test_inputs(int n_mat, double *A, double *B, double *x,
                             int n_fft, double *fft_re, double *fft_im,
                             int n_fir, int n_taps, double *fir_sig, double *fir_coeffs,
                             int img_h, int img_w, double *img_in,
                             int ctrl_steps, double *setpoints, double *meas,
                             double *gyro, double *accel) {
    for (int i = 0; i < n_mat * n_mat; i++) {
        A[i] = 0.5 * sin(0.13 * (double)(i + 1)) + 0.8;
        B[i] = 0.5 * cos(0.07 * (double)(i + 1)) + 0.6;
    }
    for (int i = 0; i < n_mat; i++) {
        x[i] = 0.3 * sin(0.2 * (double)i) + 1.0;
    }
    for (int i = 0; i < n_fft; i++) {
        fft_re[i] = cos(2.0 * 3.141592653589793 * 3.0 * i / n_fft) + 0.25 * sin(2.0 * 3.141592653589793 * 7.0 * i / n_fft);
        fft_im[i] = 0.1 * sin(2.0 * 3.141592653589793 * 3.0 * i / n_fft);
    }
    for (int i = 0; i < n_fir; i++) {
        fir_sig[i] = sin(0.05 * (double)i) + 0.2 * cos(0.31 * (double)i);
    }
    for (int k = 0; k < n_taps; k++) {
        // 归一化的低通窗系数
        double w = 0.54 - 0.46 * cos(2.0 * 3.141592653589793 * k / (n_taps - 1));
        fir_coeffs[k] = w / (double)n_taps;
    }
    for (int r = 0; r < img_h; r++) {
        for (int c = 0; c < img_w; c++) {
            img_in[r * img_w + c] = 128.0 + 60.0 * sin(0.15 * r) * cos(0.15 * c);
        }
    }
    for (int t = 0; t < ctrl_steps; t++) {
        setpoints[t] = 1.0;
        meas[t] = 1.0 - 0.8 * exp(-0.02 * (double)t) * cos(0.08 * (double)t);
        gyro[t] = 0.05 * cos(0.04 * (double)t) + 0.002;
        accel[t] = 1.25 * sin(0.04 * (double)t);
    }
}

// ============================================================================
// 模式 1: 性能、动态指令数与无故障精度基准测试 (--perf <mode_name> <golden_dir>)
// ============================================================================
static int run_perf_mode(const char *mode_name, const char *golden_dir, int dump_golden) {
    const int n_mat = 48;
    const int n_fft = 64;
    const int n_fir = 512, n_taps = 32;
    const int img_h = 48, img_w = 48;
    const int ctrl_steps = 512;

    double *A = calloc(n_mat * n_mat, sizeof(double));
    double *B = calloc(n_mat * n_mat, sizeof(double));
    double *C = calloc(n_mat * n_mat, sizeof(double));
    double *x = calloc(n_mat, sizeof(double));
    double *y = calloc(n_mat, sizeof(double));
    double *fft_re = calloc(n_fft, sizeof(double));
    double *fft_im = calloc(n_fft, sizeof(double));
    double *fft_out_re = calloc(n_fft, sizeof(double));
    double *fft_out_im = calloc(n_fft, sizeof(double));
    double *fir_sig = calloc(n_fir, sizeof(double));
    double *fir_coeffs = calloc(n_taps, sizeof(double));
    double *fir_out = calloc(n_fir, sizeof(double));
    double *img_in = calloc(img_h * img_w, sizeof(double));
    double *sobel_out = calloc(img_h * img_w, sizeof(double));
    double *blur_out = calloc(img_h * img_w, sizeof(double));
    double *setpoints = calloc(ctrl_steps, sizeof(double));
    double *meas = calloc(ctrl_steps, sizeof(double));
    double *pid_out = calloc(ctrl_steps, sizeof(double));
    double *gyro = calloc(ctrl_steps, sizeof(double));
    double *accel = calloc(ctrl_steps, sizeof(double));
    double *ekf_out = calloc(ctrl_steps, sizeof(double));

    init_test_inputs(n_mat, A, B, x, n_fft, fft_re, fft_im,
                     n_fir, n_taps, fir_sig, fir_coeffs,
                     img_h, img_w, img_in,
                     ctrl_steps, setpoints, meas, gyro, accel);

    const char *bench_names[8] = {
        "dgemm", "dgemv", "fft", "fir", "sobel", "gaussian_blur", "pid", "ekf"
    };
    int out_sizes[8] = {
        n_mat * n_mat, n_mat, n_fft, n_fir, img_h * img_w, img_h * img_w, ctrl_steps, ctrl_steps
    };
    double *out_ptrs[8] = {
        C, y, fft_out_re, fir_out, sobel_out, blur_out, pid_out, ekf_out
    };
    int iters_list[8] = {
        200, 2000, 300, 1000, 1000, 400, 2000, 2000
    };

    for (int b = 0; b < 8; b++) {
        atft_init(10000.0, 4.0, 3, 4, 3);
        atft_configure_fi(0, 0.0, -1, 42);
        // 若测试 adaptive_high，则预先设为 HIGH 模式
        if (strcmp(mode_name, "adaptive_high") == 0) {
            g_atft_state.current_mode = ATFT_MODE_HIGH;
        }

        int iters = iters_list[b];
        double t0 = now_sec();
        for (int it = 0; it < iters; it++) {
            switch (b) {
                case 0: bench_dgemm_kernel(n_mat, A, B, C); break;
                case 1: bench_dgemv_kernel(n_mat, A, x, y); break;
                case 2: bench_fft_radix2_kernel(n_fft, fft_re, fft_im, fft_out_re, fft_out_im); break;
                case 3: bench_fir_filter_kernel(n_fir, n_taps, fir_sig, fir_coeffs, fir_out); break;
                case 4: bench_sobel_kernel(img_h, img_w, img_in, sobel_out); break;
                case 5: bench_gaussian_blur_kernel(img_h, img_w, img_in, blur_out); break;
                case 6: bench_pid_controller_kernel(ctrl_steps, setpoints, meas, 1.2, 0.4, 0.05, 0.01, pid_out); break;
                case 7: bench_ekf_attitude_kernel(ctrl_steps, gyro, accel, 0.01, ekf_out); break;
            }
        }
        double t1 = now_sec();
        double avg_us = (t1 - t0) * 1e6 / (double)iters;
        uint64_t dyn_insts = g_atft_state.retired_inst_counter / (uint64_t)iters;

        char gpath[512];
        snprintf(gpath, sizeof(gpath), "%s/golden_%s.bin", golden_dir, bench_names[b]);
        double rel_err = 0.0;
        if (dump_golden) {
            FILE *fp = fopen(gpath, "wb");
            if (fp) {
                fwrite(out_ptrs[b], sizeof(double), out_sizes[b], fp);
                fclose(fp);
            }
        } else {
            double *golden_buf = calloc(out_sizes[b], sizeof(double));
            FILE *fp = fopen(gpath, "rb");
            if (fp) {
                size_t nr = fread(golden_buf, sizeof(double), out_sizes[b], fp);
                (void)nr;
                fclose(fp);
                int has_nan = 0;
                rel_err = compute_rel_l2_error(out_sizes[b], golden_buf, out_ptrs[b], &has_nan);
            }
            free(golden_buf);
        }

        printf("PERF_CSV,%s,%s,%.3f,%lu,%.6e,%lu\n",
               mode_name, bench_names[b], avg_us,
               (unsigned long)dyn_insts, rel_err,
               (unsigned long)(g_atft_state.total_votes / (uint64_t)iters));
    }

    free(A); free(B); free(C); free(x); free(y);
    free(fft_re); free(fft_im); free(fft_out_re); free(fft_out_im);
    free(fir_sig); free(fir_coeffs); free(fir_out);
    free(img_in); free(sobel_out); free(blur_out);
    free(setpoints); free(meas); free(pid_out);
    free(gyro); free(accel); free(ekf_out);
    return 0;
}

// ============================================================================
// 模式 2: 大规模蒙特卡洛单比特翻转故障注入评估 (--fi <mode_name> <golden_dir> <trials>)
// ============================================================================
static int run_fi_mode(const char *mode_name, const char *golden_dir, int trials) {
    const int n_mat = 24;
    const int n_fft = 32;
    const int n_fir = 256, n_taps = 16;
    const int img_h = 24, img_w = 24;
    const int ctrl_steps = 256;

    double *A = calloc(n_mat * n_mat, sizeof(double));
    double *B = calloc(n_mat * n_mat, sizeof(double));
    double *C = calloc(n_mat * n_mat, sizeof(double));
    double *x = calloc(n_mat, sizeof(double));
    double *y = calloc(n_mat, sizeof(double));
    double *fft_re = calloc(n_fft, sizeof(double));
    double *fft_im = calloc(n_fft, sizeof(double));
    double *fft_out_re = calloc(n_fft, sizeof(double));
    double *fft_out_im = calloc(n_fft, sizeof(double));
    double *fir_sig = calloc(n_fir, sizeof(double));
    double *fir_coeffs = calloc(n_taps, sizeof(double));
    double *fir_out = calloc(n_fir, sizeof(double));
    double *img_in = calloc(img_h * img_w, sizeof(double));
    double *sobel_out = calloc(img_h * img_w, sizeof(double));
    double *blur_out = calloc(img_h * img_w, sizeof(double));
    double *setpoints = calloc(ctrl_steps, sizeof(double));
    double *meas = calloc(ctrl_steps, sizeof(double));
    double *pid_out = calloc(ctrl_steps, sizeof(double));
    double *gyro = calloc(ctrl_steps, sizeof(double));
    double *accel = calloc(ctrl_steps, sizeof(double));
    double *ekf_out = calloc(ctrl_steps, sizeof(double));

    init_test_inputs(n_mat, A, B, x, n_fft, fft_re, fft_im,
                     n_fir, n_taps, fir_sig, fir_coeffs,
                     img_h, img_w, img_in,
                     ctrl_steps, setpoints, meas, gyro, accel);

    const char *bench_names[8] = {
        "dgemm", "dgemv", "fft", "fir", "sobel", "gaussian_blur", "pid", "ekf"
    };
    int out_sizes[8] = {
        n_mat * n_mat, n_mat, n_fft, n_fir, img_h * img_w, img_h * img_w, ctrl_steps, ctrl_steps
    };
    double *out_ptrs[8] = {
        C, y, fft_out_re, fir_out, sobel_out, blur_out, pid_out, ekf_out
    };
    // 针对每个算子的算术操作数设定单次运行注入 1~2 个单比特翻转的概率
    double fi_probs[8] = {
        4.0e-5, 8.0e-4, 1.5e-4, 1.2e-4, 1.2e-4, 4.0e-5, 5.0e-4, 1.8e-4
    };

    for (int b = 0; b < 8; b++) {
        // 先跑一次无故障获取 Golden 参考输出
        double *golden_buf = calloc(out_sizes[b], sizeof(double));
        atft_init(10000.0, 4.0, 3, 4, 3);
        atft_configure_fi(0, 0.0, -1, 1);
        switch (b) {
            case 0: bench_dgemm_kernel(n_mat, A, B, golden_buf); break;
            case 1: bench_dgemv_kernel(n_mat, A, x, golden_buf); break;
            case 2: bench_fft_radix2_kernel(n_fft, fft_re, fft_im, golden_buf, fft_out_im); break;
            case 3: bench_fir_filter_kernel(n_fir, n_taps, fir_sig, fir_coeffs, golden_buf); break;
            case 4: bench_sobel_kernel(img_h, img_w, img_in, golden_buf); break;
            case 5: bench_gaussian_blur_kernel(img_h, img_w, img_in, golden_buf); break;
            case 6: bench_pid_controller_kernel(ctrl_steps, setpoints, meas, 1.2, 0.4, 0.05, 0.01, golden_buf); break;
            case 7: bench_ekf_attitude_kernel(ctrl_steps, gyro, accel, 0.01, golden_buf); break;
        }

        int effective_runs = 0;
        int detected_runs = 0;
        int recovered_runs = 0;
        int sdc_runs = 0;
        double sum_rel_err = 0.0;

        for (int t = 0; t < trials; t++) {
            atft_init(10000.0, 4.0, 3, 4, 3);
            if (strcmp(mode_name, "adaptive") == 0) {
                g_atft_state.current_mode = ATFT_MODE_HIGH;
            }
            // 随机在主精确路径 R(0) 或近似路径 A1(1)/A2(2) 注入单比特翻转
            int target_br = (strcmp(mode_name, "native_fi") == 0) ? 0 : (t % 3);
            atft_configure_fi(1, fi_probs[b], target_br, (uint32_t)(10007 * (b + 1) + t * 31 + 1));

            memset(out_ptrs[b], 0, out_sizes[b] * sizeof(double));
            switch (b) {
                case 0: bench_dgemm_kernel(n_mat, A, B, C); break;
                case 1: bench_dgemv_kernel(n_mat, A, x, y); break;
                case 2: bench_fft_radix2_kernel(n_fft, fft_re, fft_im, fft_out_re, fft_out_im); break;
                case 3: bench_fir_filter_kernel(n_fir, n_taps, fir_sig, fir_coeffs, fir_out); break;
                case 4: bench_sobel_kernel(img_h, img_w, img_in, sobel_out); break;
                case 5: bench_gaussian_blur_kernel(img_h, img_w, img_in, blur_out); break;
                case 6: bench_pid_controller_kernel(ctrl_steps, setpoints, meas, 1.2, 0.4, 0.05, 0.01, pid_out); break;
                case 7: bench_ekf_attitude_kernel(ctrl_steps, gyro, accel, 0.01, ekf_out); break;
            }

            if (g_atft_state.fi_injected_count == 0) continue;
            effective_runs++;

            int has_nan = 0;
            double rel_err = compute_rel_l2_error(out_sizes[b], golden_buf, out_ptrs[b], &has_nan);

            if (g_atft_state.detected_faults > 0) {
                detected_runs++;
            }
            // 输出相对误差 <= 1% (0.01) 判定为成功屏蔽/恢复软错误
            if (!has_nan && rel_err <= 0.01) {
                recovered_runs++;
                sum_rel_err += rel_err;
            } else {
                sdc_runs++;
            }
        }

        double det_rate = effective_runs > 0 ? (100.0 * detected_runs / effective_runs) : 0.0;
        double rec_rate = effective_runs > 0 ? (100.0 * recovered_runs / effective_runs) : 0.0;
        double sdc_rate = effective_runs > 0 ? (100.0 * sdc_runs / effective_runs) : 0.0;
        double avg_err  = recovered_runs > 0 ? (sum_rel_err / recovered_runs) : 0.0;

        printf("FI_CSV,%s,%s,%d,%d,%.2f,%.2f,%.2f,%.6e\n",
               mode_name, bench_names[b], trials, effective_runs,
               det_rate, rec_rate, sdc_rate, avg_err);

        free(golden_buf);
    }

    free(A); free(B); free(C); free(x); free(y);
    free(fft_re); free(fft_im); free(fft_out_re); free(fft_out_im);
    free(fir_sig); free(fir_coeffs); free(fir_out);
    free(img_in); free(sobel_out); free(blur_out);
    free(setpoints); free(meas); free(pid_out);
    free(gyro); free(accel); free(ekf_out);
    return 0;
}

// ============================================================================
// 模式 3: 在轨太阳耀斑辐射风暴动态自适应切换实验 (--trace)
// 模拟 60 个时间窗口:
//   Window 0..19:  辐射平静期 (低故障率) -> 维持 LOW 模式
//   Window 20..39: 太阳耀斑辐射爆发期 (ECC 间隔急剧缩短且窗口累计超阈值) -> 自动切换 HIGH 模式
//   Window 40..59: 辐射消退恢复平静期 -> 连续 M=3 个平静窗口后滞回切回 LOW 模式
// ============================================================================
static int run_adaptive_trace_mode(void) {
    const int n_mat = 32;
    const int n_fft = 48;
    const int ctrl_steps = 256;

    double *A = calloc(n_mat * n_mat, sizeof(double));
    double *B = calloc(n_mat * n_mat, sizeof(double));
    double *C = calloc(n_mat * n_mat, sizeof(double));
    double *x = calloc(n_mat, sizeof(double));
    double *y = calloc(n_mat, sizeof(double));
    double *fft_re = calloc(n_fft, sizeof(double));
    double *fft_im = calloc(n_fft, sizeof(double));
    double *fft_out_re = calloc(n_fft, sizeof(double));
    double *fft_out_im = calloc(n_fft, sizeof(double));
    double *fir_sig = calloc(256, sizeof(double));
    double *fir_coeffs = calloc(16, sizeof(double));
    double *img_in = calloc(32 * 32, sizeof(double));
    double *sobel_out = calloc(32 * 32, sizeof(double));
    double *setpoints = calloc(ctrl_steps, sizeof(double));
    double *meas = calloc(ctrl_steps, sizeof(double));
    double *pid_out = calloc(ctrl_steps, sizeof(double));
    double *gyro = calloc(ctrl_steps, sizeof(double));
    double *accel = calloc(ctrl_steps, sizeof(double));
    double *ekf_out = calloc(ctrl_steps, sizeof(double));

    init_test_inputs(n_mat, A, B, x, n_fft, fft_re, fft_im,
                     256, 16, fir_sig, fir_coeffs,
                     32, 32, img_in,
                     ctrl_steps, setpoints, meas, gyro, accel);

    // 初始化运行时监控器: 平静期平均间隔=12000000条指令, theta=4.0 (短间隔阈值=3000000), N=3, 窗口阈值=4, 滞回窗口M=3
    atft_init(12000000.0, 4.0, 3, 4, 3);

    printf("window,env_phase,injected_ecc_events,consec_short_intervals,mode_before,mode_after,exec_time_us,dyn_insts\n");

    for (int w = 0; w < 60; w++) {
        const char *phase = "CALM_PRE";
        int ecc_events_in_win = 0;
        if (w >= 20 && w < 40) {
            phase = "SOLAR_FLARE";
            ecc_events_in_win = 5 + (w % 3); // 高能粒子通量激增
        } else if (w >= 40) {
            phase = "CALM_POST";
            ecc_events_in_win = (w % 7 == 0) ? 1 : 0;
        } else {
            ecc_events_in_win = (w % 6 == 0) ? 1 : 0;
        }

        int mode_before = (int)atft_get_mode();
        uint64_t inst_before = g_atft_state.retired_inst_counter;
        double t0 = now_sec();

        // 每个任务周期执行: 1 个关键任务 (dgemm + ekf) + 3 个次关键任务 (dgemv + fft + sobel + pid)
        for (int sub = 0; sub < 5; sub++) {
            bench_dgemm_kernel(n_mat, A, B, C);
            if (ecc_events_in_win >= 5) atft_report_ecc_event();
            bench_dgemv_kernel(n_mat, A, x, y);
            if (ecc_events_in_win >= 5) atft_report_ecc_event();
            bench_fft_radix2_kernel(n_fft, fft_re, fft_im, fft_out_re, fft_out_im);
            if (ecc_events_in_win >= 5) atft_report_ecc_event();
            bench_sobel_kernel(32, 32, img_in, sobel_out);
            if (ecc_events_in_win >= 5) atft_report_ecc_event();
            bench_pid_controller_kernel(ctrl_steps, setpoints, meas, 1.2, 0.4, 0.05, 0.01, pid_out);
            bench_ekf_attitude_kernel(ctrl_steps, gyro, accel, 0.01, ekf_out);
            bench_noncritical_telemetry_checksum(ctrl_steps, pid_out);

            // 平静期偶发单粒子事件
            if (ecc_events_in_win == 1 && sub == 2) {
                atft_report_ecc_event();
            }
        }

        double t1 = now_sec();
        uint64_t win_insts = g_atft_state.retired_inst_counter - inst_before;
        uint32_t short_cnt = g_atft_state.consec_short_intervals;

        // 窗口结束推进状态机 (检查是否需要从 HIGH 降级回 LOW)
        atft_step_window();
        int mode_after = (int)atft_get_mode();

        printf("%d,%s,%d,%u,%d,%d,%.2f,%lu\n",
               w, phase, ecc_events_in_win, short_cnt,
               mode_before, mode_after, (t1 - t0) * 1e6,
               (unsigned long)win_insts);
    }

    free(A); free(B); free(C); free(x); free(y);
    free(fft_re); free(fft_im); free(fft_out_re); free(fft_out_im);
    free(fir_sig); free(fir_coeffs);
    free(img_in); free(sobel_out);
    free(setpoints); free(meas); free(pid_out);
    free(gyro); free(accel); free(ekf_out);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage:\n"
                        "  %s --dump-golden <golden_dir>\n"
                        "  %s --perf <mode_name> <golden_dir>\n"
                        "  %s --fi <mode_name> <golden_dir> <trials>\n"
                        "  %s --trace\n", argv[0], argv[0], argv[0], argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "--dump-golden") == 0 && argc >= 3) {
        return run_perf_mode("native", argv[2], 1);
    } else if (strcmp(argv[1], "--perf") == 0 && argc >= 4) {
        return run_perf_mode(argv[2], argv[3], 0);
    } else if (strcmp(argv[1], "--fi") == 0 && argc >= 5) {
        return run_fi_mode(argv[2], argv[3], atoi(argv[4]));
    } else if (strcmp(argv[1], "--trace") == 0) {
        return run_adaptive_trace_mode();
    }
    return 1;
}
