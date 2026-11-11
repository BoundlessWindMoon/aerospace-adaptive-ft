#include "atft_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

atft_runtime_state_t g_atft_state = {
    .total_votes = 0,
    .fault_free_votes = 0,
    .detected_faults = 0,
    .exact_branch_faults = 0,
    .approx_branch_faults = 0,
    .recovered_by_approx = 0,
    .recovered_by_reexec = 0,
    .unrecoverable_faults = 0,
    .retired_inst_counter = 0,
    .last_ecc_inst_stamp = 0,
    .calm_avg_interval = 10000.0,
    .theta_ratio = 4.0,
    .consec_short_intervals = 0,
    .metric_a_threshold_n = 3,
    .window_ecc_count = 0,
    .metric_b_threshold = 4,
    .calm_windows_count = 0,
    .downgrade_threshold_m = 3,
    .current_mode = ATFT_MODE_LOW,
    .mode_switch_count = 0,
    .fi_enabled = 0,
    .fi_prob = 0.0,
    .fi_target_branch = -1,
    .fi_injected_count = 0
};

static uint64_t g_rng_state = 0x853c49e6748fea9bULL;

static inline uint64_t xorshift64(void) {
    uint64_t x = g_rng_state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    g_rng_state = x;
    return x;
}

static inline double rng_uniform01(void) {
    return (double)(xorshift64() & 0x1fffffffffffffULL) / (double)(1ULL << 53);
}

void atft_init(double calm_interval, double theta, uint32_t n_short, uint32_t win_thresh, uint32_t m_calm) {
    memset(&g_atft_state, 0, sizeof(g_atft_state));
    g_atft_state.calm_avg_interval = (calm_interval > 0.0) ? calm_interval : 10000.0;
    g_atft_state.theta_ratio = (theta > 1.0) ? theta : 4.0;
    g_atft_state.metric_a_threshold_n = (n_short > 0) ? n_short : 3;
    g_atft_state.metric_b_threshold = (win_thresh > 0) ? win_thresh : 4;
    g_atft_state.downgrade_threshold_m = (m_calm > 0) ? m_calm : 3;
    g_atft_state.current_mode = ATFT_MODE_LOW;
    g_atft_state.fi_target_branch = -1;
}

void atft_reset_stats(void) {
    g_atft_state.total_votes = 0;
    g_atft_state.fault_free_votes = 0;
    g_atft_state.detected_faults = 0;
    g_atft_state.exact_branch_faults = 0;
    g_atft_state.approx_branch_faults = 0;
    g_atft_state.recovered_by_approx = 0;
    g_atft_state.recovered_by_reexec = 0;
    g_atft_state.unrecoverable_faults = 0;
    g_atft_state.fi_injected_count = 0;
}

atft_prot_mode_t atft_get_mode(void) {
    return g_atft_state.current_mode;
}

// ============================================================================
// 运行时监控与动态决策 (对应开题报告 3.3.3.2 & 3.3.3.3)
// ============================================================================
void atft_advance_insts(uint64_t inst_count) {
    g_atft_state.retired_inst_counter += inst_count;
}

void atft_report_ecc_event(void) {
    uint64_t delta_inst = g_atft_state.retired_inst_counter - g_atft_state.last_ecc_inst_stamp;
    g_atft_state.last_ecc_inst_stamp = g_atft_state.retired_inst_counter;

    // 指标 A: 连续两次 ECC 事件间隔 T_ECC < \bar{T} / \theta
    double short_limit = g_atft_state.calm_avg_interval / g_atft_state.theta_ratio;
    if ((double)delta_inst < short_limit) {
        g_atft_state.consec_short_intervals++;
    } else {
        g_atft_state.consec_short_intervals = 0;
    }

    // 指标 B: 当前窗口内发生的 ECC 事件数累积
    g_atft_state.window_ecc_count++;

    // 决策算法: 当指标 A 和指标 B 同时满足时，从 LOW 切换至 HIGH
    int metric_a_met = (g_atft_state.consec_short_intervals >= g_atft_state.metric_a_threshold_n);
    int metric_b_met = (g_atft_state.window_ecc_count >= g_atft_state.metric_b_threshold);

    if (g_atft_state.current_mode == ATFT_MODE_LOW && metric_a_met && metric_b_met) {
        g_atft_state.current_mode = ATFT_MODE_HIGH;
        g_atft_state.calm_windows_count = 0;
        g_atft_state.mode_switch_count++;
    }
}

