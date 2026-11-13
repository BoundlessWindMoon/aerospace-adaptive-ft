## C. 近似冗余 / 降精度冗余 / 松弛表决：创新性对标与查新（必读）

**Scope**：与本课题核心创新主张直接对标的先行工作——"1 个精确副本 + 2 个近似副本 + 由程序员/剖析确定的容差阈值 δ + 分级恢复（critical 精确重执行 / semi-critical 近似结果替换 / non-critical 默认值回退）"，以 LLVM IR 变换 + C 运行时实现，目标 x86_64 与 RISC-V RVV。

**结论先行（诚实评估）**：该主张的三根支柱在已有文献中均单独存在，且其中两根已经被**组合实现过**：

- "近似副本 + 阈值比较 + 用近似结果替换" = **Reduced Precision Redundancy (RPR / ANT)，Shim & Shanbhag, IEEE TVLSI 2004** —— 逻辑几乎逐字相同，只是在电路/DSP 层。
- "近似副本 + 松弛/近似表决器" = **ATMR 谱系（Sierawski 2006 → Sánchez-Clemente TNS 2016）** 与 **Chen/Han/Lombardi, IEEE TC 2017（IDMR/ITMR 近似表决）**。
- "程序员声明容差 + 含重执行与可接受降级的分级恢复" = **Relax (ISCA 2010)**、**Rumba (ISCA 2015)**、**BRAINIAC (HPCA 2015)**、**Approxilyzer (MICRO 2016)**、**ApproxABFT (TVLSI 2023/25)**。

因此，"approximate TMR with threshold voting" 作为**概念并不新**。可辩护的是具体的**组合方式** + **软件/编译器实例化** + **RVV/空天辐射场景**。改写建议见文末。

---

### A. 近似 TMR / 近似逻辑副本（电路层）

#### A1. Reducing Soft Error Rate in Logic Circuits Through Approximate Logic Functions
- **First author / venue**：B. D. Sierawski (Bhuva, Massengill) — **IEEE Trans. Nuclear Science**, 53(6), Dec 2006
- **Mechanism**：用*近似逻辑函数*（部分指定的 cofactor）替代完整 TMR 副本，面积更小但仍能屏蔽主要的 SEU 敏感输入空间；对非完全相同的模块做多数表决。
- **Closeness**："近似副本一起表决"的**源头**。**OVERLAP RISK: MEDIUM-HIGH** —— 思想相同（近似副本 + 表决器），抽象层不同（门级网表而非程序 IR），无 δ 阈值概念，无分级恢复。
- **Code**：无。**URL**：https://ieeexplore.ieee.org/document/4033879

#### A2. Partial TMR in FPGAs Using Approximate Logic Circuits
- **First author / venue**：A. J. Sánchez-Clemente (L. Entrena, M. García-Valderas, UC3M) — **IEEE Trans. Nuclear Science**, 63(4), 2016
- **Mechanism**：综合出两个近似副本（一个过逼近、一个欠逼近原布尔函数），使表决器对所关心的全部可达输入仍输出精确值；对 SEU 关键节点做选择性/部分保护，面积相比 TMR 大幅下降。
- **Closeness**：本课题最直接被对标的 "approximate TMR" 经典论文。**OVERLAP RISK: HIGH** —— 不得把 "approximate TMR" 这个**术语**或 "1 精确 + 2 近似" 的**拓扑**声明为新。差异：该工作仍保证输出精确（没有面向用户的误差预算），故缺少 "δ 容差" 这一维。
- **Code**：无。**URL**：https://doi.org/10.1109/TNS.2016.2574961 （preprint：https://scispace.com/pdf/partial-tmr-in-fpgas-using-approximate-logic-circuits-1qrw8zpec6.pdf ）

#### A3. Exploring the Use of Approximate TMR to Mask Transient Faults in Logic with Low Area Overhead
- **First author / venue**：I. A. C. Gomes (Martins, Reis, Kastensmidt, UFRGS) — **Microelectronics Reliability**, 55(9-10), 2015
- **Mechanism**：由子电路功能近似生成近似模块构成 ATMR；刻画"屏蔽能力 vs 面积"的权衡曲线。
- **Closeness**：同一拓扑，并明确给出了本课题想主张的"可靠性-代价权衡"框架。**OVERLAP RISK: HIGH**（概念）/ **LOW**（实现层次）。
- **URL**：https://doi.org/10.1016/j.microrel.2015.06.139

#### A4. Generation Methodology for Good-Enough Approximate Modules of ATMR
- **First author / venue**：A. S. Hassan (Arifeen, Moradian, J. A. Lee) — **J. Electronic Testing (JETTA)** 34(6), 2018
- **Mechanism**：在可靠性约束下自动搜索"足够好"的近似模块——即*对近似副本偏差设定可接受阈值*。
- **Closeness**：这是"δ 受限近似副本选择"最接近的硬件对应物。**OVERLAP RISK: HIGH**（"阈值受限的近似副本"这一想法）。
- **URL**：https://doi.org/10.1007/s10836-018-5747-4

#### A5. A Flexible Inexact TMR Technique for SRAM-based FPGAs
- **First author / venue**：S. Venkataraman（with A. Kumar, TU Dresden）— **DATE 2016**
- **Mechanism**："inexact TMR"：副本为可调近似程度的非精确版本；在 SRAM FPGA 上用故障覆盖率换 LUT 数量。
- **Closeness**：**OVERLAP RISK: MEDIUM-HIGH**。使用 "inexact TMR" 一词，本质就是本课题的 "approximate TMR"。
- **URL**：https://cfaed.tu-dresden.de/files/user/akumar/publications/DATE%202016%20Camera%20ready.pdf

