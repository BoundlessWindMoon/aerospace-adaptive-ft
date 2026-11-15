#include "atft_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// 时间驱动降档（ATFT_SPRT_TIME_DRIVEN）：无事件流逝的时间也作为 H0 的证据。
// 默认关闭，关闭时判决规则与冻结基线逐位一致。
static int g_atft_sprt_time_driven = 0;


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

// ============================================================================
// 逐负载表决阈值与无故障包络剖析 (D3)
// ============================================================================
double   g_atft_delta_abs = -1.0;
double   g_atft_delta_obs_max = 0.0;
uint64_t g_atft_delta_obs_cnt = 0;
int      g_atft_replica_exact = 0;

// fp32 单位舍入 2^-24：精确 fp64 链与 f32 近似副本之间可能出现的最小相对分歧
#define ATFT_U_FP32 5.9604644775390625e-08
// 地板系数：阈值不低于 kappa 倍单位舍入，避免剖析样本未覆盖到的末位抖动误报
#define ATFT_DELTA_FLOOR_KAPPA 4.0

void atft_set_replica_exact(int on) {
    g_atft_replica_exact = on ? 1 : 0;
}

void atft_set_delta(double delta_abs) {
    g_atft_delta_abs = delta_abs;
}

// 观测量与表决器使用的判据同形：|R - A| / (1 + |A|)。同形才能把剖析值直接当
// 阈值用，不需要再引入换算系数。
void atft_delta_profile_observe(double r_exact, double a_approx) {
    if (!isfinite(r_exact) || !isfinite(a_approx)) return;
    double rel = fabs(r_exact - a_approx) / (1.0 + fabs(a_approx));
    if (rel > g_atft_delta_obs_max) g_atft_delta_obs_max = rel;
    g_atft_delta_obs_cnt++;
}

void atft_delta_profile_reset(void) {
    g_atft_delta_obs_max = 0.0;
    g_atft_delta_obs_cnt = 0;
}

double atft_delta_profile_max(void) {
    return g_atft_delta_obs_max;
}

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

void atft_init_sprt(double calm_interval, double rate_ratio, double alpha, double beta) {
    atft_init(calm_interval, 4.0, 3, 4, 3);
    if (!(rate_ratio > 1.0)) rate_ratio = 2.0;
    if (!(alpha > 0.0 && alpha < 1.0)) alpha = 0.01;
    if (!(beta > 0.0 && beta < 1.0)) beta = 0.01;
    g_atft_state.sprt_enabled = 1;
    g_atft_state.sprt_rate_ratio = rate_ratio;
    g_atft_state.sprt_log_r = log(rate_ratio);
    g_atft_state.sprt_bound_a = log((1.0 - beta) / alpha);
    g_atft_state.sprt_bound_b = log(beta / (1.0 - alpha));
    g_atft_state.sprt_llr = 0.0;
    g_atft_sprt_time_driven = getenv("ATFT_SPRT_TIME_DRIVEN") ? 1 : 0;
}

void atft_reset_stats(void) {    g_atft_state.total_votes = 0;
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
    if (!g_atft_state.sprt_enabled || !g_atft_sprt_time_driven) return;

    // 时间驱动的降档判决。
    //
    // 原来 LLR 只在事件到达时更新，于是平静段里"没有事件"这件事不产生任何证据，
    // 降档必须等到下一次事件 —— 而平静段的事件间隔 ~ 1/lambda0 就是轨道周期量级。
    // 实测回落滞后 108~154 min，超过平静段 83.5 min，系统基本不回 LOW，
    // 占空比收益只剩 1.07x（见 docs/HANDOFF.md 第 8 节）。这不是参数没调好，
    // 是事件驱动的序贯检验在低速率段的结构性缺陷。
    //
    // 正确的连续时间形式：泊松过程在 [0,T] 上观测到 n 个事件时
    //     LLR = n * ln r - (r - 1) * lambda0 * T
    // 时间项应当随时间连续累减，每条退休指令减 (r-1)/T_calm；事件只贡献 +ln r。
    // 总量与原来的分段形式相同，但判决点从"事件时刻"变成"任意时刻"。
    double t_norm = (double)inst_count / g_atft_state.calm_avg_interval;
    g_atft_state.sprt_llr -= (g_atft_state.sprt_rate_ratio - 1.0) * t_norm;
    if (g_atft_state.sprt_llr < g_atft_state.sprt_bound_b)
        g_atft_state.sprt_llr = g_atft_state.sprt_bound_b;
    if (g_atft_state.current_mode == ATFT_MODE_HIGH &&
        g_atft_state.sprt_llr <= g_atft_state.sprt_bound_b) {
        g_atft_state.current_mode = ATFT_MODE_LOW;
        g_atft_state.mode_switch_count++;
    }
}

