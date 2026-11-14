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
| `--fi-type balanced` | 注错类别：`balanced`（默认，七类等额）或逗号分隔的类别名 |
| `--trials-per-category N` | 每个注错类别的注入次数（默认取配置） |
| `--smoke-policies` / `--smoke-trials` | smoke 模式的策略与注入次数 |
| `--config path.yaml` | 指定配置文件（默认 `experiments/config/default.yaml`） |
| `--save-snapshot NAME` / `--diff-snapshot NAME` | 存档与对比 `results/snapshots/NAME` |

工具链位置由环境变量覆盖：`ATFT_ROOT`、`ATFT_CLANG`、`ATFT_CLANGXX`、`ATFT_OPT`、`ATFT_QEMU_RISCV`、`ATFT_RISCV_TRIPLE`、`ATFT_RISCV_SYSROOT`。

## 配置

实验参数在 `experiments/config/default.yaml`，加载顺序：内置默认值 ← YAML ← 命令行。

```yaml
trials: 1000
thresholds:                  # 通过环境变量传给 eval_suite
  sdc_rel_err: 0.01          # 浮点负载判为 SDC 的相对误差（整数负载恒为按位精确）
  masked_rel_err: 1.0e-12
  trial_watchdog_ms: 100     # 单次试验子进程的挂死上限
  step_budget_factor: 50     # 动态指令预算 = 无故障剖析步数 x 本系数
smoke: {trials: 60, policies: [native, exact_tmr, approx_simd]}
policies:                    # RQ1 参与对比的策略
  - {tag: native,      mode: native,      golden: true, fi: true}
  - {tag: approx_simd, mode: approx-simd, fi: true, delta: 5.0e-5}
grids:
  rq2: {budgets: [0.25, 0.5, 0.75, 1.0], rank_methods: [semantic, sdctune, shoestring, hotpath, random], trials: 300}
  rq3: {deltas: [1.0e-7, 1.0e-5, 5.0e-5, 1.0e-3, 5.0e-2, 5.0e-1], trials: 300}
  rq8: {categories: [fp, int, address, branch], policies: [native, swift, exact_tmr, approx_simd], trials: 120}
defaults: {budget: 1.0, delta: 5.0e-5, weights: {w1: 2.5, w2: 1.8, w3: 0.8}}
```

换一组对比实验（加策略、改 δ 网格、调判据阈值）只改 YAML：

```bash
python3 experiments/benchmark_harness.py --full --rq rq3 --config my.yaml
```

负载清单与保护方法不在 YAML 里：加负载要写 C（见下），加方法要写 Pass。

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

## 故障点分类与预算分配

七个**互斥**类别，分类只看指令自身的种类与结果类型，不看使用者——这样容错变换
给指令挂上内联表决之后，它仍属于同一类别，不会出现"保护越多、可注入点越少"。

| 类别 | LLVM IR 层 | RV64GCV 落点 |
| :-- | :-- | :-- |
| `fp64` | double 算术/一元运算 | `fadd.d` `fmul.d` |
| `fp32` | float 与 `<2 x float>` 算术 | `fadd.s` `vfadd.vv` |
| `int_arith` | i16/i32/i64 整数算术与逻辑 | `add` `mul` `sll` `and` |
| `cast` | trunc/zext/sext/fptrunc/fptosi/sitofp… | `fcvt.*` `sext.w` `slli+srai` |
| `load` | load 指令的结果值 | `lw` `ld` `flw` `fld` 的目的寄存器 |
| `addr` | GEP 的可变下标（扰动限制在 ±3 个元素） | `sw` `lw` 的地址寄存器 |
| `cmp_branch` | 驱动 br/select/switch 的 icmp/fcmp | `slt` `feq.d` + `beq/bne` |

对应 LLFI/PINFI (DSN'14) 的 arithmetic/cast/cmp/load 与 SASSIFI (ISPASS'17) 的
IOV/IOA 两套分类。

**预算按类别等额分配**，不按动态频率比例——与 SASSIFI (`NUM_INJECTIONS` per
IGID per BFM)、NVBitFI、LLFI/PINFI（每类别 1000 次）一致。自然频率在聚合阶段
作为第二口径出现（SASSIFI 的 analysis-time weighting）。

## 聚合口径

`experiments/aggregate.py` 是唯一实现，smoke 与正式报告共用。

