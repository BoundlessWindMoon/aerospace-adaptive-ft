## B. 近似计算编译器、精度自动调优与误差界标定（服务方向 B：δ/权重自动标定器）

说明：下面每条都给出 一作 / 会议+年份 / 机制 / 对本项目（尤其是 δ 与 w1,w2,w3 自动标定器）的相关性 / 代码 / 论文链接。凡检索中未能直接确认的字段，均标注「待核实」，不做臆测。标 ★ 的是与"δ 自动标定"直接可复用的方法学来源。

---

### 一、近似计算语言与编译框架（类型系统 / IR 层面的"近似可控性"）

**1. EnerJ: Approximate Data Types for Safe and General Low-Power Computation**
- 一作/venue：Adrian Sampson，PLDI 2011
- 机制：用类型限定符 `@Approx` / `@Precise` 把近似数据与精确数据在类型系统层面隔离，编译器静态保证近似值不会污染控制流与内存安全关键路径。
- 相关性：本项目的"指令关键性评分"本质上是 EnerJ 类型隔离的量化版。EnerJ 给出的"哪些值绝不可近似"（控制流条件、指针、循环边界）可直接作为 criticality 评分的硬约束项（w 不参与、强制精确副本），避免标定器把这类指令调到近似。
- 代码：https://sampa.cs.washington.edu/research/approximation/enerj.html ；https://github.com/uwsampa/enerj
- 论文：https://doi.org/10.1145/1993498.1993518

**2. ACCEPT: A Programmer-Guided Compiler Framework for Practical Approximate Computing**
- 一作/venue：Adrian Sampson，UW 技术报告 UW-CSE-15-01-01（2015，LLVM 实现；相关 workshop 版本见 WAX/GetMobile）
- 机制：LLVM pass 集合 + Clang 前端近似类型 + "precise-purity"分析（找出只影响可近似值的代码区域）+ 自动调优器，用训练输入在"松弛配置空间"上做启发式搜索，输出候选二进制的 质量—性能 Pareto 点。
- 相关性：★ 最接近本项目形态的开源基线（同为 LLVM pass + 运行时 + 自动调优）。其 "static feedback loop + dynamic testing feedback loop" 双环结构可直接作为 δ/w 离线标定器的工程骨架；relaxation site 枚举 + 贪心/启发式搜索是标定器的 baseline 算法模板。
- 代码：https://github.com/uwsampa/accept
- 论文：https://dada.cs.washington.edu/research/tr/2015/01/UW-CSE-15-01-01.pdf

**3. Verifying Quantitative Reliability for Programs that Execute on Unreliable Hardware（Rely）**
- 一作/venue：Michael Carbin，OOPSLA 2013
- 机制：命令式语言 Rely，允许程序员写定量可靠性规约（如 `0.99 * R(x)`：输出至少以 0.99 概率与精确执行一致），编译器用概率前置条件演算静态验证在不可靠硬件/近似算术下规约成立。
- 相关性：★ 为 "approximate TMR 容错保证" 提供可引用的形式化语义：δ 阈值投票实际上就是一个 "以概率 p 产生可接受结果" 的规约。可用 Rely 的 reliability 演算把 "δ + 3 副本" 的掩蔽概率表达成可验证性质，而不是只做实验。
- 代码/项目页：https://mcarbin.github.io/rely/
- 论文：https://doi.org/10.1145/2509136.2509546

**4. Chisel: Reliability- and Accuracy-Aware Optimization of Approximate Computational Kernels**
- 一作/venue：Sasa Misailovic，OOPSLA 2014
- 机制：给定可靠性 + 精度联合规约，Chisel 自动选择 Rely 中哪些算子用近似硬件执行，把"最小化能耗 s.t. 可靠性/精度约束"建模为整数线性规划（ILP），用求解器求解。
- 相关性：★★ 把"手调"变成"自动求解"的最直接范式迁移对象：把 (w1,w2,w3, δ) 标定写成 ILP / 混合整数规划——变量 = 每条指令选哪种近似版本（FP32 / 多项式 / int16 / 打包），目标 = 最小化开销，约束 = 端到端误差 ≤ 容忍上限 且 δ 投票不产生漏检。Chisel 的 ILP 公式与附录给出可直接照搬的约束形式。
- 代码/附录：https://mcarbin.github.io/rely/chisel
- 论文：https://doi.org/10.1145/2660193.2660231 ；PDF https://people.csail.mit.edu/rinard/paper/oopsla14.pdf

**5. Probability Type Inference for Flexible Approximate Programming（DECAF）**
- 一作/venue：Brett Boston，OOPSLA 2015
- 机制：概率类型 `@Approx(p)`，用类型推断自动把"端到端正确概率要求"向下传播为各算子的可靠性等级，只在必要处插 `check`/`endorse`；还会按不同可靠性上下文克隆函数（如多版本 `sqrt`）。
- 相关性：★ 两点直击本项目痛点：(a) 概率类型推断 = 自动推导每条指令允许的误差/失效概率，即 w 的自动化替代；(b) 按可靠性等级做函数克隆正是"多版本代码生成"，可引用其克隆数量控制策略（避免版本爆炸）。
- 代码：https://sampa.cs.washington.edu/new/research/approximation/decaf （基于 EnerJ 基础设施）
- 论文：https://homes.cs.washington.edu/~luisceze/publications/decaf-oopsla2015.pdf ；https://doi.org/10.1145/2814270.2814297

**6. FlexJava: Language Support for Safe and Modular Approximate Programming**
- 一作/venue：Jongse Park，ESEC/FSE 2015
- 机制：极简注解（只标输出端"可近似"），用近似推断分析自动反向传播出所有可近似的中间值与操作，兼容粗粒度与细粒度近似、多种质量度量与恢复策略。
- 相关性：注解负担最小化 = 本项目希望的"用户只给端到端容忍度，工具自己算 δ"。其反向传播推断算法是"端到端预算 → 指令级预算"的轻量版实现思路。
- 代码：未见公开仓库（待核实）
- 论文：https://cseweb.ucsd.edu/~hadi/doc/paper/2015-fse-flexjava.pdf ；https://doi.org/10.1145/2786805.2786807

**7. Uncertain\<T\>: A First-Order Type for Uncertain Data**
- 一作/venue：James Bornholt，ASPLOS 2014
- 机制：把不确定量包装为概率分布的一等类型，运算按贝叶斯网络语义组合，条件判断变为假设检验（"以 95% 置信度 x > y"），运行时按需采样。
- 相关性：★ 本项目的 δ 比较 `|exact - approx| < δ` 本质上是一次假设检验。Uncertain\<T\> 提供了"把阈值比较替换为带置信度的统计判定"的可引用抽象，可用来把固定 δ 升级为置信度参数化的自适应 δ（用户给置信水平，工具反解 δ）。
- 代码：https://github.com/klipto/Uncertainty （.NET 实现；镜像待核实）
- 论文：https://doi.org/10.1145/2541940.2541958

**8. Topaz: Approximate Task-Parallel Computations on Unreliable Hardware**
- 一作/venue：Sara Achour（Achour & Rinard），OOPSLA 2015
- 机制：任务粒度近似：任务在不可靠核上执行，运行时用 outlier detector（基于正常结果的统计模型）识别离群输出并在可靠核上重算该任务。
- 相关性：★★ 与"近似 TMR + δ 投票"几乎同构：Topaz 的 outlier detector 就是一个自动学习出来的 δ（从正常执行分布中统计学习判别边界），这是"自动标定 δ"最直接的先例之一——可引用其"离线学习结果分布 → 在线阈值判定 → 失败则精确重算"三段式。
- 代码：未见公开仓库（待核实）
- 论文：https://doi.org/10.1145/2814270.2814292

