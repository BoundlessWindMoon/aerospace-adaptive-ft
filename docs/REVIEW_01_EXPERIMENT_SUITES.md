# Review #1：实验负载与评测覆盖率

## 结论

1. **实验规模已达毕业水平**：12 个负载、每策略 8000 次注入，均落在已发表工作区间内（SWIFT 300 次/负载、SWIFT-R 250 次/负载、Trident 3000 次/负载）。
2. **最大短板是套件单一，不是数量**：7 个上游负载全是规整浮点数值核。顶会标配的 SPEC/MiBench/Parboil 类**不规则控制流 + 整数 + 指针**负载为 0，空天负载为 0。这同时削弱动机（规整线代上精确 ABFT 冗余开销 <3%，"TMR 太贵"站不住）。
3. **有 5 处 claim 与代码不符，必须先改**：ACCEPT 套件、CMSIS-DSP、PARSEC 3.0、"开源移植 COAST-TMR/LLTFI-SID"、SDCTune 的会议名。这是"不造假"红线，比补实验优先。
4. **缺统计口径**：1000 次/算子的置信区间从未报告，答辩必问。

---

## 一、现状：实际编译进去的是什么

| # | 负载 | 真实来源 | 是否上游原版 | 类型 |
|---|---|---|---|---|
| 1 | gemm | `third_party/polybench-c` 4.2.1 | 是（`#include`） | 规整 FP |
| 2 | 2mm | 同上 | 是 | 规整 FP |
| 3 | atax | 同上 | 是 | 规整 FP |
| 4 | bicg | 同上 | 是 | 规整 FP |
| 5 | jacobi-2d | 同上 | 是 | 规整 FP / stencil |
| 6 | kissfft | `third_party/kissfft/kiss_fft.c` | 是 | FP + 蝶形索引 |
| 7 | blackscholes | `third_party/llvm-loop-perforation/benchmarks/blackscholes` | 是（**非** PARSEC 3.0 原发行版） | FP + 超越函数 |
| 8 | sobel | `benchmarks/benchmarks.c` **自写** | 否 | 规整 FP / stencil |
| 9 | gaussian | 自写 | 否 | 规整 FP / stencil |
| 10 | fir | 自写（标注为 CMSIS-DSP） | 否 | 规整 FP |
| 11 | pid | 自写 | 否 | FP 递推 |
| 12 | ekf | 自写 | 否 | FP 递推 |
| — | telemetry_checksum | 自写，3 行 | 否 | 占位 |

上游原版 **7/12**，自写 **5/12**。`third_party/` 里的 `coast`、`LLFI`、`LLTFI`、`precimonious`、`llvm-tutor` **没有任何代码被编译进评测**。

## 二、开题 claim vs 实际

| 开题 4.3.1 claim | 实际 | 判定 |
|---|---|---|
| BLIS dgemm / dgemv | PolyBench gemm / atax / bicg | 合理替换，论文需一句说明 |
| KissFFT FFT | KissFFT 上游原版 | 达成 |
| CMSIS-DSP FIR | 自写 FIR，CMSIS-DSP 未引入 | **未达成，表述需改** |
| OpenCV 高斯 / Sobel | 自写，OpenCV 未引入 | **未达成，表述需改** |
| PID + EKF | 自写 | 达成（开题未承诺开源来源） |

## 三、与先进水平对标（已核实数据）

| 工作 | 会议/年份 | 负载数 | 套件 | 注入规模与层级 | 辐照 |
|---|---|---|---|---|---|
| SWIFT | CGO'05 | ~29 | SPEC CINT95/2000/FP2000 + MediaBench | 300 次/负载，二进制插桩寄存器级，Itanium 2 真机 | 无 |
| SWIFT-R/TRUMP | DSN'06 | 未确认 | SPEC CPU2000 + MediaBench | 250 次/负载，架构寄存器级，PowerPC 970 真机 | 无 |
| Shoestring | ASPLOS'10 | 6 | SPEC CPU2000 INT | >10k trial，物理寄存器堆级，PTLsim | 无 |
| SDCTune | **CASES'14** | 12（6 训 + 6 测） | NAS + SPLASH-2 + SPEC + PARSEC + Parboil | LLFI，IR 级；规模未确认 | 无 |
| Relyzer | ASPLOS'12 | 12 | SPLASH-2 + PARSEC + SPEC2006 | 1.6T → 3.52B 故障点剪枝，架构寄存器级，Simics+GEMS | 无 |
| Approxilyzer | MICRO'16 | 5 | PARSEC + SPLASH-2 | ~260 万次，整数+浮点寄存器级 | 无 |
| Trident | **DSN'18** | 11 | SPEC+PARSEC+Parboil+Rodinia+Lulesh 等 | 3000 次/负载（总 33000），LLFI IR 级 | 无 |
| nZDC | DAC'16 | 未确认 | MiBench | 微架构级，gem5 Cortex-A53 | 无 |
| COAST | IEEE TNS'19 | 3 | 自研自校验核 | 5000 次/设计，真芯片 SRAM 在线注入，MSP430FR5969 | **有，LANSCE 中子束** |
| ASPIS | TODAES'24 | 未确认 | 未确认 | STM32 板上注入 | 原文无 |
| **本课题** | — | **12（7 上游）** | PolyBench + KissFFT + blackscholes | **1000 次/算子/策略（8000 次/策略），IR 级 + 真实 PMU** | 无 |

