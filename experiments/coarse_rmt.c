// coarse_rmt.c — 粗粒度（整内核）冗余演示器
//
// 验证三件事（都在真实 -O3 -march=native 向量化内核上）：
//   1. 零软件税：整内核跑 N 遍 = 精确 N× native 时间（每份都是满速向量化内核，
//      不插屏障、不碰内核内部 → 没有细粒度 SWIFT 的那种去优化税）。
//   2. 多核隐藏：把 N 份放到 N 个线程并行 → 墙钟 ≈ 1× native（有空闲核时）。
//   3. 检测/纠错：DMR 比对检出故障；TMR 三份多数决纠错。
//
// 以 gemm (C = alpha*A*B + beta*C, C 为原地读写) 为例：原地内核需要在每遍前把
// 输入状态 C0 还原（memcpy，O(n^2)，相对内核 O(n^3) 可忽略）——这正是自动化里
// idempotent/输入快照那一步的手工版。
#include "benchmarks.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>

static int N;
static const double ALPHA = 1.5, BETA = 1.2;
static double *A, *B, *C0, *C1, *C2, *C3;

static double now_s(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}
static size_t bytes(void) { return (size_t)N * (size_t)N * sizeof(double); }

// 一份 = 还原输入 C0 + 跑满速向量化内核
static void run_one(double *dst) {
    memcpy(dst, C0, bytes());
    bench_pb_gemm(N, N, N, ALPHA, BETA, A, B, dst);
}
typedef struct { double *dst; } targ_t;
static void *thread_run(void *p) { run_one(((targ_t *)p)->dst); return NULL; }
// PLACEHOLDER

// 并行跑 k 份（每份一个线程，各写自己的缓冲）
static void run_parallel(int k, double **dsts) {
    pthread_t th[3]; targ_t ar[3];
    for (int i = 0; i < k; i++) { ar[i].dst = dsts[i]; pthread_create(&th[i], NULL, thread_run, &ar[i]); }
    for (int i = 0; i < k; i++) pthread_join(th[i], NULL);
}

static double time_native(int reps) {
    double t0 = now_s();
    for (int r = 0; r < reps; r++) run_one(C1);
    return (now_s() - t0) / reps;
}
static double time_serial(int k, int reps) {
    double *d[3] = {C1, C2, C3};
    double t0 = now_s();
    for (int r = 0; r < reps; r++) { for (int i = 0; i < k; i++) run_one(d[i]); (void)memcmp(C1, C2, bytes()); }
    return (now_s() - t0) / reps;
}
static double time_parallel(int k, int reps) {
    double *d[3] = {C1, C2, C3};
    double t0 = now_s();
    for (int r = 0; r < reps; r++) { run_parallel(k, d); (void)memcmp(C1, C2, bytes()); }
    return (now_s() - t0) / reps;
}

// 蒙特卡洛：量 TMR 在不同故障模型下的 SDC 逃逸率（文献口径：CMF 是 TMR 的真实软肋）。
// 每轮取一个随机输出/输入元素、一个高位（阶码/高尾数）比特：
//   indep  : 只污染 1 份副本的输出（单事件瞬态）→ 另两份干净 → 多数决必纠正 → 逃逸=0
//   common : 污染 3 份共享的输入 A[k]（共模）→ 三份跑真内核后同样地错 → 多数决一致通过错误 → 逃逸
//   double : 同一元素污染 2/3 份输出（高多重）→ 两份一致 → 多数决被带偏 → 逃逸
static uint64_t g_rng = 0x243f6a8885a308d3ULL;
static uint64_t xr(void){ g_rng ^= g_rng<<13; g_rng ^= g_rng>>7; g_rng ^= g_rng<<17; return g_rng; }
static double flip_hi(double v){ union{double d; uint64_t u;} f; f.d=v; f.u ^= (1ULL << (40 + (xr()%23))); return f.d; }
static void mc_cmf(int trials, const double *gold) {
    size_t nn = (size_t)N*N;
    int esc_indep=0, esc_common=0, esc_double=0;
    for (int t=0;t<trials;t++) {
        size_t k = xr()%nn;
        // --- common-mode：污染共享输入，三份都从它算 ---
        double save = A[k]; A[k] = flip_hi(A[k]);
        run_one(C1); run_one(C2); run_one(C3);
        A[k] = save;
        for (size_t i=0;i<nn;i++){ double m=(C1[i]==C2[i]||C1[i]==C3[i])?C1[i]:(C2[i]==C3[i]?C2[i]:C1[i]); if(m!=gold[i]){esc_common++;break;} }
        // --- indep：三份干净，只翻 C1 一个输出元素 ---
        run_one(C1); run_one(C2); run_one(C3);
        size_t j = xr()%nn; C1[j] = flip_hi(C1[j]);
        for (size_t i=0;i<nn;i++){ double m=(C1[i]==C2[i]||C1[i]==C3[i])?C1[i]:(C2[i]==C3[i]?C2[i]:C1[i]); if(m!=gold[i]){esc_indep++;break;} }
        // --- double：同一元素翻 C1、C2 ---
        run_one(C1); run_one(C2); run_one(C3);
        C1[j]=flip_hi(C1[j]); C2[j]=C1[j];
        for (size_t i=0;i<nn;i++){ double m=(C1[i]==C2[i]||C1[i]==C3[i])?C1[i]:(C2[i]==C3[i]?C2[i]:C1[i]); if(m!=gold[i]){esc_double++;break;} }
    }
    printf("cmf-montecarlo (N=%d, trials=%d)  TMR SDC 逃逸率:\n", N, trials);
    printf("  indep  单副本瞬态     : %5.1f%%   <- 多数决纠正\n", 100.0*esc_indep/trials);
    printf("  common 共模(污染共享输入): %5.1f%%   <- TMR 被击穿（三份同样错）\n", 100.0*esc_common/trials);
    printf("  double 2/3 副本同元素  : %5.1f%%   <- 高多重击穿\n", 100.0*esc_double/trials);
}

