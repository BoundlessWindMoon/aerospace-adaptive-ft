#include "atft_runtime.h"
#include "benchmarks.h"
#include "harness.h"

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

static uint64_t splitmix64(uint64_t *state) {
    uint64_t z = (*state += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

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
        fft_re[i] = cos(2.0 * 3.141592653589793 * 3.0 * i / n_fft) +
                    0.25 * sin(2.0 * 3.141592653589793 * 7.0 * i / n_fft);
        fft_im[i] = 0.1 * sin(2.0 * 3.141592653589793 * 3.0 * i / n_fft);
    }
    for (int i = 0; i < n_fir; i++) {
        fir_sig[i] = sin(0.05 * (double)i) + 0.2 * cos(0.31 * (double)i);
    }
    for (int k = 0; k < n_taps; k++) {
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
// 模式 1: 硬件 PMU 计数器性能与输出质量 (QoR) 测量
// 直接通过 Linux perf_event_open 读取 CPU 硬件退休指令数与 CPU 周期数
// ============================================================================
static int run_pmu_perf_and_qor(const char *policy_name, const char *golden_dir, int dump_golden) {
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
    double peak_vals[8] = {
        35.0, 40.0, 32.0, 1.5, 255.0, 255.0, 2.0, 1.5
    };
    int iters_list[8] = {
        200, 2000, 300, 1000, 1000, 400, 2000, 2000
    };

    harness_pmu_t pmu;
    harness_pmu_init(&pmu);

    for (int b = 0; b < 8; b++) {
        atft_init(10000.0, 4.0, 3, 4, 3);
        if (strcmp(policy_name, "adaptive_high") == 0) {
            g_atft_state.current_mode = ATFT_MODE_HIGH;
        }

        // 预热一次
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

        atft_reset_stats();
        if (strcmp(policy_name, "adaptive_high") == 0) {
            g_atft_state.current_mode = ATFT_MODE_HIGH;
        }

        int iters = iters_list[b];
        uint64_t total_hw_insts = 0, total_hw_cycles = 0;
        double t0 = now_sec();
        harness_pmu_start(&pmu);
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
        harness_pmu_stop(&pmu, &total_hw_insts, &total_hw_cycles);
        double t1 = now_sec();

        double avg_us = (t1 - t0) * 1e6 / (double)iters;
        uint64_t hw_insts = total_hw_insts / (uint64_t)iters;
        uint64_t hw_cycles = total_hw_cycles / (uint64_t)iters;

        char gpath[512];
        snprintf(gpath, sizeof(gpath), "%s/golden_%s.bin", golden_dir, bench_names[b]);
        harness_qor_metrics_t qor;
        memset(&qor, 0, sizeof(qor));
        qor.psnr_db = 120.0;

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
                harness_compute_qor(out_sizes[b], golden_buf, out_ptrs[b], peak_vals[b], &qor);
            }
            free(golden_buf);
        }

        uint64_t votes_per_iter = g_atft_state.total_votes / (uint64_t)iters;
        uint64_t false_alarms_per_iter = g_atft_state.detected_faults / (uint64_t)iters;

        printf("PMU_CSV,%s,%s,%.3f,%lu,%lu,%.6e,%.6e,%.6e,%.2f,%lu,%lu\n",
               policy_name, bench_names[b], avg_us,
               (unsigned long)hw_insts, (unsigned long)hw_cycles,
               qor.rel_l2_err, qor.mean_rel_err, qor.max_abs_err, qor.psnr_db,
               (unsigned long)votes_per_iter, (unsigned long)false_alarms_per_iter);
    }

    harness_pmu_close(&pmu);
    free(A); free(B); free(C); free(x); free(y);
    free(fft_re); free(fft_im); free(fft_out_re); free(fft_out_im);
    free(fir_sig); free(fir_coeffs); free(fir_out);
    free(img_in); free(sobel_out); free(blur_out);
    free(setpoints); free(meas); free(pid_out);
    free(gyro); free(accel); free(ekf_out);
    return 0;
}

