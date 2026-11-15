#ifndef ATFT_RUNTIME_H
#define ATFT_RUNTIME_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// 1. 代码段关键性等级 (对应开题报告 3.2 节: 基于语义信息的代码段关键性分析)
// ============================================================================
typedef enum {
    ATFT_CRIT_NON_CRITICAL = 0, // 非关键代码段: 默认值回退/容忍近似
    ATFT_CRIT_SUB_CRITICAL = 1, // 次关键代码段: 1次精确 + 2次近似 (混合TMR), 故障时用近似值替代
    ATFT_CRIT_CRITICAL     = 2  // 关键代码段:   三模冗余保护, 精确链路故障时触发恢复/重算
} atft_crit_level_t;

// ============================================================================
// 2. 运行时保护模式 (对应开题报告 3.3.3 节: 运行时监控与动态决策 LOW / HIGH)
// ============================================================================
typedef enum {
    ATFT_MODE_LOW  = 0, // 辐射平静期: 仅保护关键代码段 / 轻量化近似保护
    ATFT_MODE_HIGH = 1  // 辐射爆发期: 关键与次关键代码段全面开启混合/精确 TMR 保护
} atft_prot_mode_t;

// ============================================================================
// 3. 运行时统计与监控状态结构体
// ============================================================================
typedef struct {
    // 投票与容错统计
    uint64_t total_votes;               // 总投票次数
    uint64_t fault_free_votes;          // 无故障次数 (|R-A1|<=delta && |R-A2|<=delta)
    uint64_t detected_faults;           // 检测到的软错误总数
    uint64_t exact_branch_faults;       // 精确计算链路 R 发生软错误的次数
    uint64_t approx_branch_faults;      // 近似副本链路 A1 或 A2 发生软错误的次数
    uint64_t recovered_by_approx;       // 通过近似结果成功降级恢复的次数
    uint64_t recovered_by_reexec;       // 关键代码段触发重执行/精确恢复的次数
    uint64_t unrecoverable_faults;      // 三路均不一致的未恢复错误数

    // ECC / 软错误事件间隔与滑动窗口监控 (对应开题报告 3.3.3.2 & 3.3.3.3)
    uint64_t retired_inst_counter;      // 模拟退休指令计数器
    uint64_t last_ecc_inst_stamp;       // 上一次 ECC 事件时的退休指令数
    double   calm_avg_interval;         // 平静期平均事件间隔 \bar{T}
    double   theta_ratio;               // 间隔缩短阈值比例 \theta (间隔 < \bar{T}/\theta)
    uint32_t consec_short_intervals;    // 连续短间隔计数 (指标 A: >= N)
    uint32_t metric_a_threshold_n;      // 指标 A 阈值 N
    uint32_t window_ecc_count;          // 当前滑动窗口内的 ECC 事件数 (指标 B)
    uint32_t metric_b_threshold;        // 指标 B 窗口累积阈值
    uint32_t calm_windows_count;        // 连续低于降级阈值的平静窗口数 (用于 HIGH -> LOW 滞回恢复)
    uint32_t downgrade_threshold_m;     // 降级所需连续平静窗口数 M

    atft_prot_mode_t current_mode;      // 当前保护状态 (LOW / HIGH)
    uint64_t mode_switch_count;         // 模式切换次数

    // SPRT 变点检测 (D6)：替掉 theta / N / 窗口阈值 / M / 降档水位五个拍定常数
    int      sprt_enabled;              // 1 = 用序贯概率比检验判决模式切换
    double   sprt_llr;                  // 当前累积对数似然比
    double   sprt_log_r;                // ln(lambda1 / lambda0)
    double   sprt_rate_ratio;           // r = lambda1 / lambda0（由在轨 SEU 速率模型给）
    double   sprt_bound_a;              // 上边界 ln((1-beta)/alpha)，越过即升档
    double   sprt_bound_b;              // 下边界 ln(beta/(1-alpha))，越过即降档

    // 故障注入配置 (对应开题报告 4.3.2 可靠性评估)
    int      fi_enabled;                // 是否开启故障注入
    double   fi_prob;                   // 每次算术链执行触发单比特翻转的概率
    int      fi_target_branch;          // 注入目标: -1=随机分支, 0=精确链R, 1=近似链A1, 2=近似链A2
    uint64_t fi_injected_count;         // 实际注入的单比特翻转次数
} atft_runtime_state_t;

