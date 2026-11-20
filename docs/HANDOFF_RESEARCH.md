# 研究交接说明：面向空天计算的自适应容错框架

> 本文只**陈述现状**——项目目标、当前设计、已完成的实验与口径。
> 不含"下一步/建议/应该做什么"。调研方向请阅读者自行判断。
> 配套既有文档：`docs/DESIGN.md`、`docs/METHOD.md`、`docs/HANDOFF.md`（较早、更细）、
> `README.md`。本文是面向"交给更强模型做调研"的当前状态快照（2026-11）。

## 0. 一句话

硕士论文课题：基于 LLVM 15 的编译期自适应容错框架，面向空天（星载）计算。
两个 out-of-tree Pass（容错策略层 + 故障注入裁判层）+ 纯 C 运行时；在一组内核上
实测不同防护机制的开销与故障覆盖，并用 λ(t)（辐射率）驱动的控制器按轨道环境
自适应选择防护强度。

## 1. 论文定位与两条主张

- 主张一（空间维·结构匹配）：按被保护数据/计算的**结构**选择最省的正确机制
  （矩阵乘/卷积→ABFT 代数校验；整数→残差码；地址→切片/中值；分支→DMR；
  浮点→SIMD 打包冗余），而不是对所有指令一律三模冗余。
- 主张二（时间维·自适应）：辐射平静期关掉/降低冗余、耀斑/SAA 期开满，
  由 λ(t) 驱动，在可靠性预算内把平均开销压到接近 1×。

## 2. 系统架构（当前设计）

- `passes/AdaptiveFTPass.cpp` → `libAdaptiveFTPass.so`：容错策略层。
  按 `-atft-mode` 与一组 `-atft-*` flag 做 IR 变换插入冗余/校验。
- `harness/FaultInjectorPass.cpp` → `libFaultInjectorPass.so`：故障注入裁判层。
  按指令类别插桩单/多比特翻转钩子；`-fi-target-type=<cat|all>`、`-fi-blind`。
- `runtime/atft_runtime.c` → `libatft_rt.a`：表决器、ABFT 校验/重算、残差提交、
  λ(t) 控制器状态（`g_atft_state`）、SPRT 变点检测。
- `harness/harness_rt.c` → `libharness_rt.a`：注入钩子、PMU 计数、QoR 比对。
- `experiments/eval_suite.c`：被测程序入口（注册表 + 工作集 + FI campaign 驱动）。

### 2.1 保护机制清单与"自身是否会制造 SDC"

| 机制 | flag/mode | 形态 | 被自身故障击中时 |
|---|---|---|---|
| ABFT 代数校验 | `-atft-abft-insert`(+`-atft-layer-exclusive`) | 边界结果校验+重算 | 检出面小、可纠；repair 路径是暴露写 |
| 整数残差码 | `-atft-int-residue` | mod-65521 算术码,return 处判 | 只会误报(false DUE),不制造 SDC |
| 地址 DMR/中值 | `-atft-addr-dmr`,`-atft-addr-correct` | 下标 median-of-3 纠正 | 低；且把越界崩溃转为安全 |
| 分支 DMR | `-atft-cmp-dmr` | 复制比较+断言 | 检出→DUE(只检不纠) |
| 浮点 SIMD 冗余 | mode=`approx-simd`(+`-atft-simd-f64`) | 2 路副本打进 `<2×f64>`+表决 | 表决器是 select,被击中可能静默选错 |
| 精确 TMR | mode=`exact-tmr` | 3 路位精确多数 | 多数可纠 |
| 时序自适应 | mode=`adaptive` | LOW/HIGH 克隆(内部用 approx-simd 路径) | — |

当前默认口径（本会话刚调整，提交 `a77c705`）：
- 浮点冗余默认走**精确 SIMD**（`-atft-simd-f64 -atft-replica-exact`，2×f64）。
- `approx-simd/approx-scalar/pure-approx`（f32+δ 容差近似，"近似计算容错"遗留）
  已从默认/基线清单**退役**，仅显式 opt-in 才走；代码未物理删除，因为它与
  精确 SIMD 和 `adaptive` 模式共用同一 transform (`transformFunctionApproxTMR`)。
  实测依据：exact-SIMD 与 approx-simd 的 fp64 SDC 完全一致（都 0.7%），
  说明 δ 容差不是 SDC 来源，换精确无可靠性损失。

### 2.2 恢复：detect+retry（本会话新增，默认关）

`eval_suite.c` 的 `ATFT_RETRY_ON_DETECT`：检出（含子进程崩溃/挂死的 fail-stop）
→ 从原输入干净重执行一次。单发瞬态在重跑里消失→恢复。无故障路径零开销，
重执行代价只按检出事件付（接 λW 账本）。父级重执行覆盖"中途崩溃"类 DUE。