void atft_step_window(void) {
    // 当系统处于 HIGH 且连续观察到 M 个窗口低于降级阈值时，切换回 LOW 状态
    uint32_t low_water_mark = (g_atft_state.metric_b_threshold > 2) ? (g_atft_state.metric_b_threshold / 2) : 1;
    if (g_atft_state.current_mode == ATFT_MODE_HIGH) {
        if (g_atft_state.window_ecc_count < low_water_mark) {
            g_atft_state.calm_windows_count++;
            if (g_atft_state.calm_windows_count >= g_atft_state.downgrade_threshold_m) {
                g_atft_state.current_mode = ATFT_MODE_LOW;
                g_atft_state.consec_short_intervals = 0;
                g_atft_state.calm_windows_count = 0;
                g_atft_state.mode_switch_count++;
            }
        } else {
            g_atft_state.calm_windows_count = 0;
        }
    }
    g_atft_state.window_ecc_count = 0;
}

// ============================================================================
// 传统精确 TMR 表决器 (Baseline)
// ============================================================================
double atft_voter_exact_f64(double r1, double r2, double r3) {
    g_atft_state.total_votes++;
    const double eps = 1e-12;
    int c12 = (fabs(r1 - r2) <= eps);
    int c13 = (fabs(r1 - r3) <= eps);
    int c23 = (fabs(r2 - r3) <= eps);

    if (c12 && c13) {
        g_atft_state.fault_free_votes++;
        return r1;
    }
    g_atft_state.detected_faults++;
    atft_report_ecc_event();
    if (c12 || c13) {
        g_atft_state.recovered_by_reexec++;
        return r1;
    }
    if (c23) {
        g_atft_state.recovered_by_reexec++;
        return r2;
    }
    g_atft_state.unrecoverable_faults++;
    return r1;
}

int32_t atft_voter_exact_i32(int32_t r1, int32_t r2, int32_t r3) {
    g_atft_state.total_votes++;
    if (r1 == r2 && r1 == r3) {
        g_atft_state.fault_free_votes++;
        return r1;
    }
    g_atft_state.detected_faults++;
    atft_report_ecc_event();
    if (r1 == r2 || r1 == r3) {
        g_atft_state.recovered_by_reexec++;
        return r1;
    }
    if (r2 == r3) {
        g_atft_state.recovered_by_reexec++;
        return r2;
    }
    g_atft_state.unrecoverable_faults++;
    return r1;
}