逐维判定：

| 维度 | 本课题 | 对等水平 | 判定 |
|---|---|---|---|
| 负载数量 | 12 | 3–29，中位数 ~11 | **达标偏上** |
| 套件多样性 | 1.5 套（PolyBench + KissFFT） | 3–5 套混合 | **明显落后** |
| 负载类型覆盖 | 纯规整 FP | FP + 整数 + 不规则控制流 + 指针/图 | **明显落后** |
| 注入规模 | 8000/策略 | 250–3000/负载 | **达标** |
| 注入层级 | IR 级 | IR 级（LLFI 系）/ 寄存器 / 微架构 | 达标（与 SDCTune、Trident 同级） |
| 统计置信度 | 未报告 | Trident 报 95% CI ±0.07–1.76% | **缺失，必补** |
| 真实硬件 | x86 PMU 实测 | 多数真机或微架构模拟器 | 达标 |
| 辐照实验 | 无 | 仅 COAST / S-SETA 有 | 硕士可免，需写入效度威胁 |
| 空天负载 | 0 | — | **选题相关性缺口** |

## 四、必须修的 claim 不实（按优先级）

| 位置 | 现在的写法 | 事实 | 改法 |
|---|---|---|---|
| `generate_html_report.py:253`、`benchmarks.c:8` | "ACCEPT 近似套件" | `third_party/` 无 ACCEPT；sobel/gaussian 为自写 | 删除 ACCEPT，改"自实现的 Sobel/高斯卷积核（参照 ACCEPT 与 loop-perforation 基准语义）" |
| `benchmarks.c:9` | "CMSIS-DSP FIR" | 自写 FIR | 改"自实现 FIR 滤波器" |
| `benchmarks.c:7,106`、HTML | "PARSEC 3.0 Black-Scholes" | 来自 llvm-loop-perforation 的移植版 | 改"PARSEC Black-Scholes（经 llvm-loop-perforation 移植的单文件版本）" |
| `generate_html_report.py:756-757` | "COAST-TMR / LLTFI-SID [开源移植]" | 是 `AdaptiveFTPass.cpp` 内按论文语义**复现**的策略模式，未调用 `third_party/coast` 或 LLTFI 的 Pass | 全部改为"按原文语义在本框架内复现（reimplementation）"，并说明复现范围与原实现差异 |
| `AdaptiveFTPass.cpp:18,146`、HTML | SDCTune [HPDC'14] | **CASES 2014**（期刊版 ACM TECS 16(3) 2017） | 两处改正 |
| `AdaptiveFTPass.cpp:8` | COAST-TMR (BYU, IEEE TNS 2019) | 年份对；作者是 Bohman/James/Wirthlin/Quinn/Goeders，非 Keller & Wirthlin | 引用时写对作者 |
| 文献综述 | Trident DSN 2019 | **DSN 2018**，题为 *Modeling Soft-Error Propagation in Programs* | 改正 |

## 五、补实验优先级（按"答辩收益 / 成本"）

| 优先级 | 动作 | 成本 | 收益 |
|---|---|---|---|
| P0 | 修完上表 7 处 claim | 1 小时 | 消除造假风险 |
| P0 | 每个 SDC 率加 95% 置信区间（Leveugle DATE'09 口径） | 半天（纯后处理） | 堵住"1000 次够不够" |
| P1 | 接 2–3 个 MiBench 整数/控制流负载（如 `bitcount`、`dijkstra`、`stringsearch`） | 1 天 | 补齐唯一的硬缺口：非 FP、不规则负载 |
| P1 | 接 1–2 个 ESA **OBPMark** 核（纯 C，自带验证数据） | 1–2 天 | 把"面向空天"从标题落到实验 |
| P2 | 记录注入后真实偏差幅度，量化 δ 吞错窗口 | 半天（重跑） | 上一轮讨论的 Idea 1 |
| P3 | 真实跑一次 `third_party/coast` 的 TMR Pass 做交叉校验 | 2–3 天 | 把"复现"升级为"对比真实实现" |
| 不做 | 辐照实验 | — | 硕士无必要，写入效度威胁即可 |

---

# 附录：定稿后的实验套件（2026-11-12 已实施）

## 定稿原则

对标 SDCTune(CASES'14, 12 个 / 5 套)、Relyzer(ASPLOS'12, 12 个 / 3 套)、Trident(DSN'18, 11 个 / 5 套)
的做法：**负载数 10–15、套件来源 4–5 套、必须覆盖 FP 与整数/不规则控制流两类**。
自写内核全部删除，每个负载都是 `third_party/` 下上游源文件经 `#include` 直接编译。

## 15 个负载（0 自写）

| # | 负载 | 上游文件 | 类型 | 对应开题章节 |
|---|---|---|---|---|
| 1 | pb_gemm | polybench-c/linear-algebra/blas/gemm | FP64 规整 | 4.3.1 数值计算 |
| 2 | pb_2mm | polybench-c/linear-algebra/kernels/2mm | FP64 规整 | 4.3.1 |
| 3 | pb_atax | polybench-c/linear-algebra/kernels/atax | FP64 规整 | 4.3.1 |
| 4 | pb_bicg | polybench-c/linear-algebra/kernels/bicg | FP64 规整 | 4.3.1 |
| 5 | pb_jacobi2d | polybench-c/stencils/jacobi-2d | FP64 stencil | 4.3.1 |
| 6 | pb_ludcmp | polybench-c/linear-algebra/solvers/ludcmp | FP64 线性求解 | 替代自写 PID |
| 7 | pb_durbin | polybench-c/linear-algebra/solvers/durbin | FP64 线性预测 | 替代自写 EKF |
| 8 | pb_deriche | polybench-c/medley/deriche | FP64 图像递归滤波 | 替代自写高斯卷积 |
| 9 | pb_floyd_warshall | polybench-c/medley/floyd-warshall | **INT32 图/不规则** | 新增（补缺口） |
| 10 | pb_nussinov | polybench-c/medley/nussinov | **INT32 DP/分支密集** | 新增（补缺口） |
| 11 | parsec_blackscholes | llvm-loop-perforation/benchmarks/blackscholes | FP64 超越函数 | 4.3.1 |
| 12 | dsp_kissfft | kissfft/kiss_fft.c | FP64 DSP | 4.3.1 信号处理 |
| 13 | axc_sobel | llvm-loop-perforation/benchmarks/sobel/sobel.c | uint8 图像 | 4.3.1 图像处理 |
| 14 | coast_crc16 | coast/tests/crc16/crc16.c | **INT32 位运算** | 新增（COAST TNS'19 实测负载） |
| 15 | coast_quicksort | coast/tests/quicksort/quicksort.c | **INT32 递归+指针** | 新增（LANL，COAST TNS'19 负载） |

删除的 6 个自写内核：`axc_sobel`(自写版)、`axc_gaussian`、`dsp_fir`、`gnc_pid`、`gnc_ekf`、
`bench_noncritical_telemetry_checksum`。

套件来源 4 套：PolyBench/C 4.2.1（10）、KissFFT（1）、llvm-loop-perforation（2，含 PARSEC
Black-Scholes 移植版）、COAST tests（2）。类型覆盖 9 类：规整线代 / stencil / 线性求解 /
线性预测 / 图像递归滤波 / DSP 变换 / 超越函数 / 整数位运算与哈希 / 递归排序与图算法。

## 已验证

- `benchmarks/benchmarks.c` 单 TU 编译通过，15 个上游内核全部进入同一 LLVM Module；
- `native` / `exact-tmr` / `approx-simd` 三种策略下 15 个负载全部跑通；
- **exact-tmr 对 15 个负载输出与 golden 逐位一致**（rel_l2 = 0），证明 TMR 变换不改变语义；
- approx-simd 仅在近似实际生效处产生误差：`pb_deriche` 7.80e-5、`parsec_blackscholes` 1.08e-5，
  其余为 0（无故障时直接输出精确路 R）。
- 整数负载的 SDC 判据已改为按位精确（`bench_is_integer()`，阈值 0 而非 1%）。

## 发现的阻塞问题：5 个负载当前零保护

| 负载 | native 指令数 | exact-tmr 指令数 | 原因 |
|---|---|---|---|
| pb_floyd_warshall | 1,076,926 | 1,076,927 | 内核已内联进 `bench_*`，但 TMR 变换只复制 `double` 指令，整数通路不覆盖 |
| pb_nussinov | 204,595 | 204,595 | 同上 |
| coast_crc16 | 3,446 | 3,446 | `crc16()` 是独立外部函数，Pass 只遍历 `bench_*` 开头的函数 |
| coast_quicksort | 90,215 | 90,215 | `quick_sort()` 递归，无法 always_inline，且 Pass 不遍历它 |
| axc_sobel | 465,514 | 465,514 | `sobel_filtering()` 是独立外部函数，同样未被 Pass 遍历 |

两个根因（均在 `passes/AdaptiveFTPass.cpp`）：

1. **保护范围只到 `bench_*`**（`run()` 中 `name.startswith("bench_")`）。FP 负载之所以有效，
   是因为 PolyBench 的 `kernel_*` 被 `#define static __attribute__((always_inline)) static`
   内联进了 `bench_*`；非 static 的上游函数（sobel/crc16/quicksort）不会被内联，也就没被保护。
2. **TMR 与近似变换只处理 FP64**（`transformFunctionExactTMR` 只收集 `isDoubleTy()` 的
   load/phi/binop，投票用 `FCmpOEQ`）。整数数据通路完全没有冗余。

后果：开题 3.4.2.1 承诺的 **int32→int16 位宽压缩**在当前 15 个负载上**一个都测不到**，
因为唯一的整数负载恰好都在保护范围之外。

## 修复方案（待决策，两项都在 Pass 内）

- **A. 保护范围改为调用图闭包**：从 `bench_*` 出发 BFS 遍历被调函数，排除 `atft_*`/
  `harness_*`/`llvm.*`/libc。这样 sobel/crc16/quicksort 自动纳入，且不再依赖 always_inline
  这一脆弱前提（同时回应 nZDC(DAC'16) 与 ASP-DAC'26 指出的"IR 层冗余被后续优化削掉"问题）。
- **B. 整数数据通路冗余**：在 `transformFunctionExactTMR` / `transformFunctionApproxTMR`
  中增加 i32 分支（`ICmpEQ` 投票），复用运行时已有的 `atft_voter_exact_i32()` /
  `atft_voter_approx_i32()`。这会让 int32→int16 压缩第一次有可测量的载体。

两项完成后需重跑 RQ1–RQ8 并重新 dump golden。

---

# 附录二：方案 A + B 已实施（2026-11-13）

## A. 保护范围：`bench_*` → 调用图闭包

`passes/AdaptiveFTPass.cpp::run()` 的目标函数集合改为从 `bench_*` 入口出发做 BFS
调用图闭包，排除 `atft_*` / `harness_*` / `llvm.*` / `polybench_*` / `*_unused_main` / `main`。
原先按函数名单独补一遍 `kf_bfly*` 的特例代码已删除（闭包自动覆盖）。

新纳入保护范围的上游函数（由 `-atft-report` 的关键性报告验证）：

`sobel_filtering`(CRITICAL)、`crc16`、`quick_sort`、`kiss_fft`、`kiss_fft_stride`、
`kf_work`、`kf_factor`、`kiss_fft_alloc`、`kf_bfly2/3/4/5`、`kf_bfly_generic`。

意义：保护范围不再依赖"上游 `kernel_*` 恰好被 always_inline 内联"这一脆弱前提，
而由程序结构决定——这正面回应了 nZDC(DAC'16) 与 ASP-DAC'26 关于编译器优化
会削弱 IR 层冗余的论点。

## B. 整数数据通路精确 TMR（新增模块 2C）

新增 `transformFunctionExactIntTMR()`：收集 i32 的 load / phi / binop / store，
三路复制后用 `ICmpEQ`（而非 FP 通路的 `FCmpOEQ`）做按位一致性内联表决，
Store 汇点调用运行时已有的 `atft_voter_exact_i32()`。与 FP64 通路同样采用
"复制链不改写原数据流、内联表决由零开销 InlineAsm 保活"的设计，因此变换
是语义保持的（已由 15 个负载 exact-tmr 输出逐位一致验证）。

接线：
- `exact-tmr` 模式：`transformFunctionExactTMR` 之后追加整数 TMR；
- `approx-*` 模式：i32→i16 压缩之后，对**未被近似覆盖**的整数链（流入比较/分支/
  地址计算，截断位宽会改变控制流，因此近似不适用）回退到整数精确 TMR；
  被压缩覆盖的指令打 `atft.int16.approx` 元数据以免重复保护；
- 纯整数内核（无 FP64 可近似指令）此前会在 `selectedSet.empty()` 处提前返回，
  现改为转入整数通路保护。

## 覆盖率修复结果（硬件 PMU 实测，单次迭代平均退休指令数）

| 负载 | native | exact-tmr（修复前→修复后） | approx-simd（修复前→修复后） |
|---|---|---|---|
| pb_floyd_warshall | 1,076,926 | 1,076,927 → **8,426,612** | 1,076,926 → **8,426,612** |
| pb_nussinov | 204,595 | 204,595 → **1,916,537** | 204,595 → **1,916,537** |
| coast_crc16 | 3,446 | 3,446 → **23,469** | 3,446 → **115,806** |
| coast_quicksort | 90,215 | 90,215 → **287,389** | 90,215 → **324,149** |
| axc_sobel | 465,514 | 465,514 → **4,260,981** | 465,514 → **7,147,865** |

15 个负载在 `native` / `exact-tmr` / `approx-scalar` / `approx-simd` / `adaptive`
五种策略下全部跑通，且 **exact-tmr 输出与 golden 逐位一致**（rel_l2 = 0），
approx 仅在近似实际生效处产生误差。

## 由此暴露的三个真实结论（均为可写进论文的发现，不要掩盖）

1. **近似在整数不规则负载上没有收益——这是近似适用性的边界。**
   `pb_floyd_warshall` 与 `pb_nussinov` 的 approx-simd 指令数与 exact-tmr **完全相等**
   （8,426,612 / 1,916,537）：这两个内核没有可近似的 FP64 指令，整数链又因流入
   比较与控制流而不可截断位宽，只能退化为精确三模。应作为"关键性分析必须先判定
   可近似性"的论据，而不是当作失败。

2. **i32→i16 位宽压缩在位运算密集负载上是净亏损。**
   `coast_crc16` approx-simd 115,806 指令 vs exact-tmr 23,469（4.9× 更贵），
   `axc_sobel` 7.15M vs 4.26M，`coast_quicksort` 324K vs 287K。原因是每个操作数都要
   `trunc` + 2×`insertelement` 打包，而这些内核的运算链太短、无法摊销打包开销
   （与 survey Part G 对 `<2 x float>` 打包开销的分析同源）。
   → 需要给位宽压缩加**代价模型门控**（仅当链长 ≥ 阈值且不流入比较时才压缩），
   这正是方向 C 的工作内容。

3. **δ = 5×10⁻⁵ 对 `pb_deriche` 是误报源：1340 次表决 1340 次全部误报。**
   `pb_deriche` 的纯近似固有误差为 7.80×10⁻⁵ > δ，于是每次表决都判"精确链故障"
   并走恢复路径（最终输出仍正确，因为回退到精确值，所以这是性能/误报问题而非
   正确性问题）。`coast_crc16` 同理：1000 次表决 489 次误报（i16 截断值与 i32 原值
   本就不等）。
   → 这是开题 4.4.4 自己预判的"δ 过小导致误报"**第一次有实测数据支撑**，
   直接构成方向 B（δ 自动标定器）的动机：δ 必须按 per-function 的固有近似误差上界
   自动标定，而不是全局常数。

## 评测方法学同步改动

- 整数负载的 SDC 判据改为按位精确（`bench_is_integer()`，阈值 0 而非 1% 相对误差）；
- 所有故障注入率新增 **95% Wilson 置信区间**（`wilson_ci95()`，口径引
  Leveugle et al., DATE 2009），CSV 新增列 `sdc_ci95_lo` / `sdc_ci95_hi` /
  `sdc_ci95_halfwidth` / `det_recovered_ci95_halfwidth`。

---

# 附录三：接入 ESA OBPMark 空天基准（2026-11-14）

宿主机 `~/.bashrc` 里的代理（`http://agent.baidu.com:8891`，默认被注释掉）打开后可访问
GitHub，因此直接接入了 ESA 官方基准，不再需要退而求其次的替代方案。

## 选定：OBPMark v0.3 benchmark 1.1「影像校正与定标」

- 来源：`third_party/obpmark`（ESA + Barcelona Supercomputing Center，commit d9533e1）
- 引用：D. Steenari 等，OBPMark (On-Board Processing Benchmarks)，ESA/BSC，OBDP 2021
- 许可：ESA Public License (ESA-PL) Strong Copyleft v2.3（**注意**：该许可把"链接"也视为
  Modification，若论文产物要公开发布需评估传染范围）
- 新增 2 个负载，直接对应卫星成像仪在轨校正流水线，全部调用上游
  `src/1.1-image/src/ports/cpu/processing.c` 的内核函数：
  - `obpmark_image_prep`：[I] 偏置校正 `f_offset` → [II] 坏像元替换 `f_mask_replace`
  - `obpmark_image_proc`：[III] 辐射擦洗 `f_scrub` → [IV] 增益校正 `f_gain`
    → [V] 2×2 空间合并 `f_2x2_bin` → [VI] 多帧累加 `f_coadd`
- 唯一的适配：上游 `include/device.h` 经 `util_prints.h` → `benchmark.h` 引入 `<string>`
  等 C++ 头并声明重载 `init()`，无法用 C 编译器处理。因此提供
  `benchmarks/obpmark_shim/device.h` 作最小替身，逐字复刻其"顺序 C 版本"分支的
  `image_data_t` / `image_time_t` 类型；**计算内核源码一个字符都没改**。

至此：**17 个负载 / 5 个上游套件 / 0 自写**，其中 2 个来自 ESA 官方空天基准。
benchmark 2.1（CCSDS 121.0 数据压缩）与 2.2（CCSDS 122.0 影像压缩）暂未接入——它们的
`processing.h` 与 1.1 共用同一个 include guard `PROCESSING_H_`，单翻译单元会互相屏蔽，
需要把构建改成"多翻译单元分别编译成 .ll 再 llvm-link"。属于后续工作。

## 接入过程中发现并修复的一个真实缺陷：位宽压缩不做范围判定

接入 OBPMark 后，`approx-simd` 的输出相对误差从 1.23e-4 暴涨到 **1.17e+5**（完全错）。
根因：`transformFunctionInt32ToInt16Compression` 直接把 i32 运算截断成 i16，**从不检查
数值是否真的落在 16 位范围内**。OBPMark 的像元值约 900–9000，`f_coadd` 的多帧累加可达
3.6 万，超出 int16 的 ±32767，截断后结果彻底错误。

修复：引入 LLVM `ComputeNumSignBits`，要求结果与两个操作数都 ≥17 个符号位
（等价于"16 位有符号可表示"）才允许压缩。这正是开题报告 3.4.2.1 要求的
"识别数值范围受限的变量"那一步，之前完全没做。

修复后两个收益：
- 正确性恢复：`approx-simd` 最大相对误差回到 1.23e-4（只剩浮点近似的固有误差）；
- **开销下降**：FP 负载上的指令倍率从 9.00× 降到 **7.65×**，因为那些不安全的压缩
  本来既算错又纯属额外开销。

诚实的副作用：加上范围判定后，位宽压缩在 17 个负载上的适用面变得很小——绝大多数 i32
运算无法被证明落在 i16 范围内。这是个负面但可写的结论：**位宽压缩需要配合值域分析
或程序员标注才有收益**，否则要么不安全要么无处可用。

## 当前数据（1000 次注入/负载/策略，95% Wilson 置信区间）

fp 口径（11 个有浮点注入点的负载）：

| policy | 指令倍率 | SDC% | ±CI | Det&Rec% | DUE% | MWTF |
|---|---|---|---|---|---|---|
| native | 1.00 | 10.65 | 1.84 | 0 | 0 | 12.6 |
| EDDI | 7.67 | 1.83 | 0.50 | 0 | 54.99 | 251.3 |
| SWIFT | 5.59 | 3.65 | 1.06 | 0 | 37.53 | 21.8 |
| exact_tmr | 13.08 | 1.45 | 0.46 | 62.62 | 0.01 | 144.4 |
| **approx_simd** | **7.65** | **1.15** | 0.44 | 37.61 | 0.07 | **195.9** |

全类别口径（17 个负载全部有注入点）：native SDC 10.14%、approx_simd 8.34× / 5.15%，
exact_tmr 11.20× / 5.73%。

## 阻塞项（优先级最高）

OBPMark 两个负载上，保护后 SDC 几乎没降（prep 24.00% → 24.00%，proc 9.40% → 7.50%），
但指令倍率付到了 7.48×/11.37×。原因就是附录二末尾已记录的整数 TMR 缺陷：副本链在
`select` 与类型转换节点处断裂，store 汇点拿不到三路值，于是整数通路实际上没有被保护。
这个缺陷现在影响 **6 / 17** 个负载（4 个整数负载 + 2 个 OBPMark 负载），其中包括全部
空天负载，必须先修。

---

# 附录四：整数通路副本传播修复与模块化收尾（2026-11-14）

## 修复：整数 TMR 的副本链在 select / 位宽转换处断裂

`transformFunctionExactIntTMR` 原先只复制 i32 的 load / phi / 二元运算。
`min`/`max` 这类用条件选择表达的内核（floyd-warshall、nussinov、quicksort、
OBPMark 像元流水线）的值要经 `icmp` + `select` 才到达 Store，副本链在那里断掉，
Store 汇点拿不到三路值 —— 整数负载实际上没有被保护。

改为：可复制指令集合扩展到 `load / phi / 二元运算 / trunc / zext / sext / select / icmp`，
统一用"克隆 + 换操作数"复制；支持 i8 / i16 / i32；Store 汇点对窄类型先零扩展到 i32
再表决、表决后截回原宽度，复用运行时唯一的 i32 表决器（它同时统计检出次数）。

效果（整数口径，1000 次注入/负载，6 个整数负载）：

| policy | 指令倍率 | SDC% | 检出并恢复% | DUE% |
|---|---|---|---|---|
| native | 1.00 | 32.32 ± 2.76 | 0 | 31.15 |
| EDDI / SWIFT / LLTFI | 1.68–2.69 | 32.32 | 0 | 31.15 |
| exact_tmr | 9.06 | 28.20 ± 2.53 | 4.23 | 31.13 |
| approx_simd | 9.92 | **26.68 ± 2.50** | **11.23** | 31.13 |

修复前整数负载上所有策略的 SDC 与 native 完全相同（零保护），现在有了可测的下降。

## 剩余的覆盖边界（原理性，非 bug）

整数口径下 DUE 稳定在 31.1%，各策略一致。这些是注入到**地址计算与控制流**的故障：
下标被改写后程序写到错误位置或直接崩溃，Store 汇点的值级表决在原理上覆盖不到。
这正是 nZDC (Didehban & Shrivastava, DAC 2016) 指出的 IR 层复制的固有缺口
（EDDI 用影子地址保护访存，SWIFT 直接假设 ECC 保护内存）。应写入效度威胁，
而不是继续在值级 TMR 上加补丁。

## 浮点口径最终数据（11 个负载）

| policy | 指令倍率 | SDC% | 检出并恢复% | DUE% | MWTF |
|---|---|---|---|---|---|
| native | 1.00 | 14.01 ± 1.91 | 0 | 0 | 12.2 |
| EDDI | 7.67 | 3.65 ± 0.59 | 0 | 54.99 | 250.9 |
| SWIFT | 5.59 | 5.47 ± 1.15 | 0 | 37.53 | 21.2 |
| exact_tmr | 11.62 | 2.54 ± 0.54 | 62.62 | 0.01 | 162.2 |
| **approx_simd** | **6.35** | 2.42 ± 0.57 | 37.61 | 0.07 | **247.5** |

approx_simd 相对 exact_tmr：指令倍率 6.35× vs 11.62×（降 45%），SDC 2.42% vs 2.54%，
MWTF 247.5 vs 162.2。EDDI 的 MWTF 看似接近，但它 55% 的运行以不可恢复错误结束。

## 模块化收尾

- 基准注册表 `BENCHES[]` 驱动：数量、浮点/整数判据、实验子集全部从表推导；
  RQ5/RQ6 原先写死且已过期的下标数组（`{0,4,5,6,9,11}`，注释与实际负载早已不符）已删除，
  改为按语义标签 `BTAG_REPR` 选取；RQ4 的混合负载按名字解析。
- RQ1/RQ5/RQ6 三份重复的注入循环合并为一个 fork 隔离的公共引擎 `fi_run_campaign`。
- 删除失效代码：运行时内置注错器（`atft_configure_fi`、`atft_inject_bitflip_*`）、
  `atft_print_summary`、`atft_simd2_fmul_fadd_f32`、`harness_fi_arm_one_fault`、
  `harness_fi_was_injected`，以及已跑不通的 `experiments/run_all_experiments.sh`。
- harness 去硬编码：仓库根目录由 `__file__` 推导，工具链走 `ATFT_*` 环境变量，
  头文件路径集中为 `BENCH_INCLUDE_DIRS` / `HARNESS_INCLUDE_DIRS`。
- 默认进入 smoke 模式（约 10 秒、不写 CSV），全量需显式 `--full`（约 6 分钟）。
- 用法见 `experiments/README.md`。

---

## 附录五：时序层接入与一个被它暴露出来的陈旧二进制 bug（2026-11-14）

### 做了什么

把"条件概率 → 绝对速率"这一层补上，纯后处理，不改注入器：

| 新增 | 内容 |
|---|---|
| `experiments/orbit_rates.py` | 离线生成轨道几何剖面：SGP4 传播 → IGRF 总场强 → 偶极 L 壳 → SAA 判定。周期、SAA 占时、过境驻留全部来自同一次传播 |
| `experiments/temporal.py` | 读 RQ1 的 PMU 与 RQ6 的 P(F\|n)，输出 MTTF（带 95% CI）、MWTF、每任务失效数、按占时加权的复合 MTTF |
| `config` 的 `temporal:` 段 | 环境、故障空间、SAA 内外比扫描、检测事件数，全部可配 |
| `--rq rq9` | 接入点，缺 rq1/rq6 的 CSV 时跳过并提示 |

口径出处：求和链与复合 MTTF 来自 Ostler et al., IEEE TNS 56(6):3519-3526, 2009；
实际生效的单故障形式来自 Schirmeier, Borchert, Spinczyk, DSN 2015；MWTF 定义来自
Bohman et al., IEEE TNS 66(1), 2019；同绝对速率下的公平对比（fixed-rate campaign）
来自 Sotiropoulos et al., IEEE SOCC 2026；轨道 → 分段速率的配方来自
Jacobs et al., ACM TRETS 5(4):21, 2012。

### 轨道判据的两个独立校验

SAA 判据是 `|B| < 阈值 且 L < 2`。只用 `|B|` 会把整条磁赤道低场带一并圈进来
（与 SAA 常规地理范围的一致率只有 0.5）。阈值扫描后取 20000 nT，因为该档同时满足：

- 每次过境驻留 8.3 min（SAMPEX 轨道）/ 9.6 min（Alsat-1 轨道），落在
  Underwood (ESA SP-439, 2000) 独立实测的 5–10 min 内；
- 99.9% / 97% 的判定样本落在 SAA 的常规地理范围内。

复合 MTTF 精确还原轨道平均 MTTF（自洽性校验通过）。但要诚实说明：由于 SAA 内外
速率是从轨道平均反解的，这一步目前只校验算术，不提供新信息——要像 Ostler 那样
有信息量，必须对每个轨道条件独立取速率。

### 暴露出来的 bug：RQ5/RQ6 一直跑在上一代构建的二进制上

`RQ5`/`RQ6` 写死去找 `bin_{policy}_fi_fp_x86_64`，找不到就回落到 `bin_{policy}_fi`。
而 `fp` 是**四类别时代**的类别名；改成七类别后 RQ1 只构建
`fp64/fp32/int_arith/cast/load/addr/cmp_branch`，于是这两个实验一直静默使用
几次构建之前残留的旧文件。

修法：新增 `fi_binary()`，类别名从 `grids.rq5/rq6.fi_category` 读（当前为 `fp64`），
**不回落**，缺文件直接报错。

影响是实质性的。用新构建的注入器重跑后 P(F|n) 全线变化：

| policy | P(F\|1) 旧（陈旧二进制） | P(F\|1) 新 |
|---|---|---|
| native | 12.0% | 14.44% |
| exact_tmr | 2.0% | **0%**（0/1600） |
| approx_simd | 2.0% | **0%**（0/1600） |
| swift | 48.5% | 71.06%（全为 DUE） |

`exact_tmr` 的 P(F\|1)=0 与 Ostler 的理论预期一致（精确 TMR 单故障必被掩蔽）；
旧数据里的 2% 是陈旧二进制的假象。

### 当前结论（SAMPEX 轨道平均，4 MiB 故障空间，16000 次/点）

策略间比较统一用 **95% 下界**（$P_F$ 的 Wilson 上界）。三模方案出现零或个位数
失效，点估计无信息，拿"零失效的下界"比"有失效的点估计"不可比。

| policy | 指令倍率 | MTTF 下界 | MWTF 下界 vs native | 每 90 天失效数 |
|---|---|---|---|---|
| native | 1.00 | 1.23 h | 1 | 1683 |
| swift | 2.56 | 0.238 h | 0.056 | 8974 |
| exact_tmr | 8.74 | 373 h | 40.5 | 1.59 |
| approx_simd | 6.25 | 708 h | 101 | < 3.05 |

1. 两种三模方案的 MTTF 下界都是 native 的 **300 倍以上**。
2. `approx_simd` 的下界高于 `exact_tmr`，但 $P(F|1)$ 的差异只是 2 vs 0 个失效事件
   （各 16000 次），**Fisher 精确检验 p ≈ 0.5，不显著**；MWTF 下界 2.5 倍的差距里
   1.40 倍来自开销（8.74 vs 6.25），其余来自界的保守性。可写的结论是
   "不劣于精确三模，且便宜 29%"，不能写成"更可靠"。
3. 能显著区分的是 $k\ge2$ 的累积行为：k=8 时 `exact_tmr` 的 DUE 1.93%（约 309 个
   事件）vs `approx_simd` 0.03%（约 5 个）。但真实 λ 下 $k\ge2$ 的权重可忽略，
   不进 MTTF，只说明副本驻留变长后两者会分开。
4. `swift` 在本口径下净负（0.056×），全部由 70.59% 的 DUE 驱动。

### 第二处同类错误：RQ8 一直在跑四类别的旧名字

`BUILTIN_CONFIG["grids"]["rq8"]["categories"]` 里写死着四类别时代的
`["fp","int","address","branch"]`，`_deep_merge` 会保留它，于是改成七类别之后
RQ8 仍在跑旧四类——而 `default.yaml` 的注释声称"省略时沿用顶层 fault_categories"。
修法：删掉内置那份清单，让 `.get("categories", 顶层)` 真的回落到七类。

与陈旧二进制是同一类错误：**旧口径的残留物在改版后没有报错，而是静默生效**。

### 三个死文件已删除

`adaptive_mode_trace.csv` / `fault_injection_reliability.csv` /
`performance_accuracy.csv` 引用的是 `dgemm`/`dgemv` 这些早已删除的自写负载，
没有任何代码或文档读取它们。

### 单故障假设现在有定量依据

所有环境下单次执行的期望故障数 `a = λ·t_exec ≤ 4e-6`，`n ≥ 2` 的 Poisson 质量
`≤ 1e-50`，所以 Ostler 的求和在数值上惰性，退化为 Schirmeier 的单故障形式。
`a = 1` 需要状态在 SAA 内不被刷新地驻留 13–53 秒，而单次 kernel 执行只有 18–128 µs。
这同时说明 RQ6 的 `k ≥ 2` 曲线在真实速率下不影响 MTTF，它的价值在于界定
"副本驻留多久之后累积才开始重要"。
