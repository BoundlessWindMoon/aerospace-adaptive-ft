#include "atft_runtime.h"
#include "benchmarks.h"
#include "harness.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define NUM_BENCHES 12

static const char *BENCH_NAMES[NUM_BENCHES] = {
    "pb_gemm", "pb_2mm", "pb_atax", "pb_bicg", "pb_jacobi2d",
    "parsec_blackscholes", "axc_sobel", "axc_gaussian",
    "dsp_kissfft", "dsp_fir", "gnc_pid", "gnc_ekf"
};

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

typedef struct {
    int n_mat;
    int n_fft;
    int n_fir, n_taps;
    int img_h, img_w;
    int ctrl_steps;
    int n_opts;

    double *A, *B, *C, *D, *tmp_2mm;
    double *x, *y, *tmp_atax, *s_bicg, *q_bicg, *p_bicg, *r_bicg;
    double *jac_A, *jac_B;
    double *bs_spt, *bs_strike, *bs_rate, *bs_vol, *bs_time, *bs_prices;
    double *fft_re, *fft_im, *fft_out_re, *fft_out_im;
    double *fir_sig, *fir_coeffs, *fir_out;
    double *img_in, *sobel_out, *blur_out;
    double *setpoints, *meas, *pid_out;
    double *gyro, *accel, *ekf_out;

    double *out_ptrs[NUM_BENCHES];
    int     out_sizes[NUM_BENCHES];
    double  peak_vals[NUM_BENCHES];
    int     perf_iters[NUM_BENCHES];
} suite_workspace_t;