extern atft_runtime_state_t g_atft_state;

// ============================================================================
// 3B. 逐负载表决阈值 (D3)
//
// 阈值原先是编译期全局常数 5e-5，而各负载的固有近似误差跨四个数量级：阈值小于
// 固有误差必然 100% 误报，远大于固有误差则大量漏检。改为运行期按负载给值：
//   delta_f = max(alpha * E_f, kappa * u_f32)
//     E_f    : 无故障剖析实测的 max |R - A| / (1 + |A|) 包络
//     u_f32  : fp32 单位舍入 2^-24，精确链与 f32 近似副本的理论最小分歧量级
//     alpha  : 包络安全系数；kappa: 地板系数。两者都做敏感性消融
// g_atft_delta_abs <= 0 时表决器回落到编译期常数，冻结基线行为不变。
// 思路对应 ApproxTuner (PPoPP'21) 的逐点误差剖面，但不做搜索、不需要校准集：
// 一次无故障运行即可定出，所以能随负载泛化。
// ============================================================================
extern double g_atft_delta_abs;       // >0 时覆盖编译期阈值
extern double g_atft_delta_obs_max;   // 剖析累计：最大相对偏差
extern uint64_t g_atft_delta_obs_cnt; // 剖析累计：观测点数
extern int    g_atft_replica_exact;   // 1 = 要求两条近似副本按位相等

void   atft_delta_profile_observe(double r_exact, double a_approx);
void   atft_delta_profile_reset(void);
double atft_delta_profile_max(void);
void   atft_set_delta(double delta_abs);
void   atft_set_replica_exact(int on);

// ============================================================================
// 4. 运行时 API (供 LLVM Pass 插桩调用及基准测试驱动调用)
// ============================================================================

// 初始化与重置运行时状态
void atft_init(double calm_interval, double theta, uint32_t n_short, uint32_t win_thresh, uint32_t m_calm);

// SPRT 版初始化：alpha / beta 是指定的误报率与漏检率上界，rate_ratio 来自在轨
// SEU 速率模型（SAA 内外速率比）。判决所需事件数 = ln((1-beta)/alpha) /
// (ln r - (r-1)/r)，可在部署前算出，与过境窗口内的期望事件数直接比较。
void atft_init_sprt(double calm_interval, double rate_ratio, double alpha, double beta);

void atft_reset_stats(void);
atft_prot_mode_t atft_get_mode(void);

// 模拟推进退休指令数与上报硬件 ECC/软错误传感器事件 (对应 3.3.3.2)
void atft_advance_insts(uint64_t inst_count);
void atft_report_ecc_event(void);
void atft_step_window(void);

// EDDI (Oh et al., IEEE TR'02) / SWIFT (Reis et al., CGO'05) 双模校验器 (DMR: 仅检错触发 DUE，无法三模自恢复)
double  atft_eddi_swift_check_f64(double r_master, double r_shadow);

// LLTFI-SID (third_party/LLTFI/llvm_passes/instruction_duplication/shared_lib/SIDHelperFunctions.cpp)
// 按位与抑制高位指数尖峰的近似双模修复器
double  atft_lltfi_sid_compare_f64(double val1, double val2);

// 传统精确 TMR 多数表决器 (Baseline: 3次精确计算, COAST IEEE TNS'19)
double  atft_voter_exact_f64(double r1, double r2, double r3);
int32_t atft_voter_exact_i32(int32_t r1, int32_t r2, int32_t r3);