**9. ApproxHPVM: A Portable Compiler IR for Accuracy-Aware Optimizations**
- 一作/venue：Hashim Sharif，OOPSLA 2019（PACMPL 3(OOPSLA)）
- 机制：在 HPVM（LLVM 扩展的数据流图 IR）上加入硬件无关的精度度量，自动把端到端质量指标（如推理精度、PSNR）翻译为单个算子的精度需求，再由精度感知调度器把容错算子映射到近似硬件。
- 相关性：★★★ "端到端质量指标 → 逐算子精度预算"的自动翻译正是本项目需要的标定核心。δ 就是"算子级精度需求"的一个实例化；ApproxHPVM 的硬件无关调优阶段可原样对应到离线 profiling 阶段。
- 代码：HPVM 项目（含 ApproxHPVM/ApproxTuner）https://hpvm.readthedocs.io ，仓库 https://github.com/llvm-hpvm/hpvm （仓库地址待核实，以 hpvm.cs.illinois.edu 上的 release 链接为准）
- 论文：https://doi.org/10.1145/3360612 ；PDF http://rsim.cs.illinois.edu/Pubs/19-OOPSLA-AHPVM.pdf

**10. ApproxTuner: A Compiler and Runtime System for Adaptive Approximations**
- 一作/venue：Hashim Sharif，PPoPP 2021
- 机制：三阶段近似调优（development-time / install-time / run-time），把硬件无关与硬件相关近似解耦；核心是 predictive approximation-tuning：先一次性采集单个近似的误差剖面（error profile），再用误差组合模型解析预测端到端精度，从而避免昂贵的经验式全空间搜索（调优加速 12.8–20.4×）。
- 相关性：★★★ 本项目 δ/w 标定器的首选方法学模板。"一次性单点误差剖面 + 误差组合模型 + 预测式调优"可以几乎逐字映射到：对 FP32 降级、多项式近似、int16 压缩分别做一次误差 profiling，再组合预测端到端偏差，反解每个 voting 点的 δ。其运行时自适应阶段还对应本项目的"自适应"容错。
- 代码：同 HPVM 仓库（ApproxTuner 为 HPVM 组件）
- 论文：https://doi.org/10.1145/3437801.3446108 ；PDF https://www.hashimsharif.com/ApproxTuner.pdf

**11. ApproxMLIR: An Accuracy-Aware Compiler for Approximate Computing（新）**
- 一作：检索结果未给出一作（待核实）；venue：MLSys 2026
- 机制：把近似原语显式暴露在 MLIR 方言中，统一 Polygeist/JAX 等碎片化工具链，支持跨组件联合的离线/运行时近似调优；明确指出 ApproxHPVM/ApproxTuner 只在 LLVM IR/CUDA kernel 粒度统一，缺乏联合调优接口。
- 相关性：最新的"近似作为编译器一等公民"工作，可作为本项目 LLVM pass 设计的对照与未来工作（MLIR 迁移）论据。
- 代码：待核实
- 论文：https://proceedings.mlsys.org/paper_files/paper/2026/file/bbd3e0e9913824bbc46e7e87b11461ae-Paper-Conference.pdf

---

### 二、程序级近似变换与质量—性能自动折中

**12. Managing Performance vs. Accuracy Trade-offs with Loop Perforation**
- 一作/venue：Stelios Sidiroglou-Douskos，ESEC/FSE 2011
- 机制：编译器把循环改写为只执行部分迭代（改步长 / 概率跳过 / 改边界），用穷举 + 贪心搜索在训练输入上找"误差最小、加速最大"的穿孔配置组合。
- 相关性：★ 经典的"离线 profiling 驱动的近似旋钮标定"原型：它的 criticality 筛选（哪些循环穿孔后误差爆炸）就是 w 评分的前身，贪心组合搜索是标定器的 baseline 算法。
- 代码：未见官方开源（后续 image perforation 有实现）
- 论文：https://doi.org/10.1145/2025113.2025133 ；https://dspace.mit.edu/entities/publication/ac20ff09-ef90-40df-b06c-efb88f57728f

**13. Green: A Framework for Supporting Energy-Conscious Programming using Controlled Approximation**
- 一作/venue：Woongki Baek（Baek & Chilimbi），PLDI 2010
- 机制：程序员提供近似版本与 QoS 度量，Green 先做离线校准（calibration）阶段建立"近似等级 ↔ QoS 损失"模型，再在运行时按 QoS 下界动态选择近似等级并周期性重校准。
- 相关性：★★★ "calibration phase"这一术语与机制就是 δ 自动标定器的直接祖先，而且它同时覆盖 (a) 离线建模 和 (b) 运行时自适应重标定。建议作为核心引用。
- 代码：未见公开仓库（待核实）
- 论文：https://doi.org/10.1145/1806596.1806620

**14. SAGE: Self-Tuning Approximation for Graphics Engines**
- 一作/venue：Mehrdad Samadi，MICRO 2013
- 机制：CUDA 编译器自动生成多个近似 kernel 版本，运行时用低开销抽样检查（周期性跑精确版本对比）做在线质量监控，并据此在版本间切换以维持目标 TOQ。
- 相关性：★★ "多版本 + 周期抽样校验 + 在线调档"与本项目的多版本代码生成 + TMR 投票同构；其"抽样式精确参照"是 δ 在线自适应的低开销实现方案。
- 代码：未见公开仓库（待核实）
- 论文：MICRO 2013, pp. 13–24，http://cccp.eecs.umich.edu/papers/samadi-micro13.pdf

**15. Paraprox: Pattern-Based Approximation for Data Parallel Applications**
- 一作/venue：Mehrdad Samadi，ASPLOS 2014
- 机制：识别数据并行模式（map / scatter-gather / reduction / stencil / partition），对每种模式套用专门的近似（近似 memoization、规约抽样、stencil 子采样），无需程序员标注。
- 相关性：★ 对"SIMD/RVV 打包两个近似副本"有直接借鉴：Paraprox 的模式化近似就是在向量化友好的结构上做近似，其规约/stencil 近似模板可降低打包与重排开销。
- 代码：未见公开仓库（待核实）
- 论文：https://doi.org/10.1145/2541940.2541948

**16. AxBench: A Multiplatform Benchmark Suite for Approximate Computing**
- 一作/venue：Amir Yazdanbakhsh，IEEE Design & Test 34(2), 2017
- 机制：CPU/GPU/硬件三平台的近似计算基准集，每个 benchmark 预标注可近似区域、三档输入规模、应用特定质量度量。
- 相关性：实验章节几乎必须用它做评测对象与可近似区域 ground truth；预标注区域还可作为 criticality 评分器的标注监督信号（用来回归 w1,w2,w3）。
- 代码：http://axbench.org （含 NPU 编译器 NPiler）
- 论文：https://cseweb.ucsd.edu/~hadi/doc/paper/2017-ieee_dt-axbench.pdf

**17. Rumba: An Online Quality Management System for Approximate Computing**
- 一作/venue：Daya Khudia，ISCA 2015
- 机制：运行时用极轻量的误差预测器（线性/决策树等）预测每个输出元素的近似误差，只对预测为"大误差"的少数元素触发精确重算，从而在低开销下收紧质量尾部。
- 相关性：★★ 把"固定阈值 δ"升级为"学习出来的误差预测器 + 选择性重算"。对减少投票/重算开销与设计自适应 δ 都是高价值参考。
- 代码：未见公开仓库（待核实）
- 论文：ISCA 2015, pp. 554–566（DOI 待核实）

---

### 三、浮点精度自动调优（FP64→FP32 降级的直接对口文献）

