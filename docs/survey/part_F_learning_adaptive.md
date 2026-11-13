## F. 脆弱性预测、权重学习与运行时自适应（含空间辐射环境建模）

> 面向论文《面向空天计算的自适应容错框架研究》。本部分服务两个待加固的设计点：
> - **设计点 1（静态关键度打分）**：`S = w1*Dist + w2*LoopDepth + w3*VarLiveness`，三权重手工设定、缺乏可解释性与可验证性。
> - **设计点 2（运行时状态机）**：以 ECC 事件**到达间隔**（指标 A）与**滑动窗口计数**（指标 B）在 LOW/HIGH 两级保护间切换，底层假设软错误服从 Poisson 过程。
>
> 下文每条包含：名称 / 第一作者 / 会议+年份 / 机制 / **对本课题的用途** / 代码 / 论文链接。
> 链接均为公开可访问地址（部分 IEEE/ACM 正文需机构权限，已尽量给作者自存版 PDF）。

---

### F.1 指令级脆弱性分析：穷举注入的剪枝与等价类（本课题"关键度"的 ground truth 来源）

**F1. Relyzer** — Siva Kumar Sastry Hari, **ASPLOS 2012**（配套 DSN 2012 检测器工作）
- 机制：对"每条动态指令的每个操作数位"定义 error site，用控制等价（control equivalence）、存储等价（store equivalence）与预测性剪枝把 10^9 量级的故障空间压缩 3–6 个数量级（剪掉 99.78% 注入点），只对每个等价类的 pilot 做一次注入，用 pilot 结果外推整类；验证准确率约 96%。
- 对本课题：这是获得**逐指令 SDC 概率标签**的经典方法。要把 (w1,w2,w3) 从"拍脑袋"变成"拟合出来"，必须先有标签；Relyzer 的等价类剪枝是在可接受时间内为空天 workload（姿控、图像压缩、星上 AI 推理）生成标签集的标准做法。论文中用于论证"穷举注入不可行 → 需要静态代理指标"的论证链条。
- 论文：http://rsim.cs.illinois.edu/Pubs/12-asplos-relyzer.pdf ；讲稿 https://sivahari.github.io/talks/12-asplos-relyzer.pdf

**F2. Approxilyzer** — Radha Venkatagiri, **MICRO 2016**
- 机制：在 Relyzer 之上引入**输出质量**维度，把结果分为 masked / detected / output-corrupted，并对 corruption 量化质量退化幅度，产出"全程序指令级错误质量画像"。用户只需提供端到端质量度量。
- 对本课题：把二值的"是否 SDC"升级为**连续的危害度**。本课题的三级分类（critical / semi-critical / non-critical）如果要摆脱人为分界，最自然的做法就是用 Approxilyzer 风格的质量退化分布做聚类或分位数切分，而不是对 S 设两个阈值。
- 论文：http://rsim.cs.illinois.edu/Pubs/16-MICRO-approxilyzer.pdf

**F3. gem5-Approxilyzer** — Radha Venkatagiri, **DSN 2019**（开源）
- 机制：把 Approxilyzer 从专有 Simics + SPARC 迁移到开源 gem5 + x86，模块化以便扩展 ISA。
- 对本课题：**最现实的可复现实验平台**。本课题若做 RISC-V 星载处理器，可沿此路线扩展 ISA；其 store/control equivalence 实现可直接读代码。
- 代码：https://github.com/rsimgroup/gem5-approxilyzer
- 论文：http://rsim.cs.illinois.edu/Pubs/19-DSN-gem5Approxilyzer.pdf

**F4. Minotaur** — Abdulrahman Mahmoud, **ASPLOS 2019**
- 机制：把软件测试的覆盖率/最小化/优先级排序技术迁移到硬件错误分析，显著降低 error-injection 实验预算。
- 对本课题：为"UT 规模 vs 置信度"给出方法论依据，可用于设计本课题注入实验的采样方案。
- 论文：https://ma3mool.github.io/publications

---

### F.2 解析式与统计式 SDC 概率建模（无需注入，可直接当成打分函数替代品）

**F5. Trident** — Guanpeng Li, **DSN 2018**（Li & Pattabiraman，NVIDIA/UBC；开源）
- 机制：三层解析模型（静态指令级 f_s、控制流分叉 f_c、内存依赖 f_m）逐层计算错误传播概率，无需任何故障注入即可预测整程序与单指令 SDC 概率。整程序预测 14.83% vs 注入实测 13.59%，MAE 4.75%；去掉控制流/内存子模型后误差升至 15–20%。实现为一组 LLVM Pass + Python 驱动。
- 对本课题：**头号可替代方案**。Trident 的 f_s 子模型本质上就是"掩码/位宽/操作类型"带来的屏蔽概率，f_c 对应控制流，f_m 对应活跃变量与内存依赖——正好是学生 `VarLiveness` 想表达但没表达好的东西。建议：用 Trident 的逐指令 SDC 概率作为**回归目标**去拟合 (w1,w2,w3)，或者干脆用 Trident 概率直接替换 S。
- 代码：https://github.com/DependableSystemsLab/Trident
- 论文：https://research.nvidia.com/sites/default/files/pubs/2018-06_Modeling-Soft-Error/18-DSN-Li-Trident_0.pdf

**F6. VTrident** — Guanpeng Li, **DSN 2018**
- 机制：简化 Trident 的内存依赖子模型，用于**跨多输入**界定 SDC 概率的波动范围（INSTRUCTION-SDC-VOLATILITY），比注入或完整 Trident 快得多；给出概率区间 [S−R/2, S+R/2]。实测整程序 SDC 概率在不同输入间波动 1.54×–42×。
- 对本课题：直接回答审稿人必问的"你的静态打分换个输入还成立吗"。空天任务输入分布（过境数据、不同成像场景）差异大，必须引用此文说明**静态权重的输入敏感性**，并把 VTrident 的区间界定作为"打分置信区间"引入。
- 论文：https://blogs.ubc.ca/karthik/files/2018/04/vTrident-DSN18.pdf

**F7. GPU-Trident** — Abdul Rehman Anwer, **2020（SELSE 2019 预研 + 期刊版）**
- 机制：把 Trident 扩展到 GPU 核，补建**线程间数据依赖**模型；不建模线程间依赖时 MAE 达 17.2%，补齐后降至 5.7%，逐 kernel Pearson 相关 0.83。
- 对本课题：若框架要覆盖星上 AI 加速器/并行核，这是"并行域打分需要额外项"的直接证据。
- 论文：https://www.mbsullivan.info/attachments/papers/anwer2020gpu.pdf

**F8. ePVF** — Bo Fang, **DSN 2016**（注意：常被误引为 DSN 2017）
- 机制：在 PVF（ACE bits）基础上，用错误传播模型 + 平台相关的 crash 模型**剔除 crash-causing bits**，只留下真正导致 SDC 的位。相比原始 PVF 把脆弱位估计降低 45%–67%，识别 crash 位的 recall 89% / precision 92%，并用于指导选择性保护。
- 对本课题：说明**"脆弱"必须区分 crash 与 SDC**。空天场景里 crash 可由看门狗+重启兜住，SDC 才是致命的；本课题的三级分类若不做这个区分，会把大量"会崩但无害"的指令误判为 critical，白付开销。
- 论文：https://csdl.ieeecomputer.org/proceedings/dsn/2016/ 对应条目；摘要页 https://blogs.ubc.ca/dependablesystemslab/2016/02/27/epvf-an-enhanced-program-vulnerability-factor-methodology-for-cross-layer-resilience-analysis

