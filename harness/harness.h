#ifndef ATFT_HARNESS_H
#define ATFT_HARNESS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// 独立评测裁判层 (Standalone Evaluation Referee / Harness)
// 与被测容错策略 (Policy) 完全解耦：
//   1. 硬件 PMU 性能计数器 (Linux perf_event_open):
//      直接读取 CPU 硬件退休指令数 (PERF_COUNT_HW_INSTRUCTIONS) 与
//      CPU 周期数 (PERF_COUNT_HW_CPU_CYCLES)。
//   2. 标准两阶段单粒子翻转故障注入 (LLFI / SASSIFI One-Fault-Per-Run 标准):
//      - Phase 1 (Profiling): 统计一次无故障运行中目标动态指令总数 N_dyn
//      - Phase 2 (Injection): 均匀随机抽取动态指令步 target_step ∈ [1, N_dyn]
//                             与全位宽比特位置 target_bit ∈ [0, bitwidth-1]，
//                             精确执行单次单比特翻转 (Single-Bit Flip)。
//   3. 标准输出质量 (QoR) 与故障分类判分器:
//      - Masked (完全掩盖: 输出与 Golden 误差 <= 1e-12 且未触发恢复)
//      - Detected & Recovered (检出并成功恢复: 触发投票恢复且输出满足应用 QoR)
//      - Benign Approximation (良性误差: 落在应用容忍阈值内)
//      - SDC (Silent Data Corruption: 超出应用误差容忍界或出现 NaN/Inf)
//      - DUE (Detected Unrecoverable Error: 检出但无法恢复或崩溃)
// ============================================================================

typedef struct {
    int fd_insts;
    int fd_cycles;
} harness_pmu_t;

// 硬件 PMU 计数器接口
void     harness_pmu_init(harness_pmu_t *pmu);
void     harness_pmu_start(harness_pmu_t *pmu);
void     harness_pmu_stop(harness_pmu_t *pmu, uint64_t *out_insts, uint64_t *out_cycles);
void     harness_pmu_close(harness_pmu_t *pmu);

// 独立后置故障注入 Pass 调用的钩子函数 (被测策略对此一无所知)
void     harness_fi_reset_profile(void);
uint64_t harness_fi_get_total_dyn_insts(void);
void     harness_fi_arm_one_fault(uint64_t target_step, uint32_t target_bit);
int      harness_fi_was_injected(void);

double   __harness_fi_hook_f64(double val, int32_t static_site_id);
float    __harness_fi_hook_f32(float val, int32_t static_site_id);
int32_t  __harness_fi_hook_i32(int32_t val, int32_t static_site_id);
int16_t  __harness_fi_hook_i16(int16_t val, int32_t static_site_id);

// 标准 QoR 误差评估指标 (MRE, Relative L2, Max Abs Error, PSNR)
typedef struct {
    int    has_nan_or_inf;
    double rel_l2_err;
    double mean_rel_err;
    double max_abs_err;
    double psnr_db;
} harness_qor_metrics_t;

void harness_compute_qor(int n, const double *golden, const double *test,
                         double peak_val, harness_qor_metrics_t *out_qor);

#ifdef __cplusplus
}
#endif

#endif // ATFT_HARNESS_H