int main(int argc, char **argv) {
    N = argc > 1 ? atoi(argv[1]) : 512;
    int reps = argc > 2 ? atoi(argv[2]) : 20;
    size_t nn = (size_t)N * (size_t)N;
    A = malloc(bytes()); B = malloc(bytes()); C0 = malloc(bytes());
    C1 = malloc(bytes()); C2 = malloc(bytes()); C3 = malloc(bytes());
    for (size_t i = 0; i < nn; i++) { A[i] = (double)((i % 97) + 1) / 97.0;
        B[i] = (double)((i % 89) + 1) / 89.0; C0[i] = (double)((i % 71) + 1) / 71.0; }

    double tn  = time_native(reps);
    double td1 = time_serial(2, reps);
    double tt1 = time_serial(3, reps);
    double td2 = time_parallel(2, reps);
    double tt2 = time_parallel(3, reps);

    printf("== coarse-grained whole-kernel redundancy on gemm N=%d (-O3 -march=native), reps=%d ==\n", N, reps);
    printf("native(1x)          : %.3f ms   (1.00x)\n", tn * 1e3);
    printf("DMR serial (2 runs) : %.3f ms   (%.2fx)  <- 期望 ~2.00x = 零软件税\n", td1 * 1e3, td1 / tn);
    printf("TMR serial (3 runs) : %.3f ms   (%.2fx)  <- 期望 ~3.00x = 零软件税\n", tt1 * 1e3, tt1 / tn);
    printf("DMR 2-core parallel : %.3f ms   (%.2fx)  <- 期望 ~1x 墙钟（有空闲核）\n", td2 * 1e3, td2 / tn);
    printf("TMR 3-core parallel : %.3f ms   (%.2fx)  <- 期望 ~1x 墙钟（有空闲核）\n", tt2 * 1e3, tt2 / tn);

    // 检测/纠错：跑 3 份（干净），在 C1 注入一个位翻转
    run_one(C1); run_one(C2); run_one(C3);
    union { double d; uint64_t u; } f; f.d = C1[nn / 2]; f.u ^= (1ULL << 40); C1[nn / 2] = f.d;
    int dmr_detect = (memcmp(C1, C2, bytes()) != 0);
    // TMR 逐元素多数决：majority(C1,C2,C3)；校验是否逐位恢复到干净值(C2==C3)
    long mism = 0; for (size_t i = 0; i < nn; i++) {
        double maj = (C1[i] == C2[i] || C1[i] == C3[i]) ? C1[i] : (C2[i] == C3[i] ? C2[i] : C1[i]);
        if (maj != C2[i]) mism++;
    }
    printf("fault-injection     : DMR detected=%s ; TMR corrected=%s (mismatch-after-vote=%ld)\n",
           dmr_detect ? "YES" : "NO", mism == 0 ? "YES" : "NO", mism);

    // 刁钻注入：共模 / 多副本的 SDC 逃逸率（文献 CMF 口径）
    double *gold = malloc(bytes()); run_one(gold);
    int mc_trials = argc > 3 ? atoi(argv[3]) : 300;
    mc_cmf(mc_trials, gold);
    free(gold);
    return 0;
}