**18. Precimonious: Tuning Assistant for Floating-Point Precision**
- 一作/venue：Cindy Rubio-González，SC 2013
- 机制：LLVM 上的动态分析 + delta-debugging 式搜索，在"输出精度约束 + 性能目标"下为每个变量/指令搜索最低可行精度，并输出类型实例化建议。
- 相关性：★★★ FP64→FP32 降级决策的标准工具与标准 baseline。其"精度约束 → 搜索 → 类型重写"流程就是标定器的最小可行版本；可直接拿来生成 w 的初始经验值。
- 代码：https://github.com/corvette-berkeley/precimonious ；新版 https://github.com/ucd-plse/precimonious
- 论文：https://doi.org/10.1145/2503210.2503296

**19. Floating-Point Precision Tuning Using Blame Analysis**
- 一作/venue：Cindy Rubio-González，ICSE 2016
- 机制：用 shadow value 执行做"归责分析"（blame analysis），先快速剔除不可能降精度的变量，大幅缩小 Precimonious 的搜索空间。
- 相关性：★ 搜索空间剪枝模板。本项目指令数量级大，必须先剪枝再搜 δ/w；blame analysis 的"谁为输出误差负责"评分可直接当作 criticality 权重的一个候选定义。
- 代码：https://github.com/corvette-berkeley/shadow-execution
- 论文：https://doi.org/10.1145/2884781.2884850

**20. Exploiting Community Structure for Floating-Point Precision Tuning（HiFPTuner）**
- 一作/venue：Hui Guo（Guo & Rubio-González），ISSTA 2018
- 机制：用依赖分析 + 边 profiling 把变量聚成"社区"，在社区层级做分层搜索（先粗粒度后细粒度），比 Precimonious 的平坦搜索更快且配置更优。
- 相关性：★★ 分层搜索 = 可以先按函数/基本块标定一组 (w, δ)，再细化到指令级，避免指数空间。标定器可扩展性的关键引用。
- 代码：https://github.com/ucd-plse/HiFPTuner
- 论文：https://doi.org/10.1145/3213846.3213862

**21. Rigorous Floating-Point Mixed-Precision Tuning（FPTuner）**
- 一作/venue：Wei-Fan Chiang，POPL 2017
- 机制：把符号 Taylor 形式给出的严格误差界与整数二次规划（QCQP/MIQP）结合，在"保证误差界 ≤ 目标"的前提下求最优混合精度分配，并控制类型转换次数。
- 相关性：★★★ 与 Chisel 互补：这是"带 sound 误差界的最优精度分配"。若要给出可证明不漏检的 δ，这是最合适的求解框架（见文末模板 3）。其对类型转换次数的惩罚项正对应向量打包/拆包开销。
- 代码：https://github.com/soarlab/FPTuner
- 论文：https://doi.org/10.1145/3009837.3009846

**22. ADAPT: Algorithmic Differentiation Applied to Floating-Point Precision Tuning**
- 一作/venue：Harshitha Menon，SC 2018
- 机制：用反向模式自动微分（CoDiPack / TAPENADE）得到输出对每个中间变量的梯度，结合一阶 Taylor 构造误差模型，估计"把该变量降精度"引入的输出误差，再用贪心算法在总误差预算下分配精度。
- 相关性：★★★ 对 w1,w2,w3 最有力的"客观化"武器：指令关键性 = |∂output/∂v| × 该指令的量化误差幅度。一次反向 AD 即可对所有指令给出敏感度，替代手调权重；贪心预算分配也直接给出 δ 的分摊方案。
- 代码：https://github.com/LLNL/adapt-fp
- 论文：https://doi.org/10.1109/SC.2018.00051 ；工具综述 https://w3.cs.jmu.edu/lam2mo/talks/software_tools_mixed_precision.pdf

**23. CRAFT + FloatSmith（Tool Integration for Source-Level Mixed Precision）**
- 一作/venue：Michael O. Lam，Correctness workshop @ SC 2019（CRAFT 本体 2014）
- 机制：FloatSmith 串联 CRAFT（搜索驱动）、TypeForge（ROSE 源到源类型改写）、ADAPT（搜索空间缩减），形成端到端自动混合精度流水线，附 Docker 复现环境。
- 相关性：★★ 现成的"敏感度分析 → 搜索 → 代码变换"工程范式，可直接对照本项目的 LLVM pass + 多版本生成；Docker 化复现对对比实验很友好。
- 代码：https://github.com/crafthpc/floatsmith ；https://github.com/crafthpc/craft ；TypeForge 在 ROSE 中（https://github.com/rose-compiler/rose）
- 论文：https://sc19.supercomputing.org/proceedings/workshops/workshop_files/ws_corr106s1-file1.pdf

**24. PROMISE / CADNA：Auto-tuning for Floating-Point Precision with Discrete Stochastic Arithmetic**
- 一作/venue：Stef Graillat 等，Journal of Computational Science 2019（CADNA 本体：CPC 2008）
- 机制：CADNA 用离散随机算术（DSA）以随机舍入估计"有效位数"，PROMISE 以此为预言 + delta-debugging 搜索出满足"用户要求的正确有效位数"的最低混合精度配置；新版已扩展到自定义低精度格式。
- 相关性：★★ 给了 δ 一个统计学定义："保证 k 位有效数字"而不是拍一个绝对阈值。对把 δ 做成相对误差 / 有效位数形式非常有用（航天应用数据动态范围大，绝对 δ 不鲁棒）。
- 代码/站点：https://cadna.lip6.fr ；https://promise.lip6.fr
- 论文：https://doi.org/10.1016/j.jocs.2019.07.004 （卷期待核实）；新版 https://arxiv.org/html/2606.08339v2

**25. Verificarlo: Checking Floating-Point Accuracy through Monte Carlo Arithmetic**
- 一作/venue：Pablo de Oliveira Castro，ARITH 2016（及后续 vprec 后端）
- 机制：LLVM pass 把所有浮点运算替换为可插拔算术后端（MCA 随机舍入、cancellation 检测、vprec 可变精度模拟），从而不改源码地做数值稳定性与混合精度探索。
- 相关性：★★★ 工程上最实用：可用 Verificarlo 的 vprec 后端直接模拟 FP32 降级 / int16 压缩的误差分布，无需真机，为 δ 标定批量采集误差剖面。
- 代码：https://github.com/verificarlo/verificarlo
- 论文：https://doi.org/10.1109/ARITH.2016.31

**26. Verrou: Assessing Floating-Point Accuracy Without Recompiling**
- 一作/venue：François Févotte，NSV 2017 / 技术报告 2016
- 机制：Valgrind 工具，二进制级随机舍入模拟 + delta-debugging 搜索定位导致验证失败的代码区域。
- 相关性：★ 无需重编译即可做敏感度定位，可作为 criticality 评分器的交叉验证手段（独立于 LLVM pass 的第二意见）。
- 代码：https://github.com/edf-hpc/verrou
- 论文：NSV 2017（索引见 https://fptalks.org/community.html）

**27. GPU-FPtuner: Mixed-Precision Auto-tuning for Floating-point Applications on GPU**
- 一作/venue：Ruidong Gu，HiPC 2020
- 机制：基于 ROSE 的 GPU 混合精度自动调优器，结合代码模式与类型转换开销建模做精度分配。
- 相关性：对"精度转换开销"的显式建模，正对应问题 (b)：向量打包/拆包开销应作为代价模型的一部分进入标定目标函数。
- 代码：https://github.com/raydongpub/GPU-FPtuner
- 论文：https://doi.org/10.1109/HiPC50609.2020.00043 （待核实）

**28. GPUMixer: Performance-Driven Floating-Point Tuning for GPU Scientific Applications**
- 一作/venue：Pradeep Kotipalli 等（多机构团队），ISC 2019（Best Paper）
- 机制：先用 LLVM 分析找性能上有收益的混合精度候选（避开转换开销陷阱），再验证其精度可接受。
- 相关性：★ "先选有性能收益的，再验精度"的倒序策略，对减少 SIMD 打包开销极有价值：只在打包确实能省周期的指令组上做近似副本打包。
- 代码：未见公开仓库（待核实）
- 论文：ISC High Performance 2019（DOI 待核实）

