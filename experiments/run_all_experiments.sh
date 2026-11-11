#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="/home/users/wangtianyu15/wtyWorkspace/aerospace-adaptive-ft"
BUILD_DIR="${ROOT_DIR}/build"
RES_DIR="${ROOT_DIR}/results"
GOLDEN_DIR="${RES_DIR}/golden"

mkdir -p "${BUILD_DIR}" "${RES_DIR}" "${GOLDEN_DIR}"

echo "=== [1/5] Building LLVM Pass Plugin (libAdaptiveFTPass.so) & Runtime (libatft_rt.a) ==="
cd "${BUILD_DIR}"
cmake .. -DCMAKE_C_COMPILER=clang-15 -DCMAKE_CXX_COMPILER=clang++-15 >/dev/null
make -j4

cd "${ROOT_DIR}"
PASS_SO="${BUILD_DIR}/libAdaptiveFTPass.so"
RT_LIB="${BUILD_DIR}/libatft_rt.a"

echo "=== [2/5] Compiling Benchmarks to SSA LLVM IR & Running Criticality Analysis ==="
clang-15 -O1 -Xclang -disable-llvm-passes -S -emit-llvm benchmarks/benchmarks.c -Iinclude -o "${BUILD_DIR}/benchmarks_raw.ll"
opt-15 -passes='mem2reg,loop-simplify' -S "${BUILD_DIR}/benchmarks_raw.ll" -o "${BUILD_DIR}/benchmarks_ssa.ll"

opt-15 -load-pass-plugin="${PASS_SO}" -passes='adaptive-ft' \
    -atft-mode=analyze \
    -atft-report="${RES_DIR}/criticality_analysis.csv" \
    -disable-output "${BUILD_DIR}/benchmarks_ssa.ll"

echo "=== [3/5] Generating Transformed LLVM IR & Executables for All Protection Modes ==="
CFLAGS="-O2 -fno-unroll-loops -fno-vectorize -fno-slp-vectorize"

# 1) Native (无容错保护 Baseline)
opt-15 -load-pass-plugin="${PASS_SO}" -passes='adaptive-ft,verify' -atft-mode=native \
    -S "${BUILD_DIR}/benchmarks_ssa.ll" -o "${BUILD_DIR}/bench_native.ll" 2>/dev/null
clang-15 ${CFLAGS} experiments/eval_suite.c "${BUILD_DIR}/bench_native.ll" "${RT_LIB}" -Iinclude -Ibenchmarks -lm -o "${BUILD_DIR}/eval_native"

# 2) Exact-TMR (传统软件三模冗余 Baseline)
opt-15 -load-pass-plugin="${PASS_SO}" -passes='adaptive-ft,verify' -atft-mode=exact-tmr \
    -S "${BUILD_DIR}/benchmarks_ssa.ll" -o "${BUILD_DIR}/bench_exact_tmr.ll" 2>/dev/null
clang-15 ${CFLAGS} experiments/eval_suite.c "${BUILD_DIR}/bench_exact_tmr.ll" "${RT_LIB}" -Iinclude -Ibenchmarks -lm -o "${BUILD_DIR}/eval_exact_tmr"

# 3) Approx-TMR-Scalar (1精确+2近似 标量版，用于消融对比 SIMD 收益)
opt-15 -load-pass-plugin="${PASS_SO}" -passes='adaptive-ft,verify' -atft-mode=approx-tmr -atft-simd=false \
    -S "${BUILD_DIR}/benchmarks_ssa.ll" -o "${BUILD_DIR}/bench_approx_scalar.ll" 2>/dev/null
clang-15 ${CFLAGS} experiments/eval_suite.c "${BUILD_DIR}/bench_approx_scalar.ll" "${RT_LIB}" -Iinclude -Ibenchmarks -lm -o "${BUILD_DIR}/eval_approx_scalar"

# 4) Approx-TMR-SIMD (1精确+2近似 + <2 x float> 向量化版)
opt-15 -load-pass-plugin="${PASS_SO}" -passes='adaptive-ft,verify' -atft-mode=approx-tmr -atft-simd=true \
    -S "${BUILD_DIR}/benchmarks_ssa.ll" -o "${BUILD_DIR}/bench_approx_simd.ll" 2>/dev/null
clang-15 ${CFLAGS} experiments/eval_suite.c "${BUILD_DIR}/bench_approx_simd.ll" "${RT_LIB}" -Iinclude -Ibenchmarks -lm -o "${BUILD_DIR}/eval_approx_simd"

