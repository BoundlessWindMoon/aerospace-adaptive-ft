# 评测模块

对任意一种"用 LLVM Pass 改写 IR 的容错方法"做盲测：测真实 PMU 开销、注入单粒子故障、输出五分类结局与置信区间。

## 快速开始

```bash
python3 experiments/benchmark_harness.py            # smoke，约 10 秒，不写 CSV
python3 experiments/benchmark_harness.py --full --rq all --trials 1000   # 全量，约 6 分钟
python3 experiments/generate_html_report.py         # 生成 results/eval_dashboard.html
```

常用参数：

| 参数 | 说明 |
| :-- | :-- |
| `--full` | 全量模式，写入 `results/*.csv`（默认是 smoke） |
| `--rq rq1,rq5` | 选择实验，`all` 为全部 RQ1–RQ8 |
| `--policy native,swift,approx_simd` | 选择参与对比的策略 |
| `--trials 1000` | 每个负载的注入次数 |
| `--fi-type fp\|int\|address\|branch\|all` | 注错的指令类别 |
| `--smoke-policies` / `--smoke-trials` | smoke 模式的策略与注入次数 |
| `--save-snapshot NAME` / `--diff-snapshot NAME` | 存档与对比 `results/snapshots/NAME` |

工具链位置由环境变量覆盖：`ATFT_ROOT`、`ATFT_CLANG`、`ATFT_CLANGXX`、`ATFT_OPT`、`ATFT_QEMU_RISCV`、`ATFT_RISCV_TRIPLE`、`ATFT_RISCV_SYSROOT`。

## 三层结构

| 层 | 位置 | 作用 |
| :-- | :-- | :-- |
| 负载 | `benchmarks/` | 编译 `third_party/` 的上游内核，提供 `bench_*` 转接函数 |
| 策略 | `passes/` | 产生受保护的 IR，由 `-atft-mode=<名字>` 选择 |
| 裁判 | `harness/` + `experiments/` | PMU 计数、故障注入、结局分类、统计 |

策略层与裁判层之间只有一个约定：Pass 接受 `-atft-mode=<名字>`，裁判层用 `--policy <名字>` 调它。

## 加一个负载

1. `benchmarks/benchmarks.c`：`#include` 上游源文件 + 写一个参数转接的 `bench_*`；
2. `benchmarks/benchmarks.h`：加声明；
3. `experiments/eval_suite.c`：
   - `BENCHES[]` 加一行：名字、套件、`BK_FP`/`BK_INT`、标签（`BTAG_REPR` 进代表性子集，`BTAG_AEROSPACE` 标记空天负载）；
   - `alloc_and_init_workspace`：分配缓冲，填 `out_ptrs / out_sizes / peak_vals / perf_iters`；
   - `reset_mutable_buffers` / `invoke_benchmark` / `export_outputs`：各加一个 case；
4. 新头文件路径加到 `benchmark_harness.py` 的 `BENCH_INCLUDE_DIRS`。

负载数量、浮点/整数判据、实验子集都从 `BENCHES[]` 推导。漏改会在启动时直接 abort（`invoke_benchmark` 的 default 分支与 `workspace_selfcheck()`）。

## 加一种策略

在 `AdaptiveFTPass.cpp` 的 `run()` 里加一个 `AtftMode == "<名字>"` 分支，再在 `benchmark_harness.py` 的 `core_configs` 里加一行。裁判层不动。

## 结局分类

每次注入一个故障，比对输出与黄金参考后分为五类：

- **Masked** 相对误差 ≤ 1e-12
- **Detected & Recovered** 表决器检出且输出可接受
- **Benign** 未检出但误差在应用容限内
- **SDC** 浮点负载误差 > 1%，整数负载任何非零偏差，或出现 NaN/Inf
- **DUE** 检出不可恢复，或进程被信号杀死 / 挂死

综合指标 MWTF = 1 / (归一化指令开销 × 失效概率)，SDC 率附 95% Wilson 置信区间。

每次注入在 `fork` 的子进程里执行，父进程通过共享内存取结论：子进程崩溃或挂死（100 ms 看护 + 指令预算兜底）记为 DUE，不影响后续试验。

负载在某个注错类别下没有插桩点时记录 `dyn_target_insts=0` 且 0 次试验。

## 输出

| 文件 | 内容 |
| :-- | :-- |
| `results/rq1_pmu_performance_qor.csv` | PMU 指令数、周期、执行时间、QoR |
| `results/rq1_fault_injection_outcomes.csv` | 五分类结局、置信区间、MWTF |
| `results/rq1_*_int.csv` / `_all.csv` | 整数类别 / 全类别注错口径 |
| `results/rq2..rq8_*.csv` | 选择性保护、δ 敏感性、自适应轨迹、位域敏感性、MBU 冲击、规模扫描、指令类别与 RISC-V |
| `results/eval_dashboard.html` | 汇总看板 |