**29. FPLearner: Predicting Performance and Accuracy of Mixed-Precision Programs**
- 一作/venue：ucd-plse 团队（Rubio-González 组），ICSE 2024（venue 待核实）
- 机制：用图神经网络/深度模型作为代理模型预测某混合精度配置的性能与精度，嵌入 Precimonious / HiFPTuner 作为插件，大幅削减搜索时的实测次数（LULESH 从约 12h 降至约 3h）。
- 相关性：★★ 若 (w,δ) 搜索空间大，这是"用学习型代理模型替代真实运行"的可引用方案，直接解决标定器耗时问题。
- 代码：https://github.com/ucd-plse/FPLearner
- 论文：见仓库 README（DOI 待核实）

**30. SHVAL: Floating-Point Shadow Value Analysis（含 Medhat 的逐指令误差扩展）**
- 一作/venue：Michael O. Lam，ESPT @ SC 2016；扩展：Ramy Medhat，"Managing the Performance/Error Tradeoff of Floating-point Intensive Applications"，EMSOFT/ACM TECS 2017
- 机制：基于 Intel Pin 的重量级影子值分析，可模拟任意实数表示；Medhat 的扩展做逐地址/逐指令浮点误差分析，并据此为图像处理库生成多个混合精度版本。
- 相关性：★★ "逐指令误差贡献"正是 w 的经验度量；其在嵌入式域做性能/能耗/精度三维折中的做法与航天场景约束一致。
- 代码：https://github.com/crafthpc/shval ；https://github.com/ramymedhat/shval
- 论文：ESPT 2016；TECS 2017（DOI 待核实）

---

### 四、严格误差界分析（给 δ 一个可证明的下界）

**31. FPTaylor: Rigorous Estimation of Floating-Point Round-off Errors with Symbolic Taylor Expansions**
- 一作/venue：Alexey Solovyev，FM 2015；扩展版 ACM TOPLAS 2019
- 机制：把舍入误差视为"噪声"，构造符号 Taylor 展开（困难/超越函数作为符号系数保留），再用严格全局优化求误差上界；比 interval/affine/SMT 方法界更紧，并可输出 HOL Light 证明证书。
- 相关性：★★★ 算出"近似副本最大可能偏差 E_max"的首选工具——而 δ 必须 ≥ E_max 才不会把正常的近似误差误判为故障。这是 δ 双边约束的上支柱。
- 代码：https://github.com/soarlab/FPTaylor ；在线版 https://monadius.github.io/FPTaylorJS
- 论文：https://soarlab.org/papers/2015_fm_sjrg.pdf ；TOPLAS https://par.nsf.gov/servlets/purl/10105540

**32. Scalable yet Rigorous Floating-Point Error Analysis（Satire）**
- 一作/venue：Arnab Das，SC 2020（Best Student Paper）；Seesaw（Cluster 2021）；混合精度扩展 2025
- 机制：在符号 Taylor 形式上引入 path strength reduction、bound optimization 与信息论抽象，把可分析规模从几十个算子提升到 20 万+ 算子（四个数量级），可用"界的紧度"换"分析时间"。
- 相关性：★★★ FPTaylor 对真实 kernel 规模不够用，Satire 是"把 sound 误差界做到工程规模"的关键；其可调抽象档位让标定器能按预算选择分析精度。2025 版已支持混合精度与分支，正好覆盖多版本场景。注意：Satire 只取一阶 Taylor 项，界不保证严格 sound（见 PRECiSA 的讨论）。
- 代码：Utah SOAR Lab / Gopalakrishnan 组（见论文 artifact DOI；仓库地址待核实）
- 论文：https://dl.acm.org/doi/10.5555/3433701.3433768 ；arXiv https://arxiv.org/abs/2004.11960 ；2025 混合精度版 https://arxiv.org/html/2503.05924v1

**33. Daisy – Framework for Analysis and Optimization of Numerical Programs（Tool Paper）**
- 一作/venue：Eva Darulova，TACAS 2018
- 机制：模块化框架，集成前向数据流分析（interval / affine / SMT 定界，等同 Rosa/Fluctuat/Gappa 路线）与优化型分析（等同 FPTaylor 路线）、相对误差分析、区间细分、混合精度调优、重写优化、概率分析、初等函数近似、规约推断。
- 相关性：★★★ 一个工具里同时有误差界分析 + 混合精度分配 + 初等函数近似合成，与本项目三类近似技术一一对应，是最值得精读与复用的开源代码库。
- 代码：https://github.com/malyzajko/daisy （文档 https://github.com/malyzajko/daisy/blob/master/doc/documentation.md）
- 论文：TACAS 2018，https://doi.org/10.1007/978-3-319-89960-2_15

**34. Sound Approximation of Programs with Elementary Functions（Daisy 子工作）**
- 一作/venue：Eva Darulova（与 Anastasiia Izycheva），CAV 2019
- 机制：给定端到端误差规约，自动为程序中的 sin/cos/exp/log 等初等函数合成多项式近似，并在各函数间分配误差预算，保证总误差 sound。
- 相关性：★★★ 本项目"多项式近似 sin/cos/exp"部分的最佳对口文献，而且它解决的正是问题 (a)：误差预算如何在多个近似点之间自动分摊。必读。
- 代码：同 Daisy 仓库 https://github.com/malyzajko/daisy
- 论文：CAV 2019，https://doi.org/10.1007/978-3-030-25540-4_10 ；作者页 https://malyzajko.github.io

**35. Towards a Compiler for Reals（Rosa）**
- 一作/venue：Eva Darulova（Darulova & Kuncak），ACM TOPLAS 2017（前身 POPL 2014 "Sound Compilation of Reals"）
- 机制：源到源编译器，输入是带误差规约的实数值程序，用 SMT + 区间/affine 分析综合出满足规约的浮点或定点实现与数据类型。
- 相关性：★★ 定点/位宽综合直接对口 int32→int16 位宽压缩：Rosa 能在给定误差规约下自动选定点字长，可直接引用其位宽综合算法。
- 代码：https://github.com/malyzajko/rosa
- 论文：TOPLAS 2017，https://doi.org/10.1145/3014426

**36. Sound Mixed-Precision Optimization with Rewriting**
- 一作/venue：Eva Darulova（与 Einar Horn、Saksham Sharma），ICCPS 2018
- 机制：把 sound 混合精度分配与表达式重写（改善数值稳定性）联合优化，在 Scala/C 上实现，已并入 Daisy。
- 相关性：★★ 对应"先重写再降精度"的联合优化：重写可以扩大可用的 δ 余量，是降低近似副本误差的免费收益。
- 代码：同 Daisy 仓库
- 论文：ICCPS 2018（见 https://malyzajko.github.io）

**37. Certifying the Floating-Point Implementation of an Elementary Function Using Gappa**
- 一作/venue：Florent de Dinechin，IEEE Transactions on Computers 60(2), 2011
- 机制：Gappa 用区间算术 + 定理重写自动证明浮点代码（尤其是 libm 中多项式求值）的误差界，并生成 Coq 可检查证明。
- 相关性：★★ 多项式近似实现若要给出"可认证误差界"（航天场景说服力关键），Gappa 是标准做法。
- 代码/站点：https://gappa.gitlabpages.inria.fr/
- 论文：https://doi.org/10.1109/TC.2010.128

**38. Fluctuat（静态分析器，zonotope / affine 抽象域）**
- 一作/venue：Eric Goubault & Sylvie Putot（CEA LIST），系列工作（如 "A Logical Product Approach to Zonotope Intersection", CAV 2010）
- 机制：基于 affine/zonotope 抽象域的静态分析器，对嵌入式 C 代码给出舍入误差界，并把误差归因到各代码点（error decomposition）。
- 相关性：★★ 其误差来源分解功能天然就是 criticality 评分：每条语句对总误差的贡献量即 w 的候选定义。工业/航电领域常被引用，便于论证可信性。
- 代码：非开源（CEA 工具）
- 论文：CAV 2010，https://doi.org/10.1007/978-3-642-14295-6_22 （待核实）

