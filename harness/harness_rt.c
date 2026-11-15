#include <signal.h>
#define _GNU_SOURCE
#include "harness.h"

#include <linux/perf_event.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>

// ============================================================================
// 1. Linux 硬件性能计数器 (PMU) 封装
// ============================================================================
static long perf_event_open_sys(struct perf_event_attr *hw_event, pid_t pid,
                                int cpu, int group_fd, unsigned long flags) {
    return syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
}

void harness_pmu_init(harness_pmu_t *pmu) {
    struct perf_event_attr pe_inst, pe_cyc;
    memset(&pe_inst, 0, sizeof(pe_inst));
    pe_inst.type = PERF_TYPE_HARDWARE;
    pe_inst.size = sizeof(struct perf_event_attr);
    pe_inst.config = PERF_COUNT_HW_INSTRUCTIONS;
    pe_inst.disabled = 1;
    pe_inst.exclude_kernel = 1;
    pe_inst.exclude_hv = 1;

    memset(&pe_cyc, 0, sizeof(pe_cyc));
    pe_cyc.type = PERF_TYPE_HARDWARE;
    pe_cyc.size = sizeof(struct perf_event_attr);
    pe_cyc.config = PERF_COUNT_HW_CPU_CYCLES;
    pe_cyc.disabled = 1;
    pe_cyc.exclude_kernel = 1;
    pe_cyc.exclude_hv = 1;

    pmu->fd_insts = (int)perf_event_open_sys(&pe_inst, 0, -1, -1, 0);
    pmu->fd_cycles = (int)perf_event_open_sys(&pe_cyc, 0, -1, -1, 0);
}

void harness_pmu_start(harness_pmu_t *pmu) {
    if (pmu->fd_insts >= 0) {
        ioctl(pmu->fd_insts, PERF_EVENT_IOC_RESET, 0);
        ioctl(pmu->fd_insts, PERF_EVENT_IOC_ENABLE, 0);
    }
    if (pmu->fd_cycles >= 0) {
        ioctl(pmu->fd_cycles, PERF_EVENT_IOC_RESET, 0);
        ioctl(pmu->fd_cycles, PERF_EVENT_IOC_ENABLE, 0);
    }
}

void harness_pmu_stop(harness_pmu_t *pmu, uint64_t *out_insts, uint64_t *out_cycles) {
    uint64_t insts = 0, cycs = 0;
    if (pmu->fd_insts >= 0) {
        ioctl(pmu->fd_insts, PERF_EVENT_IOC_DISABLE, 0);
        ssize_t r = read(pmu->fd_insts, &insts, sizeof(insts));
        (void)r;
    }
    if (pmu->fd_cycles >= 0) {
        ioctl(pmu->fd_cycles, PERF_EVENT_IOC_DISABLE, 0);
        ssize_t r = read(pmu->fd_cycles, &cycs, sizeof(cycs));
        (void)r;
    }
    if (out_insts) *out_insts = insts;
    if (out_cycles) *out_cycles = cycs;
}

void harness_pmu_close(harness_pmu_t *pmu) {
    if (pmu->fd_insts >= 0) close(pmu->fd_insts);
    if (pmu->fd_cycles >= 0) close(pmu->fd_cycles);
    pmu->fd_insts = -1;
    pmu->fd_cycles = -1;
}

// ============================================================================
// 2. 独立后置故障注入运行时 (支持 LLFI/SASSIFI One-Fault-Per-Run、Spatial MBU 与 Temporal Burst)
// ============================================================================
#define MAX_BURST_FAULTS 32

