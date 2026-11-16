# 复现与溯源清单（REPRO）

论文每条结论/图表 → 产它的脚本 → 输入 → 输出 CSV。canonical 结果在
`results/opt/`；探索期一次性产物已归档到 `results/opt/archive/`（留盘、不入库）。
冻结基线 `results/rq1_*.csv` 不被任何此处脚本触碰。

运行约定：脚本从仓库根目录执行，例如 `python3 experiments/adaptive_capstone.py`。

## 1. 保护菜单与覆盖率（SDC/DUE 实测）

- **8策略×6类别×200trials FI+PMU 矩阵**
  - 脚本：`experiments/run_opt_ablation.py`（消融驱动）→ `experiments/aggregate.py`（聚合）
  - 输出：`pmu_a4_t200_matrix_solid.csv`、`fi_outcomes_a4_t200_matrix_solid.csv`
- **每核属性表（meanW/可约/各策略 cyc 比/fp64 SDC）**
  - 脚本：`experiments/build_property_table.py`
  - 输入：上面两个 matrix_solid CSV + `kernel_vectorization.csv`
  - 输出：`kernel_properties_a4_t200_matrix_solid.csv`
- **x86 静态向量化度量**
  - 脚本：`experiments/measure_vectorization.py`（objdump 真 SUT 二进制）
  - 输出：`kernel_vectorization.csv`
- **B1 开销 vs 向量化相关 + B2 λ 交叉**
  - 脚本：`experiments/analyze_cost_model.py`
  - 输入：kernel_properties + matrix_solid CSV

## 2. 粗粒度 / fork 隔离 / 范围检测 / 自检投票器

- **coarse DMR+重执行、fork 隔离 TMR**（SDC→0 / DUE→0）
  - 脚本：`experiments/eval_suite.c`（harness，`ATFT_COARSE` / `ATFT_COARSE_FORK` 环境变量）→ `aggregate.py`
  - 输出：`delta_table_a4_k4_coarse_fi.csv`、`delta_table_a4_k4_coarse_fork.csv`
- **范围/症状检测（M2）**
  - 脚本：`eval_suite.c`（`ATFT_RANGECHECK`）
  - 输出：`delta_table_a4_k4_rangecheck_k{2,4,8}.csv`
- **自检投票器 vs 普通投票器（信任基）**
  - 脚本：`experiments/measure_voter_inline.py`、`experiments/coarse_rmt.c`（`mc_voter`/`voter_self_check`）
  - 输出：`voter_inline_fi.csv`、`voter_inline_pmu.csv`

## 3. 廉价结果校验与其边界（verify/compute = Blum gap）

- **通用 Freivalds 结果校验**（Cholesky/LU ~1×、检出 ~100%）
  - 脚本：`experiments/freivalds_check.c`（`freivalds_probe` 原语）
- **stencil 极值原理界校验（负结果，界定廉价校验适用域）**
  - 脚本：`experiments/stencil_check.c`
- **粗粒度多核 scaling（gemm 演示）**
  - 脚本：`experiments/coarse_rmt.c`

## 4. 真实工作点（少核）下包络

- **核占用扫描（并行 ~1× 只在空闲核≥副本数时成立）**
  - 脚本：`experiments/few_core_envelope.py`
  - 输出：`core_saturation_sweep.csv`、`few_core_envelope.csv`
- **自适应下包络（每核选最便宜 SDC=0 策略）**
  - 脚本：`experiments/lower_envelope.py`
  - 输入：matrix_solid CSV + `coarse_overhead_subset.csv`
  - 输出：`lower_envelope_a4_t200_matrix_solid.csv`

## 5. 自适应 capstone（λ(t) 驱动 + 控制论）

- **轨道 λ 剖面（离线一次性，产物入库）**
  - 脚本：`experiments/orbit_rates.py`（SGP4+IGRF，须 sgp4/ppigrf）
  - 输出：`results/rq9_orbit_profile.csv`、`results/rq9_orbit_timeseries.csv`