**39. Certified Roundoff Error Bounds Using Semidefinite Programming（Real2Float）**
- 一作/venue：Victor Magron，ACM TOMS 2017
- 机制：用半定规划 + 平方和证书给出绝对舍入误差上界，结果可在 Coq 中检查。
- 相关性：另一条 sound 界路线，可作为 FPTaylor/Satire 的交叉验证。
- 代码：https://github.com/afd/real2float （仓库名待核实，见 fptalks 索引）
- 论文：https://doi.org/10.1145/3015465

**40. PRECiSA 4.0: Rigorous Floating-Point Round-Off Error Analysis with Certificates**
- 一作/venue：Laura Titolo（NASA LaRC），FM 2024
- 机制：支持条件分支、let-in、有界循环的 sound 误差分析，自动生成 PVS 形式化证书。
- 相关性：★★ 航空航天语境下最对口（NASA 形式化方法组），对"自适应容错框架的可信性论证"引用价值高；它也明确讨论了 Satire 式抽象导致的非 sound 风险。
- 代码：https://github.com/nasa/PRECiSA
- 论文：https://shemesh.larc.nasa.gov/fm/papers/FM2024-draft.pdf

**41. Detecting Floating-Point Errors via Atomic Conditions（Atomu）**
- 一作/venue：Daming Zou，POPL 2020
- 机制：用单个浮点运算的 atomic condition number 估计该运算引入/放大的误差，并用进化搜索找出使条件数最大的输入；插桩以 LLVM pass 实现。
- 相关性：★★★ 对 w1,w2,w3 最优雅的解析替代：条件数是"该指令对误差的放大倍数"的数学定义，可直接当作 criticality 权重的闭式公式，无需手调。其"最坏输入搜索"可用来标定 δ 的最坏情况分支。
- 代码：https://github.com/FP-Analysis/atomic-condition
- 论文：https://doi.org/10.1145/3371128

**42. Herbie: Automatically Improving Accuracy for Floating Point Expressions**
- 一作/venue：Pavel Panchekha，PLDI 2015
- 机制：随机采样输入 + MPFR 高精度参照评估表达式误差，定位误差来源，应用重写规则库与级数展开生成候选，再合并互补改写。
- 相关性：★★ 两用：(a) 在生成近似副本前先把表达式改写成数值更稳定的形式，可直接放大可用的 δ 余量；(b) 其"采样 + MPFR 参照 + 误差定位"正是离线 profiling 器的实现蓝本。
- 代码：https://github.com/herbie-fp/herbie ；站点 https://herbie.uwplse.org
- 论文：https://doi.org/10.1145/2737924.2737959 ；PDF https://herbie.uwplse.org/pldi15-paper.pdf

**43. Finding Root Causes of Floating Point Error（Herbgrind）**
- 一作/venue：Alex Sanchez-Stern，PLDI 2018
- 机制：Valgrind 动态分析，在真实大型程序中追踪影子高精度值，抽取出"误差根因表达式"（可直接喂给 Herbie）。
- 相关性：★ 把误差归因从小表达式扩展到整程序，适合在真实航天负载上定位高关键性指令。
- 代码：https://github.com/uwplse/herbgrind
- 论文：https://arxiv.org/abs/1705.10416 ；https://doi.org/10.1145/3192366.3192411

**44. FPBench: Toward Standard Floating Point Benchmarks**
- 一作/venue：Nasrine Damouche / Zachary Tatlock 等，NSV 2016（持续演进）
- 机制：FPCore 公共格式 + 基准集 + 编译器（FPCore→C 等），让 Herbie/Daisy/FPTaylor/Satire 等工具可互操作与可比较。
- 相关性：★★ 标定器若要横向对比，应以 FPCore 为输入格式，这样能直接复用 Daisy/FPTaylor/Herbie 的结果作为 ground truth。
- 代码：https://github.com/FPBench/FPBench ；站点 https://fpbench.org
- 论文：NSV 2016（见 https://fpbench.org/publications.html）

**45. Combining Tools for Optimization and Analysis of Floating-Point Computations（Herbie + Daisy）**
- 一作/venue：Heiko Becker 等，FM 2018
- 机制：把 Herbie 的（不保证 sound 的）精度优化与 Daisy 的（sound 的）误差验证串联，互相补全。
- 相关性：★★ 这正是标定器应采用的架构："启发式/profiling 给候选 δ，sound 分析器验证 δ 的安全性"。可引用其组合方法学。
- 代码：Daisy / Herbie 仓库
- 论文：FM 2018，https://doi.org/10.1007/978-3-319-95582-7_21

**46. S3FP: Efficient Search for Inputs Causing High Floating-point Errors**
- 一作/venue：Wei-Fan Chiang，PPoPP 2014
- 机制：引导式随机搜索，找出使浮点程序误差最大化的输入。
- 相关性：★★ δ 标定必须覆盖最坏情况输入，否则在轨遇到极端输入会误报/漏检。S3FP 是生成"最坏情况 profiling 输入集"的现成工具。
- 代码：https://github.com/soarlab/S3FP
- 论文：https://doi.org/10.1145/2555243.2555265

---

### 五、多项式 / 表近似与向量 libm（对口 sin/cos/exp 近似与 RVV 打包）

**47. Sollya: An Environment for the Development of Numerical Codes**
- 一作/venue：Sylvain Chevillard，ICMS 2010
- 机制：安全浮点开发环境，核心能力是生成带严格误差证书的近似多项式（`fpminimax`、`supnorm`、区间定界），是 CRlibm 等库的基础设施。
- 相关性：★★★ "多项式近似 sin/cos/exp"的生成工具应该就是 Sollya：它给出的 supnorm 误差界可直接作为该近似副本的 E_max，输入到 δ 计算公式。
- 代码/站点：https://www.sollya.org
- 论文：ICMS 2010，https://doi.org/10.1007/978-3-642-15582-6_5

**48. Metalibm: A Code Generator for Mathematical Functions**
- 一作/venue：Olga Kupriianova（Kupriianova & Lauter），ARITH 2014 / 后续 Metalibm-Lugdunum
- 机制：参数化的 libm 代码生成器：给定函数、目标精度与定义域，自动做参数归约、定义域分割、多项式次数选择并生成 C/向量代码。
- 相关性：★★★ 直接解决"每个 δ 预算对应生成一个多项式近似版本"的自动化问题——把 δ 当成生成器输入，就实现了近似强度与 δ 的联合标定；也支持向量化目标，兼顾 RVV 需求。
- 代码：https://github.com/metalibm/metalibm ；http://www.metalibm.org
- 论文：ARITH 2014 / https://hal.science/hal-01513490

**49. RLIBM 项目（An Approach to Generate Correctly Rounded Math Libraries for New FP Variants / RLIBM-32 / RLIBM-ALL / RLIBM-PROG）**
- 一作/venue：Jay P. Lim 与 Santosh Nagarakatte（Rutgers）；POPL 2021、PLDI 2021（Distinguished Paper）、POPL 2022（RLIBM-ALL）、PLDI 2022（Aanjaneya 等，RLIBM-PROG）
- 机制：核心洞见是"去逼近正确舍入的结果，而不是去逼近实数值"——于是每个输入都有一个舍入区间的自由度，可用线性规划求出次数更低的多项式；RLIBM-ALL 用单个多项式同时覆盖多种表示与所有舍入模式，RLIBM-PROG 提供 bfloat16/TF32 的渐进式近似。
- 相关性：★★★ 双重价值：(a) 近似副本用 FP32/低精度格式时，RLIBM 给出低次数、更快且误差可控的多项式，直接降低近似副本成本；(b) 其 LP 构造法把"误差预算（舍入区间）"当作约束求多项式系数，正是 δ-驱动的近似代码生成的教科书式范式。
- 代码：https://github.com/rutgers-apl/The-RLIBM-Project ；https://github.com/rutgers-apl/rlibm-all ；https://github.com/rutgers-apl/rlibm-prog
- 论文/项目页：https://people.cs.rutgers.edu/~sn349/rlibm ；POPL 2021 https://doi.org/10.1145/3434310