static void alloc_and_init_workspace(suite_workspace_t *ws, int scale_small) {
    memset(ws, 0, sizeof(*ws));
    ws->n_mat      = scale_small ? 24  : 48;
    ws->n_fft      = scale_small ? 32  : 64;
    ws->n_fir      = scale_small ? 256 : 512;
    ws->n_taps     = scale_small ? 16  : 32;
    ws->img_h      = scale_small ? 24  : 48;
    ws->img_w      = scale_small ? 24  : 48;
    ws->ctrl_steps = scale_small ? 256 : 512;
    ws->n_opts     = scale_small ? 256 : 512;

    int nm2 = ws->n_mat * ws->n_mat;
    ws->A        = calloc(nm2, sizeof(double));
    ws->B        = calloc(nm2, sizeof(double));
    ws->C        = calloc(nm2, sizeof(double));
    ws->D        = calloc(nm2, sizeof(double));
    ws->tmp_2mm  = calloc(nm2, sizeof(double));
    ws->x        = calloc(ws->n_mat, sizeof(double));
    ws->y        = calloc(ws->n_mat, sizeof(double));
    ws->tmp_atax = calloc(ws->n_mat, sizeof(double));
    ws->s_bicg   = calloc(ws->n_mat, sizeof(double));
    ws->q_bicg   = calloc(ws->n_mat, sizeof(double));
    ws->p_bicg   = calloc(ws->n_mat, sizeof(double));
    ws->r_bicg   = calloc(ws->n_mat, sizeof(double));
    ws->jac_A    = calloc(nm2, sizeof(double));
    ws->jac_B    = calloc(nm2, sizeof(double));

    ws->bs_spt    = calloc(ws->n_opts, sizeof(double));
    ws->bs_strike = calloc(ws->n_opts, sizeof(double));
    ws->bs_rate   = calloc(ws->n_opts, sizeof(double));
    ws->bs_vol    = calloc(ws->n_opts, sizeof(double));
    ws->bs_time   = calloc(ws->n_opts, sizeof(double));
    ws->bs_prices = calloc(ws->n_opts, sizeof(double));

    ws->fft_re     = calloc(ws->n_fft, sizeof(double));
    ws->fft_im     = calloc(ws->n_fft, sizeof(double));
    ws->fft_out_re = calloc(ws->n_fft, sizeof(double));
    ws->fft_out_im = calloc(ws->n_fft, sizeof(double));

    ws->fir_sig    = calloc(ws->n_fir, sizeof(double));
    ws->fir_coeffs = calloc(ws->n_taps, sizeof(double));
    ws->fir_out    = calloc(ws->n_fir, sizeof(double));

    int im2 = ws->img_h * ws->img_w;
    ws->img_in    = calloc(im2, sizeof(double));
    ws->sobel_out = calloc(im2, sizeof(double));
    ws->blur_out  = calloc(im2, sizeof(double));

    ws->setpoints = calloc(ws->ctrl_steps, sizeof(double));
    ws->meas      = calloc(ws->ctrl_steps, sizeof(double));
    ws->pid_out   = calloc(ws->ctrl_steps, sizeof(double));
    ws->gyro      = calloc(ws->ctrl_steps, sizeof(double));
    ws->accel     = calloc(ws->ctrl_steps, sizeof(double));
    ws->ekf_out   = calloc(ws->ctrl_steps, sizeof(double));

    for (int i = 0; i < nm2; i++) {
        ws->A[i] = 0.5 * sin(0.13 * (double)(i + 1)) + 0.8;
        ws->B[i] = 0.5 * cos(0.07 * (double)(i + 1)) + 0.6;
    }
    for (int i = 0; i < ws->n_mat; i++) {
        ws->x[i]      = 0.3 * sin(0.2 * (double)i) + 1.0;
        ws->p_bicg[i] = 0.4 * cos(0.17 * (double)i) + 0.9;
        ws->r_bicg[i] = 0.35 * sin(0.11 * (double)i) + 0.7;
    }
    for (int i = 0; i < ws->n_opts; i++) {
        ws->bs_spt[i]    = 50.0 + 15.0 * sin(0.1 * (double)i);
        ws->bs_strike[i] = 52.0 + 10.0 * cos(0.1 * (double)i);
        ws->bs_rate[i]   = 0.05;
        ws->bs_vol[i]    = 0.25 + 0.05 * sin(0.05 * (double)i);
        ws->bs_time[i]   = 1.0 + 0.5 * cos(0.03 * (double)i);
    }
    for (int i = 0; i < ws->n_fft; i++) {
        ws->fft_re[i] = cos(2.0 * 3.141592653589793 * 3.0 * i / ws->n_fft) +
                        0.25 * sin(2.0 * 3.141592653589793 * 7.0 * i / ws->n_fft);
        ws->fft_im[i] = 0.1 * sin(2.0 * 3.141592653589793 * 3.0 * i / ws->n_fft);
    }
    for (int i = 0; i < ws->n_fir; i++) {
        ws->fir_sig[i] = sin(0.05 * (double)i) + 0.2 * cos(0.31 * (double)i);
    }
    for (int k = 0; k < ws->n_taps; k++) {
        double w = 0.54 - 0.46 * cos(2.0 * 3.141592653589793 * k / (ws->n_taps - 1));
        ws->fir_coeffs[k] = w / (double)ws->n_taps;
    }
    for (int r = 0; r < ws->img_h; r++) {
        for (int c = 0; c < ws->img_w; c++) {
            ws->img_in[r * ws->img_w + c] = 128.0 + 60.0 * sin(0.15 * r) * cos(0.15 * c);
        }
    }
    for (int t = 0; t < ws->ctrl_steps; t++) {
        ws->setpoints[t] = 1.0;
        ws->meas[t]      = 1.0 - 0.8 * exp(-0.02 * (double)t) * cos(0.08 * (double)t);
        ws->gyro[t]      = 0.05 * cos(0.04 * (double)t) + 0.002;
        ws->accel[t]     = 1.25 * sin(0.04 * (double)t);
    }

    ws->out_ptrs[0]  = ws->C;          ws->out_sizes[0]  = nm2;            ws->peak_vals[0]  = 40.0;  ws->perf_iters[0]  = 200;
    ws->out_ptrs[1]  = ws->D;          ws->out_sizes[1]  = nm2;            ws->peak_vals[1]  = 800.0; ws->perf_iters[1]  = 150;
    ws->out_ptrs[2]  = ws->y;          ws->out_sizes[2]  = ws->n_mat;      ws->peak_vals[2]  = 600.0; ws->perf_iters[2]  = 1500;
    ws->out_ptrs[3]  = ws->q_bicg;     ws->out_sizes[3]  = ws->n_mat;      ws->peak_vals[3]  = 40.0;  ws->perf_iters[3]  = 1500;
    ws->out_ptrs[4]  = ws->jac_A;      ws->out_sizes[4]  = nm2;            ws->peak_vals[4]  = 2.0;   ws->perf_iters[4]  = 200;
    ws->out_ptrs[5]  = ws->bs_prices;  ws->out_sizes[5]  = ws->n_opts;     ws->peak_vals[5]  = 25.0;  ws->perf_iters[5]  = 600;
    ws->out_ptrs[6]  = ws->sobel_out;  ws->out_sizes[6]  = im2;            ws->peak_vals[6]  = 255.0; ws->perf_iters[6]  = 800;
    ws->out_ptrs[7]  = ws->blur_out;   ws->out_sizes[7]  = im2;            ws->peak_vals[7]  = 255.0; ws->perf_iters[7]  = 400;
    ws->out_ptrs[8]  = ws->fft_out_re; ws->out_sizes[8]  = ws->n_fft;      ws->peak_vals[8]  = 32.0;  ws->perf_iters[8]  = 300;
    ws->out_ptrs[9]  = ws->fir_out;    ws->out_sizes[9]  = ws->n_fir;      ws->peak_vals[9]  = 1.5;   ws->perf_iters[9]  = 800;
    ws->out_ptrs[10] = ws->pid_out;    ws->out_sizes[10] = ws->ctrl_steps; ws->peak_vals[10] = 2.0;   ws->perf_iters[10] = 2000;
    ws->out_ptrs[11] = ws->ekf_out;    ws->out_sizes[11] = ws->ctrl_steps; ws->peak_vals[11] = 1.5;   ws->perf_iters[11] = 2000;
}

