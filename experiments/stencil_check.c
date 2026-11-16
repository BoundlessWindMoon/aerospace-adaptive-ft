// stencil_check.c — stencil 的"离散极值原理"不变量校验（少核 1× 保护，最后一类硬骨头）
//
// jacobi-2d: B=0.2·(5 邻居)，权重和=1 且非负 → 凸组合 → 离散极值原理：更新值必落在上一场
// 的 [min,max] 内。于是输出整场 ∈ [min(A0),max(A0)]。O(n) 扫描即可查越界，~1×，且界来自
// 定理(极值原理)而非拍脑袋常数。只抓越界(大幅值)错误，界内小扰动割让——与一切范围校验同理。
#include "benchmarks.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <time.h>

static uint64_t g=0x2545f4914f6cdd1dULL;
static uint64_t xr(void){ g^=g<<13; g^=g>>7; g^=g<<17; return g; }
static double now_s(void){ struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return t.tv_sec+t.tv_nsec*1e-9; }

int main(int argc,char**argv){
    int N = argc>1?atoi(argv[1]):256;
    int tsteps = argc>2?atoi(argv[2]):40;
    int reps = argc>3?atoi(argv[3]):20;
    int trials = argc>4?atoi(argv[4]):400;
    size_t nn=(size_t)N*N;
    double *A=malloc(nn*sizeof(double)), *B=malloc(nn*sizeof(double)), *A0=malloc(nn*sizeof(double));
    for(size_t i=0;i<nn;i++) A0[i]=(double)((xr()%1000))/1000.0;   // 初始场 ∈ [0,1)
    double lo=1e300, hi=-1e300;
    for(size_t i=0;i<nn;i++){ if(A0[i]<lo)lo=A0[i]; if(A0[i]>hi)hi=A0[i]; }
    double eps=(hi-lo)*1e-9+1e-12;

    // 开销：kernel vs O(n) 极值原理扫描
    double tf0=now_s(); for(int it=0;it<reps;it++){ memcpy(A,A0,nn*sizeof(double)); memset(B,0,nn*sizeof(double)); bench_pb_jacobi2d(tsteps,N,A,B);} double tf=(now_s()-tf0)/reps;
    double tc0=now_s(); volatile int acc=0; for(int it=0;it<reps;it++){ int bad=0; for(size_t i=0;i<nn;i++) if(A[i]<lo-eps||A[i]>hi+eps){bad=1;break;} acc+=bad;} double tc=(now_s()-tc0)/reps; (void)acc;
    printf("== jacobi-2d 极值原理校验 (N=%d, tsteps=%d) ==\n",N,tsteps);
    printf("kernel %.3f ms / 校验 %.4f ms  -> 保护开销 %.3fx\n", tf*1e3, tc*1e3, (tf+tc)/tf);

    // 干净基线（应当全部在界内）
    memcpy(A,A0,nn*sizeof(double)); memset(B,0,nn*sizeof(double)); bench_pb_jacobi2d(tsteps,N,A,B);
    int clean_viol=0; for(size_t i=0;i<nn;i++) if(A[i]<lo-eps||A[i]>hi+eps) clean_viol++;

    // 注入：翻一个内部输出元素的阶码位(52-62)→ 幅值量级改变，极值原理应抓到。
    // 注：尾数位翻转若仍落在 [min,max] 内，范围类校验结构上抓不到（见输出说明）。
    int det=0;
    for(int t=0;t<trials;t++){
        memcpy(A,A0,nn*sizeof(double)); memset(B,0,nn*sizeof(double)); bench_pb_jacobi2d(tsteps,N,A,B);
        int i=1+(int)(xr()%(N-2)), j=1+(int)(xr()%(N-2)); size_t k=(size_t)i*N+j;
        union{double d;uint64_t u;} f; f.d=A[k]; f.u^=(1ULL<<(52+(xr()%11))); A[k]=f.d;
        int bad=0; for(size_t x=0;x<nn;x++) if(A[x]<lo-eps||A[x]>hi+eps){bad=1;break;}
        if(bad) det++;
    }
    printf("干净越界 %d (应为0)  界[%.3f,%.3f]  阶码翻转检出率 %.1f%% (%d/%d)\n",
           clean_viol, lo, hi, 100.0*det/trials, det, trials);
    printf("  (尾数翻转若仍落界内→范围类校验结构漏检；这类通常也在容差内)\n");
    return 0;
}