**F9. PVF（Program Vulnerability Factor）** — Vilas Sridharan, **HPCA 2009**（"Eliminating microarchitectural dependency from Architectural Vulnerability"）
- 机制：从 AVF 中剥离微架构依赖，定义只依赖程序本身的 PVF，使软件层脆弱性可在不同微架构间迁移；配套 HPCA-15 另一篇研究**输入数据对 PVF 的影响**，结论是程序脆弱性变化主要源于执行画像（哪些代码区被执行）。
- 对本课题：`Dist`/`LoopDepth` 这类静态量之所以能当代理指标，理论依据正是 PVF 的"执行画像决定脆弱性"。这是**替换权重时必须引用的第一性原理**：权重应由执行画像（剖析）给出，而非常数。
- 论文：https://www.computer.org/csdl/proceedings-article/hpca/2009/04798243/12OmNqHItFE

**F10. ACE 分析 / AVF** — Shubhendu S. Mukherjee, **MICRO 2003**
- 机制：Architecturally Correct Execution 分析，统计某结构中"对正确执行必需"的比特占比，得到 AVF = 故障影响输出的概率；基于区间的生命周期（lifetime）分析。
- 对本课题：`VarLiveness` 项的学术祖宗。应明确写成"本文的变量活跃度项是 ACE 生命周期分析的轻量化近似"，并引 Wang & Patel（ISCA 2007）指出 ACE 分析相对注入**偏保守**（高估脆弱性），这正是本课题需要校准权重的原因。
- 论文（ACE）：MICRO-36, 2003；（ACE 精度质疑）N. Wang, A. Mahesri, S. Patel, "Examining ACE analysis reliability estimates using fault injection", **ISCA 2007**

**F11. FastFlip** — Keyur Joshi, **ASPLOS 2025**
- 机制：组合式（compositional）SDC 弹性分析：分段做注入 + SDC 传播分析，再把各段结果组合，避免重分析整程序；适合代码增量修改后快速重评。
- 对本课题：本课题如果要在**星上软件迭代升级**后重新打分，这是"增量重评"的参考方案，可写进未来工作。
- 论文：https://dl.acm.org/doi/pdf/10.1145/3696443.3708938

---

### F.3 机器学习驱动的 SDC 易发指令识别（直接对标"替换手工权重"）

**F12. SDCTune** — Qining Lu, **CASES 2014**
- 机制：基于**静态 + 动态特征**的启发式经验模型，预测程序数据的 SDC proneness，再据此做可配置的选择性保护。特征里**显式包含循环嵌套深度**：文中指出内层循环比较指令的 SDC 脆弱性一般低于外层循环比较指令。
- 对本课题：**最直接的"LoopDepth 项有效性"引文**，同时也是反例——SDCTune 需要人工调参（manual tuning），与学生当前困境完全一致。论文里应写："SDCTune 同样面临人工调参问题，其后继工作 SDCAuto 通过机器学习消除了这一步，本文采用同样思路。"
- 论文：https://dl.acm.org/doi/10.1145/3014586 （TECS 期刊扩展版，含 SDCTune + SDCAuto）

**F13. SDCAuto / "Configurable Detection of SDC-causing Errors in Programs"** — Qining Lu, **ACM TECS 16(3):88, 2017**
- 机制：两个经验模型 SDCTune（人工调参）与 **SDCAuto（全自动，基于回归树/CART）**，用静态与动态特征预测 SDC proneness，无需故障注入即可排序并选择保护目标。
- 对本课题：**路线一的首选基线**。把 (Dist, LoopDepth, VarLiveness) 作为特征喂给回归树，让模型自己学出分裂点与重要度——既替换了权重，又天然给出**可解释性**（特征重要度 + 决策路径），正面回应"权重缺乏可解释性"。
- 论文：https://dl.acm.org/doi/10.1145/3014586 ；学位论文全文 https://open.library.ubc.ca/soa/cIRcle/collections/ubctheses/24/items/1.0167661

**F14. SVM 识别 SDC 易发指令** — Na Yang & Yun Wang, **IEEE Access 7:40210, 2019**
- 机制：用支持向量机对指令做 SDC 脆弱/非脆弱二分类，并论证**部分故障注入**（partial fault injection）足以训练模型。
- 对本课题：给出"只需少量注入即可标注训练集"的可行性论证，直接决定本课题实验预算是否可承受。
- 论文：https://ieeexplore.ieee.org/document/8671738

**F15. LSTM 指令 SDC 脆弱性预测** — Yunfei Liu, **ADMA 2018**（Springer LNCS）
- 机制：把指令序列当作序列数据，用 LSTM 预测单指令 SDC 脆弱性；需要较大规模注入数据集。
- 对本课题：序列模型路线的代表；其"需要大数据集"的局限是本课题选择树模型/GNN 而非深度序列模型的理由。
- 论文：https://doi.org/10.1007/978-3-030-05090-0_12

**F16. 轻量级脆弱性模型的 SDC 检测** — Yunfei Liu, **ACML 2019（PMLR v101）**
- 机制：显式构造特征向量：掩码特征 v_mask（AND 0 / OR 1 / 右移会屏蔽错误位）、循环信息向量 v_loop、数据依赖等，再做轻量回归，用于 SDC 检测器放置。
- 对本课题：**特征工程清单可直接抄**。学生的三项特征过于单薄，补上 v_mask（位屏蔽）、操作数位宽、指令类型、动态执行次数后，特征空间才足以支撑学习。
- 论文：https://proceedings.mlr.press/v101/liu19c/liu19c.pdf

**F17. PARIS** — Luanzheng Guo & Dong Li, **PRDC 2018 / JPDC 2021 期刊版**
- 机制：机器学习预测应用弹性（application resilience），避开随机注入的高成本；报告在 SDC 率预测精度上**比 Trident 高至少 63%**。
- 对本课题：**"学习法优于解析法"的最强引文**，支撑本课题选择学习路线而非纯解析建模。
- 论文：https://arxiv.org/abs/1812.02944 ；期刊版 https://www.sciencedirect.com/science/article/am/pii/S0743731521000368

**F18. 基于图神经网络的函数级 SDC 脆弱性与关键度预测** — （TR Dizin 收录，近年工作）
- 机制：面向 CPU 科学计算程序，构造**函数级**预测框架，用 GNN 在程序图（CFG/DFG/依赖图）上预测 SDC 脆弱性与关键度。
- 对本课题：把"距离/循环深度/活跃度"这些**手工图特征**换成 GNN 自动学到的图嵌入——这是替换权重最彻底的路线（见 F.6 路线三）。
- 论文：https://search.trdizin.gov.tr/tr/yayin/detay/1373681

**F19. GVR：指令级软错误脆弱性预测与排序的统一图神经框架**
- 机制：统一的 GNN 框架，同时做 GPGPU 程序指令级脆弱性**预测**与**排序（ranking）**。
- 对本课题：**"排序"而非"打分"是更正确的问题形式**。本课题真正需要的是"按关键度排序后在预算内取 Top-K 加固"，而非一个绝对分值 S。学习排序（learning-to-rank）天然绕开"权重尺度不可解释"的问题。
- 论文：https://www.researchgate.net/publication/415825060