static void reset_mutable_buffers(suite_workspace_t *ws, int b) {
    int nm2 = ws->n_mat * ws->n_mat;
    if (b == 0) {
        for (int i = 0; i < nm2; i++) ws->C[i] = 0.1 * (double)(i % 17);
    } else if (b == 1) {
        for (int i = 0; i < nm2; i++) ws->D[i] = 0.05 * (double)(i % 13);
    } else if (b == 4) {
        for (int i = 0; i < nm2; i++) {
            ws->jac_A[i] = 0.5 + 0.3 * sin(0.1 * (double)i);
            ws->jac_B[i] = 0.5 + 0.3 * cos(0.1 * (double)i);
        }
    }
}

static void invoke_benchmark(suite_workspace_t *ws, int b) {
    reset_mutable_buffers(ws, b);
    switch (b) {
        case 0: bench_pb_gemm(ws->n_mat, ws->n_mat, ws->n_mat, 1.5, 1.2, ws->A, ws->B, ws->C); break;
        case 1: bench_pb_2mm(ws->n_mat, ws->n_mat, ws->n_mat, ws->n_mat, 1.5, 1.2, ws->tmp_2mm, ws->A, ws->B, ws->A, ws->D); break;
        case 2: bench_pb_atax(ws->n_mat, ws->n_mat, ws->A, ws->x, ws->y, ws->tmp_atax); break;
        case 3: bench_pb_bicg(ws->n_mat, ws->n_mat, ws->A, ws->s_bicg, ws->q_bicg, ws->p_bicg, ws->r_bicg); break;
        case 4: bench_pb_jacobi2d(4, ws->n_mat, ws->jac_A, ws->jac_B); break;
        case 5: bench_parsec_blackscholes(ws->n_opts, ws->bs_spt, ws->bs_strike, ws->bs_rate, ws->bs_vol, ws->bs_time, ws->bs_prices); break;
        case 6: bench_axc_sobel(ws->img_h, ws->img_w, ws->img_in, ws->sobel_out); break;
        case 7: bench_axc_gaussian(ws->img_h, ws->img_w, ws->img_in, ws->blur_out); break;
        case 8: bench_dsp_kissfft(ws->n_fft, ws->fft_re, ws->fft_im, ws->fft_out_re, ws->fft_out_im); break;
        case 9: bench_dsp_fir(ws->n_fir, ws->n_taps, ws->fir_sig, ws->fir_coeffs, ws->fir_out); break;
        case 10: bench_gnc_pid(ws->ctrl_steps, ws->setpoints, ws->meas, 1.2, 0.4, 0.05, 0.01, ws->pid_out); break;
        case 11: bench_gnc_ekf(ws->ctrl_steps, ws->gyro, ws->accel, 0.01, ws->ekf_out); break;
    }
}

