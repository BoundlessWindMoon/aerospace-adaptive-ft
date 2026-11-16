// coarse_scaling.c — 粗粒度冗余的开销标度（可复现版，修 Bug1/2）
//
// 相对 coarse_rmt.c 的演示版，这里修三处会被问倒的地方：
//   1. warmup：所有缓冲先跑一遍（untimed）再计时，消除冷启动 page fault / 频率爬升
//      对分母 native 的高估。
//   2. 核数是真参数：从 sched_getaffinity 读可用核(在 taskset -c 0-(k-1) 下即 k)，
//      pthread/fork 的每份副本显式 pin 到不同核。于是 cores 这一列对被测程序可见。
//   3. 真 fork 墙钟：tmr_fork 用 fork() 起 3 个进程并行测墙钟，不是进程内 pthread。
// 每个量取 reps 次的中位数 + p10/p90。
//
// 用法：taskset -c 0-(k-1) ./coarse_scaling [N] [reps]
// 输出：CSV 行 method,N,cores,reps,native_ms,ratio_med,ratio_p10,ratio_p90
#define _GNU_SOURCE
#include "benchmarks.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mman.h>

static int N;
static const double ALPHA = 1.5, BETA = 1.2;
static double *A, *B, *C0, *C1, *C2, *C3;
static int g_cpus[256], g_ncpu;

static double now_s(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}
static size_t bytes(void) { return (size_t)N * (size_t)N * sizeof(double); }

static void pin_to(int cpu) {
    cpu_set_t s; CPU_ZERO(&s); CPU_SET(cpu, &s);
    sched_setaffinity(0, sizeof(s), &s);
}
static void read_affinity(void) {
    cpu_set_t s; CPU_ZERO(&s);
    g_ncpu = 0;
    if (sched_getaffinity(0, sizeof(s), &s) == 0)
        for (int c = 0; c < 256 && g_ncpu < 256; c++)
            if (CPU_ISSET(c, &s)) g_cpus[g_ncpu++] = c;
    if (g_ncpu == 0) { g_cpus[0] = 0; g_ncpu = 1; }
}

// 一份 = 还原输入 C0 + 跑满速向量化内核
static void run_one(double *dst) {
    memcpy(dst, C0, bytes());
    bench_pb_gemm(N, N, N, ALPHA, BETA, A, B, dst);
}
typedef struct { double *dst; int cpu; } targ_t;
static void *thread_run(void *p) {
    targ_t *a = (targ_t *)p;
    if (a->cpu >= 0) pin_to(a->cpu);
    run_one(a->dst);
    return NULL;
}
// PLACEHOLDER_MAIN

// 并行跑 k 份（pthread，每份 pin 到不同可用核）
static double par_once(int k) {
    pthread_t th[3]; targ_t ar[3];
    double *d[3] = {C1, C2, C3};
    double t0 = now_s();
    for (int i = 0; i < k; i++) { ar[i].dst = d[i]; ar[i].cpu = g_cpus[i % g_ncpu];
        pthread_create(&th[i], NULL, thread_run, &ar[i]); }
    for (int i = 0; i < k; i++) pthread_join(th[i], NULL);
    (void)memcmp(C1, C2, bytes());
    return now_s() - t0;
}
// 串行跑 k 份（同进程）
static double serial_once(int k) {
    double *d[3] = {C1, C2, C3};
    double t0 = now_s();
    for (int i = 0; i < k; i++) run_one(d[i]);
    (void)memcmp(C1, C2, bytes());
    return now_s() - t0;
}
// 真 fork：k 个进程并行，各 pin 到不同核，父进程 wait（量墙钟）
static double fork_once(int k) {
    double t0 = now_s();
    pid_t pid[3];
    for (int i = 0; i < k; i++) {
        pid[i] = fork();
        if (pid[i] == 0) { pin_to(g_cpus[i % g_ncpu]);
            run_one(C1); _exit(0); }
    }
    for (int i = 0; i < k; i++) { int st; waitpid(pid[i], &st, 0); }
    return now_s() - t0;
}
static double native_once(void) { double t0 = now_s(); run_one(C1); return now_s() - t0; }

static int cmp_d(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return x < y ? -1 : x > y ? 1 : 0;
}
// 取 reps 次中位数/p10/p90
static void stats(double (*fn)(int), int k, int reps, double *med, double *lo, double *hi) {
    double *v = malloc(reps * sizeof(double));
    for (int r = 0; r < reps; r++) v[r] = (fn == NULL) ? native_once() : fn(k);
    qsort(v, reps, sizeof(double), cmp_d);
    *med = v[reps / 2]; *lo = v[reps / 10]; *hi = v[(9 * reps) / 10];
    free(v);
}

int main(int argc, char **argv) {
    N = argc > 1 ? atoi(argv[1]) : 512;
    int reps = argc > 2 ? atoi(argv[2]) : 15;
    size_t nn = (size_t)N * (size_t)N;
    A = malloc(bytes()); B = malloc(bytes()); C0 = malloc(bytes());
    C1 = malloc(bytes()); C2 = malloc(bytes()); C3 = malloc(bytes());
    for (size_t i = 0; i < nn; i++) { A[i] = (double)((i % 97) + 1) / 97.0;
        B[i] = (double)((i % 89) + 1) / 89.0; C0[i] = (double)((i % 71) + 1) / 71.0; }
    read_affinity();

    // warmup：触碰所有缓冲 + 频率爬升，消除冷启动偏置（Bug1）
    for (int w = 0; w < 3; w++) { run_one(C1); run_one(C2); run_one(C3); }

    double nm, nlo, nhi; stats(NULL, 0, reps, &nm, &nlo, &nhi);
    struct { const char *name; double (*fn)(int); int k; } jobs[] = {
        {"dmr_serial", serial_once, 2}, {"tmr_serial", serial_once, 3},
        {"dmr_par", par_once, 2}, {"tmr_par", par_once, 3}, {"tmr_fork", fork_once, 3},
    };
    printf("native,%d,%d,%d,%.4f,1.000,1.000,1.000\n", N, g_ncpu, reps, nm * 1e3);
    for (unsigned j = 0; j < sizeof(jobs) / sizeof(jobs[0]); j++) {
        double med, lo, hi; stats(jobs[j].fn, jobs[j].k, reps, &med, &lo, &hi);
        printf("%s,%d,%d,%d,%.4f,%.3f,%.3f,%.3f\n", jobs[j].name, N, g_ncpu, reps,
               med * 1e3, med / nm, lo / nm, hi / nm);
    }
    return 0;
}