## 3. 两个实验层次（按你的框架，标注现状）

你的两层划分是对的，补充各自的当前口径：

### 层次 A — 空天负载·宏观指标（测"自适应+容错"整体）

- 目标指标：任务完成率 / 耗时 / 能量 / 截止满足率等宏观量。
- 现状：
  - 已有轨道 λ(t) 剖面（SAMPEX/Alsat-1 实测 σ、SAA 占空比、CREME96 worst-day），
    `experiments/orbit_rates.py` + `temporal.py` 把条件概率换算成 MTTF/每任务失效数。
  - 已有现代平台模型 `atft_core.py`：HPSC（SiFive X280×8，RVV 512-bit，28130 DMIPS，
    DCLS 锁步/split 两模）、Jetson Orin Nano（7W/15W 功耗档）；GNC 10Hz/100ms 硬截止。
  - "自适应 vs 锁步/全程 TMR" 的宏观收益目前是**模型估算**（吞吐=DMIPS/指令数×开销，
    叠加 SAA 占空比），见 `measure_cnn_capstone.py`、README 的 1.03×/省 48–68% 结论。
  - **能量/功耗是按档位估算**，未接实测功耗；"任务完成率"目前以可靠性预算+截止
    满足的组合表达，未有端到端任务级仿真。
- 关键既有结论（README/既有实验，非本会话复核）：真实轨道 λ 剖面下自适应开销 1.03×，
  比传统软件 TMR 省 48–68%，全可靠性区间 Pareto 占优。

### 层次 B — 容错机制·微观指标（逐 benchmark 看效果）

- 目标指标：SDC%、DUE%、det_rec%、指令数(防御面)、时间开销。
- 学界口径（已查证，`docs/LITERATURE_SURVEY.md` 另有）：SDC（静默错）是最危险结局；
  DUE（检出不可恢复/fail-stop）严格优于 SDC；目标是先把 SDC 压向 0，DUE 作可接受
  泄压阀。航天语境下 erroneous(≈SDC) 比 fail-silent 更常见且更难恢复。
- 注入口径（三种，结论须标明用哪种）：
  1. 逐类别（fi-target-type=单类别，豁免 voter 内部）：文献主口径。
  2. 全指令盲注（`-fi-target-type=all -fi-blind`）：保护码自身也可被击中，物理更真实。
  3. 时间维连发/多比特 burst（`--stress`，ATFT_BURST_COUNTS/MBU）：峰值辐射。
- SDC/DUE 分类阈值：`ATFT_SDC_REL_ERR`(默认 1%)、整数恒按位精确；崩溃/挂死=DUE。

## 4. 当前负载清单（`eval_suite.c` 注册表，46 个）

- PolyBench/C 4.2.1：gemm/2mm/3mm/atax/bicg/gemver/gesummv/mvt/symm/syr2k/syrk/
  trmm/doitgen/cholesky/lu/ludcmp/gramschmidt/trisolv/durbin/correlation/covariance/
  jacobi1d/jacobi2d/heat3d/fdtd2d/adi/seidel2d/floyd_warshall/nussinov/deriche。
- 其他通用：parsec_blackscholes、dsp_kissfft、axc_sobel。
- 空天相关（BTAG_AEROSPACE）：
  - OBPMark v0.3（ESA+BSC）image_prep / image_proc —— 星载图像处理基准。
  - COAST tests（BYU/LANL）crc16 / quicksort。
  - CNN：cnn_conv / cnn_conv_abft / cnn_conv_gemm / cnn_conv_prim（卷积+ABFT 路由）、
    cnn_cls_logit / cnn_cls_decision（端到端分类器，数值 SDC vs 决策翻转）。
  - GNC/AOCS（本会话新增）：gnc_kalman（闭环状态估计）、orbit_rk4（开环二体轨道外推）、
    aocs_pid（闭环单轴姿态控制）。

说明：BTAG_REPR 子集用于默认聚合口径；BTAG_AEROSPACE 核默认不进聚合，避免污染。
空天核的"输出符合实际"程度：Kalman/PID/orbit 用归一化合成输入与物理模型，
数值真实但规模小（为 FI 快）；真实遥测/图像输入、真实任务时长尚未接入。

## 5. 平台与约束建模（`experiments/atft_core.py`）

- HPSC：dmips=28130，vector_bits=512，gflops_vec=256，fp64，DCLS 锁步/split 两模。
- Jetson Orin Nano 8G：7W{4 核@960MHz} / 15W{6 核@1500MHz}，NEON 128-bit。
- GNC：GNC_LOOP_HZ=10，GNC_DEADLINE_S=0.1（100ms 硬截止）。
- 轨道：LEO_SAA_FRAC=0.1856，λ 平静/SAA 两档；σ 来自 SAMPEX/Alsat-1 实测。
- 约束值均锚到引用来源（NASA HPSC 白皮书、SiFive datasheet、IEEE TNS 实测 σ 等），
  见 `config/default.yaml` 的 temporal 段注释与 `atft_core.py` 顶部。

