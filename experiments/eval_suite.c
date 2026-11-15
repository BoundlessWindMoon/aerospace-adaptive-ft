#include "atft_runtime.h"
#include "benchmarks.h"
#include "harness.h"

#include <math.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

static sigjmp_buf g_crash_jmp_buf;
static volatile sig_atomic_t g_caught_crash_sig = 0;
// 只有在"一次注错试验"的保护区内才允许 siglongjmp。周期性 SIGALRM 看护会在
// 试验结束后仍可能投递一次，若此时跳回已失效的 sigsetjmp 上下文，会把已经
// 走完的循环重新执行一遍（实测表现为同一条 FI5_CSV 被打印上万次）。
static volatile sig_atomic_t g_in_fi_trial = 0;
// 堆已损坏标志：注错把指针写坏后 libc 会 abort()。此时进程内的任何后续分配
// （包括 printf）都会再次 abort，继续在同一进程里跑剩余试验是不可信的，
// 必须结束当前 benchmark 的 campaign 而不是假装继续。
static volatile sig_atomic_t g_heap_abort = 0;

static void crash_signal_handler(int sig) {
    if (!g_in_fi_trial) {
        // 保护区之外没有有效的 sigsetjmp 上下文可跳：跳回去会把已走完的循环
        // 反复重放（实测 cnt_due 累加到 2000 万、同一条汇总打印上千万次）。
        // 正确做法是恢复默认处置并重新抛出，让进程以非零状态退出，
        // 由上层 harness 记录为失败。
        signal(sig, SIG_DFL);
        raise(sig);
        return;
    }
    if (sig == SIGABRT) g_heap_abort = 1;
    g_caught_crash_sig = sig;
    siglongjmp(g_crash_jmp_buf, sig);
}

static void install_crash_handlers(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = crash_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_NODEFER;
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
    sigaction(SIGALRM, &sa, NULL);
    // 整数/地址类注错可能破坏指针并让 libc 主动 abort()（如 free(): invalid
    // pointer）；SIGABRT 同样归类为 DUE（检出但不可恢复），否则整个注入
    // campaign 会在第一次堆损坏时直接终止
    sigaction(SIGABRT, &sa, NULL);
}

// ============================================================================
// 可配置项（由环境变量注入，默认值即内置基线）
//
// 判据阈值与看护参数不写死在代码里：benchmark_harness.py 从 YAML 读出后
// 通过环境变量传进来，这样"实验怎么判"是配置而不是源码。
// ============================================================================
typedef struct {
    double        sdc_rel_err;     // 浮点负载判为 SDC 的相对误差阈值
    double        masked_rel_err;  // 判为完全掩盖的相对误差阈值
    long          watchdog_us;     // 单次注错试验的子进程挂死上限
    unsigned long budget_factor;   // 动态指令预算 = 无故障剖析步数 x 本系数
} eval_config_t;

static eval_config_t g_cfg = { 0.01, 1e-12, 100000L, 50UL };

static void eval_config_from_env(void) {
    const char *v;
    if ((v = getenv("ATFT_SDC_REL_ERR")) && *v)        g_cfg.sdc_rel_err = atof(v);
    if ((v = getenv("ATFT_MASKED_REL_ERR")) && *v)     g_cfg.masked_rel_err = atof(v);
    if ((v = getenv("ATFT_TRIAL_WATCHDOG_MS")) && *v)  g_cfg.watchdog_us = atol(v) * 1000L;
    if ((v = getenv("ATFT_STEP_BUDGET_FACTOR")) && *v) g_cfg.budget_factor = strtoul(v, NULL, 10);

    if (g_cfg.sdc_rel_err <= 0.0)    g_cfg.sdc_rel_err = 0.01;
    if (g_cfg.masked_rel_err < 0.0)  g_cfg.masked_rel_err = 1e-12;
    if (g_cfg.watchdog_us <= 0L)     g_cfg.watchdog_us = 100000L;
    if (g_cfg.budget_factor == 0UL)  g_cfg.budget_factor = 50UL;
}

// ============================================================================
// 逐负载表决阈值表 (D3) 与注错随机种子
//
//   ATFT_DELTA_TABLE   : CSV 路径，每行 "benchmark,delta"。某负载缺行则该负载
//                        回落到编译期常数（g_atft_delta_abs = -1）。
//   ATFT_REPLICA_EXACT : 1 = 要求两条近似副本按位相等
//   ATFT_FI_SEED       : 注错随机种子基数。默认与历史一致，改它即可做多种子
//                        重复实验，用来回答"单种子结论的方差有多大"。
// ============================================================================
// 1 = 用 harness 侧手工接线（对照组），0 = 只用编译期自动插桩
static int g_abft_harness = 0;

#define ATFT_MAX_DELTA_ROWS 64
static struct { char name[64]; double delta; } g_delta_table[ATFT_MAX_DELTA_ROWS];
static int g_delta_table_n = 0;
static uint64_t g_fi_seed_base = 0x123456789abcdef0ULL;

static void load_delta_table(void) {
    const char *v = getenv("ATFT_REPLICA_EXACT");
    if (v && *v && atoi(v) != 0) atft_set_replica_exact(1);
    if ((v = getenv("ATFT_ABFT")) && *v && atoi(v) != 0)
        atft_abft_set(1, (v = getenv("ATFT_ABFT_REPAIR")) ? atoi(v) : 1);
    // harness 侧的手工接线默认关闭：编译期插桩打通之后它只作为对照组存在
    if ((v = getenv("ATFT_ABFT_HARNESS")) && *v && atoi(v) != 0) g_abft_harness = 1;
    if ((v = getenv("ATFT_FI_SEED")) && *v) g_fi_seed_base = strtoull(v, NULL, 0);

    const char *path = getenv("ATFT_DELTA_TABLE");
    if (!path || !*path) return;
    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "[delta] 打不开阈值表 %s\n", path);
        return;
    }
    char line[256];
    while (fgets(line, sizeof(line), fp) && g_delta_table_n < ATFT_MAX_DELTA_ROWS) {
        char nm[64];
        double d;
        if (sscanf(line, "%63[^,],%lf", nm, &d) == 2 && d > 0.0) {
            snprintf(g_delta_table[g_delta_table_n].name,
                     sizeof(g_delta_table[g_delta_table_n].name), "%s", nm);
            g_delta_table[g_delta_table_n].delta = d;
            g_delta_table_n++;
        }
    }
    fclose(fp);
}

static void apply_delta_for_bench(const char *name) {
    for (int i = 0; i < g_delta_table_n; i++) {
        if (strcmp(g_delta_table[i].name, name) == 0) {
            atft_set_delta(g_delta_table[i].delta);
            return;
        }
    }
    atft_set_delta(-1.0);
}

// ============================================================================
// 基准注册表（声明式）
//
// 新增一个负载 = 在 BENCHES[] 里加一行 + 在 reset/invoke/export 三个 switch 里
// 各加一个 case。所有"数量""类型""子集"信息都从本表推导，代码里不再出现
// 写死的 benchmark 下标数组——早期版本在 RQ5/RQ6 里硬编码了
// {0,4,5,6,9,11} 并注释成 "gemm, jacobi2d, blackscholes, sobel, fir, ekf"，
// 在负载集变化后这些下标全部指向了别的程序，却不会报错。
// ============================================================================
typedef enum {
    BK_FP  = 0,   // 浮点负载：QoR 用相对 L2 误差，可容忍近似分支固有误差
    BK_INT = 1    // 整数/定点负载：按位精确匹配，任何非零偏差即 SDC
} bench_kind_t;

// 子集标签：供只需要代表性负载的实验（位域敏感性、MBU 冲击）按语义选取
#define BTAG_NONE      0u
#define BTAG_REPR      (1u << 0)   // 跨套件的代表性子集
#define BTAG_AEROSPACE (1u << 1)   // 空天域负载（ESA OBPMark / COAST 星载程序）

typedef struct {
    const char   *name;
    const char   *suite;
    bench_kind_t  kind;
    unsigned      tags;
} bench_meta_t;