static void free_workspace(suite_workspace_t *ws) {
    free(ws->A); free(ws->B); free(ws->C); free(ws->D); free(ws->tmp_2mm);
    free(ws->x); free(ws->y); free(ws->tmp_atax); free(ws->s_bicg);
    free(ws->q_bicg); free(ws->p_bicg); free(ws->r_bicg);
    free(ws->jac_A); free(ws->jac_B);
    free(ws->bs_spt); free(ws->bs_strike); free(ws->bs_rate);
    free(ws->bs_vol); free(ws->bs_time); free(ws->bs_prices);
    free(ws->fft_re); free(ws->fft_im); free(ws->fft_out_re); free(ws->fft_out_im);
    free(ws->fir_sig); free(ws->fir_coeffs); free(ws->fir_out);
    free(ws->img_in); free(ws->sobel_out); free(ws->blur_out);
    free(ws->setpoints); free(ws->meas); free(ws->pid_out);
    free(ws->gyro); free(ws->accel); free(ws->ekf_out);
}

static int run_pmu_perf_and_qor(const char *policy_name, const char *golden_dir, int dump_golden) {
    suite_workspace_t ws;
    alloc_and_init_workspace(&ws, 0);

    harness_pmu_t pmu;
    harness_pmu_init(&pmu);

    for (int b = 0; b < NUM_BENCHES; b++) {
        atft_init(10000.0, 4.0, 3, 4, 3);
        if (strcmp(policy_name, "adaptive_high") == 0) {
            g_atft_state.current_mode = ATFT_MODE_HIGH;
        }

        // Warmup
        invoke_benchmark(&ws, b);

        atft_reset_stats();
        if (strcmp(policy_name, "adaptive_high") == 0) {
            g_atft_state.current_mode = ATFT_MODE_HIGH;
        }

        int iters = ws.perf_iters[b];
        uint64_t total_hw_insts = 0, total_hw_cycles = 0;
        double t0 = now_sec();
        harness_pmu_start(&pmu);
        for (int it = 0; it < iters; it++) {
            invoke_benchmark(&ws, b);
        }
        harness_pmu_stop(&pmu, &total_hw_insts, &total_hw_cycles);
        double t1 = now_sec();

        double avg_us = (t1 - t0) * 1e6 / (double)iters;
        uint64_t hw_insts = total_hw_insts / (uint64_t)iters;
        uint64_t hw_cycles = total_hw_cycles / (uint64_t)iters;

        char gpath[512];
        snprintf(gpath, sizeof(gpath), "%s/golden_%s.bin", golden_dir, BENCH_NAMES[b]);
        harness_qor_metrics_t qor;
        memset(&qor, 0, sizeof(qor));
        qor.psnr_db = 120.0;

        if (dump_golden) {
            FILE *fp = fopen(gpath, "wb");
            if (fp) {
                fwrite(ws.out_ptrs[b], sizeof(double), ws.out_sizes[b], fp);
                fclose(fp);
            }
        } else {
            double *golden_buf = calloc(ws.out_sizes[b], sizeof(double));
            FILE *fp = fopen(gpath, "rb");
            if (fp) {
                size_t nr = fread(golden_buf, sizeof(double), ws.out_sizes[b], fp);
                (void)nr;
                fclose(fp);
                harness_compute_qor(ws.out_sizes[b], golden_buf, ws.out_ptrs[b], ws.peak_vals[b], &qor);
            }
            free(golden_buf);
        }

        uint64_t votes_per_iter = g_atft_state.total_votes / (uint64_t)iters;
        uint64_t false_alarms_per_iter = g_atft_state.detected_faults / (uint64_t)iters;

        printf("PMU_CSV,%s,%s,%.3f,%lu,%lu,%.6e,%.6e,%.6e,%.2f,%lu,%lu\n",
               policy_name, BENCH_NAMES[b], avg_us,
               (unsigned long)hw_insts, (unsigned long)hw_cycles,
               qor.rel_l2_err, qor.mean_rel_err, qor.max_abs_err, qor.psnr_db,
               (unsigned long)votes_per_iter, (unsigned long)false_alarms_per_iter);
    }

    harness_pmu_close(&pmu);
    free_workspace(&ws);
    return 0;
}

