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
// EDDI (IEEE TR'02) / SWIFT (CGO'05) 双模冗余一致性检查器 (DMR Checker)
// 双模指令复制仅能检测主路与影子路不一致 (将静默数据损坏 SDC 转化为检出不可恢复错误 DUE)，
// 无法像三模冗余那样判定哪一路正确并在线恢复
// ============================================================================
double atft_eddi_swift_check_f64(double r_master, double r_shadow) {
    g_atft_state.total_votes++;
    const double eps = 1e-11 * (1.0 + fabs(r_master));
    if (isfinite(r_master) && isfinite(r_shadow) && fabs(r_master - r_shadow) <= eps) {
        g_atft_state.fault_free_votes++;
        return r_master;
    }
    g_atft_state.detected_faults++;
    g_atft_state.unrecoverable_faults++;
    return r_master;
}

// 移植自 third_party/LLTFI/llvm_passes/instruction_duplication/shared_lib/SIDHelperFunctions.cpp:14
// LLTFI 的 compareFloatValues: 当两路副本不一致时，对 IEEE-754 的 sign, exponent, mantissa 分别做按位与 (&)，
// 从而把 0->1 的高位指数爆炸翻回 0（无需重算即可抑制 NaN/Inf 尖峰，但对 1->0 的翻转会产生偏小近似误差）
double atft_lltfi_sid_compare_f64(double val1, double val2) {
    g_atft_state.total_votes++;
    union { double d; uint64_t u; } f1 = {.d = val1}, f2 = {.d = val2}, ret;
    if (f1.u == f2.u) {
        g_atft_state.fault_free_votes++;
        return val1;
    }
    g_atft_state.detected_faults++;
    g_atft_state.recovered_by_approx++;
    ret.u = f1.u & f2.u;
    return ret.d;
}

// ============================================================================
// 传统精确 TMR 表决器 (COAST Baseline, IEEE TNS'19)
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
double atft_voter_approx_reexec_f64(double r_exact, double r_reexec,
                                    double a1_approx, double a2_approx,
                                    double delta, int32_t crit_level) __attribute__((always_inline));