static const bench_meta_t BENCHES[] = {
    /* name                 suite                      kind     tags */
    { "pb_gemm",            "PolyBench/C 4.2.1",       BK_FP,   BTAG_REPR },
    { "pb_2mm",             "PolyBench/C 4.2.1",       BK_FP,   BTAG_NONE },
    { "pb_atax",            "PolyBench/C 4.2.1",       BK_FP,   BTAG_NONE },
    { "pb_bicg",            "PolyBench/C 4.2.1",       BK_FP,   BTAG_NONE },
    { "pb_jacobi2d",        "PolyBench/C 4.2.1",       BK_FP,   BTAG_REPR },
    { "pb_ludcmp",          "PolyBench/C 4.2.1",       BK_FP,   BTAG_NONE },
    { "pb_durbin",          "PolyBench/C 4.2.1",       BK_FP,   BTAG_NONE },
    { "pb_deriche",         "PolyBench/C 4.2.1",       BK_FP,   BTAG_REPR },
    { "pb_floyd_warshall",  "PolyBench/C 4.2.1",       BK_INT,  BTAG_REPR },
    { "pb_nussinov",        "PolyBench/C 4.2.1",       BK_INT,  BTAG_NONE },
    { "parsec_blackscholes","PARSEC (loop-perf port)", BK_FP,   BTAG_REPR },
    { "dsp_kissfft",        "KissFFT v1.3.1",          BK_FP,   BTAG_REPR },
    { "axc_sobel",          "llvm-loop-perforation",   BK_INT,  BTAG_NONE },
    { "coast_crc16",        "COAST tests (BYU/LANL)",  BK_INT,  BTAG_AEROSPACE },
    { "coast_quicksort",    "COAST tests (BYU/LANL)",  BK_INT,  BTAG_AEROSPACE },
    { "obpmark_image_prep", "OBPMark v0.3 (ESA+BSC)",  BK_INT,  BTAG_REPR | BTAG_AEROSPACE },
    { "obpmark_image_proc", "OBPMark v0.3 (ESA+BSC)",  BK_INT,  BTAG_REPR | BTAG_AEROSPACE },
};

#define NUM_BENCHES ((int)(sizeof(BENCHES) / sizeof(BENCHES[0])))

static const char *bench_name(int b) {
    return (b >= 0 && b < NUM_BENCHES) ? BENCHES[b].name : "<invalid>";
}

static int bench_is_integer(int b) {
    return (b >= 0 && b < NUM_BENCHES) && BENCHES[b].kind == BK_INT;
}

// 按标签选出子集；返回实际写入的个数
static int bench_select_tagged(unsigned tag, int *out, int max_out) {
    int n = 0;
    for (int b = 0; b < NUM_BENCHES && n < max_out; b++) {
        if (BENCHES[b].tags & tag) out[n++] = b;
    }
    return n;
}

static int bench_index_by_name(const char *name) {
    for (int b = 0; b < NUM_BENCHES; b++) {
        if (strcmp(BENCHES[b].name, name) == 0) return b;
    }
    fprintf(stderr, "[eval_suite] unknown benchmark name: %s\n", name);
    abort();
}

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
    int n_lu, n_dur;
    int der_w, der_h;
    int n_fw, n_nus;
    int n_fft;
    int img_h, img_w;
    int n_crc, n_qs;
    int n_opts;

    double *A, *B, *C, *D, *tmp_2mm;
    double *x, *y, *tmp_atax, *s_bicg, *q_bicg, *p_bicg, *r_bicg;
    double *jac_A, *jac_B;
    double *lu_A, *lu_A0, *lu_b, *lu_x, *lu_y;
    double *dur_r, *dur_y;
    double *der_in, *der_out, *der_y1, *der_y2;
    double *bs_spt, *bs_strike, *bs_rate, *bs_vol, *bs_time, *bs_prices;
    double *fft_re, *fft_im, *fft_out_re, *fft_out_im;

    int32_t *fw_path, *fw_path0;
    int32_t *nus_table;
    char    *nus_seq;
    unsigned char *img_in, *sobel_out;
    unsigned char *crc_data;
    int32_t *qs_arr, *qs_arr0;

    // OBPMark 1.1：w/h 必须为偶数（2x2 合并），num_frames>=5 且 frame_i=2
    // （上游 f_scrub 使用 frame_i-2 .. frame_i+2 的时间邻域）
    int obp_w, obp_h, obp_nframes, obp_frame_i;
    uint16_t *obp_frames;     // 时间序列原始帧 (nframes * w * h)
    uint16_t *obp_frame;      // 当前工作帧
    uint16_t *obp_frame0;     // 工作帧的原始副本（每次试验恢复）
    uint16_t *obp_offsets;
    uint16_t *obp_gains;
    uint8_t  *obp_bad;
    uint32_t *obp_binned;
    uint32_t *obp_out;
    double   *obp_prep_d, *obp_proc_d;

    // 整数/uint8 内核的输出在计时窗口外统一导出到 double 缓冲，供 QoR 复用
    double *fw_out_d, *nus_out_d, *sobel_out_d, *crc_out_d, *qs_out_d;

    double *out_ptrs[NUM_BENCHES];
    int     out_sizes[NUM_BENCHES];
    double  peak_vals[NUM_BENCHES];
    int     perf_iters[NUM_BENCHES];
} suite_workspace_t;

static void workspace_selfcheck(const suite_workspace_t *ws);