static int run_standard_one_fault_fi(const char *policy_name, const char *golden_dir, int trials) {
    (void)golden_dir;
    suite_workspace_t ws;
    alloc_and_init_workspace(&ws, 1);

    for (int b = 0; b < NUM_BENCHES; b++) {
        double *golden_buf = calloc(ws.out_sizes[b], sizeof(double));
        atft_init(10000.0, 4.0, 3, 4, 3);
        if (strstr(policy_name, "adaptive") != NULL) {
            g_atft_state.current_mode = ATFT_MODE_HIGH;
        }
        harness_fi_reset_profile();

        invoke_benchmark(&ws, b);
        memcpy(golden_buf, ws.out_ptrs[b], ws.out_sizes[b] * sizeof(double));

        uint64_t total_dyn_insts = harness_fi_get_total_dyn_insts();
        uint64_t baseline_false_alarms = g_atft_state.detected_faults;

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

            memset(ws.out_ptrs[b], 0, ws.out_sizes[b] * sizeof(double));
            invoke_benchmark(&ws, b);

            harness_qor_metrics_t qor;
            harness_compute_qor(ws.out_sizes[b], golden_buf, ws.out_ptrs[b], ws.peak_vals[b], &qor);

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
               policy_name, BENCH_NAMES[b], (unsigned long)total_dyn_insts, trials,
               p_masked, p_det_rec, p_benign, p_sdc, p_due, avg_err);

        free(golden_buf);
    }

    free_workspace(&ws);
    return 0;
}

static int run_adaptive_trace_pmu(void) {
    suite_workspace_t ws;
    alloc_and_init_workspace(&ws, 1);

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
            invoke_benchmark(&ws, 0); // pb_gemm
            atft_advance_insts(600000);
            if (ecc_events_in_win >= 5) atft_report_ecc_event();

            invoke_benchmark(&ws, 2); // pb_atax
            atft_advance_insts(100000);
            if (ecc_events_in_win >= 5) atft_report_ecc_event();

            invoke_benchmark(&ws, 8); // dsp_kissfft
            atft_advance_insts(300000);
            if (ecc_events_in_win >= 5) atft_report_ecc_event();

            invoke_benchmark(&ws, 6); // axc_sobel
            atft_advance_insts(200000);
            if (ecc_events_in_win >= 5) atft_report_ecc_event();

            invoke_benchmark(&ws, 10); // gnc_pid
            invoke_benchmark(&ws, 11); // gnc_ekf
            bench_noncritical_telemetry_checksum(ws.ctrl_steps, ws.pid_out);
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
    free_workspace(&ws);
    return 0;
}