#### A6. Approximate Triple Modular Redundancy: A Survey
- **First author / venue**：T. Arifeen (A. S. Hassan, J.-A. Lee) — **IEEE Access** 8, 2020（另有 Ammes et al., JICS 2024 综述）
- **Mechanism**：约 20 种 ATMR 生成方法、表决器设计、MSB 保护/LSB 松弛策略与度量指标的分类体系。
- **Closeness**：**这篇综述是最大的单一威胁文档** —— 读过它的审稿人会立刻说"近似 TMR 已有 15 年文献"。本课题**必须**引用并明确定位差异。
- **URL**：https://ieeexplore.ieee.org/iel7/6287639/8948470/09151958.pdf ；JICS 综述：https://jics.org.br/ojs/index.php/JICS/article/download/970/523/4334

---

### B. 降精度冗余 / 算法噪声容忍（最危险的先行工作）

#### B1. Energy-Efficient Signal Processing via Algorithmic Noise-Tolerance (ANT)
- **First author / venue**：R. Hegde (N. R. Shanbhag) — **ISLPED 1999**；扩展为 "Soft Digital Signal Processing", **IEEE TVLSI** 9(6), 2001
- **Mechanism**：主模块在激进电压下运行（允许出错）；一个廉价的*估计器*模块预测正确输出；若 |main − estimator| 超过阈值，则改用估计器的输出。
- **Closeness**：**OVERLAP RISK: HIGH**。这就是 1999 年发明的"近似副本 + 阈值比较 + 近似结果替换"。其*判决规则*与本课题 "semi-critical → 近似结果替换" 完全一致。
- **URL**：https://doi.org/10.1109/92.974895

#### B2. Reliable Low-Power Digital Signal Processing via Reduced Precision Redundancy (RPR)
- **First author / venue**：B. Shim (S. R. Sridhara, N. R. Shanbhag) — **IEEE TVLSI** 12(5), May 2004
- **Mechanism**：*1 个精确主模块 + 1 个降精度副本*。判决规则：若 |y_main − y_RPR| > T_h，则以降精度副本的结果 y_RPR 作为修正输出；否则放行 y_main。T_h 由副本已知的量化误差界推导。
- **Closeness**：**OVERLAP RISK: VERY HIGH / CRITICAL。** 本课题的机制在结构上就是把 RPR 从 1 个近似副本推广到 2 个、从定点 DSP 硬件搬到 LLVM IR。"降精度副本""容差阈值 δ""替换为近似结果"三者全部已存在。更致命的是 **Shim & Shanbhag, "Energy-Efficient Soft-Error Tolerant Digital Signal Processing", IEEE TVLSI 14(4), 2006**，把 RPR 专门用于**软错误**（而非仅 VOS）——即本课题的故障模型。
- **Code**：无。**URL**：http://shanbhag.ece.illinois.edu/publications/shim-tvlsi-2004.pdf ；DOI https://doi.org/10.1109/TVLSI.2004.826201

#### B3. Algorithmic Noise Tolerance vs. Approximate Error Detection
- **First author / venue**：Polito 团队（Savino, Di Carlo 等）— **J. Low Power Electronics and Applications (JLPEA)** 9(2):17, 2019
- **Mechanism**：直接比较 ANT 式降精度副本与近似错误*检测*；量化"廉价副本自身误差掩盖真实故障"的情形（δ 阈值的漏检问题）。
- **Closeness**：**支持**本课题 —— 它点名了论文必须测量的根本弱点（"落在检测阈值以下的误差不可见"）。**OVERLAP RISK: LOW**，引用价值高。
- **URL**：https://iris.polito.it/bitstream/11583/2736377/1/jlpea-09-00017-v3.pdf

#### B4. Exploiting Mixed-Precision Redundancy for Soft-Error Detection in LU Decomposition
- **First author / venue**：N. Sahraneshinsamani (S. Catalán, J. R. Herrero; UJI/UPC) — **Journal of Supercomputing**, 2026（线上 2025）
- **Mechanism**：利用混合精度 LU 中*本已存在*的低精度算子作为免费的冗余副本；通过比较高/低精度结果与精度推导出的阈值检测软错误；每个 panel 仅线性工作量、常数存储。论文明确指出局限："对低位有效位敏感度降低，可能产生低于检测阈值的误差"。
- **Closeness**：**OVERLAP RISK: VERY HIGH 且极新。** 这是 2025–26 年在 CPU 上、软件层做的"降精度副本 + 阈值比较"。必须引用并区分（它只做检测、单个近似副本、单一算子核、无分级恢复、无编译器自动化）。
- **URL**：https://futur.upc.edu/44256337 ；preprint https://assets-eu.researchsquare.com/files/rs-7871757/v1_covered_e7570591-2a45-47dd-a12b-2da86e08a58e.pdf

---

### C. 松弛 / 近似 / 阈值表决器

#### C1. Two Approximate Voting Schemes for Reliable Computing
- **First author / venue**：K. Chen (J. Han, F. Lombardi) — **IEEE Trans. Computers** 66(7):1227-1239, 2017
- **Mechanism**：**IDMR**（inexact double modular redundancy）与 **ITMR**：模块允许在有界非精确范围内不一致；表决器本身是*近似的* —— 接受差异落在允许误差边界内的结果，而不要求逐比特相同。
- **Closeness**：**OVERLAP RISK: VERY HIGH。** "对非精确副本用带接受边界的近似表决"正是本课题的表决器。任何 DSN/DATE 审稿人都会引用它。
- **URL**：https://doi.org/10.1109/TC.2017.2653780

#### C2. A Fault Tolerant Voter for Approximate Triple Modular Redundancy
- **First author / venue**：T. Arifeen (A. S. Hassan, J.-A. Lee) — **Electronics (MDPI)** 8(3):332, 2019
- **Mechanism**：专门为"输入本就合法地彼此不同"的 ATMR 场景设计表决器；pass-transistor + quadded-transistor 冗余；提出 "Quality of Circuit" 度量。
- **Closeness**：说明"对非相同的近似副本表决"已是一个有独立文献的子问题。**OVERLAP RISK: MEDIUM-HIGH**（表决器设计主张）。
- **URL**：https://www.mdpi.com/2079-9292/8/3/332

