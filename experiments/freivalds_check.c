// freivalds_check.c — 通用 Freivalds 结果校验原语 + 少核保护（Cholesky / LU）
//
// 回应"能不能通用"：不给每个核手写检查器，抽出一个原语——随机向量 r 探测"定义恒等式"
// A0·r ?= factored(r)，O(n²) vs 分解 O(n³)，与算法正交。每个核只提供自己的 factored()
// 闭包（Cholesky: L·Lᵀr；LU: L·Ur）。对整个线性代数类通用，恒等式由核的代数类型决定；
// 任意非线性码回落 coarse。
#include "benchmarks.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <time.h>

static int N;
static double *A0, *work, *rr, *t1, *t2, *tmp;
static uint64_t g = 0x9e3779b97f4a7c15ULL;
static uint64_t xr(void){ g^=g<<13; g^=g>>7; g^=g<<17; return g; }
static double now_s(void){ struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return t.tv_sec+t.tv_nsec*1e-9; }

typedef void (*matvec_fn)(const double *r, double *out);   // out = M·r

// 通用原语：max over NV 个随机 r 的 ||ref(r) - test(r)|| / ||ref(r)||
static double freivalds_probe(matvec_fn ref, matvec_fn test, int NV){
    double worst=0;
    for(int v=0; v<NV; v++){
        for(int i=0;i<N;i++) rr[i]=(double)((xr()%1000))/1000.0 - 0.5;
        ref(rr, t1); test(rr, t2);
        double num=0, den=1e-300;
        for(int i=0;i<N;i++){ double d=t1[i]-t2[i]; num+=d*d; den+=t1[i]*t1[i]; }
        double e=sqrt(num/den); if(e>worst) worst=e;
    }
    return worst;
}

static void mv_A0(const double *r, double *out){
    for(int i=0;i<N;i++){ double s=0; for(int j=0;j<N;j++) s+=A0[i*N+j]*r[j]; out[i]=s; }
}
// Cholesky: work 下三角(含对角)=L，A=L·Lᵀ → L·(Lᵀr)
static void mv_chol(const double *r, double *out){
    for(int j=0;j<N;j++){ double s=0; for(int i=j;i<N;i++) s+=work[i*N+j]*r[i]; tmp[j]=s; }        // Lᵀr
    for(int i=0;i<N;i++){ double s=0; for(int j=0;j<=i;j++) s+=work[i*N+j]*tmp[j]; out[i]=s; }     // L·(Lᵀr)
}
// LU(非主元, polybench): work 上三角(含对角)=U，严格下三角=L(单位下三角) → L·(Ur)
static void mv_lu(const double *r, double *out){
    for(int i=0;i<N;i++){ double s=0; for(int j=i;j<N;j++) s+=work[i*N+j]*r[j]; tmp[i]=s; }        // Ur
    for(int i=0;i<N;i++){ double s=tmp[i]; for(int j=0;j<i;j++) s+=work[i*N+j]*tmp[j]; out[i]=s; } // L·(Ur), L 对角=1
}

// PLACEHOLDER_MAIN

static void build_spd(void){                 // Cholesky 用：A0=B·Bᵀ + nI
    double *B=calloc((size_t)N*N,sizeof(double));
    for(int i=0;i<N;i++) for(int j=0;j<=i;j++) B[i*N+j]=(double)(((i*7+j*3)%13)+1)/13.0;
    for(int i=0;i<N;i++) for(int j=0;j<N;j++){ double s=0; for(int k=0;k<N;k++) s+=B[i*N+k]*B[j*N+k]; A0[i*N+j]=s+(i==j?(double)N:0.0); }
    free(B);
}
static void build_ddom(void){                // LU 用：对角占优，保证非主元 LU 稳定
    for(int i=0;i<N;i++){ double off=0; for(int j=0;j<N;j++){ if(i!=j){ double v=(double)(((i*5+j*11)%17)+1)/17.0; A0[i*N+j]=v; off+=fabs(v);} } A0[i*N+i]=off+1.0; }
}

static void factor(const char *k){
    memcpy(work,A0,(size_t)N*N*sizeof(double));
    if(!strcmp(k,"cholesky")) bench_pb_cholesky(N,work); else bench_pb_lu(N,work);
}

int main(int argc,char**argv){
    const char *kern = argc>1?argv[1]:"cholesky";
    N = argc>2?atoi(argv[2]):256;
    int reps = argc>3?atoi(argv[3]):30;
    int trials = argc>4?atoi(argv[4]):400;
    size_t nn=(size_t)N*N;
    A0=malloc(nn*sizeof(double)); work=malloc(nn*sizeof(double));
    rr=malloc(N*sizeof(double)); t1=malloc(N*sizeof(double)); t2=malloc(N*sizeof(double)); tmp=malloc(N*sizeof(double));
    matvec_fn test = !strcmp(kern,"cholesky") ? mv_chol : mv_lu;
    if(!strcmp(kern,"cholesky")) build_spd(); else build_ddom();

    double tf0=now_s(); for(int it=0;it<reps;it++) factor(kern); double tf=(now_s()-tf0)/reps;
    factor(kern);
    double tc0=now_s(); volatile double acc=0; for(int it=0;it<reps;it++) acc+=freivalds_probe(mv_A0,test,3); double tc=(now_s()-tc0)/reps; (void)acc;
    printf("== %s Freivalds 校验 (N=%d, 通用原语) ==\n",kern,N);
    printf("分解 %.3f ms / 校验 %.3f ms  -> 保护开销 %.3fx\n", tf*1e3, tc*1e3, (tf+tc)/tf);

    factor(kern);
    double clean=freivalds_probe(mv_A0,test,3);
    double thr = clean*1e3 + 1e-9;
    int det=0;
    for(int t=0;t<trials;t++){
        factor(kern);
        int i=(int)(xr()%N), j;
        if(!strcmp(kern,"cholesky")) j=(int)(xr()%(i+1));      // L 下三角
        else j=(int)(xr()%N);                                  // L\U 全阵都是分解输出
        size_t kk=(size_t)i*N+j;
        union{double d;uint64_t u;} f; f.d=work[kk]; f.u^=(1ULL<<(40+(xr()%12))); work[kk]=f.d;
        if(freivalds_probe(mv_A0,test,3)>thr) det++;
    }
    printf("干净残差 %.2e 阈值 %.2e  注入检出率 %.1f%% (%d/%d)\n", clean, thr, 100.0*det/trials, det, trials);
    return 0;
}