**F20. LineVul / Transformer 代码脆弱性预测（方法迁移）** — **MSR 2021** 起的一系列工作
- 机制：基于预训练 Transformer 的**行级**脆弱性定位；2023–2026 年大量 LLM/BERT 系工作（如 CodeT5、VulBERTa、Vul-RAG）把代码表示学习用于脆弱性预测。
- 对本课题：注意**术语陷阱**——软件工程社区的 "vulnerability" 指安全漏洞，与本课题的"辐射软错误脆弱性"不同。可借其**表示学习与行级定位方法**，但引用时必须声明领域差异，否则是硬伤。方法清单见 https://github.com/iSEngLab/AwesomeLLM4SE

**F21. Ranger** — Zitao Chen, Guanpeng Li, Karthik Pattabiraman, **DSN 2021（最佳论文）**
- 机制：对 DNN 做自动变换，**选择性限制数值范围**（range restriction），把"关键故障"引起的巨大数值偏移压成 DNN 本身能容忍的良性故障；8 个 DNN 上弹性提升 3×–50×，精度无损，开销可忽略。
- 对本课题：如果框架覆盖星上 AI 推理负载，这是**非冗余、近零开销**的保护手段，可作为 LOW 模式下的"常开"底座（HIGH 模式再叠加冗余）。为状态机的两级设计提供了"低档位也不是完全不设防"的合理形态。
- 论文：https://zitaoc.github.io/files/Ranger-DSN21.pdf
- 代码：https://github.com/DependableSystemsLab/Ranger

**F22. TensorFI** — Zitao Chen, **ISSRE 2020**
- 机制：TensorFlow 层面的硬件/软件故障注入框架，支持单比特翻转、复杂模型（LeNet/AlexNet）。
- 对本课题：为星上 AI 负载生成脆弱性标签的现成工具。
- 代码：https://github.com/DependableSystemsLab/TensorFI

---

### F.4 剖析引导（PGO）与预算约束下的保护选择（"开销—可靠性"帕累托）

**F23. PROFiT / SWIFT / CRAFT** — George A. Reis, **ACM TACO 2(4):366-396, 2005**
- 机制：SWIFT 为纯软件冗余（输出破坏故障降低 9×，17.75%→1.95%，时间开销 +42.9%）；CRAFT 为软硬混合（降至 0.71%，25×，开销 +31.4%）；**PROFiT 是剖析引导的编译技术**——用 profile 判断哪些代码段最脆弱，据此**细粒度调节保护等级**，在不损失可靠性的前提下再省 21.2%（纯软件）/ 3.3%（混合）执行时间。
- 对本课题：**本课题的直系祖先，必引**。PROFiT 已经证明"按剖析信息分配保护等级"可行；本课题的增量必须明确说成：PROFiT 是**离线静态分档**，本文增加**在线按辐射环境切档**（即把 PROFiT 的空间维自适应扩展到时间维）。同时 PROFiT 的"profile 决定保护级别"正是替换 (w1,w2,w3) 的最直接依据——权重应来自 profile 而非常数。
- 论文：https://liberty.princeton.edu/Publications/taco05_ft.pdf

**F24. 低预算容错（harnessing soft computation）** — Daya S. Khudia & Scott Mahlke, **MICRO 2014**
- 机制：利用程序中"软计算"部分的容错性，只复制值/指令并在特定点比对；用 **edge profiling** 减少复制数量，只复制冷指令时相对已有技术开销降低 41%。
- 对本课题：把"动态执行频次（profile）"作为保护取舍的核心变量——与学生 `LoopDepth`（频次的静态近似）形成对照：**既然能直接量频次，为何要用循环深度去近似它？** 这是审稿人会问的问题，必须在论文里正面回答或直接改用 profile 计数。
- 论文：MICRO-47, 2014

**F25. 编译器 IR 级选择性指令复制 + 真实硬件错误** — **FTXS @ SC 2019**
- 机制：评估 LLVM IR 级选择性指令复制在**真实（而非合成）硬件错误**下的有效性，指出 IR 级与汇编级注入的偏差。
- 对本课题：本课题若在 LLVM IR 上实现保护插桩，这篇是**必须回应的威胁效度（threats to validity）引文**。
- 论文：https://sc19.supercomputing.org/proceedings/workshops/workshop_files/ws_ftxs105s2-file1.pdf

**F26. 优化软件导向的指令复制（GPU）** — Abdulrahman Mahmoud, **SC 2018**
- 机制：在 GPU 上把"复制哪些指令"形式化为优化问题，在给定性能预算下最大化错误检测覆盖。
- 对本课题：**预算约束下的选择问题**的标准形式化，可直接套用为本课题的"在 X% 开销预算下选哪些 critical 块加固"。
- 论文：https://www.researchgate.net/publication/331762958

**F27. 轻量输入感知的选择性指令复制** — Md Hasanur Rahman, **2025（ACM 会议论文，DOI 10.1145/3712285.3759822）**
- 机制：考虑**输入依赖性**的轻量选择性复制部署方案，解决"换输入后保护集失效"问题。
- 对本课题：与 F6（VTrident）配套，是本课题"静态打分 + 运行时修正"设计的最新同行工作，必须对比。
- 论文：https://dl.acm.org/doi/pdf/10.1145/3712285.3759822

**F28. MLGO** — Mircea Trofin（Google），**arXiv 2021 / 已合入 LLVM 主干**
- 机制：用强化学习（Policy Gradient / Evolution Strategies）训练神经网络**替换 LLVM 中的人工启发式**，已落地 inlining-for-size（体积减 3%–7%）与 register-allocation-for-performance（QPS +1.5%）。
- 对本课题：**方法论层面的"替换手工权重"的权威先例**。论文可写："编译器社区已用学习模型系统性替换人工启发式（MLGO），本文将同一范式应用于可靠性关键度判定。"这是把路线一从"工程 trick"抬升为"有先例的研究范式"的关键引文。
- 代码：https://github.com/google/ml-compiler-opt
- 论文：https://arxiv.org/abs/2101.04808 ；博客 https://research.google/blog/mlgo-a-machine-learning-framework-for-compiler-optimization

**F29. OpenTuner** — Jason Ansel, **PACT 2014**
- 机制：通用程序自动调优框架，多种搜索技术（AUC-Bandit 元技术组合）在高维配置空间搜索。
- 对本课题：如果坚持保留 S 的线性形式，那就**不要手调 (w1,w2,w3)，用 OpenTuner/贝叶斯优化/遗传算法在"SDC 率–开销"二维目标上搜 Pareto 前沿**。这是改动最小、最快能写进论文的方案（见路线二）。
- 代码：https://github.com/jansel/opentuner
- 论文：https://ieeexplore.ieee.org/document/7855909 / http://opentuner.org

---

### F.5 运行时自适应容错：档位切换的既有机制

**F30. DRM（Dynamic Reliability Management）/ RAMP** — Jayanth Srinivasan, **ISCA 2004**（+ IEEE Micro 2005 综述版）
- 机制：提出 RAMP 架构级寿命可靠性模型，并提出 **DRM**：处理器在运行时响应应用行为变化（主要通过 DVFS）以维持目标寿命可靠性 FIT/MTTF，而不是按最坏情况过度设计。明确讨论 DRM 与 DTM（动态热管理）**互不包含**。
- 对本课题：**"运行时自适应可靠性管理"这一概念的原始出处，必引**。本课题状态机的叙事应对齐 DRM：维持一个**目标可靠性预算**，而非简单的阈值触发。注意区别：DRM 针对硬失效/寿命（磨损），本课题针对**瞬时软错误**，所以控制量从 V/f 换成了保护等级——这个差异正是本课题的创新陈述点。
- 论文：http://rsim.cs.uiuc.edu/Pubs/srinivasan_isca04.pdf ；综述版 https://mail.rsimgroupdev.web.illinois.edu/Pubs/05ieee-micro.pdf