// ============================================================================
// 模式 4 (RQ5): IEEE-754 比特位域脆弱性剖析 (Sign / Exponent / Mantissa-High / Mantissa-Low)
// 解释为什么全位宽均匀采样下 ~90% 故障被掩盖，以及近似容错对高位致命翻转的精准拦截机理
// ============================================================================
static int run_bitfield_sensitivity(const char *policy_name) {
    suite_workspace_t ws;
    alloc_and_init_workspace(&ws, 1);

    const char *field_names[4] = {
        "Sign (Bit 63)",
        "Exponent (Bits 52-62)",
        "Mantissa_High (Bits 32-51)",
        "Mantissa_Low (Bits 0-31)"
    };
    const int target_benches[6] = {0, 4, 5, 6, 9, 11}; // pb_gemm, pb_jacobi2d, blackscholes, sobel, fir, ekf

    for (int f = 0; f < 4; f++) {
        int total_trials = 0;
        int cnt_masked = 0, cnt_det_rec = 0, cnt_benign = 0, cnt_sdc = 0, cnt_due = 0;

        for (int bi = 0; bi < 6; bi++) {
            int b = target_benches[bi];
            double *golden_buf = calloc(ws.out_sizes[b], sizeof(double));
            atft_init(10000.0, 4.0, 3, 4, 3);
            harness_fi_reset_profile();
            invoke_benchmark(&ws, b);
            memcpy(golden_buf, ws.out_ptrs[b], ws.out_sizes[b] * sizeof(double));

            uint64_t total_dyn_insts = harness_fi_get_total_dyn_insts();
            uint64_t baseline_false_alarms = g_atft_state.detected_faults;
            uint64_t rng = 0x55aa55aa12345678ULL ^ ((uint64_t)(f * 17 + b) << 24);

            int trials_per_b = 100;
            for (int t = 0; t < trials_per_b; t++) {
                uint64_t target_step = 1 + (splitmix64(&rng) % (total_dyn_insts > 0 ? total_dyn_insts : 1));
                uint32_t target_bit = 0;
                if (f == 0) target_bit = 63;
                else if (f == 1) target_bit = 52 + (uint32_t)(splitmix64(&rng) % 11);
                else if (f == 2) target_bit = 32 + (uint32_t)(splitmix64(&rng) % 20);
                else target_bit = (uint32_t)(splitmix64(&rng) % 32);

                atft_init(10000.0, 4.0, 3, 4, 3);
                harness_fi_arm_one_fault(target_step, target_bit);
                memset(ws.out_ptrs[b], 0, ws.out_sizes[b] * sizeof(double));
                invoke_benchmark(&ws, b);

                harness_qor_metrics_t qor;
                harness_compute_qor(ws.out_sizes[b], golden_buf, ws.out_ptrs[b], ws.peak_vals[b], &qor);

                int fault_detected = (g_atft_state.detected_faults > baseline_false_alarms);
                int unrec = (g_atft_state.unrecoverable_faults > 0);
                total_trials++;

                if (unrec) cnt_due++;
                else if (qor.has_nan_or_inf || qor.rel_l2_err > 0.01) cnt_sdc++;
                else if (fault_detected) cnt_det_rec++;
                else if (qor.rel_l2_err <= 1e-12) cnt_masked++;
                else cnt_benign++;
            }
            free(golden_buf);
        }

        printf("BITFIELD_CSV,%s,%s,%d,%.2f,%.2f,%.2f,%.2f,%.2f\n",
               policy_name, field_names[f], total_trials,
               100.0 * cnt_masked / total_trials,
               100.0 * cnt_det_rec / total_trials,
               100.0 * cnt_benign / total_trials,
               100.0 * cnt_sdc / total_trials,
               100.0 * cnt_due / total_trials);
    }

    free_workspace(&ws);
    return 0;
}