static uint64_t g_fi_dyn_step = 0;
static uint64_t g_fi_steps[MAX_BURST_FAULTS];
static uint32_t g_fi_bits[MAX_BURST_FAULTS];
static int      g_fi_num_events = 0;
static uint32_t g_fi_mbu_width = 1; // 1=SEU(单比特), 2=DBU(相邻双比特), 4=QBU(相邻四比特)
static int      g_fi_injected = 0;
// 单次试验的动态指令预算：注错可能让上游内核进入不收敛循环（如 quick_sort 的
// 分区索引被破坏后越界扫描、kf_factor 的 while(n>1) 不再收敛）。只靠 SIGALRM
// 定时器不可靠（实测曾 100% CPU 空转 12 分钟仍未被打断），因此在注错钩子内部
// 直接计数：一旦超出预算就主动 raise(SIGALRM)，由 eval_suite 的保护区归类为
// DUE。钩子位于热循环内部，因此任何执行被插桩指令的死循环都会被捕获。
static uint64_t g_fi_step_budget = 0;
static int g_fi_target_site = -1;

static inline int fi_should_apply(int32_t static_site_id, int event_index) {
    if (g_fi_target_site < 0)
        return g_fi_dyn_step == g_fi_steps[event_index];
    if (static_site_id != g_fi_target_site || g_fi_injected != 0)
        return 0;
    return event_index == 0;
}

void harness_fi_set_step_budget(uint64_t budget) {
    g_fi_step_budget = budget;
}

static inline void fi_check_budget(void) {
    if (g_fi_step_budget && g_fi_dyn_step > g_fi_step_budget) {
        g_fi_step_budget = 0; // 避免在信号处理路径上反复触发
        raise(SIGALRM);
    }
}

void harness_fi_reset_profile(void) {
    const char *target = getenv("ATFT_FI_STATIC_SITE");
    g_fi_target_site = target ? (int)strtol(target, NULL, 10) : -1;
    g_fi_dyn_step = 0;
    g_fi_num_events = 0;
    g_fi_mbu_width = 1;
    g_fi_injected = 0;
    g_fi_step_budget = 0;
}

uint64_t harness_fi_get_total_dyn_insts(void) {
    return g_fi_dyn_step;
}


void harness_fi_arm_mbu_or_burst(const uint64_t *target_steps, const uint32_t *target_bits,
                                 int num_events, uint32_t mbu_width) {
    g_fi_dyn_step = 0;
    if (num_events > MAX_BURST_FAULTS) num_events = MAX_BURST_FAULTS;
    for (int i = 0; i < num_events; i++) {
        g_fi_steps[i] = target_steps[i];
        g_fi_bits[i] = target_bits[i];
    }
    g_fi_num_events = num_events;
    g_fi_mbu_width = (mbu_width >= 1 && mbu_width <= 8) ? mbu_width : 1;
    g_fi_injected = 0;
}


static inline uint64_t make_mbu_mask64(uint32_t base_bit, uint32_t width) {
    uint64_t m = ((1ULL << width) - 1ULL);
    uint32_t shift = base_bit % (65 - width);
    return m << shift;
}

static inline uint32_t make_mbu_mask32(uint32_t base_bit, uint32_t width) {
    uint32_t m = ((1U << width) - 1U);
    uint32_t shift = base_bit % (33 - width);
    return m << shift;
}

double __harness_fi_hook_f64(double val, int32_t static_site_id) {
    (void)static_site_id;
    g_fi_dyn_step++;
    fi_check_budget();
    for (int i = 0; i < g_fi_num_events; i++) {
        if (fi_should_apply(static_site_id, i)) {
            union { double d; uint64_t u; } conv;
            conv.d = val;
            conv.u ^= make_mbu_mask64(g_fi_bits[i], g_fi_mbu_width);
            g_fi_injected++;
            val = conv.d;
        }
    }
    return val;
}

harness_v2f64_t __harness_fi_hook_v2f64(harness_v2f64_t val, int32_t static_site_id) {
    // 一个向量故障点只推进一次动态步；用静态点编号交替选择 lane，
    // 保留 f64 的完整 64-bit 故障域，同时让同一套 harness 覆盖两个 lane。
    int lane = static_site_id & 1;
    g_fi_dyn_step++;
    fi_check_budget();
    for (int i = 0; i < g_fi_num_events; i++) {
        if (fi_should_apply(static_site_id, i)) {
            union { double d; uint64_t u; } conv;
            conv.d = val[lane];
            conv.u ^= make_mbu_mask64(g_fi_bits[i], g_fi_mbu_width);
            g_fi_injected++;
            val[lane] = conv.d;
        }
    }
    return val;
}