**50. The CORE-MATH Project**
- 一作/venue：Alexei Sibidanov / Paul Zimmermann 等（INRIA），ARITH 2022 起持续
- 机制：提供开源、正确舍入的 binary32/binary64 数学函数实现，binary32 单变量函数经穷举验证；已被 AMD LibM、LLVM-libc、MSVC 等采用。
- 相关性：★★ "精确副本"的 sin/cos/exp 应以 CORE-MATH 为参照基线（它是 ground truth），这样 δ 的测量不被 libm 自身误差污染。
- 代码/站点：https://core-math.gitlabpages.inria.fr
- 论文：ARITH 2022（见站点 publications）；glibc 精度对比 https://members.loria.fr/PZimmermann/papers/glibc240.pdf

**51. SLEEF: A Portable Vectorized Library of C Standard Mathematical Functions**
- 一作/venue：Naoki Shibata（Shibata & Petrogalli），IEEE TPDS 31(6), 2020
- 机制：全部 C99 实函数的手工向量化实现，刻意减少条件分支与 scatter/gather；每个函数提供 1-ULP 与"若干 ULP 更快"两档精度；支持 SSE2/AVX2/AVX512、AArch64 NEON、CUDA 与 RISC-V RVV1/RVV2。
- 相关性：★★★ 同时命中两个需求：(a) 多档精度版本 = 现成的"近似副本"候选，且 ULP 界已给出 → 可直接算 δ；(b) 原生支持 RVV，是 SIMD/RVV 打包近似副本的实现底座与性能对照。
- 代码：https://github.com/shibatch/sleef ；站点 https://sleef.org
- 论文：https://doi.org/10.1109/TPDS.2019.2960333

**52. An Open-Source RISC-V Vector Math Library / Vectorized Nonlinear Functions with the RISC-V Vector Extension**
- 一作/venue：ARITH 2024（开源 RVV 数学库，一作待核实）；Eric Bavier 等，ARITH 2023（RVV 向量化非线性函数）
- 机制：针对 RVV 的 libm：用 FMA 对（`vfmadd` + 残差 `vfmadd(B,C,vfsub(A,S))`）实现 double-double 精度的核心求值，针对 expm1/log/cos 等做专门的误差控制与参数归约，并采用向量长度无关编码。
- 相关性：★★★ 对问题 (b)（降低向量打包开销）最直接：论文给出的 RVV 指令级技巧（避免分支、用 FMA 对代替高精度中间量、VLA 编码）可直接用于"把两个近似副本打包进一条向量指令"的实现优化。
- 代码：ARITH 2024 论文声明开源（仓库地址待核实）
- 论文：https://www.ac.uma.es/arith2024/papers/An%20Open-Source%20RISC-V%20Vector%20Math%20Library.pdf ；ARITH 2023（Bavier 等）

**53. Stochastic Optimization of Floating-Point Programs with Tunable Precision（STOKE-FP）**
- 一作/venue：Eric Schkufza，PLDI 2014
- 机制：随机超优化（MCMC 搜索）在"精度可调"的代价函数下合成等价或近似等价的浮点内核，自动发现人难以想到的低精度实现。
- 相关性：★ 除了手写规则（FP32/多项式/int16），还可引用 STOKE 做近似副本的自动合成，是未来工作的有力论据。
- 代码：https://github.com/StanfordPL/stoke
- 论文：https://doi.org/10.1145/2594291.2594302

---

### 六、近似 × 容错的交叉（关键性评分与 δ 的可靠性侧约束）

**54. Approxilyzer: Towards a Systematic Framework for Instruction-Level Approximate Computing and its Application to Hardware Resiliency**
- 一作/venue：Radha Venkatagiri，MICRO 2016
- 机制：结合程序分析（故障等价类剪枝）与少量故障注入，给出几乎每条动态指令、每个寄存器位的单位翻转错误后果（SDC / 执行异常 / 输出质量劣化程度）的完整剖面。
- 相关性：★★★★ 对 criticality 评分最权威的开源对照与 ground truth 来源：Approxilyzer 直接产出"指令 × 位 → 输出质量影响"的经验分布。w1,w2,w3 完全可以通过对 Approxilyzer 剖面做回归/拟合来自动标定，而不是手调——这是本综述里最推荐的 w 标定路线。
- 代码：gem5-Approxilyzer（见条目 55）；原版开源见作者页 https://ma3mool.github.io/publication/approxilyzer2016.html
- 论文：https://doi.org/10.1109/MICRO.2016.7783740 ；PDF 索引 http://rsim.cs.illinois.edu/Pubs/

**55. gem5-Approxilyzer: An Open-Source Tool for Application-Level Soft Error Analysis**
- 一作/venue：Radha Venkatagiri，DSN 2019
- 机制：在开源 gem5 上重实现 Approxilyzer/Relyzer，支持多 ISA（可向 RISC-V 扩展），便于研究者改造扩展。
- 相关性：★★★★ 做 RVV/RISC-V 的空天场景，这是唯一现成的开源、多 ISA、指令级软错误剖面工具。用它跑出的 SDC 概率可同时标定 w（关键性）与 δ 的可靠性侧下界（δ 必须小于"最小可致 SDC 的偏差幅度"）。
- 代码：以 DSN 2019 论文 artifact 链接为准（仓库名待核实）
- 论文：http://rsim.cs.illinois.edu/Pubs/19-DSN-gem5Approxilyzer.pdf

**56. Relyzer: Exploiting Application-Level Fault Equivalence to Analyze Application Resiliency to Transient Faults**
- 一作/venue：Siva Kumar Sastry Hari，ASPLOS 2012
- 机制：用控制/存储等价启发式把海量故障注入点剪枝成极小代表子集，从而以可行代价得到全程序的瞬态故障脆弱性图谱。
- 相关性：★★ 故障注入剪枝方法论——标定器若用故障注入验证 δ，必须引用 Relyzer 的等价类剪枝，否则实验量不可承受。
- 代码：Relyzer+ 开源于 gem5（见 https://github.com/Approxilyzer ，待核实）
- 论文：https://doi.org/10.1145/2150976.2150990

**57. Minotaur: Adapting Software Testing Techniques for Hardware Errors**
- 一作/venue：Abdulrahman Mahmoud，ASPLOS 2019
- 机制：把软件测试技术（输入最小化、测试优先级排序、覆盖准则）迁移到硬件故障脆弱性分析，大幅提升错误分析的效率与输入泛化能力。
- 相关性：★★ 标定器面临"用哪些 profiling 输入？标定结果能否泛化到新输入？"这一核心外部有效性问题，Minotaur 是直接的方法学答案（输入选择与剖面泛化）。
- 代码：见作者页 https://ma3mool.github.io/publications
- 论文：https://doi.org/10.1145/3297858.3304050

**58. Quality Programmable Vector Processors for Approximate Computing**
- 一作/venue：Swagath Venkataramani，MICRO 2013
- 机制：把"质量"编码进 ISA：指令字段携带对结果精度的期望（quality fields），硬件保证满足该指令级质量规约，同时利用其自由度省能；首个实现是质量可编程向量处理器。
- 相关性：★★★★ 与本项目设计最同构的硬件侧工作：指令级质量规约 + 向量处理器。δ 就是 quality field 的运行时版本；可引用它论证"把 δ 下沉到指令元数据"的合理性，也为 RVV 近似打包提供 ISA 层面的先例。
- 代码：未见公开仓库（待核实）
- 论文：MICRO 2013, pp. 1–12，https://doi.org/10.1145/2540708.2540710 （待核实）