// ============================================================================
// 模式 5 (RQ6): 空间多比特翻转 (Spatial MBU: 1/2/4-bit) 与时间窗口连续故障冲击 (Temporal Burst: k=1/2/4/8)
// ============================================================================
static int run_mbu_and_burst_stress(const char *policy_name) {
    suite_workspace_t ws;
    alloc_and_init_workspace(&ws, 1);
    const int target_benches[6] = {0, 1, 4, 6, 9, 11};

    // (A) Spatial MBU: 1-bit SEU, 2-bit DBU, 4-bit QBU (k=1 event per run)
    int mbu_widths[3] = {1, 2, 4};
    const char *mbu_labels[3] = {"SEU (1-bit)", "DBU (2-bit Adjacent)", "QBU (4-bit Burst)"};
    for (int mw = 0; mw < 3; mw++) {
        int total_trials = 0, cnt_ok = 0, cnt_det_rec = 0, cnt_sdc = 0, cnt_due = 0;
        for (int bi = 0; bi < 6; bi++) {
            int b = target_benches[bi];
            double *golden_buf = calloc(ws.out_sizes[b], sizeof(double));
            atft_init(10000.0, 4.0, 3, 4, 3);
            harness_fi_reset_profile();
            invoke_benchmark(&ws, b);
            memcpy(golden_buf, ws.out_ptrs[b], ws.out_sizes[b] * sizeof(double));
            uint64_t total_dyn_insts = harness_fi_get_total_dyn_insts();
            uint64_t baseline_fa = g_atft_state.detected_faults;
            uint64_t rng = 0x9988776655443322ULL ^ ((uint64_t)(mw * 31 + b) << 20);

            for (int t = 0; t < 80; t++) {
                uint64_t st = 1 + (splitmix64(&rng) % (total_dyn_insts > 0 ? total_dyn_insts : 1));
                uint32_t bt = (uint32_t)(splitmix64(&rng) % 60);
                atft_init(10000.0, 4.0, 3, 4, 3);
                harness_fi_arm_mbu_or_burst(&st, &bt, 1, (uint32_t)mbu_widths[mw]);
                memset(ws.out_ptrs[b], 0, ws.out_sizes[b] * sizeof(double));
                invoke_benchmark(&ws, b);

                harness_qor_metrics_t qor;
                harness_compute_qor(ws.out_sizes[b], golden_buf, ws.out_ptrs[b], ws.peak_vals[b], &qor);
                total_trials++;
                if (g_atft_state.unrecoverable_faults > 0) cnt_due++;
                else if (qor.has_nan_or_inf || qor.rel_l2_err > 0.01) cnt_sdc++;
                else {
                    cnt_ok++;
                    if (g_atft_state.detected_faults > baseline_fa) cnt_det_rec++;
                }
            }
            free(golden_buf);
        }
        printf("STRESS_CSV,%s,Spatial_MBU,%s,%d,%.2f,%.2f,%.2f,%.2f\n",
               policy_name, mbu_labels[mw], total_trials,
               100.0 * cnt_ok / total_trials,
               100.0 * cnt_det_rec / total_trials,
               100.0 * cnt_sdc / total_trials,
               100.0 * cnt_due / total_trials);
    }

    // (B) Temporal Multi-Fault Burst: k = 1, 2, 4, 8 faults injected within a single execution
    int burst_counts[4] = {1, 2, 4, 8};
    const char *burst_labels[4] = {"k=1 Fault/Run", "k=2 Faults/Run", "k=4 Faults/Run", "k=8 Faults/Run"};
    for (int bc = 0; bc < 4; bc++) {
        int k_events = burst_counts[bc];
        int total_trials = 0, cnt_ok = 0, cnt_det_rec = 0, cnt_sdc = 0, cnt_due = 0;
        for (int bi = 0; bi < 6; bi++) {
            int b = target_benches[bi];
            double *golden_buf = calloc(ws.out_sizes[b], sizeof(double));
            atft_init(10000.0, 4.0, 3, 4, 3);
            harness_fi_reset_profile();
            invoke_benchmark(&ws, b);
            memcpy(golden_buf, ws.out_ptrs[b], ws.out_sizes[b] * sizeof(double));
            uint64_t total_dyn_insts = harness_fi_get_total_dyn_insts();
            uint64_t baseline_fa = g_atft_state.detected_faults;
            uint64_t rng = 0xabcdef0123456789ULL ^ ((uint64_t)(bc * 43 + b) << 20);

            for (int t = 0; t < 80; t++) {
                uint64_t steps[16];
                uint32_t bits[16];
                for (int e = 0; e < k_events; e++) {
                    steps[e] = 1 + (splitmix64(&rng) % (total_dyn_insts > 0 ? total_dyn_insts : 1));
                    bits[e]  = (uint32_t)(splitmix64(&rng) % 64);
                }
                atft_init(10000.0, 4.0, 3, 4, 3);
                harness_fi_arm_mbu_or_burst(steps, bits, k_events, 1);
                memset(ws.out_ptrs[b], 0, ws.out_sizes[b] * sizeof(double));
                invoke_benchmark(&ws, b);

                harness_qor_metrics_t qor;
                harness_compute_qor(ws.out_sizes[b], golden_buf, ws.out_ptrs[b], ws.peak_vals[b], &qor);
                total_trials++;
                if (g_atft_state.unrecoverable_faults > 0) cnt_due++;
                else if (qor.has_nan_or_inf || qor.rel_l2_err > 0.01) cnt_sdc++;
                else {
                    cnt_ok++;
                    if (g_atft_state.detected_faults > baseline_fa) cnt_det_rec++;
                }
            }
            free(golden_buf);
        }
        printf("STRESS_CSV,%s,Temporal_Burst,%s,%d,%.2f,%.2f,%.2f,%.2f\n",
               policy_name, burst_labels[bc], total_trials,
               100.0 * cnt_ok / total_trials,
               100.0 * cnt_det_rec / total_trials,
               100.0 * cnt_sdc / total_trials,
               100.0 * cnt_due / total_trials);
    }

    free_workspace(&ws);
    return 0;
}