static void alloc_and_init_workspace(suite_workspace_t *ws, int scale_small) {
    memset(ws, 0, sizeof(*ws));
    ws->n_mat      = scale_small ? 24  : 48;
    ws->n_lu       = scale_small ? 24  : 40;
    ws->n_dur      = scale_small ? 24  : 40;
    ws->der_w      = scale_small ? 16  : 32;
    ws->der_h      = scale_small ? 16  : 32;
    ws->n_fw       = scale_small ? 24  : 48;
    ws->n_nus      = scale_small ? 24  : 48;
    ws->n_fft      = scale_small ? 32  : 64;
    ws->img_h      = scale_small ? 24  : 48;
    ws->img_w      = scale_small ? 24  : 48;
    ws->n_crc      = scale_small ? 100 : 200; // 上游 crc16() 的 length 参数为 unsigned char，上限 255
    ws->n_qs       = scale_small ? 290 : 580; // 上游 quicksort.c array_elements
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

    int nlu2 = ws->n_lu * ws->n_lu;
    ws->lu_A  = calloc(nlu2, sizeof(double));
    ws->lu_A0 = calloc(nlu2, sizeof(double));
    ws->lu_b  = calloc(ws->n_lu, sizeof(double));
    ws->lu_x  = calloc(ws->n_lu, sizeof(double));
    ws->lu_y  = calloc(ws->n_lu, sizeof(double));

    ws->dur_r = calloc(ws->n_dur, sizeof(double));
    ws->dur_y = calloc(ws->n_dur, sizeof(double));

    int der2 = ws->der_w * ws->der_h;
    ws->der_in  = calloc(der2, sizeof(double));
    ws->der_out = calloc(der2, sizeof(double));
    ws->der_y1  = calloc(der2, sizeof(double));
    ws->der_y2  = calloc(der2, sizeof(double));

    int nfw2 = ws->n_fw * ws->n_fw;
    ws->fw_path  = calloc(nfw2, sizeof(int32_t));
    ws->fw_path0 = calloc(nfw2, sizeof(int32_t));
    ws->fw_out_d = calloc(nfw2, sizeof(double));

    int nnus2 = ws->n_nus * ws->n_nus;
    ws->nus_seq   = calloc(ws->n_nus, sizeof(char));
    ws->nus_table = calloc(nnus2, sizeof(int32_t));
    ws->nus_out_d = calloc(nnus2, sizeof(double));

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

    int im2 = ws->img_h * ws->img_w;
    ws->img_in      = calloc(im2, sizeof(unsigned char));
    ws->sobel_out   = calloc(im2, sizeof(unsigned char));
    ws->sobel_out_d = calloc(im2, sizeof(double));

    ws->crc_data  = calloc(ws->n_crc + 1, sizeof(unsigned char));
    ws->crc_out_d = calloc(1, sizeof(double));

    ws->obp_w = scale_small ? 16 : 32;
    ws->obp_h = scale_small ? 16 : 32;
    ws->obp_nframes = 5;
    ws->obp_frame_i = 2;
    int obp_px = ws->obp_w * ws->obp_h;
    int obp_bin = (ws->obp_w / 2) * (ws->obp_h / 2);
    ws->obp_frames  = calloc((size_t)ws->obp_nframes * obp_px, sizeof(uint16_t));
    ws->obp_frame   = calloc(obp_px, sizeof(uint16_t));
    ws->obp_frame0  = calloc(obp_px, sizeof(uint16_t));
    ws->obp_offsets = calloc(obp_px, sizeof(uint16_t));
    ws->obp_gains   = calloc(obp_px, sizeof(uint16_t));
    ws->obp_bad     = calloc(obp_px, sizeof(uint8_t));
    ws->obp_binned  = calloc(obp_bin, sizeof(uint32_t));
    ws->obp_out     = calloc(obp_bin, sizeof(uint32_t));
    ws->obp_prep_d  = calloc(obp_px, sizeof(double));
    ws->obp_proc_d  = calloc(obp_bin, sizeof(double));

    ws->qs_arr   = calloc(ws->n_qs, sizeof(int32_t));
    ws->qs_arr0  = calloc(ws->n_qs, sizeof(int32_t));
    ws->qs_out_d = calloc(ws->n_qs, sizeof(double));

    for (int i = 0; i < nm2; i++) {
        ws->A[i] = 0.5 * sin(0.13 * (double)(i + 1)) + 0.8;
        ws->B[i] = 0.5 * cos(0.07 * (double)(i + 1)) + 0.6;
    }
    for (int i = 0; i < ws->n_mat; i++) {
        ws->x[i]      = 0.3 * sin(0.2 * (double)i) + 1.0;
        ws->p_bicg[i] = 0.4 * cos(0.17 * (double)i) + 0.9;
        ws->r_bicg[i] = 0.35 * sin(0.11 * (double)i) + 0.7;
    }
    // ludcmp: 对角占优矩阵，保证 LU 分解数值稳定
    for (int i = 0; i < ws->n_lu; i++) {
        for (int j = 0; j < ws->n_lu; j++) {
            double v = 0.3 * sin(0.21 * (double)(i * ws->n_lu + j + 1));
            ws->lu_A0[i * ws->n_lu + j] = (i == j) ? ((double)ws->n_lu + v) : v;
        }
        ws->lu_b[i] = 1.0 + 0.5 * cos(0.19 * (double)i);
    }
    // durbin: 归一化自相关序列（Levinson-Durbin 输入要求 |r| 递减）
    for (int i = 0; i < ws->n_dur; i++) {
        ws->dur_r[i] = 0.8 * exp(-0.12 * (double)i) * cos(0.25 * (double)i);
    }
    for (int i = 0; i < der2; i++) {
        ws->der_in[i] = 128.0 + 60.0 * sin(0.15 * (double)(i / ws->der_h)) *
                                       cos(0.15 * (double)(i % ws->der_h));
    }
    // floyd-warshall: 上游 init_array 的等价稀疏权重图
    for (int i = 0; i < ws->n_fw; i++) {
        for (int j = 0; j < ws->n_fw; j++) {
            int32_t v = (int32_t)(i * j % 7 + 1);
            if ((i + j) % 13 == 0 || (i + j) % 7 == 0 || (i + j) % 11 == 0) v = 999;
            ws->fw_path0[i * ws->n_fw + j] = (i == j) ? 0 : v;
        }
    }
    // nussinov: 碱基序列 AGCT -> 0..3
    for (int i = 0; i < ws->n_nus; i++) {
        ws->nus_seq[i] = (char)((i + 1) % 4);
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
    for (int r = 0; r < ws->img_h; r++) {
        for (int c = 0; c < ws->img_w; c++) {
            double v = 128.0 + 60.0 * sin(0.15 * r) * cos(0.15 * c);
            ws->img_in[r * ws->img_w + c] = (unsigned char)v;
        }
    }
    for (int i = 0; i < ws->n_crc; i++) {
        ws->crc_data[i] = (unsigned char)((i * 37 + 11) & 0xFF); // 合成遥测帧
    }
    for (int i = 0; i < ws->n_qs; i++) {
        ws->qs_arr0[i] = (int32_t)((i * 1103515245u + 12345u) % 32768u);
    }

    // OBPMark 输入：合成星载影像（含固定偏置、增益图、坏像元与若干辐射热点）
    for (int f = 0; f < ws->obp_nframes; f++) {
        for (int i = 0; i < obp_px; i++) {
            double v = 900.0 + 180.0 * sin(0.07 * (double)i + 0.3 * (double)f);
            if ((i + f) % 311 == 0) v += 9000.0; // 单粒子亮点，供辐射擦洗识别
            ws->obp_frames[(size_t)f * obp_px + i] = (uint16_t)v;
        }
    }
    for (int i = 0; i < obp_px; i++) {
        ws->obp_frame0[i]  = ws->obp_frames[(size_t)ws->obp_frame_i * obp_px + i];
        ws->obp_offsets[i] = (uint16_t)(40 + (i % 13));
        ws->obp_gains[i]   = (uint16_t)(1 + (i % 3));
        ws->obp_bad[i]     = (uint8_t)((i % 197 == 0) ? 1 : 0);
    }

    int nlu = ws->n_lu;
    ws->out_ptrs[0]  = ws->C;          ws->out_sizes[0]  = nm2;            ws->peak_vals[0]  = 40.0;   ws->perf_iters[0]  = 200;
    ws->out_ptrs[1]  = ws->D;          ws->out_sizes[1]  = nm2;            ws->peak_vals[1]  = 800.0;  ws->perf_iters[1]  = 150;
    ws->out_ptrs[2]  = ws->y;          ws->out_sizes[2]  = ws->n_mat;      ws->peak_vals[2]  = 600.0;  ws->perf_iters[2]  = 1500;
    ws->out_ptrs[3]  = ws->q_bicg;     ws->out_sizes[3]  = ws->n_mat;      ws->peak_vals[3]  = 40.0;   ws->perf_iters[3]  = 1500;
    ws->out_ptrs[4]  = ws->jac_A;      ws->out_sizes[4]  = nm2;            ws->peak_vals[4]  = 2.0;    ws->perf_iters[4]  = 200;
    ws->out_ptrs[5]  = ws->lu_x;       ws->out_sizes[5]  = nlu;            ws->peak_vals[5]  = 1.0;    ws->perf_iters[5]  = 500;
    ws->out_ptrs[6]  = ws->dur_y;      ws->out_sizes[6]  = ws->n_dur;      ws->peak_vals[6]  = 2.0;    ws->perf_iters[6]  = 2000;
    ws->out_ptrs[7]  = ws->der_out;    ws->out_sizes[7]  = der2;           ws->peak_vals[7]  = 255.0;  ws->perf_iters[7]  = 400;
    ws->out_ptrs[8]  = ws->fw_out_d;   ws->out_sizes[8]  = nfw2;           ws->peak_vals[8]  = 999.0;  ws->perf_iters[8]  = 60;
    ws->out_ptrs[9]  = ws->nus_out_d;  ws->out_sizes[9]  = nnus2;          ws->peak_vals[9]  = 64.0;   ws->perf_iters[9]  = 150;
    ws->out_ptrs[10] = ws->bs_prices;  ws->out_sizes[10] = ws->n_opts;     ws->peak_vals[10] = 25.0;   ws->perf_iters[10] = 600;
    ws->out_ptrs[11] = ws->fft_out_re; ws->out_sizes[11] = ws->n_fft;      ws->peak_vals[11] = 32.0;   ws->perf_iters[11] = 300;
    ws->out_ptrs[12] = ws->sobel_out_d;ws->out_sizes[12] = im2;            ws->peak_vals[12] = 255.0;  ws->perf_iters[12] = 400;
    ws->out_ptrs[13] = ws->crc_out_d;  ws->out_sizes[13] = 1;              ws->peak_vals[13] = 65535.0;ws->perf_iters[13] = 20000;
    ws->out_ptrs[14] = ws->qs_out_d;   ws->out_sizes[14] = ws->n_qs;       ws->peak_vals[14] = 32768.0;ws->perf_iters[14] = 300;
    ws->out_ptrs[15] = ws->obp_prep_d; ws->out_sizes[15] = obp_px;         ws->peak_vals[15] = 65535.0;ws->perf_iters[15] = 600;
    ws->out_ptrs[16] = ws->obp_proc_d; ws->out_sizes[16] = obp_bin;        ws->peak_vals[16] = 65535.0;ws->perf_iters[16] = 400;

    workspace_selfcheck(ws);
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
    } else if (b == 5) {
        // ludcmp 原地分解 A，每次从未分解副本恢复
        memcpy(ws->lu_A, ws->lu_A0, (size_t)ws->n_lu * ws->n_lu * sizeof(double));
    } else if (b == 8) {
        // floyd-warshall 原地更新 path
        memcpy(ws->fw_path, ws->fw_path0, (size_t)ws->n_fw * ws->n_fw * sizeof(int32_t));
    } else if (b == 9) {
        memset(ws->nus_table, 0, (size_t)ws->n_nus * ws->n_nus * sizeof(int32_t));
    } else if (b == 14) {
        // quicksort 原地排序，每次从未排序副本恢复
        memcpy(ws->qs_arr, ws->qs_arr0, (size_t)ws->n_qs * sizeof(int32_t));
    } else if (b == 15 || b == 16) {
        // OBPMark 的两个阶段都原地改写工作帧，须从原始副本恢复
        size_t px = (size_t)ws->obp_w * (size_t)ws->obp_h;
        memcpy(ws->obp_frame, ws->obp_frame0, px * sizeof(uint16_t));
        if (b == 16) {
            size_t bin = (size_t)(ws->obp_w / 2) * (size_t)(ws->obp_h / 2);
            memset(ws->obp_binned, 0, bin * sizeof(uint32_t));
            memset(ws->obp_out, 0, bin * sizeof(uint32_t));
        }
    }
}