**F31. 运行时可动态重构的 FPGA 架构（TMR/DMR/HPC 三档）** — Juan Valverde, **UPM 博士论文（CEI-UPM）**
- 机制：在 FPGA 上实现 **HPC（无冗余）/ DMR / TMR 三档**运行时可重构处理块，量化各档吞吐代价：6 块基线下切到 DMR、TMR 分别慢 1.3×、1.7×；可混档（MIX），并离线刻画"能耗—时间—容错等级"解空间。
- 对本课题：**档位切换代价的定量来源**。本课题的 LOW/HIGH 两档需要给出切换成本，这篇提供了可引用的实测比例；其"离线刻画解空间 + 在线选点"的做法也正是本课题应采用的结构。
- 论文：https://cei.upm.es/pdfs/TESIS_Juan_Valverde_Imprimir.pdf

**F32. FORTALESA / 自适应 TMR 综述（含 DMR↔TMR 运行时切换）** — 近年综述（arXiv，TMR 冗余系统全面综述）
- 机制：综述选择性与自适应冗余：设计期只加固关键模块（Selective TMR、信号概率分析 Samudrala 2004）；运行期按故障指示动态选择策略（Gonzalez 1997 的 Spring 内核反射式调度器在 TMR / Primary-Backup / Primary-Exception 间按任务关键度与资源切换，过载下任务接受率提升 10–20%）；FORTALESA 提供 DMR↔TMR 的运行时重构。
- 对本课题：**最重要的发现是"按任务关键度 + 资源状态在多种容错策略间运行时选择"在 1997 年就有原型（Gonzalez, Spring kernel）**。本课题必须引用并明确差异化：本文的触发信号来自**硬件 ECC 事件统计**而非任务调度压力。另有 ATMR、X-Rel 等近似 TMR 变体，综述亦指出研究空白：MBU（多位翻转）威胁、缺乏动态重构的高层工具链、缺少自适应容错开源工具——**这几条可直接作为本课题的 motivation 与贡献定位**。
- 论文：https://arxiv.org/abs/2603.14411 （综述；引用前请核对正式版本号与年份）

**F33. 分层容错 DNN 加速器综述（运行时在 simplex/TMR 间切换）** — **PRISMA 系统综述，2026**
- 机制：综述跨层容错与**自适应运行时机制**：按层对故障的敏感度在 simplex（高性能）与 TMR（高可靠）间动态切换；多模式 CNN 加速器（FT / HP / DS 三模式）在可靠性、性能、能效间运行时权衡；细粒度方案如 D2NN（神经元级复制 + 敏感度分析）、Selective TMR、FSA（重算恢复）。明确指出**安全可保证的模式切换（safety-guaranteed transitions）仍是开放问题**。
- 对本课题：这条"模式切换的安全性保证是开放问题"正是本课题状态机需要变点检测的理由——**切换本身必须有统计保证，否则抖动（thrashing）会同时损失性能与可靠性**。
- 论文：https://www.mdpi.com/2673-6470/6/3/54

**F34. Da-Stream：流计算中的动态自适应容错** — **ICA3PP 2025**
- 机制：按算子类型、运行时状态大小、数据流速率、依赖关系**自适应评定容错需求等级**，用回归模型估计节点故障概率，结合资源占用、故障预测与历史评分在运行时调整容错策略；恢复时间降 24.3%，CPU/内存利用率升 18.7%/12.2%。
- 对本课题：**"分级容错需求 + 运行时调档"在分布式系统领域的近期同构工作**，可作为跨领域相关工作，并借其"用故障预测概率驱动调档"的做法（优于纯阈值）。
- 论文：https://clouds.cis.unimelb.edu.au/papers/FT-Steam-ICA3PP2025.pdf

---

### F.6 用硬件信号感知错误率：ECC 计数器作为状态机输入的可行性与陷阱

**F35. Memory Errors in Modern Systems: The Good, The Bad, and The Ugly** — Vilas Sridharan, **ASPLOS 2015**
- 机制：Cielo / Hopper 两台超算的大规模现场数据。关键结论：**SRAM 故障率有强海拔依赖**（大气中子），Cielo 的 SRAM 可纠正故障显著多于 Hopper；但"可纠正故障多 ⇒ 不可纠正错误也多"这一直觉**此前并未被现场数据定量验证**；外推至 exascale，SRAM 故障随容量线性增长，系统级不可纠正错误率可达 50–100×。
- 对本课题：**双刃剑式的核心引文**。一方面支撑"可纠正错误率可作为环境辐射强度的代理指标"（海拔/通量依赖性真实存在）；另一方面本文必须诚实承认 CE→UE 的映射并非平凡。建议在论文中把指标 A/B 定位为**环境强度估计器**，而不是"UE 预测器"——这样就不被这篇文献反驳。
- 论文：https://pages.cs.wisc.edu/~remzi/Classes/739/Fall2015/Papers/memoryerrors-asplos15.pdf

**F36. A Study of DRAM Failures in the Field（SC 2012）/ Feng Shui of Supercomputer Memory（SC 2013）** — Vilas Sridharan
- 机制：现场 DRAM 失效研究；SC'13 进一步揭示 DRAM 与 SRAM 故障的**位置效应**（positional effects）。
- 对本课题：故障在空间上**非均匀**，意味着滑动窗口计数需要**按位置/通道分桶**，否则不同 DIMM 的事件混在一个窗口里会掩盖局部突发。
- 论文：https://scholar.google.com/citations?user=RVX0mRcAAAAJ

**F37. Revisiting Memory Errors in Large-Scale Production Data Centers** — Justin Meza, **DSN 2015**（Facebook 全机群 14 个月、十亿级 device-days）
- 机制：现代 DRAM 失效趋势的系统性刻画与建模，提出新的失效模式与影响因素（容量、芯片密度、工作负载）。
- 对本课题：**"ECC 事件不是独立同分布 Poisson"的最强证据来源**。真实 CE 事件高度**聚簇**（同一行/列反复报错）。这直接威胁学生的 Poisson 假设，必须在论文里处理：要么按器件去重后再计数，要么改用允许过度分散的模型（见路线四 / F.8）。
- 论文：https://users.ece.cmu.edu/~omutlu/pub/memory-errors-at-facebook_dsn15.pdf

**F38. DRAM Errors in the Wild: A Large-Scale Field Study** — Bianca Schroeder, **SIGMETRICS 2009**
- 机制：Google 机群 DRAM 错误的首个大规模现场研究；发现错误率远高于此前估计，且**强时空相关性**、"曾出错的 DIMM 更易再出错"。
- 对本课题：经典引文，用于论证"历史 CE 计数具有预测价值"（支持指标 B）同时"事件相关而非独立"（反对朴素 Poisson）。
- 论文：https://dl.acm.org/doi/10.1145/2492101.1555372

**F39. Cost-Aware Prediction of Uncorrected DRAM Errors in the Field** — Isaac Boixaderas, **SC 2020**（开源）
- 机制：随机森林用 MareNostrum 3 两年生产日志（450 万 CE、333 UE）**从可纠正错误等特征预测不可纠正错误**，减少 57% 的损失计算时间（年省 2.1 万节点小时）。方法论上强调：**precision/recall 不足以评估，必须做成本—收益分析**，并警惕训练偏倚。
- 对本课题：**路线四与状态机评估指标的直接依据**。本课题也应放弃"检测准确率"，改用**成本—收益目标**：误升档的代价是性能/功耗开销，漏升档的代价是任务失败概率——这正是两档阈值应当由优化而非手调确定的理由。
- 代码：UEPREDICT，随论文开源（见论文 Artifact 节）
- 论文：http://www.petarradojkovic.com/publications/SC-20_Prediction_of_DRAM_errors_in_the_field.pdf

