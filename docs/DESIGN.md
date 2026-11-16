# 面向空天计算的自适应容错框架 (Aerospace Adaptive Fault-Tolerant Framework)

本仓库对应硕士学位论文课题 **《面向空天计算的自适应容错框架研究》** 的原型系统与实验评估工程。

## 一、 开源参考实现库 (`third_party/`)

所有与开题报告四大核心模块及实验基准直接对应的开源实现均已下载至 `third_party/` 目录：

| 开源仓库 | 本地路径 | 对应开题报告模块 / 章节 | 核心可复用代码定位 |
| :--- | :--- | :--- | :--- |
| **COAST** (BYU 编译器辅助软件容错框架) | `third_party/coast` | **2.2 软件容错 Baseline (TMR / DWC / EDDI / CFCSS)**<br>**3.3 多版本与错误计数** | - `projects/TMR/TMR.cpp`：完整软件三模冗余 Pass<br>- `projects/DWC/DWC.cpp` & `projects/EDDI/EDDI.cpp`：双模指令复制检测<br>- `projects/CFCSS/CFCSS.cpp`：开题报告 2.2.1 节引用的控制流签名校验 Pass<br>- `projects/dataflowProtection/cloning.cpp`：IR 指令与函数克隆<br>- `projects/dataflowProtection/synchronization.cpp`：表决器（Voter）插入与全局错误计数 `TMR_ERROR_CNT` |
| **LLTFI / LLFI** (DependableSystemsLab LLVM 故障注入与指令复制) | `third_party/LLTFI`<br>`third_party/LLFI` | **3.2 关键性分析 / 3.4 算术链复制**<br>**4.3 故障注入实验评估 (Bit-Flip)** | - `LLTFI/llvm_passes/instruction_duplication/InstructionDuplication.cpp`：单条算术指令复制 (`duplicateInstruction`) 与**算术依赖链复制** (`duplicateInstructionChain`)，以及插入运行时浮点比对函数 `compareFloatValues`<br>- `LLTFI/llvm_passes/core/FaultInjectionPass.cpp`：编译期在目标寄存器/指令后插桩 `injectFault`<br>- `LLTFI/runtime_lib/FaultInjectionLib.c`：运行时单比特翻转故障注入实现 |
| **Precimonious** (UC Davis LLVM 浮点精度降级框架) | `third_party/precimonious` | **3.4.2.3 数据类型降级 (FP64 -> FP32)** | - `src/ChangePrecision.cpp` & `src/Transformer.cpp`：在 LLVM IR 层将 `double` (`FP64`) 变量及算术运算链自动降级为 `float` (`FP32`)，并在边界插入 `FPTruncInst` / `FPExtInst` |
| **llvm-loop-perforation** (Cornell CS6120 循环近似与图像 Benchmark) | `third_party/llvm-loop-perforation` | **3.2 循环深度分析 (`LoopInfo`)**<br>**3.4 近似变换与图像基准测试** | - `loop-perf/LoopPerforation.cpp`：基于 `LoopInfo` 识别规范循环并改写步长的 Pass<br>- `benchmarks/sobel/sobel.c`：开题报告 4.3.1 节指定的 **Sobel 边缘检测** 单文件 C 实现<br>- `benchmarks/img-blur/blur.c`：开题报告 4.3.1 节指定的 **图像平滑滤波** 单文件 C 实现<br>- `tests/matrix_multiply/matrix_multiply.c`：矩阵乘法测试 |
| **llvm-tutor** (Out-of-tree 新版 LLVM Pass 模板集) | `third_party/llvm-tutor` | **工程底座：现代 LLVM Pass 插件脚手架** | - `lib/RIV.cpp` & `lib/DuplicateBB.cpp`：基于 `DominatorTree` 的数据流分析 Pass 与基本块复制 Pass 标准写法（适配现代 LLVM New Pass Manager） |
| **KissFFT** | `third_party/kissfft` | **4.3.1 基准测试程序 (数字信号处理)** | - `kiss_fft.c` / `kiss_fftr.c`：开题报告 4.3.1 节明确点名的 FFT 基准测试库（支持浮点与定点 `FIXED_POINT` 16/32-bit 切换，天然契合 3.4.2.1 位宽压缩对比） |

---

## 二、 开题报告四大模块 -> 最小可行系统 (MVP) 拆解

本着 **“不搞花里胡哨、逻辑自洽、全部真跑实测、两周内交付中期答辩”** 的原则，我们将系统设计为以下四层闭环结构：

### 1. 编译时关键性分析 Pass (`passes/CriticalityAnalysisPass`)
- **输入**：基准程序 LLVM IR (`.ll`)
- **实现**（参考 `llvm-tutor/lib/RIV.cpp` + `llvm-loop-perforation`）：
  - 调用 LLVM 内置 `LoopInfo` 获取每条指令所在基本块的循环嵌套深度 `LDepth`；
  - 从函数返回值 / 输出指针 `StoreInst` 沿 SSA `Use-Def` 链反向 BFS，计算指令到输出点的最短依赖距离 `Dist`；
  - 统计指令结果的 User 数量与跨基本块跨度作为 `VarLiveness`；
  - 按开题报告公式 (3-1) $S = w_1 \cdot \text{Dist} + w_2 \cdot \text{LDepth} + w_3 \cdot \text{VarLiveness}$ 计算每个函数/计算链的得分，打上 Metadata 标签：`CRITICAL` / `SUB_CRITICAL` / `NON_CRITICAL`。