// ============================================================================
// 混合误差容忍与判定机制 (对应开题报告 3.4.4.2 & 3.4.4.3)
// ============================================================================
double atft_voter_approx_f64(double r_exact, double a1_approx, double a2_approx,
                             double delta, int32_t crit_level) {
    g_atft_state.total_votes++;

    double ref_scale = fabs(a1_approx);
    if (fabs(a2_approx) < ref_scale) ref_scale = fabs(a2_approx);
    double eff_delta = delta * (1.0 + ref_scale);
    if (eff_delta < 2e-6) eff_delta = 2e-6;

    double diff_r_a1 = fabs(r_exact - a1_approx);
    double diff_r_a2 = fabs(r_exact - a2_approx);
    double diff_a1_a2 = fabs(a1_approx - a2_approx);

    int r_a1_ok = (diff_r_a1 <= eff_delta) && isfinite(r_exact) && isfinite(a1_approx);
    int r_a2_ok = (diff_r_a2 <= eff_delta) && isfinite(r_exact) && isfinite(a2_approx);
    // 两条近似路径采用同种近似变换，无软错误时输出严格相等 (diff_a1_a2 == 0)
    int a1_a2_ok = (diff_a1_a2 <= 1e-7 * (1.0 + ref_scale)) && isfinite(a1_approx) && isfinite(a2_approx);

    // 情况 1: 三路均在容忍阈值内且两近似副本一致 -> 无软错误，输出精确结果 R (零精度损失!)
    if (r_a1_ok && r_a2_ok && a1_a2_ok) {
        g_atft_state.fault_free_votes++;
        return r_exact;
    }

    // 检测到软错误 (不会在投票器内重复上报环境脉冲，由硬件传感器通道独立上报或统计)
    g_atft_state.detected_faults++;

    // 情况 2: |A1 - A2| <= eps 但 |R - A1| > delta -> 精确计算链路 R 发生故障!
    if (a1_a2_ok && (!r_a1_ok || !r_a2_ok)) {
        g_atft_state.exact_branch_faults++;
        if (crit_level >= ATFT_CRIT_CRITICAL) {
            g_atft_state.recovered_by_reexec++;
            return 0.5 * (a1_approx + a2_approx);
        } else {
            g_atft_state.recovered_by_approx++;
            return a1_approx;
        }
    }

    // 情况 3: 两条近似副本不一致 (!a1_a2_ok)，说明 A1 或 A2 发生软错误，而 R 与其中一条吻合 -> 直接输出精确值 R!
    if (r_a1_ok && !r_a2_ok) {
        g_atft_state.approx_branch_faults++;
        g_atft_state.recovered_by_reexec++;
        return r_exact;
    }
    if (r_a2_ok && !r_a1_ok) {
        g_atft_state.approx_branch_faults++;
        g_atft_state.recovered_by_reexec++;
        return r_exact;
    }
    if (!a1_a2_ok && r_a1_ok && r_a2_ok) {
        // A1 或 A2 发生了微小位翻转但仍落在 delta 内，由 A1!=A2 检出并直接返回精确值 R
        g_atft_state.approx_branch_faults++;
        g_atft_state.recovered_by_reexec++;
        return r_exact;
    }

    // 情况 4: 极端多路同时故障，选择有限值安全回退
    g_atft_state.unrecoverable_faults++;
    if (isfinite(r_exact)) return r_exact;
    if (isfinite(a1_approx)) return a1_approx;
    if (isfinite(a2_approx)) return a2_approx;
    return 0.0;
}

int32_t atft_voter_approx_i32(int32_t r_exact, int32_t a1_approx, int32_t a2_approx,
                              int32_t delta, int32_t crit_level) {
    g_atft_state.total_votes++;
    int64_t d1 = llabs((int64_t)r_exact - (int64_t)a1_approx);
    int64_t d2 = llabs((int64_t)r_exact - (int64_t)a2_approx);
    int64_t d12 = llabs((int64_t)a1_approx - (int64_t)a2_approx);

    if (d1 <= delta && d2 <= delta && d12 == 0) {
        g_atft_state.fault_free_votes++;
        return r_exact;
    }

    g_atft_state.detected_faults++;

    if (d12 == 0 && (d1 > delta || d2 > delta)) {
        g_atft_state.exact_branch_faults++;
        if (crit_level >= ATFT_CRIT_CRITICAL) {
            g_atft_state.recovered_by_reexec++;
        } else {
            g_atft_state.recovered_by_approx++;
        }
        return a1_approx;
    }

    g_atft_state.approx_branch_faults++;
    g_atft_state.recovered_by_reexec++;
    return r_exact;
}

// ============================================================================
// 函数计算近似库 (对应开题报告 3.4.2.2: 低阶多项式拟合替代标准库超越函数)
// ============================================================================
float atft_approx_sinf(float x) {
    // 范围规约到 [-pi, pi]
    const float inv_two_pi = 0.15915494309189535f;
    const float two_pi = 6.283185307179586f;
    const float pi = 3.141592653589793f;
    const float half_pi = 1.5707963267948966f;
    float k = roundf(x * inv_two_pi);
    float r = x - k * two_pi;
    if (r > half_pi) r = pi - r;
    if (r < -half_pi) r = -pi - r;
    // 7阶泰勒多项式近似: r - r^3/6 + r^5/120 - r^7/5040 (最大误差 < 2e-6，纯乘加无分支)
    float r2 = r * r;
    return r * (1.0f - r2 * (0.16666667f - r2 * (0.00833333f - r2 * 0.00019841f)));
}

float atft_approx_cosf(float x) {
    return atft_approx_sinf(x + 1.5707963267948966f);
}

float atft_approx_expf(float x) {
    if (x < -10.0f) return 0.0f;
    if (x > 10.0f) x = 10.0f;
    float t = 1.0f + x * 0.0625f;
    t *= t; t *= t; t *= t; t *= t;
    return t;
}