#### C3. A Novel Family of Weighted Average Voters for Fault-Tolerant Systems
- **First author / venue**：G. Latif-Shabgahi 等 — **ECC 2003**（以及经典 *inexact majority voter* 文献：Lorczak/Broen voters, 1989-1994）
- **Mechanism**：带可调一致性阈值 a（与滚降参数 q）的表决器；当 q→∞ 时退化为经典的**带硬阈值 a 的 inexact majority voter**。
- **Closeness**："阈值表决 / tolerant voting"概念可追溯到 1980 年代末的容错控制领域。**OVERLAP RISK: HIGH**（术语层面）—— 绝不可把"用容差阈值替代精确相等来比较副本"当作新想法，它是教科书级的 inexact voting。
- **URL**：https://skoge.folk.ntnu.no/prost/proceedings/ecc03/pdfs/108.pdf

---

### D. 软件层近似/选择性复制与低成本检查器

#### D1. SWIFT: Software Implemented Fault Tolerance / Software-Controlled Fault Tolerance
- **First author / venue**：G. A. Reis (J. Chang, D. I. August) — **CGO 2005**；**ACM TACO** 2(4), 2005
- **Mechanism**：编译器插入指令复制 + 控制流检查，在 store 边界做校验；精确比较。
- **Closeness**：本课题必须对比的基线。**OVERLAP RISK: LOW**（创新性），但**必须**作为 baseline。
- **URL**：https://liberty.princeton.edu/Publications/taco05_ft.pdf

#### D2. Shoestring: Probabilistic Soft Error Reliability on the Cheap
- **First author / venue**：S. Feng (S. Gupta, A. Ansari, S. Mahlke) — **ASPLOS 2010**
- **Mechanism**：症状式检测 + *剖析驱动的选择性*指令复制，仅复制 "high-value" 指令；接受少量残余用户可见错误（1.6%）。
- **Closeness**：**OVERLAP RISK: HIGH**（针对主张中"剖析定义关键性 → 差异化保护"那一半）。Shoestring 已在 LLVM 中做了剖析驱动的关键性分级。差异：其分级控制的是*检测覆盖*而非*恢复策略*，且副本是精确的。
- **URL**：https://llvm.org/pubs/2010-03-ASPLOS-Shoestring.pdf

#### D3. nZDC / Generic Soft Error Data and Control Flow Error Detection by Instruction Duplication
- **First author / venue**：M. Didehban (H. So, P. Gali, A. Shrivastava, K. Lee) — **DAC 2016**；**IEEE TDSC** 2023
- **Mechanism**：通过校验 store 结果 + 复制 compare/branch 实现近零 SDC；TDSC 2023 给出架构无关变体；用 Verilog 级全处理器故障注入验证。
- **Closeness**：最先进的*精确*复制基线。**OVERLAP RISK: LOW**，但它设定了评测门槛：要做 RTL/门级注入而非仅 IR 级比特翻转。
- **URL**：https://mpslab-asu.github.io/publications/papers/Didehban2023TDSC.pdf

#### D4. COAST: Compiler-Assisted Software Fault Tolerance（含多平台扩展）
- **First author / venue**：M. Bohman / B. James (J. Goeders, M. Wirthlin, H. Quinn; BYU) — **IEEE Trans. Nuclear Science** 66(1), 2019；"Applying Compiler-Automated SW Fault Tolerance to Multiple Processor Platforms", TNS 2019
- **Mechanism**：**一组 LLVM pass，自动插入 DMR/TMR 与表决，目标 x86、ARM、RISC-V**，并经中子与重离子束流验证（MWTF 提升 11.4–28×）。
- **Closeness**：**OVERLAP RISK: VERY HIGH（针对实现层主张）。** "LLVM IR 变换 + C 运行时插入 TMR+表决、跨 x86_64 与 RISC-V 可移植、面向空天"就是 COAST，已开源且做过束流实验。本课题的实现创新因此收缩为"在类 COAST pass 之上增加的近似副本 + δ 表决 + 分级恢复"。强烈建议：基于或显式对比 COAST，而不是重新声明这套基础设施。
- **Code**：**https://github.com/byuccl/coast** （MIT）
- **URL**：https://ccl.byu.edu/assets/Applying_Compiler-Automated_Software_Fault_Tolerance_to_Multiple_Processor_Platforms_2019.pdf

#### D5. ED4I: Error Detection by Diverse Data and Duplicated Instructions
- **First author / venue**：N. Oh (S. Mitra, E. J. McCluskey) — **IEEE Trans. Computers** 51(2), 2002
- **Mechanism**：副本在*变换后的数据*（×k 多样性）上执行，因此两副本有意*不逐比特相同*；逆变换后再比较。
- **Closeness**："第二个副本不是精确拷贝"的历史先例。**OVERLAP RISK: MEDIUM** —— 是多样性而非降精度，但它否定了"软件容错中非相同副本是新事物"的说法。
- **URL**：https://doi.org/10.1109/12.980007

---

### E. 幂等性 / 免检查点重执行恢复（"精确重执行"档）

#### E1. Relax: An Architectural Framework for Software Recovery of Hardware Faults
- **First author / venue**：M. de Kruijf (S. Nomura, K. Sankaralingam) — **ISCA 2010**
- **Mechanism**：`relax` ISA 扩展让**程序员标记区域**并指定恢复动作；检测到故障时重执行该区域，或采取*程序员指定的 discard / 替代动作*。编译器支持基于 LLVM。
- **Closeness**：**OVERLAP RISK: VERY HIGH（"程序员声明策略的分级恢复"）。** Relax 已提供恰好三类恢复语义 —— 重执行、discard（以降级值继续）、向上传播 —— 由程序员按区域选择。本课题的"精确重执行 / 近似替换 / 默认值回退"是对 Relax recovery-block 分类的细化。
- **Code**：无公开仓库（LLVM 原型）。**URL**：https://llvm.org/pubs/2010-06-ISCA-Relax.pdf ；DOI https://doi.org/10.1145/1815961.1816026