float __harness_fi_hook_f32(float val, int32_t static_site_id) {
    (void)static_site_id;
    g_fi_dyn_step++;
    fi_check_budget();
    for (int i = 0; i < g_fi_num_events; i++) {
        if (fi_should_apply(static_site_id, i)) {
            union { float f; uint32_t u; } conv;
            conv.f = val;
            conv.u ^= make_mbu_mask32(g_fi_bits[i], g_fi_mbu_width);
            g_fi_injected++;
            val = conv.f;
        }
    }
    return val;
}

int32_t __harness_fi_hook_i32(int32_t val, int32_t static_site_id) {
    (void)static_site_id;
    g_fi_dyn_step++;
    fi_check_budget();
    for (int i = 0; i < g_fi_num_events; i++) {
        if (fi_should_apply(static_site_id, i)) {
            uint32_t u = (uint32_t)val ^ make_mbu_mask32(g_fi_bits[i], g_fi_mbu_width);
            g_fi_injected++;
            val = (int32_t)u;
        }
    }
    return val;
}

int64_t __harness_fi_hook_i64(int64_t val, int32_t static_site_id) {
    (void)static_site_id;
    g_fi_dyn_step++;
    fi_check_budget();
    for (int i = 0; i < g_fi_num_events; i++) {
        if (fi_should_apply(static_site_id, i)) {
            uint64_t u = (uint64_t)val ^ make_mbu_mask64(g_fi_bits[i], g_fi_mbu_width);
            g_fi_injected++;
            val = (int64_t)u;
        }
    }
    return val;
}

int16_t __harness_fi_hook_i16(int16_t val, int32_t static_site_id) {
    (void)static_site_id;
    g_fi_dyn_step++;
    fi_check_budget();
    for (int i = 0; i < g_fi_num_events; i++) {
        if (fi_should_apply(static_site_id, i)) {
            uint16_t u = (uint16_t)val ^ (uint16_t)(make_mbu_mask32(g_fi_bits[i], g_fi_mbu_width) & 0xFFFFU);
            g_fi_injected++;
            val = (int16_t)u;
        }
    }
    return val;
}

// ============================================================================
// 3. 标准 QoR 输出质量评估器
// ============================================================================
void harness_compute_qor(int n, const double *golden, const double *test,
                         double peak_val, harness_qor_metrics_t *out_qor) {
    memset(out_qor, 0, sizeof(*out_qor));
    double sum_sq_err = 0.0;
    double sum_sq_gold = 0.0;
    double sum_rel_err = 0.0;
    double max_abs = 0.0;

    for (int i = 0; i < n; i++) {
        if (!isfinite(test[i])) {
            out_qor->has_nan_or_inf = 1;
            out_qor->rel_l2_err = 1e9;
            out_qor->mean_rel_err = 1e9;
            out_qor->max_abs_err = 1e9;
            out_qor->psnr_db = 0.0;
            return;
        }
        double diff = fabs(test[i] - golden[i]);
        sum_sq_err += diff * diff;
        sum_sq_gold += golden[i] * golden[i];
        if (diff > max_abs) max_abs = diff;

        double denom = fabs(golden[i]);
        if (denom < 1e-6) denom = 1.0;
        sum_rel_err += diff / denom;
    }

    out_qor->max_abs_err = max_abs;
    out_qor->mean_rel_err = (n > 0) ? (sum_rel_err / (double)n) : 0.0;
    out_qor->rel_l2_err = (sum_sq_gold > 1e-20) ? sqrt(sum_sq_err / sum_sq_gold) : sqrt(sum_sq_err);

    double mse = (n > 0) ? (sum_sq_err / (double)n) : 0.0;
    if (mse < 1e-24) {
        out_qor->psnr_db = 120.0;
    } else {
        double p = (peak_val > 0.0) ? peak_val : 255.0;
        out_qor->psnr_db = 10.0 * log10((p * p) / mse);
    }
}