**F40. Reinforcement Learning-based Adaptive Mitigation of Uncorrected DRAM Errors** — Isaac Boixaderas, **arXiv 2024**
- 机制：在 F39 的预测之上，用**强化学习**决定何时采取缓解动作（如迁移/检查点），自适应权衡缓解开销与故障损失。
- 对本课题：**与本课题思想最接近的最新工作**，几乎是"用 ECC 信号驱动自适应容错决策"的直接同行。必须作为对比对象；本课题的差异在于目标平台（星载、辐射环境、计算冗余档位）与决策空间（代码级保护等级而非作业迁移）。
- 论文：https://arxiv.org/pdf/2407.16377

**F41. Predicting DRAM Reliability in the Field with Machine Learning** — Ioana Giurgiu, **Middleware 2017**
- 机制：用 CE 计数 + 传感器指标（处理器温度、DRAM 功耗）预测 UE。
- 对本课题：说明**多传感器融合优于单一计数**。星上可用的额外信号包括单粒子监测器读数、轨道位置（是否过 SAA）、太阳活动指数——这些应作为指标 A/B 之外的协变量输入（路线四）。
- 论文：https://dl.acm.org/doi/10.1145/3154448.3154451

**F42. Linux EDAC 子系统 / rasdaemon / MCA（Machine Check Architecture）** — 工程基础设施
- 机制：EDAC 内核模块（原 bluesmoke，2.6.16 并入主线）从内存控制器寄存器收集 CE/UE 并经内核 tracepoint 上报，把错误解码到具体 DIMM 标签；x86-64 上亦可经 **MCA/EMCA** 取 UEFI 格式扩展错误日志；用户态 `rasdaemon` 订阅 `ras:mc_event`、`ras:aer_event`、`mce:mce_record` 等 tracepoint，落盘 SQLite（`/var/lib/rasdaemon/ras-mc_event.db`），`ras-mc-ctl --error-count/--summary` 查询。单条记录含时间戳、错误类型、计数、DIMM 标签（如 `CPU_SrcID#0_MC#1_Chan#0_DIMM#0`）、物理地址、syndrome。
- 对本课题：**指标 A 与指标 B 的真实数据来源，必须写进实现章节**。到达间隔直接由 tracepoint 时间戳差分得到；滑动窗口计数由 SQLite 查询或内存环形缓冲维护；物理地址/DIMM 标签使"按位置分桶"（见 F36）可实现。工程注意：CE 上报本身有开销（见 Ferreira, CLUSTER 2021 "Understanding the effects of DRAM correctable error logging at scale"），高 CE 率下日志可能成为瓶颈——这对星载弱算力平台尤其关键。
- 文档：https://docs.kernel.org/admin-guide/RAS/main.html ；https://bluesmoke.sourceforge.net ；https://github.com/mchehab/rasdaemon
- RAS 综述幻灯：http://events17.linuxfoundation.org/sites/events/files/slides/RAS_presentation_LinuxCon_NA.pdf

---

### F.7 空间辐射环境建模：给"Poisson 速率 λ"一个物理来源

**F43. AE9/AP9/SPM（IRENE）** — 美国空军研究实验室 / Aerospace Corporation
- 机制：辐射带捕获电子（AE9）、质子（AP9）与等离子体（SPM）的**统计型**环境模型，支持百分位（95%）与 Monte Carlo 多场景输出，可量化环境本身的不确定性；以独立 Windows 应用 + 源码库形式发布，可被 SPENVIS、OMERE 等第三方集成。
- 对本课题：**λ 的物理上游**。本课题可用 AE9/AP9 按轨道根数算出沿轨通量时间序列 → 再经器件截面得到 SEU 率 → 得到随轨道位置变化的 λ(t)。这把"Poisson 速率会变"从假设变成**可计算的事实**，从而论证状态机的必要性。注意 AE9/AP9 **不含太阳质子**，需另加 ESP-PSYCHIC。
- 入口：https://www.vdl.afrl.af.mil/programs/ae9ap9/ ；最佳实践（含版本与配置建议，Aerospace TOR-2022-00016）：https://aerospace.org/sites/default/files/2023-08/Best%20Practices%20for%20Generating%20Space%20Environment%20Specifications%20TOR-2022-00016.pdf

**F44. CREME96 / CREME-MC** — Cosmic Ray Effects on Micro-Electronics
- 机制：由粒子通量谱 + 器件 LET 截面**计算 SEE/SEU 速率**的标准数值工具；亦提供 5 分钟级最坏情况太阳质子瞬变环境。
- 对本课题：**从"环境通量"到"每比特每秒上翻率"的转换环节**。论文中给 λ 的数值必须出自此类工具，而不能凭空取值。
- 入口：https://creme.isde.vanderbilt.edu/

**F45. SPENVIS** — ESA/比利时皇家天文台，Space Environment Information System
- 机制：Web 门户，集成辐射带模型（含 AE-9/AP-9）、轨道生成、TID/SEU 速率计算；输出粒子通量 vs 能量、通量 vs 轨道时间两类关键曲线——前者定上翻率基线，后者**定位上翻集中出现的轨道区段**。
- 对本课题：**指标 A/B 的"地面真值"生成器**。用 SPENVIS 的"通量 vs 轨道时间"曲线可以直接生成本课题仿真实验所需的**时变 λ(t) 输入**（过 SAA 时 λ 跳升），这正是变点检测算法的测试激励。
- 入口：https://www.spenvis.oma.be/

**F46. OMERE + FASTRAD（TRAD）**
- 机制：OMERE 为免费软件，从粒子通量到 dose-depth 曲线（TID、TNID）与 **SEE 速率**，支持 AE9/AP9；FASTRAD 为 3D CAD 辐射分析（射线追踪 + Monte Carlo），两者输入输出互通。
- 对本课题：工程侧替代/交叉验证工具；可用来做屏蔽结构敏感性分析。
- 入口：https://www.trad.fr/en/space/omere-software/

**F47. SIRE2 Toolkit** — James H. Adams Jr., **Space Weather 18, 2020（AGU）**
- 机制：整合 CREME96 的 SEE 模型与 AE9/AP9/SPM 辐射带模型的工具包。
- 对本课题：单一引文即可覆盖"环境模型 + 效应模型"的完整链路，适合在论文背景章节使用。
- 论文：https://agupubs.onlinelibrary.wiley.com/doi/full/10.1029/2019SW002364

**F48. NASA 任务辐射环境建模与分析** — **NASA/TM, NTRS 20220011775, 2022**
- 机制：工程流程文档，明确 **SPENVIS 产出数据集 → CREME96 等 SEE 速率预测工具**的标准链路，并强调只有 ~1 MeV–100 MeV 能段的粒子才显著致上翻、加速器地面测试是表征器件截面的必要前提。
- 对本课题：为论文"辐射环境—器件—软件"三层建模链给出**权威流程引文**。
- 文档：https://ntrs.nasa.gov/api/citations/20220011775/downloads/Mission_Radiation_Modeling_STI.pdf