#### E2. Static Analysis and Compiler Design for Idempotent Processing
- **First author / venue**：M. A. de Kruijf (K. Sankaralingam, S. Jha) — **PLDI 2012**
- **Mechanism**：LLVM 分析把程序划分为可自由重执行的*幂等区域*，实现免检查点恢复；开销 8.2%，而 instruction-TMR 为 30.5%、checkpoint-and-log 为 24.0%。
- **Closeness**：**OVERLAP RISK: HIGH（"LLVM pass 内的免检查点精确重执行恢复"）。** 若论文主张"无需检查点的恢复"，这篇是该主张的拥有者，且已比 instruction-TMR 快 3–4×。必须对比。
- **URL**：https://research.cs.wisc.edu/vertical/papers/2012/pldi12-idem.pdf

#### E3. Encore: Low-Cost, Fine-Grained Transient Fault Recovery
- **First author / venue**：S. Feng (S. Gupta, A. Ansari, S. Mahlke, D. I. August) — **MICRO 2011**
- **Mechanism**：概率幂等性；仅在区域入口记录存在反依赖的 live-in，从而廉价回滚并重执行。
- **Closeness**：**OVERLAP RISK: MEDIUM-HIGH**（对应 C 运行时的恢复部分）。
- **URL**：https://doi.org/10.1145/2155620.2155667

#### E4. Clover / Bolt —— 编译器导向的轻量软错误韧性与检查点
- **First author / venue**：Q. Liu, C. Jung, D. Lee, D. Tiwari — **LCTES 2015**；Bolt — **SC 2016**
- **Mechanism**：幂等恢复结合可容忍检测延迟的检测机制；编译器导向的最小检查点。
- **Closeness**：**OVERLAP RISK: MEDIUM** —— 恢复代价论证的 SOTA 参考。
- **URL**：https://people.cs.vt.edu/dongyoon/papers/SC-16-Bolt.pdf

---

### F. 质量阈值驱动的韧性调优（"δ" 主张）

#### F1. Approxilyzer: Towards a Systematic Framework for Instruction-Level Approximate Computing and its Application to Hardware Resiliency
- **First author / venue**：R. Venkatagiri (A. Mahmoud, S. K. S. Hari, S. V. Adve) — **MICRO 2016**；**gem5-Approxilyzer, DSN 2019**
- **Mechanism**：输入*未修改的程序、一个质量度量、以及可选的可接受质量阈值 δ*，产出逐指令逐比特的质量影响剖析；再据此调优哪些指令需要保护 —— **1% 的输出质量损失可在 99% 韧性覆盖下换来最多 55% 的保护开销下降**。论文中使用的符号正是 δ。
- **Closeness**：**OVERLAP RISK: VERY HIGH（"剖析定义的容差阈值 δ 驱动差异化保护"）。** 这是该思想的参考实现，开源，且 gem5 版本支持多 ISA。
- **Code**：gem5-Approxilyzer 由 Illinois RSIM 组发布（仓库链接见论文第 V 节）。
- **URL**：http://rsim.cs.illinois.edu/Pubs/16-MICRO-approxilyzer.pdf ；http://rsim.cs.illinois.edu/Pubs/19-DSN-gem5Approxilyzer.pdf

#### F2. Rumba: An Online Quality Management System for Approximate Computing
- **First author / venue**：D. S. Khudia (B. Zamirai, M. Samadi, S. Mahlke) — **ISCA 2015**（IEEE Micro Top Picks 2015）
- **Mechanism**：用*廉价预测器*（线性估计/滑动平均/决策树）估计正确结果；预测误差超阈值的元素被标记；恢复模块**只对这些元素做精确重执行**，并与检测模块并行运行。
- **Closeness**：**OVERLAP RISK: VERY HIGH。** "廉价近似参考 + 阈值 + 选择性精确重执行" = 本课题的 critical 档，2015 年已发表于 ISCA。
- **URL**：https://doi.org/10.1145/2749469.2750371

#### F3. BRAINIAC: Bringing Reliable Accuracy into Neurally-Implemented Approximate Computing
- **First author / venue**：B. Grigorian (N. Farahpour, G. Reinman) — **HPCA 2015**（前作：AHS 2014, "Dynamically Adaptive and Reliable Approximate Computing using Light-Weight Error Analysis"）
- **Mechanism**：**保真度递增的多个近似加速器级联**，每级由轻量错误检查门控；若无任何一级满足质量界，则回退到**精确执行**。
- **Closeness**：**OVERLAP RISK: VERY HIGH（"分级恢复"）。** BRAINIAC 就是三档递进：廉价近似 → 更好近似 → 精确。本课题的三档与之一一对应。
- **URL**：https://doi.org/10.1109/HPCA.2015.7056067

#### F4. EnerJ / Rely / Chisel —— 带质量契约的语言级近似
- **First author / venue**：A. Sampson (PLDI 2011, EnerJ)；M. Carbin (OOPSLA 2013, Rely)；S. Misailovic (OOPSLA 2014, Chisel)
- **Mechanism**：类型系统 / 程序逻辑，程序员*标注*可近似数据并给出定量的可靠性与精度要求；编译器选择可证明满足这些要求的近似实现。
- **Closeness**：**OVERLAP RISK: HIGH（"程序员定义容差"）。** "程序员声明 δ" 的接口自 2011 年起就是近似计算的标准做法。应作为接口来源引用，而非作为创新声明。
- **URL**：https://dl.acm.org/doi/10.1145/1993498.1993518 （EnerJ）

---

### G. ABFT 与低成本/松弛检查器（数值核与 DNN）