**59. Approximation-Based Fault Tolerance in Image Processing Applications**
- 一作/venue：Marcello Biasielli，IEEE Transactions on Emerging Topics in Computing 10(2), 2022（亦见其 DATE/ETS 系列）
- 机制：用一个低成本近似版本作为检测器与（部分）恢复手段：近似版本与原版本输出差异超阈值时触发重算，从而以远低于 TMR 的开销获得可接受的故障覆盖。
- 相关性：★★★★ 与"近似 TMR + δ"几乎同一命题，且是近年期刊级系统工作。必须引用并明确区分本项目贡献（本项目是 1 精确 + 2 近似三副本 + δ 容忍投票 + 自动标定，它是 1 近似检测器 + 重算）。其对阈值选择的讨论是 δ 标定的直接对手方法。
- 代码：未见公开仓库（待核实）
- 论文：https://www.computer.org/csdl/journal/ec/2022/02/09505312/1vLAzleyrvy

**60. Approximate TMR（ATMR）系列与 QAMR / Test and Reliability of Approximate Hardware**
- 代表：Brian Sierawski 等（2006，ATMR 首次提出）；QAMR（四互补近似副本保证完整掩蔽）；综述章节 "Test and Reliability of Approximate Hardware"
- 机制：ATMR 用近似副本替换 TMR 的冗余模块，面积/功耗从 +200% 降下来，但掩蔽能力下降（投票器输入本身就不相等，故障不一定被掩蔽）；QAMR 用四个"互补"近似副本，保证任意时刻至少三个输出精确，从而恢复 TMR 级别的掩蔽能力。
- 相关性：★★★★ 这是本项目必须正面回应的已知缺陷：ATMR 的近似误差与故障误差混淆导致掩蔽能力下降。而"δ 容忍投票"正是针对这一缺陷的软件层答案；QAMR 的"互补性条件"给了一个可引用的替代设计点与对比基线。
- 论文：综述章节 https://inria.hal.science/hal-03888016/file/BookChapter___Test_and_Reliability_of_Approximate_Hardware.pdf ；学位论文综述 https://lume.ufrgs.br/bitstream/handle/10183/193344/001092238.pdf

**61. A Fault Tolerant Voter for Approximate Triple Modular Redundancy**
- 一作/venue：A. S. M. Arifeen 等，Electronics 8(3):332, 2019（MDPI，开放获取）
- 机制：指出 ATMR 中投票器是可靠性瓶颈（因三路输入本身不等，故障不一定被掩蔽），提出容错投票器设计并给出 ATMR 可靠性解析模型 R_ATMR。
- 相关性：★★★ 给出 ATMR 可靠性的解析表达式，可直接用于 δ 的理论分析：把 δ 作为参数代入，可解析地画出 "δ ↔ 掩蔽概率 ↔ 误报率" 的权衡曲线，这是论文理论部分最需要的一张图。
- 论文：https://www.mdpi.com/2079-9292/8/3/332

**62. Flikker: Saving DRAM Refresh-Power through Critical Data Partitioning**
- 一作/venue：Song Liu，ASPLOS 2011
- 机制：程序员把数据分为"关键/非关键"，非关键数据放在低刷新率（易错）DRAM 区。
- 相关性：★ "关键性划分"的早期经典，是 criticality scoring 的概念起点；也提示把评分扩展到数据而非仅指令。
- 论文：https://doi.org/10.1145/1950365.1950391

**63. Significance-Driven Computing / Scalable Effort Hardware Design**
- 代表：V. K. Chippa，"Scalable Effort Hardware Design"（DAC 2010）、"Dynamic Effort Scaling"（DAC 2011）、"Computing Approximately, and Efficiently"（DATE 2015 综述）
- 机制：核心原则是"按计算对输出质量的显著性（significance）决定近似力度"，并把质量—效率旋钮显式暴露给软件，使硬件可在任意质量点运行。
- 相关性：★★★ "significance-driven" 就是 criticality scoring 的学术命名。DATE 2015 综述是引用"关键性驱动近似"这一思想源头的最佳单篇出处。
- 论文：https://past.date-conference.com/proceedings-archive/2015/pdf/1113.pdf

**64. Approximate Computing Survey, Part I: Terminology and Software & Hardware Approximation Techniques**
- 一作/venue：Vasileios Leon 等，arXiv 2023（ACM CSUR 投稿版本；另见 Mittal, ACM CSUR 48(4), 2016 经典综述）
- 机制：系统梳理近似编程语言、编译器变换、近似电路与架构的术语与技术分类，含 Rely/Chisel/EnerJ/ACCEPT 等的统一比较。
- 相关性：论文第二章综述的框架性引用来源，可用于定位本项目在分类体系中的位置（编译器层 + 软件冗余 + 可靠性约束）。
- 论文：https://arxiv.org/html/2307.11124v2 ；Mittal 综述 https://doi.org/10.1145/2893356

---

### 七、必读 Top 10

1. **ApproxTuner**（Sharif, PPoPP 2021）——"单点误差剖面 + 误差组合模型 + 预测式调优"，δ 标定器的主干方法学。
2. **ApproxHPVM**（Sharif, OOPSLA 2019）——端到端质量指标 → 逐算子精度预算的自动翻译，预算分摊的理论基础。
3. **ADAPT**（Menon, SC 2018）——反向模式 AD 给出所有指令的敏感度，把 w1,w2,w3 从手调变成可计算量。
4. **gem5-Approxilyzer / Approxilyzer**（Venkatagiri, DSN 2019 / MICRO 2016）——开源指令级软错误剖面，w 的监督标签与 δ 可靠性下界的来源。
5. **FPTaylor + Satire**（Solovyev, FM 2015 / TOPLAS 2019；Das, SC 2020）——近似副本偏差 E_max 的 sound 上界，δ 不漏判的数学保证。
6. **Chisel**（Misailovic, OOPSLA 2014）+ **FPTuner**（Chiang, POPL 2017）——把 (w, δ, 版本选择) 写成带 sound 误差约束的整数（二次）规划。
7. **Daisy 及 CAV 2019 初等函数近似**（Darulova）——一个开源框架内同时覆盖误差分析、混合精度、多项式近似与误差预算分摊，与本项目三类近似技术一一对应。
8. **Green**（Baek & Chilimbi, PLDI 2010）——"calibration phase + 运行时重校准"，δ 自动标定器的直接命名与机制祖先。
9. **Quality Programmable Vector Processors**（Venkataramani, MICRO 2013）+ **SLEEF**（Shibata, TPDS 2020）/ **RVV libm**（ARITH 2023/2024）——指令级质量规约 + RVV 多精度向量数学，覆盖打包开销优化。
10. **Approximation-Based Fault Tolerance**（Biasielli, TETC 2022）+ **ATMR 容错投票器**（Arifeen, Electronics 2019）——近似×容错交叉的最近对手方法与 ATMR 可靠性解析模型，必须对标的基线。

补充强烈建议：**Precimonious / HiFPTuner**（搜索与剪枝骨架）、**Atomu**（条件数 = w 的闭式定义）、**Verificarlo**（批量采集误差剖面的工程底座）、**S3FP**（最坏情况输入生成）。

---

### 八、如何用这些工作设计 δ 自动标定器

把 δ 和 (w1,w2,w3) 的手调问题拆成四个可独立引用、可独立验证的子问题：误差上界、故障下界、权重客观化、搜索与泛化。下面 5 个模板按可落地顺序排列。