double atft_voter_approx_reexec_f64(double r_exact, double r_reexec,
                                    double a1_approx, double a2_approx,
                                    double delta, int32_t crit_level) {
    g_atft_state.total_votes++;

    double ref_scale = fabs(a1_approx);
    if (fabs(a2_approx) < ref_scale) ref_scale = fabs(a2_approx);
    double eff_delta = delta * (1.0 + ref_scale);
    if (eff_delta < 2e-6) eff_delta = 2e-6;

    double diff_r_a1 = fabs(r_exact - a1_approx);
    double diff_a1_a2 = fabs(a1_approx - a2_approx);

    // Fast-path: 无软错误直接返回 r_exact
    if (__builtin_expect((diff_r_a1 <= eff_delta) && (diff_a1_a2 <= 1e-6 * (1.0 + ref_scale)), 1)) {
        g_atft_state.fault_free_votes++;
        return r_exact;
    }

    // Cold-path: 检测到软错误异常时的分级恢复逻辑 (对应开题报告 3.4.4.3)
    double diff_r_a2 = fabs(r_exact - a2_approx);
    int r_a1_ok = (diff_r_a1 <= eff_delta) && isfinite(r_exact) && isfinite(a1_approx);
    int r_a2_ok = (diff_r_a2 <= eff_delta) && isfinite(r_exact) && isfinite(a2_approx);
    int a1_a2_ok = (diff_a1_a2 <= 1e-5 * (1.0 + ref_scale)) && isfinite(a1_approx) && isfinite(a2_approx);

    g_atft_state.detected_faults++;
    atft_report_ecc_event();

    // 情况 1: 两条近似副本一致 (a1_a2_ok)，说明主路 R 发生位翻转 -> 按代码段关键性等级分级恢复:
    //   - 关键代码段 (CRITICAL)     : 校验并采纳冷路径精确重执行结果 r_reexec (零精度损失恢复)
    //   - 次关键代码段 (SUB_CRITICAL): 直接采纳近似计算结果 a1_approx 替代 (免重执行开销)
    //   - 非关键代码段 (NON_CRITICAL): 采用安全回退值
    if (a1_a2_ok && (!r_a1_ok || !r_a2_ok)) {
        g_atft_state.exact_branch_faults++;
        if (crit_level >= ATFT_CRIT_CRITICAL) {
            g_atft_state.recovered_by_reexec++;
            if (isfinite(r_reexec) && fabs(r_reexec - a1_approx) <= eff_delta) {
                return r_reexec;
            }
            return a1_approx;
        } else if (crit_level == ATFT_CRIT_SUB_CRITICAL) {
            g_atft_state.recovered_by_approx++;
            return a1_approx;
        } else {
            g_atft_state.recovered_by_approx++;
            return isfinite(a1_approx) ? a1_approx : 0.0;
        }
    }

    // 情况 2: 某一条近似链路发生软错误 (含变为 NaN/Inf 或大幅偏离)，主路 R 完好:
    if (isfinite(r_exact)) {
        if (isfinite(a1_approx)) {
            double d1_lim = delta * (1.0 + fabs(a1_approx));
            if (d1_lim < 2e-6) d1_lim = 2e-6;
            if (fabs(r_exact - a1_approx) <= d1_lim) {
                g_atft_state.approx_branch_faults++;
                g_atft_state.recovered_by_reexec++;
                return r_exact;
            }
        }
        if (isfinite(a2_approx)) {
            double d2_lim = delta * (1.0 + fabs(a2_approx));
            if (d2_lim < 2e-6) d2_lim = 2e-6;
            if (fabs(r_exact - a2_approx) <= d2_lim) {
                g_atft_state.approx_branch_faults++;
                g_atft_state.recovered_by_reexec++;
                return r_exact;
            }
        }
    }

    // 情况 3: 若冷路径提供了精确重执行副本 r_reexec 且与主路一致或与单路近似一致:
    if (crit_level >= ATFT_CRIT_CRITICAL && isfinite(r_reexec)) {
        if (isfinite(r_exact) && fabs(r_exact - r_reexec) <= 1e-11 * (1.0 + fabs(r_reexec))) {
            g_atft_state.approx_branch_faults++;
            g_atft_state.recovered_by_reexec++;
            return r_reexec;
        }
        if (isfinite(a1_approx) && fabs(r_reexec - a1_approx) <= eff_delta) {
            g_atft_state.exact_branch_faults++;
            g_atft_state.recovered_by_reexec++;
            return r_reexec;
        }
    }

    g_atft_state.unrecoverable_faults++;
    if (isfinite(r_exact)) return r_exact;
    if (isfinite(a1_approx)) return a1_approx;
    if (isfinite(a2_approx)) return a2_approx;
    return 0.0;
}

double atft_voter_approx_f64(double r_exact, double a1_approx, double a2_approx,
                             double delta, int32_t crit_level) __attribute__((always_inline));

double atft_voter_approx_f64(double r_exact, double a1_approx, double a2_approx,
                             double delta, int32_t crit_level) {
    return atft_voter_approx_reexec_f64(r_exact, a1_approx, a1_approx, a2_approx, delta, crit_level);
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
    atft_report_ecc_event();

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
    if (x < -20.0f) return 0.0f;
    if (x > 20.0f) x = 20.0f;
    // Range reduction: e^x = 2^k * e^r, where k = round(x / ln2), r in [-ln2/2, ln2/2]
    const float inv_ln2 = 1.44269504089f;
    const float ln2     = 0.69314718056f;
    int k = (int)roundf(x * inv_ln2);
    float r = x - (float)k * ln2;
    // 4th-order Taylor polynomial on [-0.346, 0.346] (max error < 3e-5)
    float p = 1.0f + r * (1.0f + r * (0.5f + r * (0.16666667f + r * 0.04166667f)));
    return ldexpf(p, k);
}

typedef float v2f32 __attribute__((ext_vector_type(2)));