#### G1. ApproxABFT: Approximate Algorithm-Based Fault Tolerance for Neural Network Processing（及 Vision Transformer 版本）
- **First author / venue**：X. Xue (Cheng Liu, Y. Han 等，中科院计算所) — **IEEE TVLSI** 2023/2025；arXiv:2302.10469
- **Mechanism**：带**自适应误差容忍阈值**的 ABFT：checksum 不匹配量与*逐块容差*比较，**仅当偏差超过阈值时才触发恢复**；动态块粒度优化平衡层间敏感度。结果：冗余计算减少 43.4%，可容忍软错误率提升约一个数量级。论文明确以航天安全关键部署为动机。
- **Closeness**：**本清单中对"以阈值取代精确比较 + 选择性恢复"这一主张的 OVERLAP RISK 最高者。** 已发表、很新、动机相同（航天）、机制相同（只是在算法层）。必须引用并显式区分。
- **URL**：https://arxiv.org/pdf/2302.10469

#### G2. Algorithm-Based Fault Tolerance for Matrix Operations
- **First author / venue**：K.-H. Huang (J. A. Abraham) — **IEEE Trans. Computers** C-33(6), 1984
- **Mechanism**：行/列 checksum 不变量以 O(n²) 代价检测并定位矩阵运算中的错误；按 checksum 修正恢复。注意：ABFT 的比较**从来**使用数值容差（舍入阈值）而非逐比特相等。
- **Closeness**：**OVERLAP RISK: MEDIUM-HIGH（"阈值比较"）** —— 不能声称基于容差的接受是新的；ABFT 自 1984 年起出于浮点必要性就一直这么做。
- **URL**：https://doi.org/10.1109/TC.1984.1676475

#### G3. FT-BLAS: A Fault-Tolerant High-Performance BLAS Implementation on x86 CPUs
- **First author / venue**：Y. Wu（Chen 等，UC Riverside）— **IEEE TPDS** 34(12), 2023
- **Mechanism**：混合策略：内存受限的 L1/L2 用寄存器级复制指令（借 AVX-512 mask 寄存器存比较中间结果），计算受限的 L3 用融合进 GEMM 汇编核的 ABFT checksum；开销 <3%，并快于 MKL/OpenBLAS/BLIS。
- **Closeness**：性能参照点 —— 说明在 x86 上精心工程化的*精确*软件冗余可做到 <3%。**这直接威胁本课题的代价论证**：若精确 ABFT 只要 3%，则近似副本的动机需要更锐利的界定（不规则/非线性代数负载、SEU 主导环境）。
- **Code**：见论文（UCR 组发布）。**URL**：https://www.computer.org/csdl/journal/td/2023/12/10262212/1QJwvP8Q9vq

#### G4. FT-GEMM / TurboFFT / FT-K-means —— GPU 上融合式在线 ABFT
- **First author / venue**：S. Wu / J. Chen 组（UC Riverside）— **ICS 2023** 及后续
- **Mechanism**：thread/warp/threadblock 三级 checksum 融合进 GEMM 流水；每分钟注入数百个错误下平均开销 8.89%。
- **Closeness**：**OVERLAP RISK: LOW**（机制），但对代价基线高度相关。**URL**：https://www.alphaxiv.org/abs/2305.02444

#### G5. FT-CNN: Algorithm-Based Fault Tolerance for Convolutional Neural Networks
- **First author / venue**：K. Zhao (S. Di, X. Liang, J. Chen, F. Cappello) — **IEEE TPDS** 32(7), 2021
- **Mechanism**：四种面向卷积的 checksum ABFT 方案，并给出覆盖率/开销的解析对比。
- **Closeness**：**OVERLAP RISK: LOW**；用于 "ABFT ≠ 冗余" 的对比维度。**URL**：https://arxiv.org/abs/2003.12203

#### G6. ConvGuard: Low-Cost Online Convolution Checksum Checker
- **First author / venue**：G. Dimitrakopoulos 组 — **IEEE TVLSI** 2021
- **Mechanism**：发现卷积的新*不变式*，使检查器仅用输入图像**边界像素**即可隐式预测输出 checksum，是极廉价的不变量检查器。
- **Closeness**："用不变量检查器取代副本"的实例。既支持本课题"廉价检查优于副本"的前提，也与之竞争。**OVERLAP RISK: MEDIUM**。
- **URL**：https://gdimitrak.github.io/papers/tvlsi21.pdf

---

### H. DNN 加速器中的降精度 / 裁剪式纠正

#### H1. Ranger: A Low-Cost Fault Corrector for DNNs through Range Restriction
- **First author / venue**：Z. Chen (G. Li, K. Pattabiraman) — **DSN 2021**
- **Mechanism**：在激活层之后插入范围限制算子；超出范围（故障）的值被压回剖析得到的合法区间，**无需重算即纠正错误输出**，残余误差由模型固有韧性吸收。
- **Closeness**：**OVERLAP RISK: HIGH（"用近似替换取代重执行"）。** Ranger 的恢复 = 用剖析得到的界替换被污染的值；本课题 semi-critical 档 = 用近似副本的值替换。哲学相同，都由剖析驱动。且有开源代码。
- **Code**：**https://github.com/DependableSystemsLab/Ranger** （MIT）；故障注入器 **https://github.com/DependableSystemsLab/TensorFI**
- **URL**：https://zitaoc.github.io/files/Ranger-DSN21.pdf

#### H2. FT-ClipAct: Resilience Analysis of DNNs and Improving their Fault Tolerance using Clipped Activation
- **First author / venue**：L.-H. Hoang (M. A. Hanif, M. Shafique) — **DATE 2020**
- **Mechanism**：统计剖析逐层激活分布以导出裁剪阈值 T；超过 T 的值映射为零 —— 即**剖析定义的阈值 + 默认值回退**。
- **Closeness**：**OVERLAP RISK: HIGH（针对 "non-critical 默认值回退" 档）** —— "把可疑值映射为安全默认值、阈值来自剖析"正是 FT-ClipAct 的贡献。
- **URL**：https://arxiv.org/abs/1912.00941 ；https://past.date-conference.com/proceedings-archive/2020/pdf/0820.pdf