// 整数 / uint8 内核的输出统一导出到 double 缓冲（在计时与注入窗口之外调用，
// 不污染 PMU 指令计数）
static void export_outputs(suite_workspace_t *ws, int b) {
    if (b == 8) {
        int n = ws->n_fw * ws->n_fw;
        for (int i = 0; i < n; i++) ws->fw_out_d[i] = (double)ws->fw_path[i];
    } else if (b == 9) {
        int n = ws->n_nus * ws->n_nus;
        for (int i = 0; i < n; i++) ws->nus_out_d[i] = (double)ws->nus_table[i];
    } else if (b == 12) {
        int n = ws->img_h * ws->img_w;
        for (int i = 0; i < n; i++) ws->sobel_out_d[i] = (double)ws->sobel_out[i];
    } else if (b == 14) {
        for (int i = 0; i < ws->n_qs; i++) ws->qs_out_d[i] = (double)ws->qs_arr[i];
    } else if (b == 15) {
        int n = ws->obp_w * ws->obp_h;
        for (int i = 0; i < n; i++) ws->obp_prep_d[i] = (double)ws->obp_frame[i];
    } else if (b == 16) {
        int n = (ws->obp_w / 2) * (ws->obp_h / 2);
        for (int i = 0; i < n; i++) ws->obp_proc_d[i] = (double)ws->obp_out[i];
    }
    // b == 13 (crc16) 的结果由 invoke_benchmark 直接写入 crc_out_d
}

// 代数校验层的调用点。校验器本身是通用过程（runtime/atft_runtime.c），
// 这里按 `-atft-dump-sums` + eqsat 合成出的式子为被识别的内核接上维度与步长。
// 编译期自动插这几行调用是剩下的工程步，不影响下面测到的代价与检出数字。
static double g_abft_rowsum[4096];
// beta != 0 时重算整行需要的原数据副本。容量单独记，n 变大时必须重分配 ——
// --scale 模式会在同一进程里换多个规模，只按第一次的 n 分配会越界写堆。
static double *g_abft_cold = NULL;
static size_t g_abft_cold_cap = 0;

// 冷路径重执行：双模检错置位后，整段重跑一次。
// 故障注入器按"动态步号"注入，第一次执行已经走过那个步号，所以重跑是干净的；
// 这正是用极低故障率（a <= 4e-6）换掉常驻第三份副本的那笔交易。
static void invoke_once(suite_workspace_t *ws, int b);

static void invoke_benchmark(suite_workspace_t *ws, int b) {
    invoke_once(ws, b);
    if (atft_dmr_take_flag()) invoke_once(ws, b);   // 检到不一致：重算一次
}

static void invoke_once(suite_workspace_t *ws, int b) {
    reset_mutable_buffers(ws, b);
    int n = ws->n_mat;
    if (g_abft_harness && g_atft_abft_enabled && (b == 0 || b == 1) && n <= 4096) {
        const double *src = (b == 0) ? ws->C : ws->tmp_2mm;
        atft_abft_pre_rowsum(src, n, n, n, g_abft_rowsum);
        if (b == 0) {
            size_t need = (size_t)n * (size_t)n;
            if (g_abft_cold_cap < need) {
                double *grown = (double *)realloc(g_abft_cold, sizeof(double) * need);
                if (grown) {
                    g_abft_cold = grown;
                    g_abft_cold_cap = need;
                }
            }
            if (g_abft_cold_cap >= need) atft_abft_pre_copy(src, n, n, n, g_abft_cold);
        }
    }
    switch (b) {
        case 0:
            bench_pb_gemm(n, n, n, 1.5, 1.2, ws->A, ws->B, ws->C);
if (g_abft_harness) {
            atft_abft_post_gemm(ws->C, ws->A, ws->B, n, n, n, n, n, n,
                                1.5, 1.2, g_abft_rowsum, g_abft_cold);
            }
            break;
        case 1:
            bench_pb_2mm(n, n, n, n, 1.5, 1.2, ws->tmp_2mm, ws->A, ws->B, ws->A, ws->D);
if (g_abft_harness) {
            atft_abft_post_gemm(ws->tmp_2mm, ws->A, ws->B, n, n, n, n, n, n,
                                1.5, 0.0, g_abft_rowsum, NULL);
            }
            break;
        case 2:
            bench_pb_atax(n, n, ws->A, ws->x, ws->y, ws->tmp_atax);
if (g_abft_harness) {
            atft_abft_check_matvec(ws->tmp_atax, ws->A, ws->x, n, n, n, 0);
            atft_abft_check_matvec(ws->y, ws->A, ws->tmp_atax, n, n, n, 1);
            }
            break;
        case 3:
            bench_pb_bicg(n, n, ws->A, ws->s_bicg, ws->q_bicg, ws->p_bicg, ws->r_bicg);
if (g_abft_harness) {
            atft_abft_check_matvec(ws->q_bicg, ws->A, ws->p_bicg, n, n, n, 0);
            atft_abft_check_matvec(ws->s_bicg, ws->A, ws->r_bicg, n, n, n, 1);
            }
            break;
        case 4: bench_pb_jacobi2d(4, ws->n_mat, ws->jac_A, ws->jac_B); break;
        case 5: bench_pb_ludcmp(ws->n_lu, ws->lu_A, ws->lu_b, ws->lu_x, ws->lu_y); break;
        case 6: bench_pb_durbin(ws->n_dur, ws->dur_r, ws->dur_y); break;
        case 7: bench_pb_deriche(ws->der_w, ws->der_h, 0.25, ws->der_in, ws->der_out, ws->der_y1, ws->der_y2); break;
        case 8: bench_pb_floyd_warshall(ws->n_fw, ws->fw_path); break;
        case 9: bench_pb_nussinov(ws->n_nus, ws->nus_seq, ws->nus_table); break;
        case 10: bench_parsec_blackscholes(ws->n_opts, ws->bs_spt, ws->bs_strike, ws->bs_rate, ws->bs_vol, ws->bs_time, ws->bs_prices); break;
        case 11: bench_dsp_kissfft(ws->n_fft, ws->fft_re, ws->fft_im, ws->fft_out_re, ws->fft_out_im); break;
        case 12: bench_axc_sobel(ws->img_h, ws->img_w, ws->img_in, ws->sobel_out); break;
        case 13: ws->crc_out_d[0] = (double)bench_coast_crc16(ws->crc_data, (unsigned char)ws->n_crc); break;
        case 14: bench_coast_quicksort(ws->qs_arr, ws->n_qs); break;
        case 15: bench_obpmark_image_prep(ws->obp_w, ws->obp_h, ws->obp_frame,
                                          ws->obp_offsets, ws->obp_bad); break;
        case 16: bench_obpmark_image_proc(ws->obp_w, ws->obp_h, ws->obp_nframes,
                                          ws->obp_frame_i, ws->obp_frame, ws->obp_frames,
                                          ws->obp_gains, ws->obp_binned, ws->obp_out); break;
        default:
            // 注册表里新增了负载但忘了加 case：立刻失败，不要静默跑错程序
            fprintf(stderr, "[eval_suite] invoke_benchmark: benchmark %d (%s) 未实现\n",
                    b, bench_name(b));
            abort();
    }
}

