// semiring_check_kernel.c — Inno 4 在真实内核 + 传播故障模型下的验证
//
// 相对 numpy 探底(只翻最终输出、无传播)，这里更接近真实软错误：
//   在 Floyd-Warshall 的 O(n^3) 计算"中途"翻一个距离元素的比特，让其后的 pivot
//   步把错误传播开，再在最终输出上用三角不等式检查。并测"检出→重执行"的端到端
//   残余 SDC(确定性内核重跑必然恢复)。
//
// 图初始化与 eval_suite 的 floyd 一致(i*j%7+1，带 999 稀疏权重，对角 0)。
// 用法：gcc -O3 -march=native experiments/semiring_check_kernel.c -o build/skk && ./build/skk
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define N 70

static uint64_t rng = 0x9e3779b97f4a7c15ULL;
static uint64_t xr(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return rng; }

static void init_graph(int32_t *p) {
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++) {
            int32_t v = (int32_t)(i * j % 7 + 1);
            if ((i + j) % 13 == 0 || (i + j) % 7 == 0 || (i + j) % 11 == 0) v = 999;
            p[i * N + j] = (i == j) ? 0 : v;
        }
}

// Floyd-Warshall（min-plus）。inj_k>=0 时在 pivot 步 k==inj_k 前翻 p[inj_ij] 的第 inj_b 位
static void floyd(int32_t *p, int inj_k, int inj_ij, int inj_b) {
    for (int k = 0; k < N; k++) {
        if (k == inj_k) p[inj_ij] ^= (1 << inj_b);
        for (int i = 0; i < N; i++)
            for (int j = 0; j < N; j++) {
                int32_t t = p[i * N + k] + p[k * N + j];
                if (t < p[i * N + j]) p[i * N + j] = t;
            }
    }
}

// 三角不等式检查：t 个随机 pivot（t<=0 即全量 N 个）。违反返回 1。
static int triangle_viol(const int32_t *D, int t) {
    for (int s = 0; s < (t > 0 ? t : N); s++) {
        int k = (t > 0) ? (int)(xr() % N) : s;
        for (int i = 0; i < N; i++)
            for (int j = 0; j < N; j++)
                if (D[i * N + j] > D[i * N + k] + D[k * N + j]) return 1;
    }
    return 0;
}

int main(int argc, char **argv) {
    int trials = argc > 1 ? atoi(argv[1]) : 6000;
    int32_t g[N * N], w[N * N], clean[N * N];
    init_graph(clean);
    memcpy(g, clean, sizeof(g));
    floyd(g, -1, 0, 0);                         // golden

    const char *names[2] = {"low(0-10)", "all(0-31)"};
    int blo[2] = {0, 0}, bhi[2] = {11, 31};
    for (int mode = 0; mode < 2; mode++) {
        // mode 0 = 计算中途注入(传播)；mode 1 = 只翻最终输出(无传播，= numpy 探底口径)
        printf("== %s ==\n", mode == 0 ? "计算中途注入（传播）" : "只翻最终输出（无传播）");
        for (int cat = 0; cat < 2; cat++) {
            int fa = 0;
            long sdc = 0, det_full = 0, det8 = 0, resid = 0;
            for (int t = 0; t < trials; t++) {
                int ik = (int)(xr() % N), ii = (int)(xr() % (N * N));
                int b = blo[cat] + (int)(xr() % (bhi[cat] - blo[cat] + 1));
                memcpy(w, clean, sizeof(w));
                if (mode == 0) {
                    floyd(w, ik, ii, b);
                } else {
                    floyd(w, -1, 0, 0);
                    w[ii] ^= (1 << b);
                }
                if (memcmp(w, g, sizeof(g)) == 0) {
                    if (triangle_viol(w, 0)) fa++;
                    continue;
                }
                sdc++;
                int df = triangle_viol(w, 0), d8 = triangle_viol(w, 8);
                if (df) det_full++;
                if (d8) det8++;
                if (!df) resid++;
            }
            double s = sdc ? sdc : 1;
            printf("  %-10s  SDC=%ld  检出(全量)=%.1f%%  检出(8 pivot)=%.1f%%  "
                   "检出+重跑后残余 SDC=%.2f%%  误报=%d\n",
                   names[cat], sdc, det_full / s * 100, det8 / s * 100,
                   resid / s * 100, fa);
        }
    }
    return 0;
}