// ============================================================================
// 模式 2: 标准两阶段 One-Fault-Per-Run 故障注入实验 (LLFI / SASSIFI / SC'19 标准)
// Phase 1: 跑一次无故障执行，获取真实总动态目标指令数 N_dyn
// Phase 2: 跑 M 次独立实验，每次均匀随机抽 k ∈ [1, N_dyn] 与 bit ∈ [0, 63]，
//          按五分类统计结果：
//          - Masked (完全掩盖: rel_l2 <= 1e-12 且 detected == 0)
//          - Detected_Recovered (投票器检出并恢复: detected > 0 且 rel_l2 <= 0.01)
//          - Benign_Approx (未超阈值但误差落在 (1e-12, 0.01] 可接受界内)
//          - SDC (静默数据损坏: rel_l2 > 0.01 或 NaN/Inf，且 unrecoverable == 0)
//          - DUE (检出但三路不一致无法恢复: unrecoverable > 0)
// ============================================================================
static int run_standard_one_fault_fi(const char *policy_name, const char *golden_dir, int trials) {
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
    double peak_vals[8] = {
        20.0, 20.0, 16.0, 1.5, 255.0, 255.0, 2.0, 1.5
    };

    for (int b = 0; b < 8; b++) {
        // Phase 1: Profiling 阶段 (无故障运行一次，统计总动态目标指令数 N_dyn 并获取基准输出)
        double *golden_buf = calloc(out_sizes[b], sizeof(double));
        atft_init(10000.0, 4.0, 3, 4, 3);
        if (strstr(policy_name, "adaptive") != NULL) {
            g_atft_state.current_mode = ATFT_MODE_HIGH;
        }
        harness_fi_reset_profile();

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

        uint64_t total_dyn_insts = harness_fi_get_total_dyn_insts();
        uint64_t baseline_false_alarms = g_atft_state.detected_faults;

        // Phase 2: One-Fault-Per-Run 均匀采样注入阶段
        int cnt_masked = 0;
        int cnt_det_rec = 0;
        int cnt_benign = 0;
        int cnt_sdc = 0;
        int cnt_due = 0;
        double sum_valid_rel_err = 0.0;
        int valid_runs = 0;

        uint64_t rng = 0x123456789abcdef0ULL ^ ((uint64_t)(b + 1) << 32);

        for (int t = 0; t < trials; t++) {
            uint64_t target_step = 1 + (splitmix64(&rng) % (total_dyn_insts > 0 ? total_dyn_insts : 1));
            uint32_t target_bit  = (uint32_t)(splitmix64(&rng) % 64);

            atft_init(10000.0, 4.0, 3, 4, 3);
            if (strstr(policy_name, "adaptive") != NULL) {
                g_atft_state.current_mode = ATFT_MODE_HIGH;
            }
            harness_fi_arm_one_fault(target_step, target_bit);

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

            harness_qor_metrics_t qor;
            harness_compute_qor(out_sizes[b], golden_buf, out_ptrs[b], peak_vals[b], &qor);

            int fault_detected = (g_atft_state.detected_faults > baseline_false_alarms);
            int unrec = (g_atft_state.unrecoverable_faults > 0);

            if (unrec) {
                cnt_due++;
            } else if (qor.has_nan_or_inf || qor.rel_l2_err > 0.01) {
                cnt_sdc++;
            } else {
                valid_runs++;
                sum_valid_rel_err += qor.rel_l2_err;
                if (fault_detected) {
                    cnt_det_rec++;
                } else if (qor.rel_l2_err <= 1e-12) {
                    cnt_masked++;
                } else {
                    cnt_benign++;
                }
            }
        }

        double p_masked  = 100.0 * cnt_masked / (double)trials;
        double p_det_rec = 100.0 * cnt_det_rec / (double)trials;
        double p_benign  = 100.0 * cnt_benign / (double)trials;
        double p_sdc     = 100.0 * cnt_sdc / (double)trials;
        double p_due     = 100.0 * cnt_due / (double)trials;
        double avg_err   = valid_runs > 0 ? (sum_valid_rel_err / valid_runs) : 0.0;

        printf("FI5_CSV,%s,%s,%lu,%d,%.2f,%.2f,%.2f,%.2f,%.2f,%.6e\n",
               policy_name, bench_names[b], (unsigned long)total_dyn_insts, trials,
               p_masked, p_det_rec, p_benign, p_sdc, p_due, avg_err);

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
// 模式 3: 在轨太阳耀斑辐射风暴动态自适应切换轨迹 (使用硬件 PMU 测量每窗口真实指令数)
// ============================================================================
static int run_adaptive_trace_pmu(void) {
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

    harness_pmu_t pmu;
    harness_pmu_init(&pmu);
    atft_init(12000000.0, 4.0, 3, 4, 3);

    printf("window,env_phase,injected_ecc_events,consec_short_intervals,mode_before,mode_after,exec_time_us,hw_insts,hw_cycles\n");

    for (int w = 0; w < 60; w++) {
        const char *phase = "CALM_PRE";
        int ecc_events_in_win = 0;
        if (w >= 20 && w < 40) {
            phase = "SOLAR_FLARE";
            ecc_events_in_win = 5 + (w % 3);
        } else if (w >= 40) {
            phase = "CALM_POST";
            ecc_events_in_win = (w % 7 == 0) ? 1 : 0;
        } else {
            ecc_events_in_win = (w % 6 == 0) ? 1 : 0;
        }

        int mode_before = (int)atft_get_mode();
        uint64_t hw_insts = 0, hw_cycles = 0;
        double t0 = now_sec();
        harness_pmu_start(&pmu);

        for (int sub = 0; sub < 5; sub++) {
            bench_dgemm_kernel(n_mat, A, B, C);
            atft_advance_insts(600000);
            if (ecc_events_in_win >= 5) atft_report_ecc_event();

            bench_dgemv_kernel(n_mat, A, x, y);
            atft_advance_insts(100000);
            if (ecc_events_in_win >= 5) atft_report_ecc_event();

            bench_fft_radix2_kernel(n_fft, fft_re, fft_im, fft_out_re, fft_out_im);
            atft_advance_insts(300000);
            if (ecc_events_in_win >= 5) atft_report_ecc_event();

            bench_sobel_kernel(32, 32, img_in, sobel_out);
            atft_advance_insts(200000);
            if (ecc_events_in_win >= 5) atft_report_ecc_event();

            bench_pid_controller_kernel(ctrl_steps, setpoints, meas, 1.2, 0.4, 0.05, 0.01, pid_out);
            bench_ekf_attitude_kernel(ctrl_steps, gyro, accel, 0.01, ekf_out);
            bench_noncritical_telemetry_checksum(ctrl_steps, pid_out);
            atft_advance_insts(200000);

            if (ecc_events_in_win == 1 && sub == 2) {
                atft_report_ecc_event();
            }
        }

        harness_pmu_stop(&pmu, &hw_insts, &hw_cycles);
        double t1 = now_sec();
        uint32_t short_cnt = g_atft_state.consec_short_intervals;

        atft_step_window();
        int mode_after = (int)atft_get_mode();

        printf("%d,%s,%d,%u,%d,%d,%.2f,%lu,%lu\n",
               w, phase, ecc_events_in_win, short_cnt,
               mode_before, mode_after, (t1 - t0) * 1e6,
               (unsigned long)hw_insts, (unsigned long)hw_cycles);
    }

    harness_pmu_close(&pmu);
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
                        "  %s --pmu-perf <policy_name> <golden_dir>\n"
                        "  %s --fi-one <policy_name> <golden_dir> <trials>\n"
                        "  %s --trace-pmu\n", argv[0], argv[0], argv[0], argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "--dump-golden") == 0 && argc >= 3) {
        return run_pmu_perf_and_qor("native", argv[2], 1);
    } else if (strcmp(argv[1], "--pmu-perf") == 0 && argc >= 4) {
        return run_pmu_perf_and_qor(argv[2], argv[3], 0);
    } else if (strcmp(argv[1], "--fi-one") == 0 && argc >= 5) {
        return run_standard_one_fault_fi(argv[2], argv[3], atoi(argv[4]));
    } else if (strcmp(argv[1], "--trace-pmu") == 0) {
        return run_adaptive_trace_pmu();
    }
    return 1;
}