**F49. 在轨 SRAM SEU 特性与空间环境相关性** — **《强激光与粒子束》（HPLPB）2025**
- 机制：实测在轨 SRAM SEU 数据与粒子通量、地磁参数、质子能谱联合分析。关键定量结论：**97.5% 的 SEU 事件集中在南大西洋异常区（SAA）内，峰值位于 L≈1.24–1.25**；SER 与 ≥10 MeV 质子通量呈显著幂律相关（R≈0.73）；由地面质子辐照截面算出的理论 SER 与观测同量级但**系统性偏低**；三次小型太阳质子事件期间未探测到 SEU；**地磁暴期间因 SAA 内质子通量耗空，SER 显著下降**。
- 对本课题：**价值最高的实测引文**。它给出：(a) λ 随轨道位置呈**双模**分布（SAA 内/外），强力支持两档状态机；(b) SAA 过境持续约数分钟到十几分钟，为滑动窗口长度**定出物理量级**；(c) 地磁暴反而降低 LEO SER——说明"太阳活动强 ⇒ 升档"这种朴素规则在 LEO 是错的，本课题应依赖**实测 ECC 事件**而非太阳活动指数做切换，这恰好为"基于 ECC 的在线检测"提供了合理性论证。
- 论文：https://www.hplpb.com.cn/en/article/doi/10.11884/HPLPB202537.250269

**F50. South Atlantic Anomaly and CubeSat Design Considerations** — J. A. Fennelly, **SPIE 9604, 2015**
- 机制：分析 SAA 对 LEO 立方星的 TID 与 SEU 率影响及设计对策。
- 对本课题：小卫星/立方星平台（本课题的典型目标平台）下 SAA 效应的引文。
- 论文：https://www.spiedigitallibrary.org/conference-proceedings-of-spie/9604/960406/

**F51. Landsat 7 ETM+ 在轨 SEU 趋势数据库** — USGS（长期运行数据）
- 机制：从 1958 年 Explorer 1 起几乎所有 LEO 卫星都观测到 SEU；Landsat IAS 趋势库显示 SEU **绝大多数发生于 SAA**，其次为高纬极光区（>60°）。
- 对本课题：公开可引用的长期在轨统计，支撑"SEU 事件在时间上呈**突发簇**而非均匀 Poisson"。
- 数据：https://www.usgs.gov/landsat-missions/single-event-upset

---

### F.8 控制论与统计检测：把两指标状态机做成有理论保证的检测器

**F52. CUSUM（累积和控制图）** — E. S. Page, **Biometrika 1954**
- 机制：以对数似然比 `log p(y_k|θ1)/log p(y_k|θ0)` 为随机游走步长：变点前漂移为负、变点后为正；游走越过阈值 h 即报警。在**最坏平均检测延迟**意义下对突变检测是最优的。CUSUM 可被理解为**反复运行的一串 SPRT**（报警前运行的 SPRT 次数 G 满足 G−1 服从几何分布）。
- 对本课题：**状态机升级的核心武器**（详见 §F.10）。
- 参考：https://arxiv.org/html/1711.03539v2（含 CUSUM–SPRT 关系与延迟分析的清晰推导）

**F53. SPRT（序贯概率比检验）** — Abraham Wald, **1945**
- 机制：在两个假设 H0: λ=λ0 与 H1: λ=λ1 之间序贯累积对数似然比，上/下界由目标误报率 α 与漏报率 β 解析给出：上界 `log((1−β)/α)`，下界 `log(β/(1−α))`。期望样本量在同等错误率下最小。
- 对本课题：**让阈值从"调参"变成"由 α、β 反解"**——这是回应"阈值缺乏依据"最省力且最有说服力的一步。
- 参考：Wald, *Sequential Analysis*, 1947

**F54. Page-Hinkley 检验**
- 机制：CUSUM 的单边变体，累积 `(y_k − μ0 − δ)` 并跟踪其与历史最小值之差，超过阈值 λ_PH 即报警；带遗忘因子（decay）版本适合非平稳流。
- 对本课题：**对"速率上升"单边敏感**，正好匹配本课题只关心"错误率变高要升档"的非对称需求；实现仅需两个标量状态，适合星载弱算力。
- 参考实现：`river` 库 `drift.PageHinkley`；https://riverml.xyz

**F55. FOCuS：函数剪枝的快速在线变点检测** — Gaetano Romano, **JMLR 24, 2023**
- 机制：对 CUSUM / MOSUM / mMOSUM / Page-CUSUM 四类在线统计量给出统一框架，并提出函数剪枝（functional pruning）算法 FOCuS，使"对所有可能变点位置取最大"的精确计算达到近 O(1) 均摊更新代价；支持未知变点前均值。
- 对本课题：**滑动窗口的理论替代品**。学生的"滑动窗口计数"其实就是 MOSUM，而 MOSUM 需要人工选窗长 w；FOCuS/Page-CUSUM **对所有窗长取最大**，从而消去窗长这个超参数——又一个手工参数被原理性地去掉。
- 论文：https://www.jmlr.org/papers/volume24/21-1230/21-1230.pdf
- 代码：https://github.com/gtromano/FOCuS

**F56. Poisson 过程的在线贝叶斯变点检测** — **Statistics and Computing, 2025**（网络 Poisson 过程 + 社群结构）
- 机制：对速率 λ 取 Gamma 共轭先验（λ ~ Gamma(α, β)），用 CAVI 做在线变分推断，变点后**重置数据流**；并给出"若无孤立节点则期望等待时间 w ≤ 2/(λ_11 N)"这类速率界。显式利用了"速率 λ 的齐次 Poisson 过程在长度 t 区间上的计数服从 Poisson(λt)"。
- 对本课题：**与学生 Poisson 假设完全对齐的贝叶斯版本**。Gamma-Poisson 共轭给出 λ 的闭式后验，可直接输出"当前处于高辐射态的后验概率"，再用期望损失最小化（而非阈值）决定档位——比两个硬阈值优雅得多，且天然带**滞回**（后验概率需跨过不同的升/降档判决边界）。Gamma-Poisson 混合即负二项分布，**顺带解决 F37 揭示的事件聚簇/过度分散问题**。
- 论文：https://link.springer.com/article/10.1007/s11222-025-10606-w

**F57. 基于变点检测的分段平稳多臂老虎机（CUSUM-UCB / PHT-UCB）** — Fang Liu, **AAAI 2018 / arXiv 1711.03539**
- 机制：把 CUSUM 与 Page-Hinkley 嵌入 UCB 老虎机，在分段平稳环境中获得近最优遗憾界；给出 CUSUM 与 SPRT 的关系与平均运行长度（ARL）分析。
- 对本课题：如果本课题想做**自动学习档位策略**（而非固定两档规则），这是"变点检测 + 在线决策"的完整理论模板：变点检测负责识别环境切换，老虎机负责在新环境下重新探索最优保护等级。可写入未来工作。
- 论文：https://arxiv.org/html/1711.03539v2

**F58. ruptures：Python 变点检测库** — Charles Truong, **Signal Processing 2020**
- 机制：离线/在线变点检测算法集合（PELT、Binseg、BottomUp、Window），多种代价函数。
- 对本课题：**离线标定工具**。用它在历史 ECC 日志上离线找出真实变点，作为在线检测器的 ground truth 来评估延迟与误报率。
- 代码：https://github.com/deepcharles/ruptures
- 文档：https://centre-borelli.github.io/ruptures-docs/

---

## F.9 用学习/剖析替代 (w1, w2, w3) 的 4 条可落地路线

