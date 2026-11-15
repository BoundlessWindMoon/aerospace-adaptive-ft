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
    return 0;
}