#### H3. FitAct: Error-Resilient DNNs via Fine-Grained Post-Trainable Activation Functions
- **First author / venue**：B. Ghavami (Z. Fang 等, SFU) — **DATE 2022**
- **Mechanism**：逐神经元学习的激活界（粒度比 FT-ClipAct/Ranger 更细），事后训练得到。
- **Closeness**：**OVERLAP RISK: MEDIUM**；说明"阈值粒度"的设计空间已被充分探索。**URL**：https://www.sfu.ca/~zhenman/files/C22-DATE2022-FitAct.pdf

#### H4. ThUnderVolt: Enabling Aggressive Voltage Underscaling and Timing Error Resilience for Energy-Efficient Deep Learning Accelerators
- **First author / venue**：J. Zhang (K. Rangineni, Z. Ghodsi, S. Garg) — **DAC 2018**
- **Mechanism**：在电压欠标定下允许 MAC 阵列发生时序错误，并以算法方式补偿而非精确纠正。
- **Closeness**：**OVERLAP RISK: LOW-MEDIUM**（硬件、时序故障），但它是"接受有界误差而非精确纠正"的代表性加速器论文。
- **URL**：https://dl.acm.org/doi/10.1109/DAC.2018.8465918

#### H5. MATIC: Learning Around Errors for Efficient Low-Voltage Neural Network Accelerators
- **First author / venue**：S. Kim (P. Howe, T. Moreau, A. Alaghi, L. Ceze, V. Sathe) — **DATE 2018**
- **Mechanism**：memory-adaptive training + in-situ synaptic canaries，使权重 SRAM 可在失效点以下工作；错误被*训练绕过*而非纠正。
- **Closeness**：**OVERLAP RISK: LOW**，但作为"以近似容忍取代冗余"的必要引用。**URL**：https://arxiv.org/abs/1706.04332

#### H6. Minerva: Enabling Low-Power, Highly-Accurate Deep Neural Network Accelerators
- **First author / venue**：B. Reagen (P. Whatmough, R. Adolf, … D. Brooks) — **ISCA 2016**
- **Mechanism**：在精度约束下协同设计激进位宽缩减与 SRAM 故障缓解（bit-masking / razor 式）。
- **Closeness**：**OVERLAP RISK: LOW**；作为"降精度 + 故障缓解"的先例引用。**URL**：https://doi.org/10.1109/ISCA.2016.32

---

### I. 近似计算 × 辐射/SEU，以及随机计算

#### I1. Test and Reliability of Approximate Hardware（书章；approximation-aware fault classification）
- **First author / venue**：M. Traiola, A. Bosio, A. Virazel 等 — Springer, 2022（HAL: hal-03888016）；相关工作见 **ETS/ITC/VTS** 2018-2021
- **Mechanism**：用最坏情况误差界（WCE）把近似电路中的故障重分类为*可接受*与*不可接受* —— 即对故障影响施加 δ 阈值；带来显著的良率/测试成本收益。
- **Closeness**：**OVERLAP RISK: HIGH（"δ 阈值定义什么算错误"）。** 近似测试社区多年前就形式化了"阈值以下的误差可接受"。本课题 δ 形式化的必读文献。
- **URL**：https://inria.hal.science/hal-03888016/file/BookChapter___Test_and_Reliability_of_Approximate_Hardware.pdf

#### I2. Accurate Reliability Analysis Methods for Approximate Computing Circuits
- **First author / venue**：Z. Wang 等 — **Tsinghua Science and Technology** 26(6), 2021
- **Mechanism**：针对近似电路的 PTM 解析可靠性估计（近似带来的误差与故障带来的误差必须分离）。
- **Closeness**：支撑本课题必须回答的"如何区分副本近似误差与故障诱发误差" —— **近似 TMR 的核心科学难题**。**OVERLAP RISK: LOW**。
- **URL**：https://www.sciopen.com/local/article_pdf/10.26599/TST.2020.9010032.pdf

#### I3. An Architecture for Fault-Tolerant Computation with Stochastic Logic
- **First author / venue**：W. Qian (X. Li, M. D. Riedel, K. Bazargan, D. J. Lilja) — **IEEE Trans. Computers** 60(1), 2011
- **Mechanism**：随机比特流表示使每一位权重相同，单比特翻转仅造成 ±1/N 的误差；在极高错误率下仍优雅降级，且不需要任何副本或表决器。
- **Closeness**：对本课题整体前提的*替代方案* —— 通过改变数值表示免费获得优雅降级。**OVERLAP RISK: LOW**，但审稿人可能质问"为何不改用随机/冗余数值表示？"。配套：**Alaghi & Hayes, "Survey of Stochastic Computing", ACM TECS 12(2s), 2013**；反面意见：**Wang & Ni, "Can Stochastic Computing Truly Tolerate Bit Flips?"**
- **URL**：http://www.cctbio.ece.umn.edu/wiki/images/2/21/Qian_Li_Riedel_Bazargan_Lilja_An_Architecture_for_Fault-Tolerant_Computation_with_Stochastic_Logic.pdf

---

### J. RISC-V / RVV 专项与空天平台冗余

#### J1. Enhancing Fault Tolerance in High-Performance Computing: A Real Hardware Case Study on a RISC-V Vector Processing Unit
- **First author / venue**：M. Barbirotta (F. Minervini, C. Rojas Morales, A. Cristal, O. Unsal, M. Olivieri; Sapienza + BSC) — **IEEE Open Journal of the Computer Society** 5:553-565, 2024, DOI 10.1109/OJCS.2024.3468895
- **Mechanism**：在 **Vitruvius+ RVV 1.0 解耦向量协处理器**（向量寄存器最长 16384 bit）内部实现指令级复制；在真实硬件上做故障注入实验。其相关工作一节明确讨论了**"先把任务近似再复制"的任务级复制方案，即把近似计算与时间冗余结合**，因为近似重执行更便宜。
- **Closeness**：**OVERLAP RISK: VERY HIGH（针对 RVV 目标主张）** —— 若论文声称"首次为 RISC-V RVV 做容错"，这篇已先占，*而且*它已经把近似副本思想列为已知工作。
- **URL**：https://www.computer.org/csdl/journal/oj/2024/01/10694791/20wCXd2vwSA ；preprint https://www.techrxiv.org/doi/pdf/10.36227/techrxiv.171177404.45560143