// 混合近似 TMR 动态阈值投票器 (核心创新点 3.4.4: 1次精确 R + 2次近似 A1, A2 + 关键代码段冷路径精确重执行 R_reexec)
// 若 |R - A1| <= delta 且 |R - A2| <= delta -> 返回 R
// 若 |A1 - A2| <= delta 但 |R - A1| > delta -> 判定精确链路故障，按 crit_level 分级恢复:
//   - ATFT_CRIT_CRITICAL     : 采纳冷路径精确重执行结果 r_reexec (零精度损失恢复)
//   - ATFT_CRIT_SUB_CRITICAL : 采纳近似副本结果 a1_approx (免重执行降级替代)
//   - ATFT_CRIT_NON_CRITICAL : 安全回退
double  atft_voter_approx_f64(double r_exact, double a1_approx, double a2_approx,
                              double delta, int32_t crit_level);
double  atft_voter_approx_reexec_f64(double r_exact, double r_reexec,
                                     double a1_approx, double a2_approx,
                                     double delta, int32_t crit_level);
int32_t atft_voter_approx_i32(int32_t r_exact, int32_t a1_approx, int32_t a2_approx,
                              int32_t delta, int32_t crit_level);

// 面向超越函数的低阶多项式近似 (对应 3.4.2.2 函数计算近似)
float atft_approx_sinf(float x);
float atft_approx_cosf(float x);
float atft_approx_expf(float x);


// 单比特翻转故障注入函数 (供 LLVM IR 插桩或实验评估使用, 参考 LLFI/LLTFI)

// ============================================================================
// 5. 代数校验层（ABFT）：由等价饱和合成的校验式对应的通用校验器
//
// 校验式本身由 `-atft-dump-sums` + `experiments/eqsat_checker.py` 自动搜出：
//   矩阵乘  sum_j C[i][j] == beta*sum_j C_old[i][j] + alpha*sum_k A[i][k]*(sum_j B[k][j])
//   矩阵向量 sum_i y[i]   == sum_j (sum_i A[i][j]) * x[j]
// 这里只实现"按搜到的式子算一遍并比对"的通用过程，不含任何内核专用逻辑：
// 维度与步长由调用方传入，判据阈值按数制的单位舍入导出（与 D3 的 delta 同源）。
//
// 代价：校验 O(n^2)，原计算 O(n^3)，所以渐近上近乎免费。
// 判据：|lhs - rhs| <= kappa * u_fp64 * n * ||A||_inf * ||B||_inf
//       （Huang & Abraham 1984 的校验和 + Smith & van de Geijn 2015 的舍入界形式）
// ============================================================================
typedef struct {
    uint64_t checks;        // 执行的校验次数（行数）
    uint64_t flagged;       // 判为异常的行数
    uint64_t repaired;      // 重算修复的行数
} atft_abft_stats_t;

extern atft_abft_stats_t g_atft_abft;
extern int g_atft_abft_enabled;      // 0 = 不做校验（基线行为）
extern int g_atft_abft_repair;       // 1 = 检出后重算该行

void atft_abft_set(int enabled, int repair);

// 矩阵乘：C = beta*C + alpha*A*B，行主序、行步长由调用方给
// pre 在内核前记录 C 的行和，post 在内核后校验（必要时重算整行）
void atft_abft_pre_rowsum(const double *C, int ni, int nj, long ldc, double *rowsum);
// c_old 给 beta != 0 时重算整行所需的原数据副本（可为 NULL：那时只检不纠）
void atft_abft_post_gemm(double *C, const double *A, const double *B,
                         int ni, int nj, int nk, long ldc, long lda, long ldb,
                         double alpha, double beta, const double *rowsum_pre,
                         const double *c_old);
void atft_abft_pre_copy(const double *C, int ni, int nj, long ldc, double *dst);

// 矩阵向量：y[j] (或 tmp[i]) 的全和校验。vec_len 给 y 的长度，
// mode=0 表示 y[i] = sum_j A[i][j]*x[j]，mode=1 表示 y[j] = sum_i A[i][j]*x[i]
void atft_abft_check_matvec(const double *y, const double *A, const double *x,
                            int m, int n, long lda, int mode);

// 打印统计摘要

#ifdef __cplusplus
}
#endif

#endif // ATFT_RUNTIME_H