// 启动自检：注册表与工作区必须一一对应。宁可在第一秒崩，也不要让某个负载
// 带着空输出缓冲跑完 1000 次注入、最后得出"SDC 0%"的假结论。
static void workspace_selfcheck(const suite_workspace_t *ws) {
    int bad = 0;
    for (int b = 0; b < NUM_BENCHES; b++) {
        if (ws->out_ptrs[b] == NULL) {
            fprintf(stderr, "[selfcheck] %-22s out_ptr 未设置\n", bench_name(b));
            bad = 1;
        }
        if (ws->out_sizes[b] <= 0) {
            fprintf(stderr, "[selfcheck] %-22s out_size=%d 非法\n", bench_name(b), ws->out_sizes[b]);
            bad = 1;
        }
        if (ws->peak_vals[b] <= 0.0) {
            fprintf(stderr, "[selfcheck] %-22s peak_val=%g 非法\n", bench_name(b), ws->peak_vals[b]);
            bad = 1;
        }
        if (ws->perf_iters[b] <= 0) {
            fprintf(stderr, "[selfcheck] %-22s perf_iters=%d 非法\n", bench_name(b), ws->perf_iters[b]);
            bad = 1;
        }
    }
    if (bad) {
        fprintf(stderr, "[selfcheck] 基准注册表与工作区不一致，终止\n");
        abort();
    }
}

static void free_workspace(suite_workspace_t *ws) {
    free(ws->A); free(ws->B); free(ws->C); free(ws->D); free(ws->tmp_2mm);
    free(ws->x); free(ws->y); free(ws->tmp_atax); free(ws->s_bicg);
    free(ws->q_bicg); free(ws->p_bicg); free(ws->r_bicg);
    free(ws->jac_A); free(ws->jac_B);
    free(ws->lu_A); free(ws->lu_A0); free(ws->lu_b); free(ws->lu_x); free(ws->lu_y);
    free(ws->dur_r); free(ws->dur_y);
    free(ws->der_in); free(ws->der_out); free(ws->der_y1); free(ws->der_y2);
    free(ws->fw_path); free(ws->fw_path0); free(ws->fw_out_d);
    free(ws->nus_seq); free(ws->nus_table); free(ws->nus_out_d);
    free(ws->bs_spt); free(ws->bs_strike); free(ws->bs_rate);
    free(ws->bs_vol); free(ws->bs_time); free(ws->bs_prices);
    free(ws->fft_re); free(ws->fft_im); free(ws->fft_out_re); free(ws->fft_out_im);
    free(ws->img_in); free(ws->sobel_out); free(ws->sobel_out_d);
    free(ws->crc_data); free(ws->crc_out_d);
    free(ws->qs_arr); free(ws->qs_arr0); free(ws->qs_out_d);
    free(ws->obp_frames); free(ws->obp_frame); free(ws->obp_frame0);
    free(ws->obp_offsets); free(ws->obp_gains); free(ws->obp_bad);
    free(ws->obp_binned); free(ws->obp_out);
    free(ws->obp_prep_d); free(ws->obp_proc_d);
}

static int run_pmu_perf_and_qor(const char *policy_name, const char *golden_dir, int dump_golden) {
    suite_workspace_t ws;
    alloc_and_init_workspace(&ws, 0);

    harness_pmu_t pmu;
    harness_pmu_init(&pmu);

    for (int b = 0; b < NUM_BENCHES; b++) {
        atft_init(10000.0, 4.0, 3, 4, 3);
        apply_delta_for_bench(bench_name(b));
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

        export_outputs(&ws, b);

        char gpath[512];
        snprintf(gpath, sizeof(gpath), "%s/golden_%s.bin", golden_dir, bench_name(b));
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
               policy_name, bench_name(b), avg_us,
               (unsigned long)hw_insts, (unsigned long)hw_cycles,
               qor.rel_l2_err, qor.mean_rel_err, qor.max_abs_err, qor.psnr_db,
               (unsigned long)votes_per_iter, (unsigned long)false_alarms_per_iter);
    }

    harness_pmu_close(&pmu);
    free_workspace(&ws);
    return 0;
}

// ============================================================================
// 统一的故障注入执行引擎（fork 进程隔离）
//
// RQ1 / RQ5(位域敏感性) / RQ6(MBU 与连发冲击) 原先各写了一份注入循环，三份都在
// 同进程内循环注错。注错一旦破坏指针，libc 会在任意 malloc/free 上 abort，
// 同进程方案无法恢复（实测过 2000 万次重复打印与 12 分钟空转）。现在三者共用
// 本引擎：每次试验 fork 一个子进程，子进程自带挂死看护与指令预算，
// 父进程只读共享内存里的结论 —— 这也让 "one fault per run" 名副其实。
// ============================================================================
#define FI_MAX_EVENTS 16

typedef struct {
    uint64_t steps[FI_MAX_EVENTS];
    uint32_t bits[FI_MAX_EVENTS];
    int      n_events;
    uint32_t mbu_width;
} fi_fault_spec_t;

typedef struct {
    double rel_l2_err;
    int    has_nan_or_inf;
    int    fault_detected;   // 表决器检出并完成恢复
    int    unrecoverable;    // 仅检错不可恢复（EDDI/SWIFT 双模）或三路不一致
} fi_trial_result_t;

typedef struct {
    int    trials;
    int    masked, det_rec, benign, sdc, due;
    int    valid_runs;
    double sum_valid_rel_err;
    uint64_t total_dyn_insts;
} fi_stats_t;

// 故障点选取策略：由各实验提供，决定一次试验注哪些位
typedef void (*fi_pick_fn)(uint64_t *rng, uint64_t total_dyn_insts,
                           fi_fault_spec_t *out, void *ctx);

static void fi_stats_accumulate(fi_stats_t *st, int b, int child_ok,
                                const fi_trial_result_t *r) {
    st->trials++;
    if (!child_ok || r->unrecoverable) {
        st->due++;
        return;
    }
    // 整数负载不允许近似容忍：任何非零偏差即 SDC
    double sdc_threshold = bench_is_integer(b) ? 0.0 : g_cfg.sdc_rel_err;
    if (r->has_nan_or_inf || r->rel_l2_err > sdc_threshold) {
        st->sdc++;
        return;
    }
    st->valid_runs++;
    st->sum_valid_rel_err += r->rel_l2_err;
    if (r->fault_detected)                        st->det_rec++;
    else if (r->rel_l2_err <= g_cfg.masked_rel_err) st->masked++;
    else                                          st->benign++;
}

// 对单个 benchmark 跑 trials 次注入；golden 与动态步数在内部完成剖析
static void fi_run_campaign(suite_workspace_t *ws, int b, const char *policy_name,
                            int trials, uint64_t seed,
                            fi_pick_fn pick, void *ctx, fi_stats_t *st) {
    memset(st, 0, sizeof(*st));

    double *golden_buf = calloc((size_t)ws->out_sizes[b], sizeof(double));
    if (!golden_buf) {
        fprintf(stderr, "[fi] golden buffer alloc failed for %s\n", bench_name(b));
        return;
    }

    atft_init(10000.0, 4.0, 3, 4, 3);
    apply_delta_for_bench(bench_name(b));
    if (strstr(policy_name, "adaptive") != NULL) g_atft_state.current_mode = ATFT_MODE_HIGH;
    harness_fi_reset_profile();
    invoke_benchmark(ws, b);
    export_outputs(ws, b);
    memcpy(golden_buf, ws->out_ptrs[b], (size_t)ws->out_sizes[b] * sizeof(double));
    st->total_dyn_insts = harness_fi_get_total_dyn_insts();

    if (st->total_dyn_insts == 0) {
        // 该负载在当前注错类别下没有插桩点：如实返回 0 次试验，
        // 由上层决定是否记录，绝不把"没注入"写成"没损坏"
        free(golden_buf);
        return;
    }

    fi_trial_result_t *shm = mmap(NULL, sizeof(fi_trial_result_t),
                                  PROT_READ | PROT_WRITE,
                                  MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shm == MAP_FAILED) {
        fprintf(stderr, "[fi] mmap failed for %s\n", bench_name(b));
        free(golden_buf);
        return;
    }

    uint64_t rng = seed;
    for (int t = 0; t < trials; t++) {
        fi_fault_spec_t spec;
        memset(&spec, 0, sizeof(spec));
        spec.mbu_width = 1;
        pick(&rng, st->total_dyn_insts, &spec, ctx);
        if (spec.n_events <= 0) spec.n_events = 1;
        if (spec.n_events > FI_MAX_EVENTS) spec.n_events = FI_MAX_EVENTS;

        memset(shm, 0, sizeof(*shm));
        pid_t pid = fork();
        if (pid < 0) {
            fprintf(stderr, "[fi] fork failed (%s, trial %d)\n", bench_name(b), t);
            st->trials++;
            st->due++;
            continue;
        }

        if (pid == 0) {
            struct itimerval cw;
            memset(&cw, 0, sizeof(cw));
            cw.it_value.tv_usec = g_cfg.watchdog_us; // 挂死上限，默认处置直接自杀
            signal(SIGALRM, SIG_DFL);
            setitimer(ITIMER_REAL, &cw, NULL);

            atft_init(10000.0, 4.0, 3, 4, 3);
            apply_delta_for_bench(bench_name(b));
            if (strstr(policy_name, "adaptive") != NULL) g_atft_state.current_mode = ATFT_MODE_HIGH;
            uint64_t base_alarms = g_atft_state.detected_faults;

            harness_fi_arm_mbu_or_burst(spec.steps, spec.bits, spec.n_events, spec.mbu_width);
            // 预算设为无故障剖析步数的 50 倍：正常执行不可能超过，死循环必然超过
            harness_fi_set_step_budget(st->total_dyn_insts * (uint64_t)g_cfg.budget_factor);

            memset(ws->out_ptrs[b], 0, (size_t)ws->out_sizes[b] * sizeof(double));
            invoke_benchmark(ws, b);
            export_outputs(ws, b);

            harness_qor_metrics_t q;
            harness_compute_qor(ws->out_sizes[b], golden_buf, ws->out_ptrs[b],
                                ws->peak_vals[b], &q);
            shm->rel_l2_err     = q.rel_l2_err;
            shm->has_nan_or_inf = q.has_nan_or_inf;
            shm->fault_detected = (g_atft_state.detected_faults > base_alarms);
            shm->unrecoverable  = (g_atft_state.unrecoverable_faults > 0);
            _exit(0);
        }

        int status = 0;
        while (waitpid(pid, &status, 0) < 0 && errno == EINTR) { }
        int child_ok = WIFEXITED(status) && WEXITSTATUS(status) == 0;
        fi_stats_accumulate(st, b, child_ok, shm);
    }

    munmap(shm, sizeof(fi_trial_result_t));
    free(golden_buf);
}