# 5) Adaptive-FT (编译期关键性分级 + 运行时感知多版本动态切换)
opt-15 -load-pass-plugin="${PASS_SO}" -passes='adaptive-ft,verify' -atft-mode=adaptive -atft-simd=true \
    -S "${BUILD_DIR}/benchmarks_ssa.ll" -o "${BUILD_DIR}/bench_adaptive.ll" 2>/dev/null
clang-15 ${CFLAGS} experiments/eval_suite.c "${BUILD_DIR}/bench_adaptive.ll" "${RT_LIB}" -Iinclude -Ibenchmarks -lm -o "${BUILD_DIR}/eval_adaptive"

# 6) 故障注入专用可执行文件 (插桩单比特翻转函数)
opt-15 -load-pass-plugin="${PASS_SO}" -passes='adaptive-ft,verify' -atft-mode=native -atft-fi=true \
    -S "${BUILD_DIR}/benchmarks_ssa.ll" -o "${BUILD_DIR}/bench_native_fi.ll" 2>/dev/null
clang-15 -O2 experiments/eval_suite.c "${BUILD_DIR}/bench_native_fi.ll" "${RT_LIB}" -Iinclude -Ibenchmarks -lm -o "${BUILD_DIR}/eval_native_fi"

opt-15 -load-pass-plugin="${PASS_SO}" -passes='adaptive-ft,verify' -atft-mode=exact-tmr -atft-fi=true \
    -S "${BUILD_DIR}/benchmarks_ssa.ll" -o "${BUILD_DIR}/bench_exact_tmr_fi.ll" 2>/dev/null
clang-15 -O2 experiments/eval_suite.c "${BUILD_DIR}/bench_exact_tmr_fi.ll" "${RT_LIB}" -Iinclude -Ibenchmarks -lm -o "${BUILD_DIR}/eval_exact_tmr_fi"

opt-15 -load-pass-plugin="${PASS_SO}" -passes='adaptive-ft,verify' -atft-mode=approx-tmr -atft-simd=true -atft-fi=true \
    -S "${BUILD_DIR}/benchmarks_ssa.ll" -o "${BUILD_DIR}/bench_approx_simd_fi.ll" 2>/dev/null
clang-15 -O2 experiments/eval_suite.c "${BUILD_DIR}/bench_approx_simd_fi.ll" "${RT_LIB}" -Iinclude -Ibenchmarks -lm -o "${BUILD_DIR}/eval_approx_simd_fi"

echo "=== [4/5] Running Performance, Dynamic Instruction Count & Accuracy Evaluation ==="
"${BUILD_DIR}/eval_native" --dump-golden "${GOLDEN_DIR}" >/dev/null

PERF_OUT="${RES_DIR}/performance_accuracy.csv"
echo "tag,mode,benchmark,exec_time_us,dyn_insts,rel_l2_error,votes_per_iter" > "${PERF_OUT}"
"${BUILD_DIR}/eval_native"        --perf native        "${GOLDEN_DIR}" >> "${PERF_OUT}"
"${BUILD_DIR}/eval_exact_tmr"     --perf exact_tmr     "${GOLDEN_DIR}" >> "${PERF_OUT}"
"${BUILD_DIR}/eval_approx_scalar" --perf approx_scalar "${GOLDEN_DIR}" >> "${PERF_OUT}"
"${BUILD_DIR}/eval_approx_simd"   --perf approx_simd   "${GOLDEN_DIR}" >> "${PERF_OUT}"
"${BUILD_DIR}/eval_adaptive"      --perf adaptive_low  "${GOLDEN_DIR}" >> "${PERF_OUT}"
"${BUILD_DIR}/eval_adaptive"      --perf adaptive_high "${GOLDEN_DIR}" >> "${PERF_OUT}"

echo "=== [5/5] Running Monte-Carlo Single-Bit-Flip Fault Injection & Solar-Flare Adaptive Trace ==="
FI_OUT="${RES_DIR}/fault_injection_reliability.csv"
echo "tag,mode,benchmark,total_trials,effective_fault_runs,detection_rate_pct,recovery_rate_pct,sdc_rate_pct,avg_recovered_rel_error" > "${FI_OUT}"
"${BUILD_DIR}/eval_native_fi"      --fi native_fi  "${GOLDEN_DIR}" 1500 >> "${FI_OUT}"
"${BUILD_DIR}/eval_exact_tmr_fi"   --fi exact_tmr  "${GOLDEN_DIR}" 1500 >> "${FI_OUT}"
"${BUILD_DIR}/eval_approx_simd_fi" --fi approx_tmr "${GOLDEN_DIR}" 1500 >> "${FI_OUT}"

TRACE_OUT="${RES_DIR}/adaptive_mode_trace.csv"
"${BUILD_DIR}/eval_adaptive" --trace > "${TRACE_OUT}"

echo "=== All Experiments Completed Successfully! ==="