> 先摆清问题：`S = w1*Dist + w2*LoopDepth + w3*VarLiveness` 的三个毛病是
> (a) 权重无来源；(b) 三项量纲不同、未归一化，加权和没有物理意义；(c) 三级分类的两个阈值同样手工设定。
> 四条路线按"改动量从小到大"排列，建议**路线一必做、路线二做消融、路线三作为创新点、路线四写未来工作**。

### 路线一（推荐主线）：保留特征，换成可解释的树模型/学习排序
- **做法**：把 (Dist, LoopDepth, VarLiveness) 扩充为特征向量，补上 F16 给出的特征清单——位屏蔽特征 v_mask（AND 0 / OR 1 / 右移会吞掉错误位）、操作数位宽、指令类型、**动态执行次数（profile 计数，而非用 LoopDepth 近似）**、是否参与地址计算、到最近 store/branch 的距离。用 LLVM Pass 提取后，以 Trident（F5）算出的逐指令 SDC 概率为回归目标，训练**梯度提升树 / 回归树（CART）**；或直接训练 **learning-to-rank** 模型只学相对顺序。
- **为什么能解决可解释性**：树模型给出特征重要度（SHAP 值）与决策路径，可直接回答"为什么这条指令被判为 critical"——比三个常数权重的可解释性强得多。同时**三级分类的边界由树的分裂点自动学出**，顺带消去两个手工阈值。
- **依据引文**：F13（SDCAuto 用 CART 做同样的事，明确以"消除 SDCTune 的人工调参"为动机）、F12（证实 LoopDepth 有效但需人工调）、F17（PARIS 证明学习法比 Trident 解析法精度高 ≥63%）、F28（MLGO：编译器社区用学习模型系统替换人工启发式的权威先例）、F19（GVR：排序比打分更贴合"预算内取 Top-K"的真实需求）。
- **标签来源**：Trident（F5，零注入，首选）；少量注入校准用 gem5-Approxilyzer（F3）或 LLFI/TensorFI（F22）。F14 已论证**部分注入即足以训练**。
- **工作量**：1 个 LLVM 分析 Pass + 一个 scikit-learn 脚本。这是**最快能写进论文且最站得住**的改法。

### 路线二（必做的消融实验）：保留线性形式，但用多目标搜索解出权重
- **做法**：不改 S 的线性形式，把 (w1, w2, w3) 与两个分类阈值共 5 个参数交给自动调优器，目标函数是二维的 **(SDC 率, 性能/功耗开销)**，输出 **Pareto 前沿**而不是单点。工具：OpenTuner（F29）/ 贝叶斯优化（scikit-optimize、Optuna）/ NSGA-II 遗传算法。
- **为什么必须做**：这是审稿人最容易想到的 baseline。做了它，论文就能给出"手工权重点落在 Pareto 前沿内侧多少"的定量结论，直接把"权重缺乏依据"这个弱点转化为一张**有说服力的对比图**。
- **依据引文**：F29（OpenTuner 的 AUC-Bandit 元技术组合）、F26（SC'18：预算约束下选哪些指令复制的标准形式化）、F31（TMR/DMR/HPC 三档的"能耗—时间—容错等级"解空间刻画 + 实测档位代价 1.3×/1.7×，可作为开销模型）、F39（**评估必须用成本—收益分析，不能只看 precision/recall**——这决定目标函数怎么写）。
- **注意**：三项特征必须先做分位数归一化，否则线性加权在量纲上就不成立；这一点应在论文中明确交代。

### 路线三（创新点）：用 GNN 在程序图上学表示，彻底取消手工特征
- **做法**：把函数/基本块的 CFG + 数据依赖图 + 调用图喂给 GNN（GAT/GIN），节点特征只用最原始的指令操作码与类型，让网络自己学出"距离输出多远""在多深的循环里""活跃区间多长"这些结构信息。输出为节点级 SDC 概率或关键度排序。
- **为什么更强**：`Dist`、`LoopDepth`、`VarLiveness` 本质上都是**图上的手工聚合算子**（最短路径、嵌套层数、区间长度），而 GNN 的消息传递是这些算子的可学习泛化——这就是本课题最自然的理论叙事："本文将手工图特征替换为可学习的图表示"。
- **依据引文**：F18（函数级 GNN 预测 SDC 脆弱性与关键度）、F19（GVR 统一 GNN 框架做指令级预测与排序）、F10/F9（ACE/PVF 奠定"程序结构决定脆弱性"的理论基础，是 GNN 用程序图的合理性来源）、F20（Transformer/LineVul 系代码表示学习方法可借，但须声明"安全漏洞 ≠ 辐射软错误脆弱性"的领域差异）。
- **风险**：数据量需求大，星载 benchmark 稀缺。缓解办法是用 Trident（F5）在大量开源程序上批量生成伪标签做预训练，再在空天 workload 上微调。

### 路线四：剖析引导的在线权重自适应（打通两个设计点）
- **做法**：承认权重**不该是常数**。F9（PVF）的结论是"程序脆弱性变化主要源于执行画像"，F6（VTrident）实测整程序 SDC 概率跨输入波动 1.54×–42×。因此让权重/关键度随运行时 profile 更新：用轻量采样计数器维护各基本块执行频次，周期性重算关键度排序，并与状态机联动——**LOW 模式下只保护 Top-K₁，HIGH 模式下扩展到 Top-K₂**，K 由当前 λ 估计与开销预算共同决定。
- **为什么是本课题的真正卖点**：它把"静态打分"和"运行时状态机"从两个割裂的机制合成一个闭环——这是本文相对 PROFiT（F23，纯离线分档）的核心增量。
- **依据引文**：F23（PROFiT：剖析引导的保护等级调节，直系祖先，必引并明确差异）、F24（MICRO'14：用 edge profiling 直接量执行频次，开销降 41%——同时是对"用 LoopDepth 近似频次"的质疑，必须正面回应）、F6（跨输入波动的定量证据）、F40（RL 自适应缓解：最接近的最新同行工作）、F34（Da-Stream：用故障预测概率而非硬阈值驱动调档）、F41（多信号融合优于单一计数）。

---

## F.10 把 LOW/HIGH 状态机升级为有统计依据的变点检测

### 现状与问题
学生当前设计：
- **指标 A** = 相邻 ECC 事件的**到达间隔** `Δt_k = t_k − t_{k−1}`；若 Δt 过小则倾向升档。
- **指标 B** = 长度 W 的**滑动窗口内事件计数** `N_W`；若 N_W 超阈值则升档。
- 两指标各配一个硬阈值，构成 LOW↔HIGH 状态机。

三个统计学硬伤：
1. **阈值无来源**：无法回答"误报率是多少、检测延迟是多少"。
2. **窗长 W 无来源**：W 太短则噪声触发，太长则响应迟滞；当前靠试。
3. **抛掉了统计量**：到达间隔与计数是**同一个 Poisson 过程的两个充分统计量的片面使用**。在齐次 Poisson 过程下，`Δt ~ Exp(λ)` 且 `N_W ~ Poisson(λW)`，二者都是 λ 的函数——两个独立阈值并列判决是冗余且不自洽的（F56 明确给出 `N(t) ~ Poisson(λt)` 这一等价关系）。

### 正确的问题形式化
把档位切换重述为**Poisson 速率的在线变点检测**：
- H0: 速率 λ = λ0（LOW 环境，如 SAA 外；由 AE9/AP9 + CREME96 算出，见 F43/F44）
- H1: 速率 λ = λ1 > λ0（HIGH 环境，如 SAA 内；F49 实测 97.5% 的 SEU 集中于 SAA，峰值 L≈1.24–1.25）