// ----------------------------------------------------------------------------
// RQ1: 全位宽均匀单比特注入（one fault per run）
// ----------------------------------------------------------------------------
static void pick_uniform_single_bit(uint64_t *rng, uint64_t total_dyn,
                                    fi_fault_spec_t *out, void *ctx) {
    (void)ctx;
    out->n_events = 1;
    out->mbu_width = 1;
    out->steps[0] = 1 + (splitmix64(rng) % total_dyn);
    out->bits[0]  = (uint32_t)(splitmix64(rng) % 64);
}

static int run_standard_one_fault_fi(const char *policy_name, const char *golden_dir, int trials) {
    (void)golden_dir;
    suite_workspace_t ws;
    alloc_and_init_workspace(&ws, 1);
    install_crash_handlers();

    for (int b = 0; b < NUM_BENCHES; b++) {
        fi_stats_t st;
        uint64_t seed = g_fi_seed_base ^ ((uint64_t)(b + 1) << 32);
        fi_run_campaign(&ws, b, policy_name, trials, seed,
                        pick_uniform_single_bit, NULL, &st);

        int denom = st.trials > 0 ? st.trials : 1;
        double avg_err = st.valid_runs > 0 ? (st.sum_valid_rel_err / st.valid_runs) : 0.0;

        printf("FI5_CSV,%s,%s,%lu,%d,%.2f,%.2f,%.2f,%.2f,%.2f,%.6e\n",
               policy_name, bench_name(b), (unsigned long)st.total_dyn_insts, st.trials,
               100.0 * st.masked / denom,
               100.0 * st.det_rec / denom,
               100.0 * st.benign / denom,
               100.0 * st.sdc / denom,
               100.0 * st.due / denom,
               avg_err);
    }

    free_workspace(&ws);
    return 0;
}