// ============================================================================
// 模式 6 (RQ7): PolyBench 跨存储层次数据规模扩展测试 (L1 Cache -> L2 Cache -> L3/DRAM)
// 矩阵维度 N ∈ {32 (24KB, L1), 64 (96KB, L2), 128 (384KB, L2/L3), 256 (1.5MB, L3)}
// ============================================================================
static int run_scale_sweep_pmu(const char *policy_name) {
    int sizes[4] = {32, 64, 128, 256};
    const char *tier_names[4] = {"N=32 (24KB L1 Cache)", "N=64 (96KB L2 Cache)", "N=128 (384KB L2/L3)", "N=256 (1.5MB L3/DRAM)"};
    int iters_arr[4] = {200, 60, 15, 4};

    harness_pmu_t pmu;
    harness_pmu_init(&pmu);

    for (int s = 0; s < 4; s++) {
        int n = sizes[s];
        int iters = iters_arr[s];
        double *A = calloc(n * n, sizeof(double));
        double *B = calloc(n * n, sizeof(double));
        double *C = calloc(n * n, sizeof(double));
        for (int i = 0; i < n * n; i++) {
            A[i] = 0.5 * sin(0.11 * (double)i) + 0.8;
            B[i] = 0.5 * cos(0.07 * (double)i) + 0.6;
            C[i] = 0.1;
        }

        atft_init(10000.0, 4.0, 3, 4, 3);
        bench_pb_gemm(n, n, n, 1.5, 1.2, A, B, C); // warmup

        uint64_t hw_insts = 0, hw_cycles = 0;
        double t0 = now_sec();
        harness_pmu_start(&pmu);
        for (int it = 0; it < iters; it++) {
            bench_pb_gemm(n, n, n, 1.5, 1.2, A, B, C);
        }
        harness_pmu_stop(&pmu, &hw_insts, &hw_cycles);
        double t1 = now_sec();

        double avg_us = (t1 - t0) * 1e6 / (double)iters;
        uint64_t inst_per_iter = hw_insts / (uint64_t)iters;
        uint64_t cyc_per_iter  = hw_cycles / (uint64_t)iters;
        double ipc = cyc_per_iter > 0 ? ((double)inst_per_iter / (double)cyc_per_iter) : 0.0;

        printf("SCALE_CSV,%s,%d,%s,%.2f,%lu,%lu,%.2f\n",
               policy_name, n, tier_names[s], avg_us,
               (unsigned long)inst_per_iter, (unsigned long)cyc_per_iter, ipc);

        free(A); free(B); free(C);
    }

    harness_pmu_close(&pmu);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
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
    } else if (strcmp(argv[1], "--bitfield") == 0 && argc >= 3) {
        return run_bitfield_sensitivity(argv[2]);
    } else if (strcmp(argv[1], "--stress") == 0 && argc >= 3) {
        return run_mbu_and_burst_stress(argv[2]);
    } else if (strcmp(argv[1], "--scale") == 0 && argc >= 3) {
        return run_scale_sweep_pmu(argv[2]);
    }
    return 1;
}