| 量 | 聚合方式 | 依据 |
| :-- | :-- | :-- |
| 指令倍率等比值 | **几何平均** | SWIFT/SWIFT-R 图题 "GeoMean is across all benchmarks" |
| SDC / DUE 等率值 | **算术平均 + 95% CI** | SWIFT/SWIFT-R 的 "Average"；CI 为本工作增量 |
| 负载权重 | 套件内等权 → **套件间等权** | Relyzer (ASPLOS'12) 每 suite 等量取样的精神 |
| 类别权重 | 七类等权（主口径）+ 频率加权（副口径） | SASSIFI |

主口径只覆盖**基线程序本身存在的**故障类别：保护方案引入的新故障点（例如近似
副本带来的 fp32 运算）单列在 `extra_categories`，不混进主口径，否则不同策略的
类别集合不一致、失去可比性。

```bash
python3 experiments/aggregate.py --policy-detail approx_simd
python3 experiments/aggregate.py --json results/rq1_aggregate.json
```

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

## 时序层：从条件概率到绝对速率（RQ9）

上面所有率值都是**条件概率** `P(结局 | 一个故障落在可注入点上)`。要报 MTTF 或
"每任务失效多少次"，必须乘上一个绝对到达率。这一步由 `experiments/temporal.py`
做，纯后处理，读 RQ1 的 PMU 与 RQ6 的 `P(F|n)`，不跑新 campaign：

$$a=\lambda t_{exec},\quad P_F=\sum_{n\ge1}P(F|n)\frac{a^ne^{-a}}{n!},\quad \text{MTTF}=\frac{t_{exec}}{P_F}$$

```bash
/tmp/orbenv/bin/python experiments/orbit_rates.py     # 离线一次，生成轨道剖面
python3 experiments/benchmark_harness.py --rq rq9      # 读 rq1/rq6 的 CSV 出表
```

口径与出处：

| 环节 | 用的方案 |
| :-- | :-- |
| 求和链 `P_F = Σ P(F\|n)P(n)` 与复合 MTTF 的占时加权 | Ostler et al., IEEE TNS 56(6):3519-3526, 2009 |
| 实际生效的单故障归一化 `λ = g·Δt·Δm` | Schirmeier, Borchert, Spinczyk, DSN 2015 |
| MWTF 定义（单位有用工作的失效数之倒数） | Bohman et al., IEEE TNS 66(1), 2019 |
| 同绝对速率下的公平对比（fixed-rate campaign） | Sotiropoulos et al., IEEE SOCC 2026 |
| 轨道 → 地磁场 → 分段速率 | Jacobs et al., ACM TRETS 5(4):21, 2012 |

必须和论文一起声明的三件事：

- 实际生效的是 **Schirmeier 的单故障形式**。所有环境下 $a\le4\times10^{-6}$，
  $n\ge2$ 的 Poisson 质量 $\le10^{-50}$，Ostler 的求和在数值上是惰性的，
  只作为 $a$ 增大时的推广保留。
- Ostler 的 $t_{scrub}$ 是平台常数，我们用的是**一次执行**，所以分母随策略变化。
  这让运行时开销进入可靠性指标（= SOCC'26 的 MTTF 定义），不是 Ostler 的口径。
- 故障空间 $\Delta m$ 全策略取同一常数（即 fixed-rate campaign），没有
  proportional-rate 伴侣。SOCC'26 实证两者给出相同的加固优先级排序。

轨道剖面 `experiments/orbit_rates.py` 把周期、SAA 占时、过境驻留**从同一次
SGP4 传播**算出，再与同一颗卫星实测的 $\sigma_{avg}$ 配对，避免跨轨道混用参数。
SAA 判据是 IGRF 总场强低于阈值且 L 壳在内带范围内；阈值经扫描后取
20000 nT，因为该档的过境驻留 8.3–9.6 min 落在 Underwood (ESA SP-439, 2000)
独立实测的 5–10 min 内，且 97–99.9% 的判定样本落在 SAA 的常规地理范围内。
该脚本需要 `sgp4` 与 `ppigrf`，只在重新生成剖面时运行；产物入库。

## 输出

| 文件 | 内容 |
| :-- | :-- |
| `results/rq1_pmu_performance_qor.csv` | PMU 指令数、周期、执行时间、QoR |
| `results/rq1_fault_injection_outcomes.csv` | 五分类结局、置信区间、MWTF |
| `results/rq1_*_int.csv` / `_all.csv` | 整数类别 / 全类别注错口径 |
| `results/rq2..rq8_*.csv` | 选择性保护、δ 敏感性、自适应轨迹、位域敏感性、MBU 冲击、规模扫描、指令类别与 RISC-V |
| `results/rq9_temporal_mttf.csv` | 各环境下的 MTTF（带 95% CI）、MWTF、每任务失效数、复合 MTTF |
| `results/rq9_detection_budget.csv` | SAA 过境窗口 vs 攒到第 k 个事件的延迟，反应式控制器的延迟下界 |
| `results/rq9_orbit_profile.csv` | 每条轨道的周期、SAA 占时、过境驻留、与 SAA 地理范围的一致率 |
| `results/rq9_orbit_timeseries.csv` | 沿轨的 \|B\|、L 壳与 SAA 标记 |
| `results/eval_dashboard.html` | 汇总看板 |