**λ0 与 λ1 不是待调参数，而是可由辐射环境模型 + 器件截面算出的物理量**——这一句就把"阈值拍脑袋"的指责彻底消解。

### 映射一：指标 A（到达间隔）→ 指数分布的 SPRT / CUSUM
在 Poisson 过程下 `Δt ~ Exp(λ)`，单次观测的对数似然比有闭式：
```
LLR_k = log[ λ1·exp(−λ1·Δt_k) / (λ0·exp(−λ0·Δt_k)) ]
      = log(λ1/λ0) − (λ1 − λ0)·Δt_k
```
含义直观：**每来一个 ECC 事件就加一个正的常数 `log(λ1/λ0)`，而等待时间越长就扣掉越多 `(λ1−λ0)·Δt`**。这正是学生"间隔小则升档"的直觉的严格形式。

- **SPRT 版（F53）**：累积 `Λ_n = Σ LLR_k`；
  - `Λ_n ≥ log((1−β)/α)` → 判 H1，**升档至 HIGH**
  - `Λ_n ≤ log(β/(1−α))` → 判 H0，**降档至 LOW**
  - 之间则继续观察、不动作。
  **阈值由目标误报率 α 与漏报率 β 解析给出**，不再是调参。例如 α=10⁻³、β=10⁻² 时上界 ≈ 6.9、下界 ≈ −6.9。
- **CUSUM 版（F52，推荐用于持续监测）**：`g_k = max(0, g_{k−1} + LLR_k)`，`g_k > h` 报警后重置。CUSUM 在最坏平均检测延迟意义下最优，且可证明它等价于**反复运行的一串 SPRT**（报警前运行的 SPRT 次数减一服从几何分布，见 F52 参考推导）。h 可由目标平均运行长度（ARL₀，即无变点时的平均误报间隔）标定——ARL₀ 应取为**任务可接受的误升档频率**（如一轨一次）。
- **单边简化版（F54，推荐星载部署）**：Page-Hinkley 只需两个标量寄存器、对"速率上升"单边敏感，完全匹配本课题"只需对变坏敏感"的非对称需求，计算量可忽略。

### 映射二：指标 B（滑动窗口计数）→ MOSUM，且窗长可被消去
学生的 `N_W` 就是统计文献中的 **MOSUM（移动和）统计量**：`M_w(n) = |S(n−w, n)|/√w`（F55 式 (2)）。
- **问题的本质**：MOSUM 必须指定窗长 w，这就是"W 无来源"的根源。
- **解法**：改用 **Page-CUSUM**：`P(n) = max_{0≤w<n} |S(n−w, n)|/√w`（F55 式 (4)），即**对所有可能窗长取最大**，窗长超参数被原理性消除。朴素实现是 O(n)，但 **FOCuS 算法（F55）用函数剪枝把精确计算降到近 O(1) 均摊更新**，适合星载在线运行。
- 对计数型数据，相应的 LLR 为 Poisson 形式：窗口内观测到 n 个事件、窗长 t 时
  `LLR = n·log(λ1/λ0) − (λ1 − λ0)·t`
  ——**与映射一的到达间隔 LLR 在形式上完全一致**。这正好证明两个指标本应统一到同一个统计量，而不是两条并列规则。**这个统一性是论文中值得明确写出来的一个小结论。**

### 映射三：滞回（hysteresis）与抖动抑制的正确做法
学生若用同一阈值双向切换，必然在阈值附近抖动。两种有依据的做法：
1. **SPRT 的双边界天然就是滞回**：升档用上界、降档用下界，中间是"不动作带"，带宽由 α、β 决定——**滞回宽度因此有了统计含义**，而非人为设定。
2. **贝叶斯后验 + 期望损失**（F56）：对 λ 取 Gamma 共轭先验，在线维护后验 `λ | data ~ Gamma(α0 + N, β0 + T)`，输出"当前处于 HIGH 环境的后验概率 p"；档位决策由**期望损失最小化**给出：
   `升档当且仅当 p·C_miss > (1−p)·C_overhead`
   其中 `C_overhead` 是误升档的性能/功耗代价（可由 F31 实测的 1.3×/1.7× 档位代价量化），`C_miss` 是漏升档导致任务失败的代价。F39（SC'20）明确论证了**可靠性机制必须用成本—收益而非 precision/recall 来评估**，这条就是它在本课题的落地形式。

### 必须处理的建模风险：事件不是纯 Poisson
- **证据**：F37（Facebook DSN'15）与 F38（Google SIGMETRICS'09）都表明 ECC 事件**高度聚簇、时空相关**，"出过错的 DIMM 更易再出错"；F36（SC'13）揭示位置效应；F51（Landsat 长期在轨数据）显示 SEU 呈突发簇。纯 Poisson 假设会导致**系统性高估事件率、频繁误升档**。
- **三条缓解措施**：
  1. **按物理位置分桶**：利用 EDAC 的 DIMM 标签与物理地址（F42）把事件按 DIMM/通道/行分组，对重复命中的同一行做去重后再计数——把"器件级退化"与"环境级辐射"两类信号分离。
  2. **改用 Gamma-Poisson（负二项）模型**：F56 的 Gamma 先验天然产生负二项边际分布，允许**过度分散（over-dispersion）**，LLR 形式仍有闭式。这是对"非 Poisson"最经济的修补。
  3. **融合环境协变量**：F41（Middleware'17）证明 CE 计数 + 温度 + 功耗优于单一计数。星上可加入的协变量：轨道位置（是否过 SAA）、单粒子监测器读数、地磁指数。**注意 F49 的反直觉实测结论——地磁暴期间 LEO 的 SER 反而下降（SAA 内质子耗空），所以"太阳活动强 ⇒ 升档"是错的**；这恰好论证了本课题应以**实测 ECC 事件**为主信号，环境模型只作先验。

### 可验证的实验设计（配合 §F.7 工具链）
1. 用 SPENVIS（F45）按目标轨道生成"通量 vs 轨道时间"曲线，经 CREME96（F44）转为 **λ(t) 时间序列**（过 SAA 时跳升，F49 给出 97.5% 集中度与 L 壳位置）。
2. 按 λ(t) 采样生成 ECC 事件流（含 F37 风格的聚簇噪声注入作为鲁棒性测试）。
3. 对比四种检测器：**固定双阈值（学生原方案） / Page-Hinkley / CUSUM-SPRT / FOCuS-PageCUSUM**，指标为：平均检测延迟、ARL₀（误报间隔）、档位抖动次数、以及 F39 风格的**成本—收益净收益**。
4. 用 ruptures（F58）在真实/合成日志上离线标定 ground-truth 变点，作为延迟与误报的评判基准。

---

### 附：本节引文的论文章节建议归位
- **绪论/研究背景**：F43–F51（辐射环境与在轨实测，建立 λ 时变的物理必然性）、F30（DRM 概念源头）
- **相关工作（静态关键度）**：F1–F11（注入剪枝与解析建模）、F12–F22（学习法）
- **相关工作（运行时自适应）**：F23–F29（剖析引导与预算优化）、F30–F34（档位切换）、F35–F42（硬件信号）
- **方法章（替换权重）**：F5、F13、F17、F19、F23、F28、F29
- **方法章（状态机）**：F52–F58，λ0/λ1 取值依据 F43/F44/F49
- **实验与威胁效度**：F6、F25、F37、F38、F39、F49