static int run_adaptive_trace_pmu(void) {
    suite_workspace_t ws;
    alloc_and_init_workspace(&ws, 1);

    // 一个窗口内的混合负载：按名字解析下标，权重为模拟的退休指令数
    struct { const char *name; uint64_t inst_weight; int reports_ecc; } mix_spec[] = {
        { "pb_gemm",            600000, 1 },
        { "pb_atax",            100000, 1 },
        { "dsp_kissfft",        300000, 1 },
        { "obpmark_image_proc", 200000, 1 },
        { "pb_ludcmp",          100000, 0 },
        { "pb_durbin",           50000, 0 },
        { "coast_crc16",         50000, 0 },
    };
    const int n_mix = (int)(sizeof(mix_spec) / sizeof(mix_spec[0]));
    struct { int idx; uint64_t inst_weight; int reports_ecc; } trace_mix[sizeof(mix_spec) / sizeof(mix_spec[0])];
    for (int m = 0; m < n_mix; m++) {
        trace_mix[m].idx = bench_index_by_name(mix_spec[m].name);
        trace_mix[m].inst_weight = mix_spec[m].inst_weight;
        trace_mix[m].reports_ecc = mix_spec[m].reports_ecc;
    }

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
            // 负载按**名字**解析（见 trace_mix[]），避免注册表变动后
            // 这里的下标静默指向别的程序
            for (int m = 0; m < n_mix; m++) {
                invoke_benchmark(&ws, trace_mix[m].idx);
                atft_advance_insts(trace_mix[m].inst_weight);
                if (trace_mix[m].reports_ecc && ecc_events_in_win >= 5) {
                    atft_report_ecc_event();
                }
            }
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
// ----------------------------------------------------------------------------
// RQ5: IEEE-754 位域敏感性（符号 / 阶码 / 高位尾数 / 低位尾数）
// ----------------------------------------------------------------------------
typedef struct {
    int field;              // 0=sign, 1=exponent, 2=mantissa_high, 3=mantissa_low
} bitfield_ctx_t;

static void pick_bitfield(uint64_t *rng, uint64_t total_dyn,
                          fi_fault_spec_t *out, void *ctx) {
    const bitfield_ctx_t *c = (const bitfield_ctx_t *)ctx;
    out->n_events = 1;
    out->mbu_width = 1;
    out->steps[0] = 1 + (splitmix64(rng) % total_dyn);
    switch (c->field) {
        case 0:  out->bits[0] = 63; break;                                        // 符号位
        case 1:  out->bits[0] = 52 + (uint32_t)(splitmix64(rng) % 11); break;      // 阶码
        case 2:  out->bits[0] = 32 + (uint32_t)(splitmix64(rng) % 20); break;      // 高位尾数
        default: out->bits[0] = (uint32_t)(splitmix64(rng) % 32); break;           // 低位尾数
    }
}

static int run_bitfield_sensitivity(const char *policy_name) {
    suite_workspace_t ws;
    alloc_and_init_workspace(&ws, 1);
    install_crash_handlers();

    const char *field_names[4] = {
        "Sign (Bit 63)",
        "Exponent (Bits 52-62)",
        "Mantissa_High (Bits 32-51)",
        "Mantissa_Low (Bits 0-31)"
    };
    const int trials_per_bench = 100;

    // 代表性子集由注册表的 BTAG_REPR 标签决定，不再写死下标
    int targets[NUM_BENCHES];
    int n_targets = bench_select_tagged(BTAG_REPR, targets, NUM_BENCHES);

    for (int f = 0; f < 4; f++) {
        fi_stats_t agg;
        memset(&agg, 0, sizeof(agg));
        bitfield_ctx_t ctx = { f };

        for (int bi = 0; bi < n_targets; bi++) {
            int b = targets[bi];
            fi_stats_t st;
            uint64_t seed = 0x55aa55aa12345678ULL ^ ((uint64_t)(f * 17 + b) << 24);
            fi_run_campaign(&ws, b, policy_name, trials_per_bench, seed,
                            pick_bitfield, &ctx, &st);
            agg.trials += st.trials;
            agg.masked += st.masked;
            agg.det_rec += st.det_rec;
            agg.benign += st.benign;
            agg.sdc += st.sdc;
            agg.due += st.due;
        }

        int denom = agg.trials > 0 ? agg.trials : 1;
        printf("BITFIELD_CSV,%s,%s,%d,%.2f,%.2f,%.2f,%.2f,%.2f\n",
               policy_name, field_names[f], agg.trials,
               100.0 * agg.masked / denom,
               100.0 * agg.det_rec / denom,
               100.0 * agg.benign / denom,
               100.0 * agg.sdc / denom,
               100.0 * agg.due / denom);
    }

    free_workspace(&ws);
    return 0;
}

// ============================================================================
// 模式 5 (RQ6): 空间多比特翻转 (Spatial MBU: 1/2/4-bit) 与时间窗口连续故障冲击
// ============================================================================
typedef struct {
    uint32_t mbu_width;   // 空间多比特宽度：1=SEU, 2=DBU, 4=QBU
    int      k_events;    // 时间维连发故障数
} stress_ctx_t;

static void pick_stress(uint64_t *rng, uint64_t total_dyn,
                        fi_fault_spec_t *out, void *ctx) {
    const stress_ctx_t *c = (const stress_ctx_t *)ctx;
    int k = c->k_events > 0 ? c->k_events : 1;
    if (k > FI_MAX_EVENTS) k = FI_MAX_EVENTS;
    out->n_events  = k;
    out->mbu_width = c->mbu_width > 0 ? c->mbu_width : 1;
    for (int e = 0; e < k; e++) {
        out->steps[e] = 1 + (splitmix64(rng) % total_dyn);
        // MBU 模式下基位要留出相邻宽度的空间，故限制在 [0,60)
        out->bits[e]  = (uint32_t)(splitmix64(rng) % (out->mbu_width > 1 ? 60 : 64));
    }
}

static void stress_report(const char *policy_name, const char *group,
                          const char *label, const fi_stats_t *agg) {
    int denom = agg->trials > 0 ? agg->trials : 1;
    int ok = agg->masked + agg->det_rec + agg->benign;
    printf("STRESS_CSV,%s,%s,%s,%d,%.2f,%.2f,%.2f,%.2f\n",
           policy_name, group, label, agg->trials,
           100.0 * ok / denom,
           100.0 * agg->det_rec / denom,
           100.0 * agg->sdc / denom,
           100.0 * agg->due / denom);
}

static int run_mbu_and_burst_stress(const char *policy_name) {
    suite_workspace_t ws;
    alloc_and_init_workspace(&ws, 1);
    install_crash_handlers();

    int targets[NUM_BENCHES];
    int n_targets = bench_select_tagged(BTAG_REPR, targets, NUM_BENCHES);

    // 每个负载的试验数与 k 的取值都可配：P(F|n) 的置信区间宽度直接取决于
    // 试验数，而 n 的采样点决定 Ostler 求和的插值精度。
    const char *v = getenv("ATFT_STRESS_TRIALS");
    int trials_per_bench = (v && *v) ? atoi(v) : 80;
    if (trials_per_bench <= 0) trials_per_bench = 80;

    int burst_counts[FI_MAX_EVENTS];
    int n_burst = 0;
    if ((v = getenv("ATFT_BURST_COUNTS")) && *v) {
        const char *p = v;
        while (*p && n_burst < FI_MAX_EVENTS) {
            int k = atoi(p);
            if (k > 0 && k <= FI_MAX_EVENTS) burst_counts[n_burst++] = k;
            while (*p && *p != ',') p++;
            if (*p == ',') p++;
        }
    }
    if (n_burst == 0) {
        const int defaults[4] = {1, 2, 4, 8};
        for (n_burst = 0; n_burst < 4; n_burst++) burst_counts[n_burst] = defaults[n_burst];
    }

    // (A) 空间多比特翻转：1-bit SEU / 2-bit DBU / 4-bit QBU（每次运行 1 个事件）
    const uint32_t mbu_widths[3] = {1, 2, 4};
    const char *mbu_labels[3] = {"SEU (1-bit)", "DBU (2-bit Adjacent)", "QBU (4-bit Burst)"};
    for (int mw = 0; mw < 3; mw++) {
        fi_stats_t agg;
        memset(&agg, 0, sizeof(agg));
        stress_ctx_t ctx = { mbu_widths[mw], 1 };
        for (int bi = 0; bi < n_targets; bi++) {
            int b = targets[bi];
            fi_stats_t st;
            uint64_t seed = 0x9988776655443322ULL ^ ((uint64_t)(mw * 31 + b) << 20);
            fi_run_campaign(&ws, b, policy_name, trials_per_bench, seed,
                            pick_stress, &ctx, &st);
            agg.trials += st.trials; agg.masked += st.masked; agg.det_rec += st.det_rec;
            agg.benign += st.benign; agg.sdc += st.sdc; agg.due += st.due;
        }
        stress_report(policy_name, "Spatial_MBU", mbu_labels[mw], &agg);
    }

    // (B) 时间维连发故障：单次运行内注入 k 个独立故障，k 由配置给出
    for (int bc = 0; bc < n_burst; bc++) {
        fi_stats_t agg;
        memset(&agg, 0, sizeof(agg));
        stress_ctx_t ctx = { 1, burst_counts[bc] };
        char label[48];
        snprintf(label, sizeof(label), "k=%d Fault%s/Run", burst_counts[bc],
                 burst_counts[bc] == 1 ? "" : "s");
        for (int bi = 0; bi < n_targets; bi++) {
            int b = targets[bi];
            fi_stats_t st;
            uint64_t seed = 0xabcdef0123456789ULL ^ ((uint64_t)(bc * 43 + b) << 20);
            fi_run_campaign(&ws, b, policy_name, trials_per_bench, seed,
                            pick_stress, &ctx, &st);
            agg.trials += st.trials; agg.masked += st.masked; agg.det_rec += st.det_rec;
            agg.benign += st.benign; agg.sdc += st.sdc; agg.due += st.due;
        }
        stress_report(policy_name, "Temporal_Burst", label, &agg);
    }

    free_workspace(&ws);
    return 0;
}

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

// 导出基准注册表，供 Python 侧聚合脚本读取（避免套件归属在两处各写一遍）
// ----------------------------------------------------------------------------
// D3: 无故障近似误差包络剖析
//
// 用 -atft-delta-profile 构建出的二进制跑一遍无故障执行，表决点会把
// |R - A| / (1 + |A|) 的最大值累积到运行时。每个负载一行，供上层定出
//   delta_f = max(alpha * E_f, kappa * u_fp32)
// 这里只报原始包络 E_f，alpha / kappa 留给上层做敏感性消融。
// ----------------------------------------------------------------------------
static int run_delta_profile(void) {
    suite_workspace_t ws;
    const char *small_env = getenv("ATFT_PROFILE_SMALL");
    int profile_small = small_env && *small_env && atoi(small_env) != 0;
    alloc_and_init_workspace(&ws, profile_small);
    const char *variant = getenv("ATFT_PROFILE_VARIANT");
    if (variant && *variant) {
        // 只用于独立 profile，不影响默认 campaign。scaled 改变输入幅度，
        // 用来验证阈值是否只适配某一个固定数值范围。
        if (strcmp(variant, "scaled") == 0) {
            const double scale = 3.0;
            int nm2 = ws.n_mat * ws.n_mat;
            for (int i = 0; i < nm2; i++) {
                ws.A[i] *= scale;
                ws.B[i] *= scale;
                ws.C[i] *= scale;
                ws.D[i] *= scale;
                ws.jac_A[i] *= scale;
                ws.jac_B[i] *= scale;
            }
            for (int i = 0; i < ws.n_mat; i++) {
                ws.x[i] *= scale;
                ws.p_bicg[i] *= scale;
                ws.r_bicg[i] *= scale;
            }
            for (int i = 0; i < ws.n_lu * ws.n_lu; i++) ws.lu_A0[i] *= scale;
            for (int i = 0; i < ws.n_lu; i++) ws.lu_b[i] *= scale;
            // Durbin 的相关序列需要满足稳定性条件；scaled_safe 保持它不变，
            // scaled 则作为越界输入压力测试，观察阈值在失稳场景下的行为。
            if (strcmp(variant, "scaled_safe") != 0) {
                for (int i = 0; i < ws.n_dur; i++) ws.dur_r[i] *= scale;
            }
            for (int i = 0; i < ws.der_w * ws.der_h; i++) ws.der_in[i] *= scale;
            for (int i = 0; i < ws.n_opts; i++) {
                ws.bs_spt[i] *= scale;
                ws.bs_strike[i] *= scale;
                ws.bs_rate[i] *= scale;
                ws.bs_vol[i] *= scale;
                ws.bs_time[i] *= scale;
            }
            for (int i = 0; i < ws.n_fft; i++) {
                ws.fft_re[i] *= scale;
                ws.fft_im[i] *= scale;
            }
        }
    }

    printf("benchmark,env_max_rel_dev,observations\n");
    for (int b = 0; b < NUM_BENCHES; b++) {
        atft_init(10000.0, 4.0, 3, 4, 3);
        atft_set_delta(-1.0);
        atft_delta_profile_reset();
        reset_mutable_buffers(&ws, b);
        invoke_benchmark(&ws, b);
        printf("%s,%.9e,%llu\n", bench_name(b), atft_delta_profile_max(),
               (unsigned long long)g_atft_delta_obs_cnt);
    }

    free_workspace(&ws);
    return 0;
}

// ----------------------------------------------------------------------------
// D6: 模式切换判决规则的蒙特卡洛评估
//
// 被评估的两套规则都来自 runtime/atft_runtime.c 的同一份实现（不在这里另写一
// 遍），只是初始化方式不同：
//   legacy : 连续 N 次短间隔 + 窗口累计阈值 + M 个平静窗口降档（五个拍定常数）
//   sprt   : Wald 序贯概率比检验，alpha / beta 是指定的误报与漏检上界
//
// 到达过程按轨道剖面生成：平静段速率 lambda0、SAA 过境段 lambda1 = r*lambda0，
// 过境驻留与轨道周期取 results/rq9_orbit_profile.csv 选中的那一行（b=20000 nT），
// 速率比 r 取 temporal.saa_ratio 的扫描点。时间折算成退休指令数喂给检测器。
//
// 报的是三件事：平静段误报率、过境段检出率与检测延迟、过境后回到 LOW 的滞后。
// 这三项合起来才是"切换规则好不好"的完整判据。
// ----------------------------------------------------------------------------
static double exp_sample(uint64_t *rng, double rate_per_min) {
    double u = (double)(splitmix64(rng) >> 11) / 9007199254740992.0; // [0,1)
    if (u <= 1e-18) u = 1e-18;
    return -log(u) / rate_per_min;
}

static int run_switch_eval(void) {
    const double ips = 1.0e9;        // 指令/分钟，仅用于把时间折算成指令计数
    double dwell_min = 8.29;         // SAA 过境驻留（rq9_orbit_profile.csv，20000 nT）
    double period_min = 96.58;       // 轨道周期
    double events_in_dwell = 9.0;    // 过境窗口内的期望事件数（标称速率比下）
    int transits = 2000;
    const char *v;
    if ((v = getenv("ATFT_SWITCH_TRANSITS")) && *v) transits = atoi(v);
    if ((v = getenv("ATFT_SWITCH_DWELL_MIN")) && *v) dwell_min = atof(v);
    if ((v = getenv("ATFT_SWITCH_EVENTS")) && *v) events_in_dwell = atof(v);

    double ratios[8] = {4.0, 25.0, 100.0, 200.0, 450.0};
    int n_ratios = 5;

    printf("detector,param,rate_ratio,transits,false_alarm_pct,detect_pct,"
           "mean_delay_min,mean_events_to_detect,mean_recovery_lag_min,"
           "events_to_decide_theory\n");

    for (int ri = 0; ri < n_ratios; ri++) {
        double r = ratios[ri];
        double lam1 = events_in_dwell / dwell_min;   // 过境段速率（事件/分钟）
        double lam0 = lam1 / r;                      // 平静段速率
        double calm_interval_inst = ips / lam0;      // 检测器看到的 T_calm（指令）
        double calm_min = period_min - dwell_min;

        // detector: 0..3 = legacy with window = dwell/{8,4,2,1}; 4..5 = sprt alpha=beta={0.01,0.05}
        for (int d = 0; d < 6; d++) {
            uint64_t rng = 0xA5A5C0FFEEULL + (uint64_t)(ri * 97 + d);
            int fa = 0, detected = 0, recovered = 0;
            double sum_delay = 0.0, sum_events = 0.0, sum_recov = 0.0;
            double win_min = dwell_min / (double)(1 << (3 - (d < 4 ? d : 0)));
            double alpha = (d == 4) ? 0.01 : 0.05;

            for (int t = 0; t < transits; t++) {
                if (d < 4) atft_init(calm_interval_inst, 4.0, 3, 4, 3);
                else       atft_init_sprt(calm_interval_inst, r, alpha, alpha);

                double next_win = win_min;
                double clock = 0.0;
                // --- 平静段 ---
                double tnext = exp_sample(&rng, lam0);
                int alarmed_in_calm = 0;
                while (clock + tnext < calm_min) {
                    clock += tnext;
                    while (d < 4 && clock > next_win) { atft_step_window(); next_win += win_min; }
                    atft_advance_insts((uint64_t)(tnext * ips));
                    atft_report_ecc_event();
                    if (atft_get_mode() == ATFT_MODE_HIGH) alarmed_in_calm = 1;
                    tnext = exp_sample(&rng, lam0);
                }
                if (alarmed_in_calm) fa++;

                // --- SAA 过境段 ---
                double saa_t = 0.0;
                int ev = 0, det = 0, det_events = 0;
                double delay = 0.0;
                // 过境段开始前先把残余的平静段时间走完
                atft_advance_insts((uint64_t)((calm_min - clock) * ips));
                clock = calm_min;
                int mode_at_entry = (int)atft_get_mode();
                tnext = exp_sample(&rng, lam1);
                while (saa_t + tnext < dwell_min) {
                    saa_t += tnext;
                    while (d < 4 && (calm_min + saa_t) > next_win) { atft_step_window(); next_win += win_min; }
                    atft_advance_insts((uint64_t)(tnext * ips));
                    atft_report_ecc_event();
                    ev++;
                    if (!det && atft_get_mode() == ATFT_MODE_HIGH) {
                        det = 1;
                        delay = saa_t;
                        det_events = ev;
                    }
                    tnext = exp_sample(&rng, lam1);
                }
                if (mode_at_entry == (int)ATFT_MODE_LOW && det) {
                    detected++;
                    sum_delay += delay;
                    sum_events += det_events;
                }

                // --- 过境后平静段：量回到 LOW 的滞后 ---
                if (atft_get_mode() == ATFT_MODE_HIGH) {
                    double post = 0.0;
                    tnext = exp_sample(&rng, lam0);
                    while (post < calm_min) {
                        post += tnext;
                        while (d < 4 && (calm_min + dwell_min + post) > next_win) {
                            atft_step_window(); next_win += win_min;
                        }
                        atft_advance_insts((uint64_t)(tnext * ips));
                        atft_report_ecc_event();
                        if (atft_get_mode() == ATFT_MODE_LOW) break;
                        tnext = exp_sample(&rng, lam0);
                    }
                    if (atft_get_mode() == ATFT_MODE_LOW) {
                        recovered++;
                        sum_recov += post;
                    }
                }
            }

            double gain = log(r) - (r - 1.0) / r;
            double n_theory = (d >= 4) ? (log((1.0 - alpha) / alpha) / gain) : 0.0;
            char param[64];
            if (d < 4) snprintf(param, sizeof(param), "window=%.2fmin", win_min);
            else       snprintf(param, sizeof(param), "alpha=beta=%.2f", alpha);
            printf("%s,%s,%.0f,%d,%.2f,%.2f,%.3f,%.2f,%.3f,%.2f\n",
                   d < 4 ? "legacy" : "sprt", param, r, transits,
                   100.0 * fa / transits,
                   100.0 * detected / transits,
                   detected ? sum_delay / detected : -1.0,
                   detected ? sum_events / detected : -1.0,
                   recovered ? sum_recov / recovered : -1.0,
                   n_theory);
        }
    }
    return 0;
}

static int list_benchmarks(void) {    printf("name,suite,kind,tags\n");
    for (int b = 0; b < NUM_BENCHES; b++) {
        printf("%s,%s,%s,%s%s%s\n",
               BENCHES[b].name, BENCHES[b].suite,
               BENCHES[b].kind == BK_INT ? "int" : "fp",
               (BENCHES[b].tags & BTAG_REPR) ? "repr" : "",
               ((BENCHES[b].tags & BTAG_REPR) && (BENCHES[b].tags & BTAG_AEROSPACE)) ? "|" : "",
               (BENCHES[b].tags & BTAG_AEROSPACE) ? "aerospace" : "");
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        return 1;
    }
    if (strcmp(argv[1], "--list-benchmarks") == 0) {
        return list_benchmarks();
    }
    eval_config_from_env();
    load_delta_table();
    if (strcmp(argv[1], "--delta-profile") == 0) {
        return run_delta_profile();
    }
    if (strcmp(argv[1], "--switch-eval") == 0) {
        return run_switch_eval();
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
