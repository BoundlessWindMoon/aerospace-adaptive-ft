// 微基准：证伪"x86 FP TMR 地板 ~4.4×"。对比三种冗余布局在本机(AVX-512 空闲)上的开销。
// kernel = 3 阶 Horner map(无跨迭代依赖，可向量化)。
//   native     : 标量循环(编译器自由向量化)——基线
//   inlane_dmr : 每元素 {x,x} 塞进 <2 x double>(128b)，两 lane 互为冗余——现 Pass 的布局
//   wide_dmr   : 每步 4 实元 + 4 冗余塞进 zmm<8 x double>(512b)，低半/高半比对——建议布局
// 若 wide 远低于 inlane、逼近甚至优于 native，则"地板"是布局选错，不是 x86 固有。
#include <immintrin.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define N 4096
#define ITERS 200000

static inline uint64_t rdtsc(void) {
    unsigned hi, lo;
    __asm__ __volatile__("lfence; rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static const double C0 = 1.5, C1 = -0.3, C2 = 0.7, C3 = 0.11;

// Horner 3 阶，标量
static inline double horner(double x) {
    return ((C3 * x + C2) * x + C1) * x + C0;
}

double x[N], y[N];
volatile uint64_t g_sig; // 防优化掉签名

static uint64_t run_native(void) {
    for (int i = 0; i < N; i++) y[i] = horner(x[i]);
    uint64_t s = 0;
    for (int i = 0; i < N; i++) { uint64_t b; __builtin_memcpy(&b, &y[i], 8); s ^= b; }
    return s;
}

// in-lane DMR：每元素复制进 128b 两 lane，算一遍，lane0 存回，XOR 签名累 2 lane 低位
static uint64_t run_inlane(void) {
    __m128d sig = _mm_setzero_pd();
    __m128d c0 = _mm_set1_pd(C0), c1 = _mm_set1_pd(C1),
            c2 = _mm_set1_pd(C2), c3 = _mm_set1_pd(C3);
    for (int i = 0; i < N; i++) {
        __m128d vx = _mm_set1_pd(x[i]);            // 每值广播(lane-build 税)
        __m128d r = _mm_fmadd_pd(c3, vx, c2);
        r = _mm_fmadd_pd(r, vx, c1);
        r = _mm_fmadd_pd(r, vx, c0);
        y[i] = _mm_cvtsd_f64(r);                   // 取 lane0 存回(extract 税)
        sig = _mm_xor_pd(sig, r);
    }
    uint64_t a, b;
    __m128d lo = sig;
    __builtin_memcpy(&a, &lo, 8);
    __m128d hi = _mm_unpackhi_pd(sig, sig);
    __builtin_memcpy(&b, &hi, 8);
    return a ^ b; // 两 lane 一致则=0 的结构(此处仅取用，防优化)
}

// wide DMR：每步 4 实元装进 zmm 低半，_mm512_broadcast_f64x4 一条指令造出高半冗余，
// 8 lane 一起算(4 实 + 4 冗余)，低半/高半比对，低半存回。lane-build 从每值降到每 4 元一条。
static uint64_t run_wide(void) {
    __m512d sig = _mm512_setzero_pd();
    __m512d c0 = _mm512_set1_pd(C0), c1 = _mm512_set1_pd(C1),
            c2 = _mm512_set1_pd(C2), c3 = _mm512_set1_pd(C3);
    for (int i = 0; i < N; i += 4) {
        __m256d x4 = _mm256_loadu_pd(&x[i]);           // 4 实元
        __m512d vx = _mm512_broadcast_f64x4(x4);       // {x0..x3,x0..x3} 一条指令
        __m512d r = _mm512_fmadd_pd(c3, vx, c2);
        r = _mm512_fmadd_pd(r, vx, c1);
        r = _mm512_fmadd_pd(r, vx, c0);
        __m256d rlo = _mm512_extractf64x4_pd(r, 0);
        _mm256_storeu_pd(&y[i], rlo);                  // 低半存回
        sig = _mm512_xor_pd(sig, r);
    }
    __m256d slo = _mm512_extractf64x4_pd(sig, 0);
    __m256d shi = _mm512_extractf64x4_pd(sig, 1);
    __m256d d = _mm256_xor_pd(slo, shi);               // 低半^高半，一致则全 0
    uint64_t acc = 0, tmp[4];
    _mm256_storeu_pd((double *)tmp, d);
    for (int k = 0; k < 4; k++) acc ^= tmp[k];
    return acc;
}

// scalar_dmr：不手动 SIMD。算两条独立标量链(主/影)，每元素 XOR 签名累主^影，
// 循环出口比一次，主链存回。全标量 → 交给 clang -O3 的循环向量化器自己加宽。
// 验证"Pass 只需发标量复制+签名、让 -O3 向量化"能否拿到 wide 收益。
static uint64_t run_scalar_dmr(void) {
    uint64_t sig = 0;
    for (int i = 0; i < N; i++) {
        double xi = x[i];
        double a = ((C3 * xi + C2) * xi + C1) * xi + C0; // 主
        double b = ((C3 * xi + C2) * xi + C1) * xi + C0; // 影(独立链)
        y[i] = a;
        uint64_t ab, bb;
        __builtin_memcpy(&ab, &a, 8);
        __builtin_memcpy(&bb, &b, 8);
        sig ^= (ab ^ bb);
    }
    return sig; // 无故障恒 0
}

static uint64_t bench(uint64_t (*fn)(void), const char *name) {
    uint64_t sink = 0;
    for (int w = 0; w < 1000; w++) sink ^= fn();       // warmup
    uint64_t t0 = rdtsc();
    for (int it = 0; it < ITERS; it++) sink ^= fn();
    uint64_t t1 = rdtsc();
    double cyc_per_elem = (double)(t1 - t0) / ((double)ITERS * N);
    printf("%-12s %8.3f cyc/elem   (sink=%llu)\n", name, cyc_per_elem,
           (unsigned long long)sink);
    return t1 - t0;
}

int main(void) {
    for (int i = 0; i < N; i++) x[i] = 0.001 * (i % 997) + 0.5;
    uint64_t tn = bench(run_native, "native");
    uint64_t ti = bench(run_inlane, "inlane_dmr");
    uint64_t ts = bench(run_scalar_dmr, "scalar_dmr");
    uint64_t tw = bench(run_wide, "wide_dmr");
    printf("\ninlane/native=%.2fx  scalar_dmr/native=%.2fx  wide/native=%.2fx\n",
           (double)ti / tn, (double)ts / tn, (double)tw / tn);
    return 0;
}