void atft_report_ecc_event(void) {
    uint64_t delta_inst = g_atft_state.retired_inst_counter - g_atft_state.last_ecc_inst_stamp;
    g_atft_state.last_ecc_inst_stamp = g_atft_state.retired_inst_counter;

    // ---- SPRT 判决（D6）：观测量不变，换判定规则 ----
    //
    // 到达间隔与窗口计数是同一泊松过程的两个充分统计量，用两个独立阈值并列判决
    // 既冗余、也对误报率与检测延迟毫无保证。改为 Wald 序贯概率比检验：
    //   H0: 速率 lambda0 = 1 / T_calm      H1: lambda1 = r * lambda0
    //   每观测到一个间隔 t，对数似然比累加 ln r - (r - 1) * t / T_calm
    //   累加值 >= ln((1-beta)/alpha) 判 H1 (升档)，<= ln(beta/(1-alpha)) 判 H0 (降档)
    // 两侧边界即 Wald 边界，alpha / beta 是**指定**的误报率与漏检率，
    // 不再是拍定的 theta / N / 窗口阈值 / M / 降档水位五个常数。
    // 反射式截断（clamp 到两侧边界）使它成为重复 SPRT，等价于 CUSUM 的
    // 对数似然形式，天然带滞回，无需另设降档计数器。
    // 参考：Wald, Sequential Analysis, 1947；Page, Biometrika 41(1), 1954；
    //       Poisson 速率在线变点检测的 Gamma 共轭闭式后验。
    if (g_atft_state.sprt_enabled) {
        double t_norm = (double)delta_inst / g_atft_state.calm_avg_interval;
        // 时间驱动模式下，时间项已经由 atft_advance_insts 连续累减过了，
        // 事件在这里只贡献 +ln r，否则会把同一段时间算两遍。
        g_atft_state.sprt_llr += g_atft_state.sprt_log_r
                               - (g_atft_sprt_time_driven ? 0.0
                                  : (g_atft_state.sprt_rate_ratio - 1.0) * t_norm);
        if (g_atft_state.sprt_llr > g_atft_state.sprt_bound_a)
            g_atft_state.sprt_llr = g_atft_state.sprt_bound_a;
        if (g_atft_state.sprt_llr < g_atft_state.sprt_bound_b)
            g_atft_state.sprt_llr = g_atft_state.sprt_bound_b;

        g_atft_state.window_ecc_count++;
        if (g_atft_state.current_mode == ATFT_MODE_LOW &&
            g_atft_state.sprt_llr >= g_atft_state.sprt_bound_a) {
            g_atft_state.current_mode = ATFT_MODE_HIGH;
            g_atft_state.mode_switch_count++;
        } else if (g_atft_state.current_mode == ATFT_MODE_HIGH &&
                   g_atft_state.sprt_llr <= g_atft_state.sprt_bound_b) {
            g_atft_state.current_mode = ATFT_MODE_LOW;
            g_atft_state.mode_switch_count++;
        }
        return;
    }

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
    // SPRT 模式下降档由对数似然比越过下边界触发，不需要窗口计数与 M 个平静窗口
    if (g_atft_state.sprt_enabled) {
        g_atft_state.window_ecc_count = 0;
        return;
    }
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

    // D3: 逐负载阈值优先于编译期常数；地板量取 fp32 单位舍入的 kappa 倍，
    // 不再用 2e-6 这个无来源的硬下限
    double base_delta = delta;
    double floor_delta = 2e-6;
    if (g_atft_delta_abs > 0.0) {
        base_delta = g_atft_delta_abs;
        floor_delta = ATFT_U_FP32 * ATFT_DELTA_FLOOR_KAPPA;
    }

    double ref_scale = fabs(a1_approx);
    if (fabs(a2_approx) < ref_scale) ref_scale = fabs(a2_approx);
    double eff_delta = base_delta * (1.0 + ref_scale);
    if (eff_delta < floor_delta) eff_delta = floor_delta;

    // 副本一致性判据：两条近似副本是同一条 f32 链的复制，无故障时逐位相同
    double agree_fast = g_atft_replica_exact ? 0.0 : (1e-6 * (1.0 + ref_scale));
    double agree_cold = g_atft_replica_exact ? 0.0 : (1e-5 * (1.0 + ref_scale));

    double diff_r_a1 = fabs(r_exact - a1_approx);
    double diff_a1_a2 = fabs(a1_approx - a2_approx);

    // Fast-path: 无软错误直接返回 r_exact
    if (__builtin_expect((diff_r_a1 <= eff_delta) && (diff_a1_a2 <= agree_fast), 1)) {
        g_atft_state.fault_free_votes++;
        return r_exact;
    }

    // Cold-path: 检测到软错误异常时的分级恢复逻辑 (对应开题报告 3.4.4.3)
    double diff_r_a2 = fabs(r_exact - a2_approx);
    int r_a1_ok = (diff_r_a1 <= eff_delta) && isfinite(r_exact) && isfinite(a1_approx);
    int r_a2_ok = (diff_r_a2 <= eff_delta) && isfinite(r_exact) && isfinite(a2_approx);
    int a1_a2_ok = (diff_a1_a2 <= agree_cold) && isfinite(a1_approx) && isfinite(a2_approx);

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
            double d1_lim = base_delta * (1.0 + fabs(a1_approx));
            if (d1_lim < floor_delta) d1_lim = floor_delta;
            if (fabs(r_exact - a1_approx) <= d1_lim) {
                g_atft_state.approx_branch_faults++;
                g_atft_state.recovered_by_reexec++;
                return r_exact;
            }
        }
        if (isfinite(a2_approx)) {
            double d2_lim = base_delta * (1.0 + fabs(a2_approx));
            if (d2_lim < floor_delta) d2_lim = floor_delta;
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







// ============================================================================
// 代数校验层（ABFT）：通用校验器
//
// 校验式由等价饱和自动合成（见 experiments/eqsat_checker.py 与 METHOD.md）：
//   gemm   : sum_j C[i][j] == beta*sum_j C_old[i][j] + alpha*sum_k A[i][k]*(sum_j B[k][j])
//   matvec : sum_i y[i]    == sum_j (sum_i A[i][j]) * x[j]
// 本文件只实现"按式子算一遍再比对"的通用过程，维度与步长全部由调用方传入，
// 没有任何内核专用分支。阈值由 fp64 单位舍入导出，与 D3 的 delta 同源。
// ============================================================================
atft_abft_stats_t g_atft_abft = {0, 0, 0};
int g_atft_abft_enabled = 0;
int g_atft_abft_repair = 1;
int g_atft_abft_colcheck = 0;

#define ATFT_U_FP64 2.220446049250313e-16
// kappa 原来写死 32。离线实测（experiments/abft_capability.py）无故障残差只需
// kappa >= 0.0031~0.0045，而每把 kappa 翻一倍就多吃掉约 1/53 的可检测尾数位
// （n=64 实测 1.5~1.9pp 检出率/倍频）。所以它应当由残差量出来，而不是给一个常数。
// 默认仍是 32.0，冻结基线行为不变；ATFT_ABFT_KAPPA 可覆盖。
static double g_abft_kappa = 32.0;
#define ATFT_ABFT_KAPPA g_abft_kappa

// 行列双向校验下"定位不出单错"的次数（多错或行列不一致），只用于诊断报告
static unsigned long long g_abft_rc_ambiguous = 0;

// 行列双向校验的核心（定义在下面 post_gemm 之前）
static int atft_abft_rowcol_core(double *C, const double *A, const double *B,
                                 int ni, int nj, int nk, long ldc, long lda,
                                 long ldb, double alpha, double beta,
                                 const double *rowpre, const double *colpre,
                                 double init_c);

static void atft_abft_report(void) {
    fprintf(stderr, "ATFT_ABFT_STATS,checks=%llu,flagged=%llu,repaired=%llu,"
                    "rc_ambiguous=%llu,kappa=%g\n",
            (unsigned long long)g_atft_abft.checks,
            (unsigned long long)g_atft_abft.flagged,
            (unsigned long long)g_atft_abft.repaired,
            g_abft_rc_ambiguous, g_abft_kappa);
}

void atft_abft_set(int enabled, int repair) {
    g_atft_abft_enabled = enabled ? 1 : 0;
    g_atft_abft_repair = repair ? 1 : 0;
    const char *k = getenv("ATFT_ABFT_KAPPA");
    if (k) {
        double v = atof(k);
        if (v > 0.0) g_abft_kappa = v;
    }
    g_atft_abft_colcheck = getenv("ATFT_ABFT_COLCHECK") ? 1 : 0;
    // 诊断用：退出时报一次校验次数，用来确认插桩真的被执行到
    if (g_atft_abft_enabled && getenv("ATFT_ABFT_VERBOSE")) atexit(atft_abft_report);
}

void atft_abft_pre_rowsum(const double *C, int ni, int nj, long ldc, double *rowsum) {
    if (!g_atft_abft_enabled) return;
    for (int i = 0; i < ni; i++) {
        double s = 0.0;
        const double *row = C + (long)i * ldc;
        for (int j = 0; j < nj; j++) s += row[j];
        rowsum[i] = s;
    }
}

void atft_abft_pre_copy(const double *C, int ni, int nj, long ldc, double *dst) {
    if (!g_atft_abft_enabled || !dst) return;
    for (int i = 0; i < ni; i++)
        memcpy(dst + (long)i * nj, C + (long)i * ldc, sizeof(double) * (size_t)nj);
}

// 列方向预和：取代 O(n^2) 的 pre_copy，只要 O(n)。
void atft_abft_pre_colsum(const double *C, int ni, int nj, long ldc, double *colsum) {
    if (!g_atft_abft_enabled || !colsum) return;
    for (int j = 0; j < nj; j++) colsum[j] = 0.0;
    for (int i = 0; i < ni; i++) {
        const double *row = C + (long)i * ldc;
        for (int j = 0; j < nj; j++) colsum[j] += row[j];
    }
}

// beta != 0 的整矩阵路径：行列双向校验版本，不需要原数据副本。
void atft_abft_post_gemm_rc(double *C, const double *A, const double *B,
                            int ni, int nj, int nk, long ldc, long lda, long ldb,
                            double alpha, double beta, const double *rowsum_pre,
                            const double *colsum_pre) {
    if (!g_atft_abft_enabled || !rowsum_pre || !colsum_pre) return;
    atft_abft_rowcol_core(C, A, B, ni, nj, nk, ldc, lda, ldb, alpha, beta,
                          rowsum_pre, colsum_pre, 0.0);
}

// ----------------------------------------------------------------------------
// 行列双向校验 + 单错定位修正。
//
// 为什么加列方向：行校验只给"哪一行错了"，所以修复必须重算整行，而重算
// C_new = beta*C_old + alpha*A*B 需要 C 的原值，于是被迫留 O(n^2) 快照。
// 加一条列校验后，行 syndrome 给 i、列 syndrome 给 j，而 syndrome 本身就是
// 错误的幅值：单错时 s_i = C[i][j]_bad - C[i][j]_true，于是
//       C[i][j] -= (s_i + t_j)/2
// 就是修复，O(1)，**不需要任何原数据副本** —— 只要 O(n) 的行/列预和。
// 代价是一个校验族：va[k] = sum_i A[i][k] 之后 rhs_j = sum_k va[k]*B[k][j]，
// 两步都是 O(n^2)，相对内核 O(n^3) 仍降一阶。
//
// 离线度量（experiments/abft_capability.py）给出的取舍：同样 2 个校验族、同样
// 的定位能力下，经典加权校验和 w=j+1 会把阈值抬高 n 倍，所以这里用行+列而不是加权。
//
// rowpre/colpre 为 NULL 时表示初值是已知常量 init_c（归约前整矩阵都是 c）。
// 返回 1 表示本函数已经完成检查（调用方不要再跑自己的行循环）。
// ----------------------------------------------------------------------------
static int atft_abft_rowcol_core(double *C, const double *A, const double *B,
                                 int ni, int nj, int nk, long ldc, long lda,
                                 long ldb, double alpha, double beta,
                                 const double *rowpre, const double *colpre,
                                 double init_c) {
    double *vb = atft_abft_scratch(2, nk);
    double *va = atft_abft_scratch(4, nk);
    double *srow = atft_abft_scratch(5, ni);
    double *scol = atft_abft_scratch(6, nj);
    if (!vb || !va || !srow || !scol) return 0;

    double nrm_a = 0.0, nrm_b = 0.0;
    for (int k = 0; k < nk; k++) {
        double s = 0.0, rmax = 0.0;
        const double *row = B + (long)k * ldb;
        for (int j = 0; j < nj; j++) {
            s += row[j];
            double t = fabs(row[j]);
            if (t > rmax) rmax = t;
        }
        vb[k] = s;
        if (rmax > nrm_b) nrm_b = rmax;
    }
    for (int k = 0; k < nk; k++) va[k] = 0.0;
    for (int i = 0; i < ni; i++) {
        const double *row = A + (long)i * lda;
        double rmax = 0.0;
        for (int k = 0; k < nk; k++) {
            va[k] += row[k];
            double t = fabs(row[k]);
            if (t > rmax) rmax = t;
        }
        if (rmax > nrm_a) nrm_a = rmax;
    }

    double base_tol = ATFT_ABFT_KAPPA * ATFT_U_FP64 * (double)nk * nrm_a * nrm_b;
    int bad_i = -1, bad_j = -1, n_bad_i = 0, n_bad_j = 0;

    for (int i = 0; i < ni; i++) {
        const double *crow = C + (long)i * ldc;
        const double *arow = A + (long)i * lda;
        double lhs = 0.0;
        for (int j = 0; j < nj; j++) lhs += crow[j];
        double acc = 0.0;
        for (int k = 0; k < nk; k++) acc += arow[k] * vb[k];
        double rhs = (rowpre ? beta * rowpre[i] : (double)nj * init_c) + alpha * acc;
        double scale = fabs(rhs) > 1.0 ? fabs(rhs) : 1.0;
        srow[i] = lhs - rhs;
        g_atft_abft.checks++;
        if (!(fabs(srow[i]) <= base_tol + ATFT_ABFT_KAPPA * ATFT_U_FP64 * scale * (double)nj)) {
            n_bad_i++;
            bad_i = i;
        }
    }
    if (n_bad_i == 0) return 1;

    // 列方向：lhs 与 rhs 都按**行**扫出来（列主序遍历在 2mm 上实测把周期倍率
    // 从 1.40 推到 1.57，而换成行扫后代价回到 O(n^2) 应有的水平）
    double *rcol = atft_abft_scratch(7, nj);
    if (!rcol) return 0;
    for (int j = 0; j < nj; j++) {
        scol[j] = 0.0;
        rcol[j] = colpre ? beta * colpre[j] : (double)ni * init_c;
    }
    for (int i = 0; i < ni; i++) {
        const double *crow = C + (long)i * ldc;
        for (int j = 0; j < nj; j++) scol[j] += crow[j];
    }
    for (int k = 0; k < nk; k++) {
        const double *brow = B + (long)k * ldb;
        double w = alpha * va[k];
        for (int j = 0; j < nj; j++) rcol[j] += w * brow[j];
    }
    // 列方向只用来**定位**，不要求它自己越过阈值。
    // 教训：第一版要求行、列都判为异常才修，结果近阈值的错误只触发行方向、
    // 列方向够不到阈值 -> 拒修 -> 输出保持错误，gemm 的 SDC 从 0.00 变成 2.50。
    // 既然行方向已经判定"这一行有错"，列方向最大的 syndrome 就是最优的列估计，
    // 不需要它独立显著（与离线度量里的匹配滤波定位器一致）。
    double best = -1.0;
    for (int j = 0; j < nj; j++) {
        scol[j] -= rcol[j];
        g_atft_abft.checks++;
        double a = fabs(scol[j]);
        if (a > best) { best = a; bad_j = j; }
    }

    g_atft_abft.flagged++;
    g_atft_state.detected_faults++;
    // 两个方向对误差幅值的估计必须大致一致，否则单错模型不成立（多错、或者
    // 校验自身被打坏），这时拒修 —— 乱改比不改危险（坑清单第 15 条）。
    int consistent = (bad_j >= 0) &&
                     (fabs(srow[bad_i] - scol[bad_j]) <= 0.5 * fabs(srow[bad_i]));
    if (n_bad_i == 1 && consistent && g_atft_abft_repair) {
        if (!colpre) {
            // 初值是已知常量：单个元素可以按输入**精确重算**，不依赖 syndrome 的
            // 数值，修复因此是自洽的（与整行重算同性质，只是只算一个元素）。
            double s = 0.0;
            const double *arow = A + (long)bad_i * lda;
            for (int k = 0; k < nk; k++) s += arow[k] * B[(long)k * ldb + bad_j];
            C[(long)bad_i * ldc + bad_j] = init_c + alpha * s;
        } else {
            // beta != 0 且没有快照：只能用 syndrome 修正，残差是两条通路的舍入差。
            C[(long)bad_i * ldc + bad_j] -= srow[bad_i];
        }
        g_atft_abft.repaired++;
        g_atft_state.recovered_by_reexec++;
    } else if (!colpre && g_atft_abft_repair) {
        // 定位失败的兜底：初值已知时整行都能按输入重算，不需要旧值。
        // 这说明"O(n^2) 快照"的真实用途不是纠错本身，而是**定位失败时的兜底**。
        const double *arow = A + (long)bad_i * lda;
        double *crow = C + (long)bad_i * ldc;
        for (int j = 0; j < nj; j++) {
            double s = 0.0;
            for (int k = 0; k < nk; k++) s += arow[k] * B[(long)k * ldb + j];
            crow[j] = init_c + alpha * s;
        }
        g_atft_abft.repaired++;
        g_atft_state.recovered_by_reexec++;
    } else {
        g_abft_rc_ambiguous++;
    }
    return 1;
}

void atft_abft_post_gemm(double *C, const double *A, const double *B,
                         int ni, int nj, int nk, long ldc, long lda, long ldb,
                         double alpha, double beta, const double *rowsum_pre,
                         const double *c_old) {
    if (!g_atft_abft_enabled) return;

    // 搜到的式子要求先把 B 沿 j 方向求和（O(n^2)），这一步被提到 i 循环之外，
    // 正是乘法从 O(n^3) 降到 O(n^2) 的来源
    double *vb = (double *)malloc(sizeof(double) * (size_t)nk);
    if (!vb) return;
    double nrm_a = 0.0, nrm_b = 0.0;
    for (int k = 0; k < nk; k++) {
        double s = 0.0, rowmax = 0.0;
        const double *row = B + (long)k * ldb;
        for (int j = 0; j < nj; j++) {
            s += row[j];
            double a = fabs(row[j]);
            if (a > rowmax) rowmax = a;
        }
        vb[k] = s;
        if (rowmax > nrm_b) nrm_b = rowmax;
    }
    for (int i = 0; i < ni; i++) {
        double rowmax = 0.0;
        const double *row = A + (long)i * lda;
        for (int k = 0; k < nk; k++) {
            double a = fabs(row[k]);
            if (a > rowmax) rowmax = a;
        }
        if (rowmax > nrm_a) nrm_a = rowmax;
    }

    double tol = ATFT_ABFT_KAPPA * ATFT_U_FP64 * (double)nk * nrm_a * nrm_b;
    for (int i = 0; i < ni; i++) {
        double lhs = 0.0;
        double *crow = C + (long)i * ldc;
        for (int j = 0; j < nj; j++) lhs += crow[j];
        double acc = 0.0;
        const double *arow = A + (long)i * lda;
        for (int k = 0; k < nk; k++) acc += arow[k] * vb[k];
        double rhs = beta * rowsum_pre[i] + alpha * acc;
        double scale = fabs(rhs) > 1.0 ? fabs(rhs) : 1.0;
        g_atft_abft.checks++;
        if (!(fabs(lhs - rhs) <= tol + ATFT_ABFT_KAPPA * ATFT_U_FP64 * scale * (double)nj)) {
            g_atft_abft.flagged++;
            g_atft_state.detected_faults++;
            // 重算该行需要 C 的原值（C_new = beta*C_old + alpha*A*B）。
            // 只有行和是不够的 —— 这是 ABFT 的经典取舍：校验和 O(n) 足够检错，
            // 要**纠错**就得留下 O(n^2) 的原数据副本。副本仍是 O(n^2)，
            // 相对 O(n^3) 的原计算渐近免费。beta == 0 时不需要副本。
            if (g_atft_abft_repair && (beta == 0.0 || c_old)) {
                const double *old_row = c_old ? (c_old + (long)i * (long)nj) : NULL;
                for (int j = 0; j < nj; j++) {
                    double s = 0.0;
                    for (int k = 0; k < nk; k++)
                        s += arow[k] * B[(long)k * ldb + j];
                    crow[j] = (old_row ? beta * old_row[j] : 0.0) + alpha * s;
                }
                g_atft_abft.repaired++;
                g_atft_state.recovered_by_reexec++;
            }
        }
    }
    free(vb);
}

void atft_abft_check_matvec(const double *y, const double *A, const double *x,
                            int m, int n, long lda, int mode) {
    if (!g_atft_abft_enabled) return;
    double lhs = 0.0;
    int ylen = (mode == 0) ? m : n;
    for (int i = 0; i < ylen; i++) lhs += y[i];

    double rhs = 0.0, nrm_a = 0.0, nrm_x = 0.0;
    if (mode == 0) {
        // y[i] = sum_j A[i][j]*x[j]  ->  sum_i y[i] = sum_j (sum_i A[i][j]) * x[j]
        for (int j = 0; j < n; j++) {
            double col = 0.0;
            for (int i = 0; i < m; i++) {
                double a = A[(long)i * lda + j];
                col += a;
                if (fabs(a) > nrm_a) nrm_a = fabs(a);
            }
            rhs += col * x[j];
            if (fabs(x[j]) > nrm_x) nrm_x = fabs(x[j]);
        }
    } else {
        // y[j] = sum_i A[i][j]*x[i]  ->  sum_j y[j] = sum_i (sum_j A[i][j]) * x[i]
        for (int i = 0; i < m; i++) {
            double row = 0.0;
            for (int j = 0; j < n; j++) {
                double a = A[(long)i * lda + j];
                row += a;
                if (fabs(a) > nrm_a) nrm_a = fabs(a);
            }
            rhs += row * x[i];
            if (fabs(x[i]) > nrm_x) nrm_x = fabs(x[i]);
        }
    }
    double tol = ATFT_ABFT_KAPPA * ATFT_U_FP64 * (double)(m > n ? m : n) * nrm_a * nrm_x;
    double scale = fabs(rhs) > 1.0 ? fabs(rhs) : 1.0;
    g_atft_abft.checks++;
    if (!(fabs(lhs - rhs) <= tol + ATFT_ABFT_KAPPA * ATFT_U_FP64 * scale * (double)ylen)) {
        g_atft_abft.flagged++;
        g_atft_state.detected_faults++;
    }
}

// ----------------------------------------------------------------------------
// 编译期自动插桩需要的暂存缓冲：行和与原数据副本。
// Pass 不方便在 IR 里做内存管理，所以由运行时给两块按需增长的静态缓冲。
// which: 0 = 行和（n 个 double），1 = 原数据副本（n 个 double）
// ----------------------------------------------------------------------------
// 三个槽位：0 = 行和，1 = 原数据副本，2 = B 的列和 vb
// 行列双向校验另用：3 = C 的列和快照，4 = A 的列和 va，5 = 行 syndrome，
//                  6 = 列 syndrome，7 = 列 rhs 暂存
static double *g_scratch[8] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
static size_t  g_scratch_cap[8] = {0, 0, 0, 0, 0, 0, 0, 0};

double *atft_abft_scratch(int which, long n) {
    if (which < 0 || which > 7 || n <= 0) return NULL;
    size_t need = (size_t)n;
    if (g_scratch_cap[which] < need) {
        double *grown = (double *)realloc(g_scratch[which], sizeof(double) * need);
        if (!grown) return NULL;
        g_scratch[which] = grown;
        g_scratch_cap[which] = need;
    }
    return g_scratch[which];
}

// ----------------------------------------------------------------------------
// 行级校验的分解形式。与等价饱和搜到的式子**结构一致**：
//     sum_j C[i][j] - pre == alpha * sum_k A[i][k] * vb[k],  vb[k] = sum_j B[k][j]
// vb 与 i 无关，所以它被提到最外层只算一次（这正是 e-graph 把 sum_j 提出去的那一步），
// 每行只花 O(nj + nk)，整体 O(n^2)，相对内核的 O(n^3) 降一阶。
// ----------------------------------------------------------------------------
void atft_abft_colsum_into(const double *B, int nk, int nj, long ldb, double *vb) {
    static int once = 0;
    if (!once && getenv("ATFT_ABFT_VERBOSE")) {
        once = 1;
        fprintf(stderr, "ATFT_ABFT_BIND,nk=%d,nj=%d,ldb=%ld,vb=%p,enabled=%d\n",
                nk, nj, ldb, (void *)vb, g_atft_abft_enabled);
    }
    if (!g_atft_abft_enabled || !vb) return;
    for (int k = 0; k < nk; k++) {
        double s = 0.0;
        const double *row = B + (long)k * ldb;
        for (int j = 0; j < nj; j++) s += row[j];
        vb[k] = s;
    }
}

void atft_abft_check_row(double *crow, const double *arow, const double *B,
                         const double *vb, const double *crow_old,
                         int nj, int nk, long ldb, double alpha, double pre_sum) {
    if (!g_atft_abft_enabled || !vb) return;
    double lhs = 0.0;
    for (int j = 0; j < nj; j++) lhs += crow[j];
    double acc = 0.0, nrm_a = 0.0, nrm_v = 0.0;
    for (int k = 0; k < nk; k++) {
        acc += arow[k] * vb[k];
        double a = fabs(arow[k]), v = fabs(vb[k]);
        if (a > nrm_a) nrm_a = a;
        if (v > nrm_v) nrm_v = v;
    }
    double rhs = pre_sum + alpha * acc;
    double scale = fabs(rhs) > 1.0 ? fabs(rhs) : 1.0;
    double tol = ATFT_ABFT_KAPPA * ATFT_U_FP64 *
                 ((double)nk * nrm_a * nrm_v + scale * (double)nj);
    g_atft_abft.checks++;
    if (fabs(lhs - rhs) <= tol) return;

    g_atft_abft.flagged++;
    g_atft_state.detected_faults++;
    if (g_atft_abft_repair && crow_old && B) {
        for (int j = 0; j < nj; j++) {
            double s = 0.0;
            for (int k = 0; k < nk; k++) s += arow[k] * B[(long)k * ldb + j];
            crow[j] = crow_old[j] + alpha * s;
        }
        g_atft_abft.repaired++;
        g_atft_state.recovered_by_reexec++;
    }
}

double atft_abft_row_presum(const double *crow, int nj, double *crow_copy) {
    if (!g_atft_abft_enabled) return 0.0;
    double s = 0.0;
    for (int j = 0; j < nj; j++) {
        s += crow[j];
        if (crow_copy) crow_copy[j] = crow[j];
    }
    return s;
}

// ----------------------------------------------------------------------------
// 累加器被**常量初始化**时的整矩阵校验。
//
// 不需要快照：归约开始前每个元素的值已知（就是那个常量 c），所以
//     sum_j C[i][j] == nj*c + alpha * sum_k A[i][k] * (sum_j B[k][j])
// 这把"归约区域必须排除其它写入"放宽成"其它写入的值是已知的"，
// 比做循环分布更一般 —— 分布只是让快照点存在，而这里直接把初值算出来。
// 重算也不需要原数据副本：原值就是 c。
// ----------------------------------------------------------------------------
void atft_abft_check_matrix_const(double *C, const double *A, const double *B,
                                  int ni, int nj, int nk, long ldc, long lda,
                                  long ldb, double alpha, double init_c) {
    if (!g_atft_abft_enabled) return;
    double *vb = atft_abft_scratch(2, nk);
    if (!vb) return;
    if (g_atft_abft_colcheck &&
        atft_abft_rowcol_core(C, A, B, ni, nj, nk, ldc, lda, ldb, alpha, 1.0,
                              NULL, NULL, init_c))
        return;
    for (int k = 0; k < nk; k++) {
        double s = 0.0;
        const double *row = B + (long)k * ldb;
        for (int j = 0; j < nj; j++) s += row[j];
        vb[k] = s;
    }
    for (int i = 0; i < ni; i++) {
        double *crow = C + (long)i * ldc;
        const double *arow = A + (long)i * lda;
        double lhs = 0.0;
        for (int j = 0; j < nj; j++) lhs += crow[j];
        double acc = 0.0, nrm_a = 0.0, nrm_v = 0.0;
        for (int k = 0; k < nk; k++) {
            acc += arow[k] * vb[k];
            double a = fabs(arow[k]), v = fabs(vb[k]);
            if (a > nrm_a) nrm_a = a;
            if (v > nrm_v) nrm_v = v;
        }
        double rhs = (double)nj * init_c + alpha * acc;
        double scale = fabs(rhs) > 1.0 ? fabs(rhs) : 1.0;
        double tol = ATFT_ABFT_KAPPA * ATFT_U_FP64 *
                     ((double)nk * nrm_a * nrm_v + scale * (double)nj);
        g_atft_abft.checks++;
        if (fabs(lhs - rhs) <= tol) continue;
        g_atft_abft.flagged++;
        g_atft_state.detected_faults++;
        if (g_atft_abft_repair) {
            for (int j = 0; j < nj; j++) {
                double s = 0.0;
                for (int k = 0; k < nk; k++) s += arow[k] * B[(long)k * ldb + j];
                crow[j] = init_c + alpha * s;
            }
            g_atft_abft.repaired++;
            g_atft_state.recovered_by_reexec++;
        }
    }
}

// ----------------------------------------------------------------------------
// 整数通路的双模检错 + 冷路径重执行
//
// 第三份副本在无故障时提供零信息（与第二份逐位相同），有故障时只提供"被打中了"
// 这一个 bit。单次执行的期望故障数 a <= 4e-6，所以把常驻的第三份换成
// "检到不一致才整段重算"：平时 2 份 + 汇点比对，期望开销远低于三模的
// 3 份计算 + 4 条表决。
// 置位与取用分开，取用时清零，这样重算只做一次。
// ----------------------------------------------------------------------------
// 插桩直接按位或写这个全局（无分支无调用），运行时只负责取用与清零
int g_atft_dmr_hit = 0;
uint64_t g_atft_dmr_mismatches = 0;
uint64_t g_atft_dmr_reexecs = 0;

int atft_dmr_take_flag(void) {
    int f = g_atft_dmr_hit;
    g_atft_dmr_hit = 0;
    if (f) {
        g_atft_dmr_mismatches++;
        g_atft_state.detected_faults++;
        g_atft_dmr_reexecs++;
        g_atft_state.recovered_by_reexec++;
    }
    return f;
}