#### J2. A RISC-V Fault-Tolerant Soft-Processor Based on Full/Partial Heterogeneous Dual-Core Protection
- **First author / venue**：F. Vigli (M. Barbirotta, A. Cheikh, M. Olivieri) — **IEEE Access** 12:30495-30506, 2024
- **Mechanism**：异构双核，其中检查核更弱/功能不完整，即**非相同（降能力）冗余**配合选择性保护。
- **Closeness**：**OVERLAP RISK: HIGH** —— "异构/更弱的副本"在结构上就是"近似副本"的硬件对偶。
- **URL**：https://doi.org/10.1109/ACCESS.2024.3369517

#### J3. Microcontroller Compiler-Assisted Software Fault Tolerance（束流验证的 COAST）
- **First author / venue**：M. Bohman (B. James, M. Wirthlin, H. Quinn, J. Goeders) — **IEEE Trans. Nuclear Science** 66(1):226-232, 2019（NSREC）
- **Mechanism**：面向航天 COTS 微控制器的 LLVM pass DMR/TMR；中子束流与故障注入双重验证；平均故障覆盖 98.9%。
- **Closeness**：为任何"空天计算容错框架"主张设定评测标准：**束流实验或至少 RTL 级注入 + MWTF 指标，而不仅是 IR 级比特翻转模拟**。
- **URL**：https://space.pitt.edu/sites/default/files/2024-10/Microcontroller-Compiler-Assisted-Software-Fault-Tolerance.pdf

#### J4. Evaluation of Algorithm-Based Fault Tolerance for Machine Learning and Computer Vision under Neutron Radiation
- **First author / venue**：S. Roffe (A. George, NSF SHREC) — **IEEE Aerospace Conference 2020**
- **Mechanism**：在空天级 COTS 平台上对 ML/CV 负载的 ABFT 做束流实测。
- **Closeness**：**OVERLAP RISK: LOW**，但它是现存最接近"空天 + 松弛校验"的实验研究；本课题"空天计算"定位需要它。
- **URL**：https://doi.org/10.1109/AERO47225.2020.9172617

#### J5. Software Fault Tolerance in Real-Time Systems: Identifying the Future Research Questions
- **First author / venue**：F. Reghenzani (G. Massari, W. Fornaciari 等) — **ACM Computing Surveys** 55(14s), 2023
- **Mechanism**：综述；明确把"近似计算带来一个新维度——用输出精度换实时性和/或容错保证"列为**开放研究问题**，并综述了混合关键性方案（Zhou 2017：HI-criticality 任务 3×/5× 重执行 + 丢弃 LO-criticality 作业）。
- **Closeness**：**本课题定位的最佳引用** —— 权威综述声明这一组合仍是开放问题。同时也是警告：混合关键性实时文献已经在做"按关键性分级冗余 + 优雅丢弃非关键工作"。
- **URL**：https://dl.acm.org/doi/10.1145/3589950

---

## 本课题相比已有工作的真实差异点与应如何表述创新性

- **不要把 "approximate TMR" 或 "阈值投票" 作为创新点。** 这两个术语都有 15-25 年的专属文献：ATMR 自 Sierawski (TNS 2006) 起、inexact majority voter 自 1980s 末、"approximate voting over inexact replicas" 已由 Chen/Han/Lombardi (IEEE TC 2017) 正式定名，RPR (Shim & Shanbhag, TVLSI 2004) 更是逐字做了"精确主体 + 降精度副本 + 阈值比较 + 用近似结果替换"。若论文声明"提出近似 TMR"，审稿人一句话即可否掉。正确表述是："**将电路级 ATMR/RPR 与 ANT 的误差容限思想上移到编译器 IR 层，并与可幂等重执行的分级恢复统一在一个框架内**"。
- **真正尚未被占住的位置是"组合 + 自动化 + 可移植"，而不是任何单一机制。** 现有工作各占一角：RPR 有阈值替换但只在 DSP 硬件、单副本、无重执行；Relax/Idempotence 有分级恢复但副本是精确的；COAST 有 LLVM + x86/ARM/RISC-V 的 DMR/TMR 但投票是比特严格相等；Approxilyzer 有 δ 剖析但只做"保不保护"的二元决策；ApproxABFT 有自适应阈值但绑定在 NN 的 checksum 上。**"δ 驱动的三级恢复策略，由编译器自动插桩、对通用 C 程序、跨 x86_64 与 RVV 一致生效"目前确无同款** —— 创新性应写在"自动化综合与跨 ISA 可移植的策略框架"这一层，而不是机制层。
- **必须正面回答"近似副本自身误差与故障误差不可分"这一科学难题，这反而可以成为真正的贡献点。** 当 |exact − approx| ≤ δ 时，故障被静默吞掉（Polito JLPEA 2019 与 UPC 2026 LU 论文都明确点出"低位有效位误差落在检测阈值以下"）。如果论文能给出 **δ 的可证明选取方法：由副本近似误差上界 E_approx 与应用可接受质量损失 Q 共同确定，并量化由此产生的 SDC 漏检率 / 残余误差分布**，这是 RPR 与 ApproxABFT 都未严格做的部分，比"提出近似 TMR"强得多。
- **RVV 这条线要立刻改写措辞。** Barbirotta 等 (IEEE OJ-CS 2024, Vitruvius+ RVV 1.0) 已在真实 RVV 硬件上做指令级复制并在相关工作中点名"先近似再复制"的任务级方案；Vigli 等 (IEEE Access 2024) 已做异构弱核（即"近似副本"的硬件对偶）。可行的差异化表述是："**在 RVV 可变向量长度 / 向量寄存器重命名语义下，如何在 IR 层生成降精度向量副本（例如 fp32 主体 + fp16/bf16 或缩短 LMUL 的副本）并做向量化的 δ 归约投票**" —— 这是向量化近似冗余的具体编译问题，确实没有现成答案。
- **成本论证需要重新找靶子，否则动机站不住。** FT-BLAS (TPDS 2023) 用精确冗余在 x86 上做到 <3% 开销，FT-GEMM 约 8.9%，Idempotence (PLDI 2012) 的恢复开销仅 8.2% 且已优于 instruction-TMR 的 30.5%。因此"TMR 太贵所以要近似"这个动机对规整线性代数是不成立的。应把目标明确限定在 **不规则控制流 / 非线性代数 / 无 ABFT 不变量可用的通用程序，且在空天高 SEU 率与严格功耗约束下**，并用实测数据证明在这一类负载上精确冗余确实是 2-3× 开销。
- **分级恢复的表述要承认谱系并提出可量化的新东西。** "精确重执行 / 近似替换 / 默认值回退"三档分别对应 Relax 的 recovery block、RPR+Ranger 的值替换、FT-ClipAct 的 clip-to-zero，BRAINIAC (HPCA 2015) 已经做了"多保真度级联 + 精确兜底"。可主张的新意在于 **恢复档位的自动判定**：用剖析得到的 per-region 质量敏感度 + 实时/能量预算，在编译期（而非人工标注）给每个区域分配恢复策略，并给出端到端质量 SLA 的可达性论证。这是 Relax（要程序员标注）与 Approxilyzer（只做保护与否）之间的真实空白。

