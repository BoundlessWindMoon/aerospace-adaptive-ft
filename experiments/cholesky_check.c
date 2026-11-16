// cholesky_check.c — Cholesky 的 Freivalds 式结果校验（少核 1× 保护的硬骨头实验）
//
// 可向量化但不可归约的核（Cholesky/LU/stencil）在单核上只能靠 coarse 复制付 ~2×。
// 这里用 Blum/Freivalds 结果校验绕开复制：A=L·Lᵀ 是定义恒等式，取随机 r 验
// A·r ?= L·(Lᵀr)，O(n²) vs 分解 O(n³/3) → 渐近 ~1×；且校验与分解算法正交（查等式不重跑），
// 不共享分解的故障。度量：开销比 + 注入下的检出率/误报。
#include "benchmarks.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <time.h>

static int N;
static double *A0, *work, *r, *t1, *t2;
static uint64_t g=0x9e3779b97f4a7c15ULL;
static uint64_t xr(void){ g^=g<<13; g^=g>>7; g^=g<<17; return g; }
static double now_s(void){ struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return t.tv_sec+t.tv_nsec*1e-9; }

static void build_spd(void){
    // A0 = B·Bᵀ + n·I，B 下三角，保证对称正定
    double *B=calloc((size_t)N*N,sizeof(double));
    for(int i=0;i<N;i++){ for(int j=0;j<=i;j++) B[i*N+j]=(double)(((i*7+j*3)%13)+1)/13.0; }
    for(int i=0;i<N;i++) for(int j=0;j<N;j++){ double s=0; for(int k=0;k<N;k++) s+=B[i*N+k]*B[j*N+k]; A0[i*N+j]=s + (i==j?(double)N:0.0); }
    free(B);
}
// Freivalds 残差：max over NV 个随机 r 的 ||A0·r - L·(Lᵀr)|| / ||A0·r||（多向量降漏检）
static double freivalds_resid(void){
    double worst=0;
    for(int v=0;v<3;v++){
        for(int i=0;i<N;i++) r[i]=(double)((xr()%1000))/1000.0 - 0.5;
        for(int i=0;i<N;i++){ double s=0; for(int j=0;j<N;j++) s+=A0[i*N+j]*r[j]; t1[i]=s; }   // lhs=A0·r
        for(int j=0;j<N;j++){ double s=0; for(int i=j;i<N;i++) s+=work[i*N+j]*r[i]; t2[j]=s; }   // Lᵀr
        double num=0,den=1e-300;
        for(int i=0;i<N;i++){ double s=0; for(int j=0;j<=i;j++) s+=work[i*N+j]*t2[j]; double d=t1[i]-s; num+=d*d; den+=t1[i]*t1[i]; }
        double e=sqrt(num/den); if(e>worst) worst=e;
    }
    return worst;
}

int main(int argc,char**argv){
    N = argc>1?atoi(argv[1]):256;
    int reps = argc>2?atoi(argv[2]):30;
    int trials = argc>3?atoi(argv[3]):300;
    size_t nn=(size_t)N*N;
    A0=malloc(nn*sizeof(double)); work=malloc(nn*sizeof(double));
    r=malloc(N*sizeof(double)); t1=malloc(N*sizeof(double)); t2=malloc(N*sizeof(double));
    build_spd();

    // --- 开销：分解时间 vs 校验时间 ---
    double tf0=now_s(); for(int it=0;it<reps;it++){ memcpy(work,A0,nn*sizeof(double)); bench_pb_cholesky(N,work);} double tf=(now_s()-tf0)/reps;
    memcpy(work,A0,nn*sizeof(double)); bench_pb_cholesky(N,work);
    double tc0=now_s(); volatile double acc=0; for(int it=0;it<reps;it++) acc+=freivalds_resid(); double tc=(now_s()-tc0)/reps; (void)acc;
    printf("== Cholesky Freivalds 校验 (N=%d) ==\n",N);
    printf("分解 %.3f ms / 校验 %.3f ms  -> 保护开销 %.3fx (检测)\n", tf*1e3, tc*1e3, (tf+tc)/tf);

    // --- 干净残差基线 + 注入检出 ---
    memcpy(work,A0,nn*sizeof(double)); bench_pb_cholesky(N,work);
    double clean=freivalds_resid();
    double thr = clean*1e3 + 1e-9;   // 阈值=干净残差的 1000 倍（留量级余量）
    int det=0;
    for(int t=0;t<trials;t++){
        memcpy(work,A0,nn*sizeof(double)); bench_pb_cholesky(N,work);
        int i=(int)(xr()%N), j=(int)(xr()%(i+1));        // 只翻 L 的下三角（分解真正写的部分）
        size_t k=(size_t)i*N+j;
        union{double d;uint64_t u;} f; f.d=work[k]; f.u^=(1ULL<<(40+(xr()%12))); work[k]=f.d;
        if(freivalds_resid()>thr) det++;
    }
    printf("干净残差 %.2e  阈值 %.2e\n", clean, thr);
    printf("注入检出率 %.1f%% (%d/%d)  <- 大幅值分解错误\n", 100.0*det/trials, det, trials);
    return 0;
}