### 2. 多版本与混合近似 TMR 代码生成 Pass (`passes/ApproxTMRPass`)
- **输入**：带关键性标签的 LLVM IR
- **实现**（融合 `LLTFI/InstructionDuplication.cpp` 的链复制 + `Precimonious/Transformer.cpp` 的精度降级）：
  - **传统精确 TMR 版本（Baseline）**：参考 `COAST` / `LLTFI`，将算术链复制 2 份精确副本（共 3 份 FP64/INT32 精确计算），末尾插入精确多数表决器 `voter_exact_tmr(r1, r2, r3)`；
  - **近似混合 TMR 版本（本课题核心）**：保留 1 条精确计算链 $R$（如 `double` / 标准 `sin`/`exp`），克隆 2 条近似计算链 $A_1, A_2$（在克隆链上执行 `double -> float` 降级、`i32 -> i16` 截断、或将 `sin/exp` 替换为二阶/三阶泰勒多项式近似函数，并支持打包为 2-lane SIMD 向量指令执行），末尾插入带阈值 $\delta$ 的三路动态投票函数 `voter_approx_tmr(R, A1, A2, delta, criticality_level)`。

### 3. 运行时自适应监控与误差恢复库 (`runtime/libatft_rt.a`)
- **纯 C 实现，零外部依赖，直接链接进被测程序**：
  - **三路阈值投票与分级恢复**（对应开题报告 3.4.4 节）：
    - 若 $|R - A_1| \le \delta$ 且 $|R - A_2| \le \delta$：无故障，直接返回精确值 $R$（零精度损失）；
    - 若 $|A_1 - A_2| \le \delta$ 但 $|R - A_1| > \delta$：判定精确链路 $R$ 发生单粒子翻转（软错误），**次关键代码**直接返回 $(A_1 + A_2)/2$ 降级恢复，**关键代码**触发重执行回调；
    - 若仅某一条近似链路偏离（如 $|R - A_1| \le \delta$ 而 $|R - A_2| > \delta$）：判定近似副本软错误，屏蔽故障并直接返回精确值 $R$。
  - **辐射事件间隔与滑动窗口监控器**（对应开题报告 3.3.3 节）：
    - 维护退休指令计数模拟值与滑动窗口 ECC 计数，实现报告中的 **指标 A**（连续 $N$ 次间隔 $< \bar{T}/\theta$）与 **指标 B**（窗口累计超阈值）状态机，动态切换全局模式指针 `g_protection_mode` (`LOW` $\leftrightarrow$ `HIGH`)。

### 4. 故障注入器与基准测试套件 (`benchmarks/` & `experiments/`)
- **故障注入**：复用 `LLTFI` 的 IR 级单比特翻转逻辑，在计算链执行过程中按指定故障率随机抽取寄存器值翻转 1 个 bit（IEEE-754 符号位/指数位/尾数位或整数位）；
- **基准测试（15 个负载，全部为 `third_party/` 上游源文件直接编译，无自写内核）**：
  1. **FP64 规整线性代数**：`pb_gemm` / `pb_2mm` / `pb_atax` / `pb_bicg`（PolyBench/C 4.2.1）
  2. **FP64 Stencil**：`pb_jacobi2d`（PolyBench/C stencils）
  3. **FP64 线性求解与线性预测**：`pb_ludcmp` / `pb_durbin`（PolyBench/C solvers，替代原自写 PID/EKF）
  4. **FP64 图像递归滤波**：`pb_deriche`（PolyBench/C medley，替代原自写高斯卷积）
  5. **INT32 不规则控制流**：`pb_floyd_warshall` / `pb_nussinov`（PolyBench/C medley）
  6. **超越函数密集**：`parsec_blackscholes`（PARSEC Black-Scholes，llvm-loop-perforation 单文件移植版）
  7. **信号处理**：`dsp_kissfft`（KissFFT v1.3.1 原版 `kiss_fft.c`）
  8. **图像处理**：`axc_sobel`（llvm-loop-perforation 上游 `benchmarks/sobel/sobel.c`）
  9. **INT32 整数位运算与递归**：`coast_crc16` / `coast_quicksort`（COAST tests，BYU/LANL，
     即 COAST 在 IEEE TNS'19 做 LANSCE 中子束实测时所用负载）

  说明：开题报告 4.3.1 原计划的 BLIS `dgemm`/`dgemv`、CMSIS-DSP FIR、OpenCV 高斯/Sobel 分别
  由 PolyBench/C 的 `gemm`/`atax`/`bicg`、KissFFT、llvm-loop-perforation 的上游 `sobel.c` 与
  PolyBench `deriche` 等价替代，原则是**每个负载都必须有可追溯的上游开源实现**。
- **自动产出三张中期答辩核心图表的数据**：
  1. **性能对比**：Native vs 传统软件 TMR vs 近似混合 TMR vs 自适应模式（执行时间与动态指令数，验证开销降低 $\ge 1/3$）；
  2. **可靠性对比**：10,000 次蒙特卡洛单比特翻转故障注入下的故障检测率（$\ge 95\%$）、SDC（静默数据损坏）率、恢复成功率；
  3. **精度与动态自适应曲线**：输出相对误差（$\le 1\%$）及模拟太阳耀斑辐射脉冲下的 `LOW -> HIGH -> LOW` 状态切换轨迹。