**模板 1：δ 的双边夹逼（sound 上界 × 经验下界）—— 最核心，建议作为论文主定理**

δ 必须同时满足两个方向的约束，二者分别由两条独立文献线提供：

- 上界侧（避免误报）：δ ≥ E_approx_max，其中 E_approx_max 是近似副本相对精确副本的最大可能偏差。用 **FPTaylor**（FM 2015 / TOPLAS 2019）对小表达式、**Satire**（SC 2020，含 2025 混合精度版）对 kernel 规模，分别给出 sound 上界；多项式近似部分的 supnorm 界由 **Sollya** / **Gappa**（TC 2011）直接产出；int32→int16 位宽压缩的界由 **Rosa**（TOPLAS 2017）的定点综合给出；若需 NASA 级证书，用 **PRECiSA 4.0**（FM 2024）生成 PVS 证明。
- 下界侧（避免漏检）：δ ≤ Δ_fault_min，即"会造成不可接受输出质量的最小故障引起偏差"。用 **gem5-Approxilyzer**（DSN 2019）跑出指令×位的错误后果剖面，取"导致 SDC 的偏差幅度分布"的低分位数作为 Δ_fault_min；注入点剪枝引用 **Relyzer**（ASPLOS 2012）的故障等价类。
- 若 [E_approx_max, Δ_fault_min] 为空，则该指令不可用近似 TMR（退回精确 TMR 或换更弱的近似版本），这正好给出一条可证明的版本选择判据。可靠性的解析刻画引用 **ATMR 容错投票器**（Electronics 2019）的 R_ATMR 模型，画出 δ–掩蔽率–误报率曲线。

**模板 2：w1,w2,w3 的客观化替代（敏感度分析代替手调）**

不要再调权重，而是把三个权重分量替换为三个可计算的敏感度量：

- 一阶输出敏感度：用 **ADAPT**（SC 2018）的反向模式 AD 得到 |∂output/∂v|，一次反向传播即覆盖全部指令；
- 局部误差放大率：用 **Atomu**（POPL 2020）的 atomic condition number 作为该运算的误差放大闭式系数；另一条路线是 **Fluctuat** 的 error decomposition（逐语句误差贡献）或 **SHVAL**（ESPT 2016）的逐指令影子值误差；
- 经验 SDC 风险：用 **Approxilyzer**（MICRO 2016）的指令级错误剖面作为标签，对前两项做线性/回归拟合，从而用数据拟合出 w1,w2,w3 而非手设。

搜索空间剪枝引用 **Blame Analysis**（ICSE 2016），分层（函数 → 基本块 → 指令）标定引用 **HiFPTuner**（ISSTA 2018）；交叉验证敏感度用 **Verrou**（NSV 2017）作为独立第二意见。

**模板 3：端到端预算 → 逐点 δ 的自动分摊（预算分配问题）**

把"用户只给一个端到端质量容忍度 Q"作为唯一输入，逐投票点的 δ_i 由工具分摊：

- 翻译机制引用 **ApproxHPVM**（OOPSLA 2019）的"端到端指标 → 算子精度需求"硬件无关调优阶段；注解负担最小化可引用 **FlexJava**（FSE 2015）的反向近似推断。
- 分摊的 sound 版本引用 **Daisy / Sound Approximation of Programs with Elementary Functions**（CAV 2019），它恰好解决"多个初等函数近似之间如何分配误差预算"。
- 分摊的最优化版本写成整数（二次）规划：目标 = 最小化开销（含向量打包/拆包代价），约束 = Σ 误差贡献 ≤ Q 且每个 δ_i 落在模板 1 的可行区间。公式骨架引用 **Chisel**（OOPSLA 2014，可靠性 + 精度联合 ILP）与 **FPTuner**（POPL 2017，带 sound 界的 MIQP，且显式惩罚类型转换次数——正对应打包开销）。
- 若要把"每条指令允许的失效概率"也一并推导，引用 **DECAF**（OOPSLA 2015）的概率类型推断，并用其函数克隆策略控制多版本数量，避免版本爆炸。

**模板 4：离线 profiling 流水线（误差剖面采集 + 预测式调优 + 泛化性论证）**

- 流水线结构引用 **ApproxTuner**（PPoPP 2021）：development-time 采集每种近似（FP32 降级 / 多项式 / int16 / 打包）的一次性误差剖面 → 用误差组合模型解析预测端到端偏差 → install-time 做硬件相关调优 → runtime 自适应。该文报告这种预测式调优比经验式搜索快 12.8–20.4×。
- 剖面采集的实现底座用 **Verificarlo** 的 vprec 后端（无需真机即可模拟任意精度/位宽）；参照真值用 **CORE-MATH**（正确舍入，避免 libm 自身误差污染 δ 的测量）；小表达式误差采样方法引用 **Herbie**（PLDI 2015）的"随机采样 + MPFR 参照 + 误差定位"，整程序误差归因引用 **Herbgrind**（PLDI 2018）。
- 最坏情况覆盖：用 **S3FP**（PPoPP 2014）生成使误差最大化的输入，确保标定出的 δ 在极端在轨输入下仍成立。
- 输入选择与泛化性（"标定用的 profiling 输入能否代表在轨真实负载"）这一外部有效性问题，引用 **Minotaur**（ASPLOS 2019）的输入最小化/优先级排序方法，并用 **AxBench**（IEEE D&T 2017）的三档输入规模做跨输入鲁棒性实验；基准与工具互操作用 **FPBench** 的 FPCore 格式。
- 若搜索仍太慢，引入 **FPLearner**（ucd-plse）的学习型代理模型替代真实运行；验证环节采用 **Herbie + Daisy**（FM 2018）的组合架构："启发式给候选 δ，sound 分析器验证 δ 的安全性"。

**模板 5：运行时自适应 δ（离线标定 + 在线重校准闭环）**

框架名为"自适应"，就应闭环而非一次性标定：

- 闭环结构引用 **Green**（PLDI 2010）的"离线校准 + 运行时按 QoS 下界选档 + 周期性重校准"；
- 低开销在线校验引用 **SAGE**（MICRO 2013）的周期性抽样精确执行；
- 阈值的统计学习版引用 **Topaz**（OOPSLA 2015）的 outlier detector（从正常输出分布学出判别边界，等价于"学出来的 δ"）与 **Rumba**（ISCA 2015）的轻量误差预测器 + 选择性重算；
- 若要把固定 δ 升级为带置信度的判定（航天数据动态范围大，绝对阈值不鲁棒），用 **Uncertain\<T\>**（ASPLOS 2014）的假设检验抽象，或 **PROMISE/CADNA**（JoCS 2019）的"保证 k 位有效数字"相对化定义，让 δ 随数据量级自动缩放；
- 近似强度与 δ 的联合生成：用 **Metalibm** / **RLIBM** 把 δ 预算直接当作多项式生成器的输入约束（RLIBM 的 LP 构造法把误差预算写成线性约束求系数），实现"给定 δ 自动产出对应强度的近似副本"。

**关于问题 (b)（降低向量打包开销）可直接引用的方法学**

- **GPUMixer**（ISC 2019）的"先筛性能有收益的候选再验精度"倒序策略，避免在打包无收益处浪费近似预算；
- **FPTuner**（POPL 2017）与 **GPU-FPtuner**（HiPC 2020）把类型转换 / 精度转换开销显式写进目标函数；
- **SLEEF**（TPDS 2020）与 ARITH 2023/2024 的 RVV 数学库给出的无分支、FMA 对、向量长度无关实现技巧；
- **Paraprox**（ASPLOS 2014）的模式化近似（在向量化友好的结构上近似，减少 shuffle/重排）；
- ISA / 硬件侧的论证支撑用 **Quality Programmable Vector Processors**（MICRO 2013）的 quality field 设计。