## 6. 当前关键实验结果（事实 + 口径标注）

均为本会话复核过的数值，除非标"既有"。空天核=gemm/kalman/orbit/pid/cnn_cls_decision。

- 逐类别（口径 1，部署栈 struct_full 精确 SIMD，ATFT_ABFT=1）：
  各类别 SDC 从 native 的 4.4–20.5% 压到 **0–0.9%**，开销 ~1.1–1.2×（随子集/规模浮动）。
- 全指令盲注（口径 2，k=1，300 trials/核，**列序：OK/DET_REC/SDC/DUE**）：
  - native：SDC 0.7–11%，DUE 6–21%。
  - struct_full：SDC **0–0.7%**，det_rec 69–98%，DUE 多数下降（12→1.7、21→8 等）。
  - 即"保护码也被打"的物理口径下，部署栈仍净降 SDC，且多数降 DUE。
- 防御面（静态 IR 指令增量，budget=1.0 全程）：exact_tmr +91.9%、approx_simd +102.5%、
  struct_full+ABFT **+162.7%**（四机制叠加，代码面最大；收益在运行时而非代码面）。
- 残余损失归因（口径 1）：残余 SDC 几乎只剩 fp64 0.9%（逃逸检测）；残余 DUE 集中在
  **cmp_branch 10.6% + int_arith 9.1%**（这两条机制只检不纠→安全停机）。
- detect+retry（挂到 cmp_branch+int_arith，口径 1）：DUE 11.1%→0、8.9%→0，SDC 不变，
  无故障零开销；根因是这些 DUE 为中途崩溃，需父级重执行而非末尾重判。
- 既有（README，未本会话复核）：自适应开销 1.03×，比软件 TMR 省 48–68%，Pareto 占优。

## 7. 度量脚本索引（`experiments/`，均从仓库根运行）

- `benchmark_harness.py`：构建基础设施 + RQ1–RQ8 主评测。
- `measure_e2e_struct.py`：部署栈逐类别 SDC/DUE/开销（struct_full 现为精确 SIMD）。
- `measure_blind_sdc.py`：全指令盲注口径（fi-blind+all）。
- `measure_authenticity.py`：SDC rel_err 容差扫描（真实性复核）。
- `measure_defense_surface.py`：防御面静态指令占比。
- `measure_cnn_capstone.py`：HPSC 上软件 FT vs 硬件锁步的宏观吞吐（模型估算）。
- `orbit_rates.py` / `temporal.py`：轨道 λ 剖面 → MTTF/每任务失效数。
- 其余 `measure_*.py`：各机制的专项消融（addr/cmp/int/load/sphere/budget 等）。

## 8. 口径与易错点（诚实记录，防重蹈）

- **STRESS_CSV 列序**：`...,trials,OK%,DET_REC%,SDC%,DUE%`（第 6/7/8/9 列）。
  **FI5_CSV 列序**：`...,trials,masked,DET_REC,benign,SDC,DUE,avg_err`（SDC=第9、DUE=第10）。
  本会话一度把 DET_REC 误读成 SDC，得出"部署栈 SDC 81%"的错误结论，后修正为
  det_rec 81%/SDC~0。**任何手写 awk/parse 先核列序**。
- 逐类别口径豁免 voter 内部，会系统性**高估**保护收益；全指令盲注才含保护码自易损。
- 子进程崩溃=DUE，发生在 invoke 中途，末尾重判够不到，需父级重执行。
- 开销数随子集/规模浮动；报结论须带 benchmark 子集与 scale。
- ABFT 运行时默认关，需 `ATFT_ABFT=1` 才真正校验（编译期插桩≠运行期启用）。

## 9. 现状缺口（事实陈述，非任务指派）

- 层次 A 的"能量""任务完成率"目前是**模型/估算**口径，未有端到端任务级仿真与实测功耗。
- 空天核规模小、输入为合成，未接真实遥测/图像与真实任务时长。
- 残余 SDC（fp64 逃逸检测）与残余 DUE（cmp_branch/int_arith）各自的上限/机理未系统刻画。
- "自身易损面"只到静态指令增量 + 全指令盲注两个口径，未做器件级/门级交叉验证。
- detect+retry 的重执行代价目前按整核上界估，区域级（只重跑崩溃所在循环）未实现度量。
- 约束（功率/码尺寸/吞吐/截止）已锚真实 spec，但多约束联合下的 Pareto 前沿只在部分维度展开。