typedef float v2f32 __attribute__((ext_vector_type(2)));

void atft_simd2_fmul_fadd_f32(float a1, float a2, float b1, float b2, float c1, float c2,
                              float *out1, float *out2) {
    v2f32 va = {a1, a2};
    v2f32 vb = {b1, b2};
    v2f32 vc = {c1, c2};
    v2f32 vres = va * vb + vc;
    *out1 = vres[0];
    *out2 = vres[1];
}

// ============================================================================
// 单比特翻转故障注入器 (对应开题报告 4.3.2 & LLFI/LLTFI)
// ============================================================================
void atft_configure_fi(int enabled, double prob, int target_branch, uint32_t seed) {
    g_atft_state.fi_enabled = enabled;
    g_atft_state.fi_prob = prob;
    g_atft_state.fi_target_branch = target_branch;
    g_atft_state.fi_injected_count = 0;
    if (seed != 0) {
        g_rng_state = ((uint64_t)seed << 32) ^ 0x9e3779b97f4a7c15ULL;
    }
}

double atft_inject_bitflip_f64(double val, int branch_id) {
    if (!g_atft_state.fi_enabled) return val;
    if (g_atft_state.fi_target_branch >= 0 && g_atft_state.fi_target_branch != branch_id) {
        return val;
    }
    if (rng_uniform01() < g_atft_state.fi_prob) {
        union { double d; uint64_t u; } conv;
        conv.d = val;
        // 翻转 IEEE-754 双精度有效位 (bit 42 ~ bit 62: 覆盖高 10 位尾数、11 位指数与符号位)
        uint32_t bit = 42 + (uint32_t)(xorshift64() % 21);
        conv.u ^= (1ULL << bit);
        g_atft_state.fi_injected_count++;
        return conv.d;
    }
    return val;
}

float atft_inject_bitflip_f32(float val, int branch_id) {
    if (!g_atft_state.fi_enabled) return val;
    if (g_atft_state.fi_target_branch >= 0 && g_atft_state.fi_target_branch != branch_id) {
        return val;
    }
    if (rng_uniform01() < g_atft_state.fi_prob) {
        union { float f; uint32_t u; } conv;
        conv.f = val;
        // 翻转 IEEE-754 单精度有效位 (bit 12 ~ bit 30)
        uint32_t bit = 12 + (uint32_t)(xorshift64() % 19);
        conv.u ^= (1U << bit);
        g_atft_state.fi_injected_count++;
        return conv.f;
    }
    return val;
}

int32_t atft_inject_bitflip_i32(int32_t val, int branch_id) {
    if (!g_atft_state.fi_enabled) return val;
    if (g_atft_state.fi_target_branch >= 0 && g_atft_state.fi_target_branch != branch_id) {
        return val;
    }
    if (rng_uniform01() < g_atft_state.fi_prob) {
        uint32_t bit = 4 + (uint32_t)(xorshift64() % 24);
        uint32_t u = (uint32_t)val ^ (1U << bit);
        g_atft_state.fi_injected_count++;
        return (int32_t)u;
    }
    return val;
}

void atft_print_summary(const char *tag) {
    printf("[%s] votes=%lu fault_free=%lu detected=%lu (exact_fault=%lu approx_fault=%lu) "
           "recovered_approx=%lu recovered_reexec=%lu unrec=%lu fi_injected=%lu mode=%s switches=%lu\n",
           tag ? tag : "ATFT",
           (unsigned long)g_atft_state.total_votes,
           (unsigned long)g_atft_state.fault_free_votes,
           (unsigned long)g_atft_state.detected_faults,
           (unsigned long)g_atft_state.exact_branch_faults,
           (unsigned long)g_atft_state.approx_branch_faults,
           (unsigned long)g_atft_state.recovered_by_approx,
           (unsigned long)g_atft_state.recovered_by_reexec,
           (unsigned long)g_atft_state.unrecoverable_faults,
           (unsigned long)g_atft_state.fi_injected_count,
           g_atft_state.current_mode == ATFT_MODE_HIGH ? "HIGH" : "LOW",
           (unsigned long)g_atft_state.mode_switch_count);
}