- **一套控制器跑三轨道 + 五组实验**
  - 脚本：`experiments/adaptive_capstone.py`
  - 输入：`kernel_properties_*_matrix_solid.csv`、`coarse_overhead_subset.csv`、`rq9_orbit_timeseries.csv`
  - 输出：
    - 菜单+切换门限 → `capstone_menu.csv`
    - 三轨道 payoff（串行/并行）→ `capstone_payoff.csv`、`capstone_trace_{leo,sso,geo_flare}_{serial,parallel}.csv`
    - 协态 vs 速率上限 → `capstone_costate.csv`
    - 真实连续 λ（省 TMR 48–68%）→ `capstone_continuous.csv`
    - 可靠性-开销 Pareto 前沿 → `capstone_pareto.csv`
    - 在线 ECC 流 λ̂ SNR 负结果 → `capstone_online.csv`

## 归档说明

`results/opt/archive/`：2026-11 的 pgsp / path-frontier / delta-profile / boundary /
c1–c4 / 标定（cal/hpc_cal）/ 验证（ftcheck/frontier_validation）/ approxcheck 等
探索一次性产物，均已被上面 canonical 结果取代，留盘备查、不入 VCS。

## 如何补一个实验

固定契约，照这五步做，新实验即可被复现也能进本清单：

1. **复用核心**，别复制粘贴菜单/求解/轨迹逻辑。从 `experiments/atft_core.py` import：
   ```python
   from atft_core import (build_menu, load_continuous_lambda,
                          integrate, choose_ratecap, choose_costate,
                          solve_to_budget, trace_leo, trace_sso, trace_geo_flare)
   ```
2. **读** `results/opt/` 下的 canonical CSV（见上面各节），不要读 `archive/`，
   不要改已冻结的输入（`kernel_properties_*_matrix_solid.csv`、`coarse_overhead_subset.csv`、
   `rq9_orbit_timeseries.csv`、`results/rq1_*`）。
3. **写** 输出到 `results/opt/<实验名>.csv`，一个实验一个文件，列名自解释。
4. **可复现**：任何随机必须带固定种子（如 `np.random.default_rng(12345)`），
   不得用系统时间/无种子随机。新脚本放 `experiments/`，从仓库根目录运行。
5. **登记**：在本文件对应小节补一行「结论 → 脚本 → 输入 → 输出」。

需要新的 FI 覆盖率/开销数据（新核或新策略）才动 `experiments/eval_suite.c`——那是
重编译路径，改完要重跑 `run_opt_ablation.py` + `aggregate.py` 重生成 matrix_solid。

## 负载无关评测器（tools/）

`tools/workload_eval.py` 把"负载"和"测试"解耦：测试逻辑固定，任何负载只要给一张
符合契约的剖面表就能无感跑通并产出 md 报告。

- **契约**：CSV 列 `benchmark,policy,overhead_x,e_sdc`，每个 benchmark 至少含一行
  `policy=native`。`results/opt/capstone_menu.csv` 即一份合法剖面（当前 PolyBench 负载）。
- **用法**：`python3 tools/workload_eval.py [--menu <profile.csv>] [--name <负载名>]`
  → 产出 `tools/reports/<负载名>_report.md`（连续 λ payoff / 达标性 / Pareto / 要点）。
- **例子**：`tools/example_workload.csv` 是一份与 PolyBench 无关的合成负载，
  `--menu tools/example_workload.csv` 跑出独立报告，评测器代码一行不改。

### 统一报告

`tools/build_report.py` 把所有层面汇成**一个 md**（`tools/reports/full_report.md`）：
①保护菜单(o,e) ②策略级覆盖与开销(solid FI 矩阵) ③少核下包络 ④自适应 payoff(现跑)
⑤在线 λ̂。数据**生成**仍分散各源（各节标题标了 CSV），本工具只统一**汇总消费**；
换负载/补实验后重跑即可刷新整份报告。
    python3 tools/build_report.py