---

## Must-read Top 10

1. **Shim, Sridhara & Shanbhag — "Reliable Low-Power DSP via Reduced Precision Redundancy", IEEE TVLSI 2004**（以及 Shim & Shanbhag, TVLSI 2006 软错误版）。唯一最具威胁的论文；在写下任何创新性表述之前先读它。http://shanbhag.ece.illinois.edu/publications/shim-tvlsi-2004.pdf
2. **Chen, Han & Lombardi — "Two Approximate Voting Schemes for Reliable Computing", IEEE TC 66(7), 2017。** 占住了"对非精确副本做近似表决"（IDMR/ITMR）。https://doi.org/10.1109/TC.2017.2653780
3. **Arifeen, Hassan & Lee — "Approximate Triple Modular Redundancy: A Survey", IEEE Access 2020。** 必须对标的 ATMR 全景图。https://ieeexplore.ieee.org/iel7/6287639/8948470/09151958.pdf
4. **Sánchez-Clemente, Entrena & García-Valderas — "Partial TMR in FPGAs Using Approximate Logic Circuits", IEEE TNS 63(4), 2016。** ATMR 的标准构造。https://doi.org/10.1109/TNS.2016.2574961
5. **de Kruijf, Nomura & Sankaralingam — "Relax", ISCA 2010** 与 **de Kruijf, Sankaralingam & Jha — "Static Analysis and Compiler Design for Idempotent Processing", PLDI 2012。** 占住了"程序员声明的分级恢复"与"LLVM 内免检查点重执行"。https://llvm.org/pubs/2010-06-ISCA-Relax.pdf ， https://research.cs.wisc.edu/vertical/papers/2012/pldi12-idem.pdf
6. **Venkatagiri et al. — "Approxilyzer", MICRO 2016**（及 gem5-Approxilyzer, DSN 2019）。占住了"剖析导出的质量阈值 δ 驱动韧性/开销调优"，且有开源工具链，应复用而非重造。http://rsim.cs.illinois.edu/Pubs/16-MICRO-approxilyzer.pdf
7. **Xue, Liu et al. — "ApproxABFT", IEEE TVLSI 2023/2025, arXiv:2302.10469。** 自适应容差阈值 + 选择性恢复，航天动机；最接近的在世竞争者。https://arxiv.org/pdf/2302.10469
8. **Bohman / James, Goeders & Wirthlin — COAST（IEEE TNS 2019 两篇）+ 代码。** LLVM 自动化 DMR/TMR 跨 x86/ARM/RISC-V，且经束流验证；本课题的实现主张必须相对于它来陈述。https://github.com/byuccl/coast
9. **Khudia, Zamirai, Samadi & Mahlke — "Rumba", ISCA 2015** 与 **Grigorian, Farahpour & Reinman — "BRAINIAC", HPCA 2015。** 廉价近似参考 + 阈值 + 选择性精确重执行 / 多保真度级联带精确兜底 = 本课题的分级恢复，均已发表。https://doi.org/10.1145/2749469.2750371 ， https://doi.org/10.1109/HPCA.2015.7056067
10. **Barbirotta et al. — "Enhancing Fault Tolerance in HPC: A Real Hardware Case Study on a RISC-V Vector Processing Unit", IEEE OJ-CS 5, 2024** 与 **Reghenzani et al. — "Software Fault Tolerance in Real-Time Systems", ACM CSUR 2023。** 前者先占了 RVV 创新性并把近似复制列为已知工作；后者是权威地指出"精度 vs 容错权衡仍为开放问题"的最佳定位引用。https://www.computer.org/csdl/journal/oj/2024/01/10694791/20wCXd2vwSA ， https://dl.acm.org/doi/10.1145/3589950

---

### 本报告的诚实性说明

上文 DOI/URL 取自实际检索到的页面；其中少数几条（Encore MICRO 2011、EnerJ PLDI 2011、Huang & Abraham 1984、Minerva ISCA 2016、Roffe 2020、Vigli 2024）是依据引用它们的论文中的文献条目推断的标准 DOI，而非从出版商页面直接确认 —— 列入正式参考文献前请逐条核验这 6 条。代码可用性仅确认了 COAST、Ranger、TensorFI；FT-BLAS / FT-GEMM / gem5-Approxilyzer 的仓库在各自论文中有提及，但本报告未验证其 URL。

**条目总数：46**（A1-A6 = 6、B1-B4 = 4、C1-C3 = 3、D1-D5 = 5、E1-E4 = 4、F1-F4 = 4、G1-G6 = 6、H1-H6 = 6、I1-I3 = 3、J1-J5 = 5；每条均带 OVERLAP RISK 评级，部分条目内另含同系列配套文献）
