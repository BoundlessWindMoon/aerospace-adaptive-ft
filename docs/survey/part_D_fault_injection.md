## D. 故障注入方法学、可靠性评测基础设施与基准集

> 本篇面向课题"面向空天计算的自适应容错框架研究"（LLVM Pass + C runtime，近似 TMR + 阈值投票，x86_64 与 RISC-V rv64gcv）的评测侧文献与工具调研。
> 现有 harness 的基线为：LLVM IR 级 one-fault-per-run 位翻转注入（参考 LLFI/LLTFI）、`perf_event_open` PMU 计数、5 类结局分类（Masked / Detected&Recovered / Benign / SDC / DUE）与 MWTF 指标；基准集为 PolyBench/C、KissFFT、PARSEC Black-Scholes、ACCEPT suite。
> 每条给出：名称 / 第一作者 / 会议+年份 / 做什么 / 与本课题的关系（引用点、可借用的东西、对现有 harness 的方法学批评）/ 代码 URL / 论文 URL。
> 标注说明：`[必引]` = 论文方法学章节应直接引用；`[可用]` = 可直接复用代码或脚本；`[风险]` = 指出现有 harness 的效度缺口。
> 条目总数：**53**（D.1 编译器/IR 级 8 条、D.2 二进制/模拟器级 11 条、D.3 加速器/ML 栈 5 条、D.4 统计严谨性与效度 9 条、D.5 指标体系 4 条、D.6 现场数据与辐照 4 条、D.7 基准集 12 条），另含 D.8 方法学改进建议 7+1 条与 D.9 引用优先级速查表。

---

### D.1 编译器 / LLVM IR 级故障注入器

#### D1-1. LLFI `[必引][可用]`
- 第一作者 / venue：Qining Lu, Guanpeng Li, Karthik Pattabiraman 等（UBC DependableSystemsLab）；工具论文 QRS 2015（"LLFI: An Intermediate Code-Level Fault Injection Tool for Hardware Faults"），精度论文 Jiesheng Wei, DSN 2014。
- 做什么：在 LLVM IR 上做 instrument（`instrument` + profiling + injection 两阶段），对选定指令的目标寄存器做单比特翻转，支持硬件故障模型与软件故障模型（LLFI 独有），结局分类由用户脚本判定；带 YAML 配置、GUI 与 trace 回溯能力。
- 与本课题关系：这是现有 harness 的直系参考实现。论文中应明确说明"one-fault-per-run + IR 级目标寄存器翻转"这一模型继承自 LLFI；同时必须引用其配套的精度评估论文（见 D4-3），否则会被审稿人质疑 IR 级注入的效度。LLFI 的 `input.yaml` 中 `fi_cycle`/`fi_index` 选择逻辑和 profiling-then-inject 两阶段结构可直接作为本课题注入器的"注入点均匀采样"正确性论据。
- 代码：https://github.com/DependableSystemsLab/LLFI
- 论文：https://blogs.ubc.ca/karthik/files/2015/07/qrs-camera-ready.pdf

#### D1-2. LLTFI `[必引][可用]`
- 第一作者 / venue：Udit Kumar Agarwal, ISSRE 2022（Tools & Artifacts Track）；后续 ISSRE 2023 用于 LLM 弹性评估。
- 做什么：LLFI 的现代化继任者，向后兼容 LLFI，升级到较新 LLVM，并把注入面扩展到 TensorFlow/PyTorch 经 ONNX-MLIR 下降后的 IR；提供 docker、CI 与 `architecture.md`（pass pipeline、selector 类层次、hardware/software/ML fault modes、runtime library 接口）。
- 与本课题关系：如果现有 harness 还在老 LLVM 上维护自写 pass，LLTFI 的 selector 类层次是最值得照抄的工程结构（instruction selector / register selector / bit selector 解耦），便于后续扩展多比特与突发故障模型。LLTFI 明确声明"尚不支持软件故障模式"，这正好说明本课题只做硬件瞬态故障是合理的 scope 限定。
- 代码：https://github.com/DependableSystemsLab/LLTFI
- 论文：https://ieeexplore.ieee.org/document/9700378 （WiP 版）/ ISSRE'22 正式版见仓库 README 链接

#### D1-3. PINFI `[必引]`
- 第一作者 / venue：Jiesheng Wei, DSN 2014（与 LLFI 同文提出，作为 x86 汇编级对照组）。
- 做什么：基于 Intel Pin 的动态二进制注入器，在 x86 指令执行后篡改目的寄存器值，代表"低层注入"的 ground truth。
- 与本课题关系：这是验证 IR 级注入效度的标准对照手段。本课题在 x86_64 上完全可以复现"LLFI vs PINFI"式的交叉验证实验（哪怕只在 2–3 个 benchmark 上做 1000 次注入），用来回答"IR 级注入是否高估了 Masked 比例"。不做这一步是现有 harness 最大的效度风险 `[风险]`。
- 代码：https://github.com/DependableSystemsLab/pinfi
- 论文：https://blogs.ubc.ca/karthik/files/2014/04/DSN-Jiesheng.pdf

#### D1-4. KULFI `[可用]`
- 第一作者 / venue：Vishal C. Sharma / Ganesh Gopalakrishnan（Utah SoarLab），工具型（约 2013）。
- 做什么：Kontrollable Utah LLVM Fault Injector，LLVM 指令级单比特随机错误注入，强调注入点可控（按函数、按指令类型、按动态计数窗口过滤）。
- 与本课题关系：其"可控注入域"思想对本课题很关键——自适应 TMR 需要分别度量"被保护区域"与"未保护区域"的脆弱性，必须能把注入限制在特定函数/循环体内。KULFI 的过滤器设计可直接借用。
- 代码：https://github.com/soarlab/KULFI
- 论文：见仓库 README（Utah 技术报告 / DPDNS 系列）

#### D1-5. VULFI `[可用]`
- 第一作者 / venue：Vishal C. Sharma, DPDNS 2016（"Towards Resiliency Evaluation of Vector Programs"）。
- 做什么：LLVM IR 级注入框架，支持 C/C++/ISPC/OpenCL/MPI-C，特别针对**向量（SIMD）程序**的弹性评估。
- 与本课题关系：直接相关且常被忽视——本课题目标之一是 RISC-V **rv64gcv**（RVV 向量扩展）。向量寄存器的单比特翻转会同时污染多个 lane，其 SDC 行为与标量完全不同。VULFI 是为数不多显式处理向量语义的 IR 级注入器，应作为"向量注入模型"的引用来源，并据此在 harness 中补充"向量元素选择"维度（哪个 lane、几个 lane）`[风险]`。
- 代码：https://github.com/utahfmr/VULFI （项目页 http://utahfmr.github.io/VULFI）
- 论文：见项目页 "Cite our work" 链接

#### D1-6. FlipIt (UIUC / LLNL 系) `[可用]`
- 第一作者 / venue：Jon Calhoun, Euro-Par Workshops 2014。
- 做什么：LLVM pass 形式的 HPC 注入器。注入点在编译期枚举、运行期按用户给定的概率分布激活（而非固定一次），支持按字节/比特位置约束、按 MPI rank 约束；配套 FaultSight 做失效可视化分析。
- 与本课题关系：提供了"概率激活"这一与 one-fault-per-run 不同的注入范式，适合模拟空间环境中按 FIT 率推导的真实到达过程（SEU 率 × 暴露时间）。本课题若要给出"在 LEO/GEO 实际通量下 MWTF 的绝对值"，概率激活模型比 one-fault-per-run 更贴近，可作为二级实验。
- 代码：https://github.com/aperson40/FlipIt （作者亦有后续镜像仓库）
- 论文：https://lukeo.cs.illinois.edu/files/2014_CaOlSn_FlipIt.pdf

#### D1-7. REFINE `[必引][可用]`
- 第一作者 / venue：Giorgis Georgakoudis, SC 2017。
- 做什么：把注入点下移到 **LLVM 后端（MachineIR / 代码生成阶段）**，从而获得接近二进制级注入的精度，同时保留编译器级的可移植性、速度与高层语义关联能力。论文系统指出当前 IR 级 FI 实践的主要精度问题（注入到被优化掉的值、虚拟寄存器与物理寄存器不对应、未覆盖溢出/寻址计算等）。
- 与本课题关系：**对现有 harness 最直接、最具杀伤力的方法学批评来源** `[风险]`。审稿人几乎必然会问"IR 级注入的虚拟寄存器与真实物理寄存器不是一回事，你的 SDC 率是否失真"。两种应对：(a) 引用 REFINE 承认局限并做敏感性分析；(b) 直接把注入 pass 移到后端（REFINE 的实现路径对 x86_64 与 RISC-V 均可复用，这对双 ISA 目标尤其划算）。
- 代码：https://github.com/ggeorgakoudis/REFINE
- 论文：https://pureadmin.qub.ac.uk/ws/files/135663187/main.pdf

#### D1-8. Trident / VTrident `[必引]`
- 第一作者 / venue：Guanpeng Li, DSN 2018（"Modeling Soft-Error Propagation in Programs"）。
- 做什么：不做大规模注入，而是用三层概率模型（静态指令级 / 控制流级 / 内存级）**解析式预测** SDC 概率，精度接近 FI 而成本降低数个数量级；VTrident 进一步做跨输入预测。
- 与本课题关系：自适应容错框架需要一个**在线或编译期的廉价脆弱性估计器**来决定"哪段代码启用 TMR、阈值设多宽"。Trident 正是这种估计器的标准参考；可作为本课题"自适应决策依据"的对照基线（我的 PMU + 静态分析启发式 vs Trident 的解析模型）。
- 代码：https://github.com/DependableSystemsLab/Trident
- 论文：https://research.nvidia.com/sites/default/files/pubs/2018-06_Modeling-Soft-Error/18-DSN-Li-Trident_0.pdf

---

### D.2 二进制 / 虚拟机 / 微架构模拟器级注入

#### D2-1. Xception `[必引（历史）]`
- 第一作者 / venue：João Carreira, IEEE Transactions on Software Engineering 1998（早期版本 1995）。
- 做什么：利用处理器内建的调试与性能监控硬件（debug registers、异常机制）在**不修改被测程序**的前提下注入故障，并精确观测故障对指令流的影响。是"用 PMU/调试单元做可靠性观测"这条技术路线的源头。
- 与本课题关系：现有 harness 用 `perf_event_open` 读 PMU 计数，其学术谱系就是 Xception。方法学章节介绍"为何用硬件性能计数器做在线观测/触发"时应引用它作为起点，并说明本课题只用 PMU 做**观测与自适应决策输入**而非注入载体。
- 代码：无开源（商业化为 Critical Software 的 Xception 产品）
- 论文：https://ieeexplore.ieee.org/document/666828

#### D2-2. FERRARI `[必引（历史）]`
- 第一作者 / venue：Ghani A. Kanawati, IEEE Transactions on Computers 1995。
- 做什么：纯软件实现的注入器（Fault and ERRor Automatic Real-time Injector），用 UNIX ptrace/软陷阱在用户态程序的寄存器与内存中注入瞬态/永久故障，并统计检测延迟与覆盖率。
- 与本课题关系：确立了"软件实现故障注入（SWIFI）"的术语与基本实验流程（golden run + 故障运行 + 结局比对）。本课题 5 类结局分类的祖先就在这里；引用它可以把 Masked/Detected/SDC 的分类传统追溯到 1990 年代，增强分类法的正当性。
- 代码：无开源
- 论文：https://ieeexplore.ieee.org/document/364536

#### D2-3. GOOFI / GOOFI-2 `[必引]`
- 第一作者 / venue：Joakim Aidemark（GOOFI, DSN 2001）；Daniel Skarin（GOOFI-2, EDCC 2010），Chalmers。
- 做什么：Generic Object-Oriented Fault Injection 工具，统一驱动多种注入技术（预运行时 SWIFI、调试器接口 nexus/JTAG、以及模拟级注入），并自带实验管理与数据分析数据库。GOOFI-2 支持通过 Nexus 调试接口对真实嵌入式处理器注入。
- 与本课题关系：给出"实验管理层"的参考设计（campaign 定义、golden run 库、结局自动分类、结果数据库）。本课题 harness 若要跑数万次注入，应该照它的结构把"注入配置 / 运行 / 分类 / 统计"四层解耦，并保存每次注入的完整元组（注入点 PC、指令类型、寄存器号、比特位、时间戳、结局），否则后期无法做分层统计。
- 代码：无公开仓库（Chalmers 内部发布）
- 论文：https://ieeexplore.ieee.org/document/941390

#### D2-4. FIES（QEMU-based）`[可用]`
- 第一作者 / venue：Andrea Höller, MTV 2014；后续 DSN-W 2015、DSD 2015。
- 做什么：修改 QEMU TCG 的注入框架，支持 IEC 61508 规定的故障模型：寄存器单元与地址译码故障、CPU 指令与条件标志故障、DRAM 单元与地址译码故障；瞬态位翻转与永久 stuck-at 并存；故障以 XML 描述传入。主要支持 ARM。
- 与本课题关系：若要在 RISC-V 上做**全系统级**注入（含内存、地址译码、控制流寄存器）而不想上 gem5 的仿真成本，QEMU 路线是最现实的；FIES 的 XML 故障库格式和 TCG 注入 hook 位置可直接移植到 `qemu-system-riscv64`。它也提供了"注入不止于目的寄存器"的论据 `[风险]`：现有 harness 只注入 IR 值，完全不覆盖地址译码、标志位、PC。
- 代码：https://github.com/ahoeller/fies
- 论文：https://tugraz.elsevierpure.com/ws/portalfiles/portal/87145070/dsd_short_camera_ready.pdf

#### D2-5. ARCHIE `[可用]`
- 第一作者 / venue：Florian Hauschild, FDTC 2021。
- 做什么：QEMU 基础上的架构无关故障评估框架，支持指令跳过、寄存器/内存位翻转、按时间或触发条件激活，自动化大规模 campaign。
- 与本课题关系：相比 FIES 更新、更活跃，覆盖 **instruction skip** 这一对控制流容错极为重要的故障模型。本课题的阈值投票器若被 instruction-skip 型故障击中（跳过投票分支），行为与数据位翻转完全不同；补一组 instruction-skip 实验能显著增强"投票器自身可靠性"论证。
- 代码：https://github.com/Fraunhofer-AISEC/archie
- 论文：https://ieeexplore.ieee.org/document/9585805

#### D2-6. FAIL* `[必引][可用]`
- 第一作者 / venue：Horst Schirmeier, EDCC 2015（工具）；配套 DSN 2015 方法学论文（见 D4-4）。
- 做什么：FAult Injection Leveraged——开放、可复用的 SIHFT（软件实现硬件容错）评估框架，基于 Bochs/gem5 后端做**全故障空间**分析：先 trace golden run 得到每个内存/寄存器比特的活跃区间（def-use 区间），再把等价故障合并（fault-space pruning），只对代表点注入，从而得到无采样误差的精确结果；服务器 + 多 worker 的并行 campaign 架构。
- 与本课题关系：这是"如何不靠随机采样也能把结论说准"的最佳范例，也是本课题最值得采纳的一项升级。它的 def/use 区间剪枝直接回答了"死值上的位翻转被天然 masked"这件事 `[风险]`——如果 harness 在 IR 上均匀采样而不剔除死值与未读回的值，Masked 比例会被系统性高估。
- 代码：https://github.com/danceos/fail
- 论文：https://ess.cs.uni-osnabrueck.de/static/papers/Schirmeier-2015-EDCC.pdf

#### D2-7. GemFI `[必引]`
- 第一作者 / venue：Konstantinos Parasyris, DSN 2014。
- 做什么：基于 gem5 的全系统周期级注入工具，支持功能模式与周期精确模式、多 ISA（Alpha/x86）、checkpoint 快进、工作站集群并行 campaign。
- 与本课题关系：微架构级注入的经典基线。本课题引用它来界定 scope："我们做的是架构可见状态的注入，不覆盖 ROB/IQ/LSQ 等微架构结构，因此报告的是 PVF 类而非 AVF 类指标"。这句界定能挡掉大量"你的数字不是真实 AVF"的质疑。
- 代码：https://github.com/gem5-fault-injection（历史仓库，已较旧）
- 论文：https://nbellas.github.io/files/C33.DSN_2014.pdf

#### D2-8. GeFIN / MaFIN（differential FI）`[必引]`
- 第一作者 / venue：Athanasios Chatzidimitriou & Dimitris Gizopoulos, IISWC 2015（差分注入）；ISPASS 2016（"Anatomy of Microarchitecture-level Reliability Assessment: Throughput and Accuracy"）。
- 做什么：在 gem5（GeFIN）与 MARSS（MaFIN）上用相同原则实现的两套微架构注入器，对 caches、register file、buffers、queues 等**所有存储阵列**注入瞬态/间歇/永久故障，并系统比较两套工具、两种 ISA 的结论差异。
- 与本课题关系：提供"同一问题换一个工具就得到不同答案"的量化证据，是讨论**工具间效度（inter-tool validity）**的必引文献。本课题若只有一套 IR 级注入器，应在 limitations 中引用它说明单工具结论的不确定性，并说明 x86_64 与 RISC-V 两套结果间差异的可能来源（ISA 寄存器数、指令密度）而非简单归因于算法。
- 代码：GeFIN 部分代码见 https://github.com/athchatz（及 CLERECO 项目发布）
- 论文：https://www.clereco.eu/images/publications/IISWC.2015.28.pdf

#### D2-9. gem5-Approxilyzer `[必引][可用]`
- 第一作者 / venue：Radha Venkatagiri, DSN 2019。
- 做什么：把 Approxilyzer（原基于 Simics/SPARC）移植到开源 gem5 + x86，开放源码；对几乎每条动态指令的每个操作数比特给出单比特错误的结局与输出质量，靠 control-equivalence / store-equivalence / known-outcome 三类剪枝把注入数降低数个数量级。论文还报告了 Blackscholes 这类小 footprint 程序剪枝率最低（约 56.8%）这一细节。
- 与本课题关系：**与本课题高度重合**：同样用 x86 + Blackscholes，同样关心"输出质量"而非仅二值正确性，这正是近似 TMR 阈值投票所需的质量轴。强烈建议采纳其结局分类（masked / detected / SDC-Good / SDC-Maybe / SDC-Bad）来细化现有 5 类分类中的 Benign 与 SDC 边界，并引用其剪枝技术为"我只跑 N 次注入"提供正当性。
- 代码：https://github.com/rsimgroup/gem5-approxilyzer
- 论文：http://rsim.cs.illinois.edu/Pubs/19-DSN-gem5Approxilyzer.pdf

#### D2-10. InjectV（gem5 + RISC-V）`[可用]`
- 第一作者 / venue：smilies-polito（Politecnico di Torino）；近期论文/预印本，面向 RISC-V 故障注入攻击建模，在 FISSC（含 VerifyPIN 加固变体）上评估，报告相对传统 FI 约 95.8% 时间节省。
- 做什么：gem5 上的 RISC-V 故障注入框架，支持精确模式（寄存器名→gem5 RISC-V 寄存器映射、比特下标边界检查）与引导式注入（瞄准控制流判定、计数器、比较等安全关键点），含 CampaignManager。
- 与本课题关系：填补了"gem5 系注入器大多不支持 RISC-V"的空白（GemFI 只有 x86/ARM/Alpha）。本课题的 rv64gcv 目标若需要微架构级交叉验证，InjectV 是目前最省力的起点。其"引导式注入瞄准控制流判定点"的做法可直接用于攻击本课题的阈值投票器，做最坏情况分析。
- 代码：https://github.com/smilies-polito/injectv
- 论文：https://arxiv.org/abs/2606.12011 （较新预印本，引用前请核对正式发表版本）

#### D2-11. CHAOS（gem5）`[可用]`
- 第一作者 / venue：近期开源预印本（"CHAOS: Controlled Hardware fAult injectOr System for gem5"）。
- 做什么：模块化、可配置的 gem5 注入框架，目标是支持 gem5 全部 ISA（含 RISC-V）与现代 gem5 版本，细粒度控制注入时刻、故障类型与目标部件；论文同时综述了既有 gem5 注入器的过时问题（FIMSIM 基于旧 M5、GemFI 基于老版本、gem5-Approxilyzer 不支持并发故障与特殊寄存器）。
- 与本课题关系：其相关工作一节可直接作为"为何现有 gem5 注入器都不够用"的引用来源；若要做多故障/并发故障实验（对 TMR 尤其关键——TMR 只在"不同副本同时出错"时才失效），CHAOS 的并发故障支持是少见的。
- 代码：见论文中给出的开源仓库链接（arXiv HTML 版）
- 论文：https://arxiv.org/html/2602.02119 （预印本，引用前核对）

---

### D.3 加速器 / ML 栈注入器（方法学可借用）

#### D3-1. SASSIFI `[参考]`
- 第一作者 / venue：Siva Kumar Sastry Hari, ISPASS 2017。
- 做什么：基于 SASSI（SASS 级编译器插桩）的 GPU 架构级注入器，支持多种错误模型与注入模式（IOV：随机选一条动态线程-指令，注入其目的寄存器；IOA：架构状态注入）。已 deprecated，官方建议改用 NVBitFI。
- 与本课题关系：其 **IOV/IOA 模型区分**是本课题该借用的概念：IOV（instruction output value）等价于当前 harness 的做法，IOA（architectural state）则对应寄存器文件/内存的持久状态注入。只做 IOV 会漏掉"寄存器中长期驻留的值被击中"这类故障，而后者恰是空间 SEU 的主要形态 `[风险]`。
- 代码：https://github.com/NVlabs/sassifi
- 论文：https://ieeexplore.ieee.org/document/7975296

#### D3-2. NVBitFI `[参考][可用]`
- 第一作者 / venue：Timothy Tsai, DSN 2021。
- 做什么：基于 NVBit 的动态二进制插桩注入器，无需源码或重编译，支持新架构 GPU 与预编译库；三步流程（profile → 选参数 → 注入），每次运行只注入一个错误，可按指令组（写 GPR、FP32、FP64、load）筛选。
- 与本课题关系：**注入参数选择的工程范式最值得照抄**：把"kernel 选择 / 指令计数 / 寄存器选择 / 比特模式选择"写成一个可复现的参数文件（含随机数种子），从而使每次注入可被精确重放。这直接对应 D.8 中的 golden-run 与注入可复现性建议。
- 代码：https://github.com/NVlabs/nvbitfi
- 论文：https://www.mbsullivan.info/attachments/papers/tsai2021nvbitfi.pdf

#### D3-3. PyTorchFI `[参考]`
- 第一作者 / venue：Abdulrahman Mahmoud, DSN-W (DSML) 2020。
- 做什么：PyTorch 运行时扰动工具，对权重或神经元（激活）注入位翻转/随机值，pip 可安装。
- 与本课题关系：若课题后续把"空天载荷智能处理"（如星上推理）纳入 case study，这是最低成本的入口。方法学上它示范了"在张量语义层而非寄存器层定义故障"的做法，可类比到本课题的**近似 TMR 阈值定义**（按张量/数组元素的相对误差阈值而非逐比特比较）。
- 代码：https://github.com/pytorchfi/pytorchfi
- 论文：https://ieeexplore.ieee.org/document/9201299

#### D3-4. TensorFI `[参考]`
- 第一作者 / venue：Zitao Chen, ISSRE 2020。
- 做什么：TensorFlow 计算图级注入框架，可注入硬件与软件故障，通过复制并改写 dataflow graph 实现，不需改动 TF 源码；同组另有 TF-DM（数据故障）。
- 与本课题关系：提供"图/算子级注入"与"硬件级注入"结论差异的讨论素材；对本课题而言主要价值是**结局分类与质量度量的定义方式**（SDC 以 top-1 翻转定义 vs 以数值阈值定义），可支撑阈值投票器中"多大偏差算错"的参数选择论证。
- 代码：https://github.com/DependableSystemsLab/TensorFI
- 论文：https://people.ece.ubc.ca/zitaoc/files/issre20-tensorfi.pdf

#### D3-5. GoldenEye `[参考][可用]`
- 第一作者 / venue：Abdulrahman Mahmoud, DSN 2022。
- 做什么：统一评估新兴数值格式（定点、浮点、bfloat、block floating point、AdaptivFloat 等）在 DNN 加速器中的可靠性与精度，内置多种故障模型与快速近似评估通道。
- 与本课题关系：**数值格式与脆弱性强相关**这一结论对近似 TMR 的阈值设计至关重要：float32 的指数位翻转造成巨大偏差、尾数位翻转可被阈值吸收，而定点则相反。建议本课题在实验中按"指数位 / 尾数位 / 符号位"分层报告结局，并引用 GoldenEye 作为分层正当性来源 `[风险]`（当前 harness 若对 64 位均匀选比特位，会把阈值投票的有效性严重低估或高估）。
- 代码：https://github.com/ma3mool/goldeneye
- 论文：https://ieeexplore.ieee.org/document/9833696

---

### D.4 统计严谨性与注入效度（本篇最关键的一节）

#### D4-1. Statistical Fault Injection: Quantified Error and Confidence `[必引][必须采纳]`
- 第一作者 / venue：Régis Leveugle, DATE 2009。被引 600+。
- 做什么：给出从有限总体中抽样注入时**样本量—误差界—置信度**的闭式关系（基于比例估计的正态近似与有限总体校正）：
  `n = N / (1 + e² · (N-1) / (z² · p(1-p)))`，`N→∞` 时退化为 `n = z²p(1-p)/e²`。常用表：95% 置信、±5% 误差 → n≈384；99% → 663；±1% / 95% → 9581。
- 与本课题关系：**现有 harness 必须补这一段**。论文里只写"每个 benchmark 注入 1000 次"而不给误差界，是最容易被挑的方法学硬伤 `[风险]`。采纳方式：(1) 对每个 benchmark × 每个配置声明 N（总故障空间大小 = 动态指令数 × 目标比特数）与所选 n，并给出 e 与 z；(2) 所有 SDC 率、Masked 率均附 95% 置信区间（Wilson 区间优于正态近似，小 p 时尤甚）；(3) 当比较"启用 TMR vs 未启用"的 SDC 率差异时，用两比例检验或区间不重叠判定，而不是直接比点估计。
- 代码：无（公式实现十行；建议在 harness 中加 `scripts/sample_size.py`）
- 论文：https://ieeexplore.ieee.org/document/5090716

#### D4-2. Quantitative Evaluation of Soft Error Injection Techniques `[必引]`
- 第一作者 / venue：Hyungmin Cho, Haesung Mirkhani, Chen-Yong Cher, Jacob Abraham, Subhasish Mitra, DAC 2013。
- 做什么：以 **RTL/门级 flip-flop 注入为 ground truth**，系统评估各类高抽象层注入技术（架构级、寄存器级、程序级）的准确性，量化其对 SDC/DUE 率的偏差与偏差来源。结论：抽象层越高，误差越大且偏差方向不恒定。
- 与本课题关系：与 REFINE 并列为"IR 级注入效度"的两大批评来源。本课题应在 threats-to-validity 一节同时引用 D4-2 与 D4-3，并明确声明本课题的结论是**相对比较性的**（有/无自适应 TMR 的相对改善）而非绝对 SER 预测——相对比较对抽象层偏差的鲁棒性远高于绝对值。
- 代码：无
- 论文：https://ieeexplore.ieee.org/document/6560631 （DAC'13）

#### D4-3. Quantifying the Accuracy of High-Level Fault Injection Techniques `[必引]`
- 第一作者 / venue：Jiesheng Wei, DSN 2014。
- 做什么：用同一组 benchmark 对比 LLFI（IR 级）与 PINFI（x86 汇编级）的结局分布，量化差异并定位差异来源：IR 指令与机器指令的一对多映射、编译器优化后值的生命周期变化、IR 不可见的寄存器溢出与地址计算、以及 IR 层没有对应物的状态（标志位、栈指针）。
- 与本课题关系：这篇论文既是现有 harness 的**正当性来源**（它的总体结论是 IR 级注入在 SDC 率上与汇编级"基本可比"），也是其**局限性来源**。写法建议：引用其结论支持 IR 级方法的可用性，同时引用其差异分析来界定不声称绝对 SER 预测能力。
- 代码：https://github.com/DependableSystemsLab/pinfi
- 论文：https://blogs.ubc.ca/karthik/files/2014/04/DSN-Jiesheng.pdf

#### D4-4. Avoiding Pitfalls in Fault-Injection Based Comparison of Program Susceptibility `[必引][必须采纳]`
- 第一作者 / venue：Horst Schirmeier, DSN 2015。
- 做什么：系统列举"用 FI 比较两个程序版本（如加固前 vs 加固后）易犯的陷阱"，核心有两条：(1) **故障空间大小随加固而改变**——加固版本执行更久、占用更多内存，故障空间更大，若两边都按"相同注入次数"比较，等价于用了不同的故障率，比较无效；(2) golden run 必须**确定性**（异步事件需在相同时间点重放），否则结局分类不可靠。论文给出按"故障空间加权"的正确比较方法。
- 与本课题关系：**对本课题的杀伤力最大，必须正面处理** `[风险]`。近似 TMR 会把执行时间拉长 2–3 倍，故障空间同比增大；如果用"每版本各注入 1000 次"的 SDC 率直接比较，就会系统性低估 TMR 的开销代价，结论是不可靠的。这正是 MWTF 指标被提出的原因（见 D5-3）——但 MWTF 只修正了"工作量"维度，内存驻留故障空间维度仍需按 Schirmeier 的方法加权。
- 代码：https://github.com/danceos/fail
- 论文：https://ess.cs.uni-osnabrueck.de/static/papers/Schirmeier-2015-DSN.pdf

#### D4-5. One Bit is (Not) Enough `[必引][必须采纳]`
- 第一作者 / venue：Behrooz Sangchoolie, Karthik Pattabiraman, Johan Karlsson, DSN 2017；扩展版 TDSC 19(3), 2020；前置工作 Ayatolahi SAFECOMP 2013（单/双比特对比）。
- 做什么：大规模实证研究单比特 vs 多比特（相邻比特、同寄存器不同比特、burst）翻转对程序结局的影响差异。核心结论：在多数情况下单比特模型给出的 SDC 率与多比特相近甚至略高（因为多比特更容易触发崩溃 → 转为 DUE 而非 SDC），但对**某些程序与某些数据类型（尤其浮点、指针）单比特模型会显著低估 SDC**。
- 与本课题关系：现有 harness 为 one-fault-per-run 单比特模型，必须给出正当性。推荐做法：主实验保持单比特（并引用本文作为单比特模型在 SDC 率上的可接受性依据），同时补一组 **双比特/相邻多比特**敏感性实验（哪怕只在 3–4 个 kernel 上做）。这对近似 TMR 尤其必要——TMR 的失效条件是"两个及以上副本同时错"，多比特与空间相关故障（MBU，空间辐射环境中占比不低）正是击穿 TMR 的关键故障模式 `[风险]`。
- 代码：无统一仓库（基于 Chalmers GOOFI 系工具链）
- 论文：https://research.chalmers.se/publication/248841 ；TDSC 扩展版 https://ieeexplore.ieee.org/document/8945754

#### D4-6. 编译优化等级对注入结论的影响 `[必引]`
- 第一作者 / venue：Behrooz Sangchoolie, EDCC 2014（"A study of the impact of bit-flip errors on programs compiled with different optimization levels"）。
- 做什么：实证表明同一程序在 -O0/-O1/-O2/-O3 下的故障结局分布显著不同（寄存器压力、值生命周期、死代码消除都变了）。
- 与本课题关系：直接的实验协议要求——本课题的 TMR 由 LLVM Pass 插入，pass 的插入位置与后续优化的交互会改变脆弱性剖面。必须在论文中**固定并声明优化等级与 pass 顺序**，并说明 TMR 副本未被 CSE/GVN 合并（这既是正确性要求也是效度要求），最好附一组 -O2/-O3 的对照数据 `[风险]`。
- 代码：无
- 论文：https://ieeexplore.ieee.org/document/6821103

#### D4-7. Relyzer `[必引]`
- 第一作者 / venue：Siva Kumar Sastry Hari, ASPLOS 2012。
- 做什么：利用**应用级故障等价性**把全部错误位点（error site = 某动态指令某操作数某比特）划分为等价类，仅对每类的代表（pilot）注入，再把结果推广到全类；剪枝技术包括 bounding addresses、def-use、control-equivalence（控制深度 N=50）、store-equivalence。相对逐点注入可剪枝 3–5 个数量级。
- 与本课题关系：给出"少注入、强结论"的正统路径，是对纯随机采样的替代。即便本课题不实现完整等价类分析，也应至少实现其中最便宜的两项：**def-use 剪枝**（跳过死值）与 **bounding addresses**（地址类值的位翻转结局可解析预测），这能显著提升每次注入的信息量。
- 代码：http://rsim.cs.illinois.edu/relyzer/
- 论文：http://rsim.cs.illinois.edu/Pubs/12-asplos-relyzer.pdf

#### D4-8. Approxilyzer `[必引]`
- 第一作者 / venue：Radha Venkatagiri, MICRO 2016。
- 做什么：在 Relyzer 之上引入**输出质量**维度，为几乎每个错误位点给出"结局 + 质量退化程度"，并据此同时服务近似计算（哪些指令可近似）与低成本容错（哪些指令必须保护）；输出分类为 masked / detected / SDC-Good / SDC-Maybe / SDC-Bad。
- 与本课题关系：**本课题的直接理论支撑**。近似 TMR + 阈值投票本质上就是"按质量容忍度区分保护强度"，Approxilyzer 提供了该决策的量化依据与成熟分类法。强烈建议把现有 5 类分类中的 "Benign" 明确定义为 Approxilyzer 的 SDC-Good（输出有差异但在质量阈值内），并引用此文说明该定义不是本课题自创。另注意其博士论文披露的实践细节：过往研究只覆盖按等价类大小排序的前 99% 错误位点，最后 1% 代价过高——这是一个可诚实引用的"覆盖率声明"范式。
- 代码：http://rsim.cs.illinois.edu/approxilyzer/ ；gem5 版见 D2-9
- 论文：http://rsim.cs.illinois.edu/Pubs/16-MICRO-approxilyzer.pdf

#### D4-9. ePVF `[必引]`
- 第一作者 / venue：Bo Fang, DSN 2016（"ePVF: An Enhanced Program Vulnerability Factor Methodology"）。
- 做什么：指出经典 PVF 把"所有传播到输出的错误"都算作脆弱，而实际上其中很大一部分会表现为**崩溃（crash）**而非 SDC；ePVF 通过识别崩溃致因（主要是越界访存）把崩溃从 PVF 中剥离，得到对 SDC 更准确的估计。
- 与本课题关系：直接关系到本课题 5 类分类中 DUE 与 SDC 的切分正当性。可引用 ePVF 说明"为什么必须把 crash/hang 单列为 DUE 而不是合并进 SDC"，并借用其崩溃概率的估计方法（按 `/proc` 中进程地址空间大小推算指针位翻转的越界概率）为分析式预测提供快速通道。
- 代码：https://github.com/DependableSystemsLab/ePVF
- 论文：https://blogs.ubc.ca/karthik/files/2016/04/ePVF-DSN16.pdf

---

### D.5 可靠性指标体系

#### D5-1. AVF / ACE 分析 `[必引]`
- 第一作者 / venue：Shubhendu S. Mukherjee, MICRO 2003（"A Systematic Methodology to Compute the Architectural Vulnerability Factors for a High-Performance Microprocessor"）；配套 Mukherjee HPCA 2005 的 SDC/DUE 分类图；Biswas ISCA 2005/2006 扩展到地址型结构。
- 做什么：提出 Architecturally Correct Execution (ACE) 概念与 AVF 定义：
  `AVF_structure = (1/N) · Σ_i (ACE cycles of bit i / total cycles)`。
  un-ACE 包括无效/未用状态、误预测路径、NOP、动态死指令、预测假的谓词指令等。AVF 用于对结构的原始 FIT 率做 derate。同时给出 SDC/DUE 的经典结局判定树（有无保护、是否被读）。
- 与本课题关系：**本课题结局分类（Masked/SDC/DUE）的学术源头，必引**。更重要的是用它来界定本课题指标的性质：本课题在 IR 级注入，不建模微架构结构占用，因此得到的是程序侧脆弱性（PVF 类），不能称作 AVF。此外 un-ACE 中的"动态死指令"正是 D.8 中"死值掩蔽"建议的理论依据。
- 代码：无
- 论文：https://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.81.7255&rep=rep1&type=pdf （HPCA'05 综述版）

#### D5-2. PVF / HVF `[必引]`
- 第一作者 / venue：Vilas Sridharan & David Kaeli, HPCA 2009（PVF："Eliminating Microarchitectural Dependency from Architectural Vulnerability"）；HVF 见其 SELSE/后续工作。
- 做什么：把 AVF 分解为与微架构无关的软件分量 PVF 和硬件分量 HVF，使 `AVF ≈ PVF × HVF`（在架构寄存器文件等结构上验证良好），PVF 可用廉价 profiling 获得。
- 与本课题关系：**本课题最应自称报告的指标就是 PVF 类指标**，这既诚实又恰当：LLVM IR 级注入得到的本质是程序对架构可见状态位翻转的敏感度，与具体微架构解耦。好处是结论可同时适用于 x86_64 与 RISC-V 的多种实现；代价是不能直接换算成某颗星载处理器的 FIT。论文中应写出 `AVF = PVF × HVF` 并说明本课题只测前者。
- 代码：无
- 论文：https://ieeexplore.ieee.org/document/4798243

#### D5-3. MWTF（Mean Work To Failure）`[必引][已采纳，需补定义严谨性]`
- 第一作者 / venue：George A. Reis, ISCA 2005（"Design and Evaluation of Hybrid Fault-Detection Systems"，CRAFT）。
- 做什么：提出 `MWTF = (amount of work completed) / (number of errors encountered) = 1 / (AVF × execution_time)`（按单位工作量归一化），用来在"可靠性提升"与"性能下降"之间做公平比较；文中给出 MWTF_SDC 与 MWTF_DUE 两种变体，并以未加固（NOFT）为基线归一化。
- 与本课题关系：现有 harness 已用 MWTF，这是正确选择，但必须补三点严谨性：(1) 明确"work"的单位（PolyBench 的一次 kernel 调用？KissFFT 的一次变换？必须是与输入规模解耦的量）；(2) 分别报告 MWTF_SDC 与 MWTF_DUE，不要合并——对空天任务而言 SDC 远比 DUE 危险（DUE 可由看门狗重启恢复），两者加权不同；(3) MWTF 仅修正执行时间维度的故障空间，不修正内存驻留量维度，需配合 D4-4 的加权方法。
- 代码：无
- 论文：https://pages.cs.wisc.edu/~isca2005/papers/03B-01.PDF

#### D5-4. 结局分类法（outcome taxonomy）的约定 `[必引]`
- 来源：Mukherjee HPCA 2005（SDC/DUE/benign/corrected 判定树）+ Approxilyzer MICRO 2016（SDC-Good/Maybe/Bad）+ ePVF DSN 2016（crash 剥离）+ 各工具实践（LLFI：benign/SDC/crash/hang）。
- 做什么：业界事实约定大致为：Masked（输出与 golden 一致）、Detected & Corrected / Recovered（被机制捕获并恢复）、Detected Unrecoverable Error（DUE：crash、hang/timeout、断言失败）、Silent Data Corruption（SDC：输出错且未被检出），以及近似计算语境下在 SDC 内部再按质量阈值切出 Benign/Acceptable。
- 与本课题关系：本课题 5 类分类（Masked / Detected&Recovered / Benign / SDC / DUE）与上述约定一致且合理，但需在论文中给出**互斥且可判定的操作化定义**：Benign 必须给出明确的质量度量与阈值（如相对 L2 误差 < ε，ε 需与应用 QoS 挂钩而非随意取值），且该阈值须与投票器阈值在概念上区分开（投票阈值是机制参数，Benign 阈值是评测判据，二者相同会导致循环论证 `[风险]`）。另需明确 hang 的判定方式（相对 golden run 的超时倍数 k，k 值需报告）。
- 代码 / 论文：见上述各条

---

### D.6 现场数据与辐照实验方法学（空天场景的现实锚点）

#### D6-1. Silent Data Corruptions at Scale（Meta/Facebook）`[必引]`
- 第一作者 / venue：Harish Dattatraya Dixit, arXiv 2021（并有 Meta 工程博客；后续 DSN 2022 "Detecting silent data corruptions in the wild"）。
- 做什么：报告 Meta 数十万台机器规模上的静默数据损坏现场调查：缺陷核在特定数据值/特定指令序列下产生错误结果（经典例子：Scala 中 `Int(1.1^53)` 在某核上恒返回 0），需数月定位；给出 fleet-scale 的检测（ripple/ 定向测试）与缓解实践。
- 与本课题关系：为"SDC 是真实且必须在软件层处理的问题"提供最有力的现场证据，适合放在引言。对本课题方法学的启示也很重要：现场 SDC 具有**数据依赖性与可重现性**（不是均匀随机位翻转），这意味着均匀随机注入模型刻画的是辐射型 SEU 而非制造缺陷型 CEE——论文需明确限定本课题的故障模型为辐射致瞬态故障 `[风险]`。
- 代码：无
- 论文：https://arxiv.org/pdf/2102.11245

#### D6-2. Cores that Don't Count（Google）`[必引]`
- 第一作者 / venue：Peter H. Hochschild, HotOS 2021, pp. 9–16。
- 做什么：提出 CEE（Corrupt Execution Error）与 "mercurial cores" 概念：每数千台机器出现若干缺陷核，错误率随 workload、频率、电压、温度变化达数个数量级，有时可确定性复现；列出锁语义违反、加解密错误等具体表现，并讨论部署前/后筛查、离线/在线筛查、基础设施级 vs 应用级筛查的权衡。
- 与本课题关系：与 D6-1 构成引言的双支柱。其"错误率强依赖 f/V/T"的观察，对星上计算尤其相关（温度剧烈循环、辐照累积致老化），可用来论证**自适应**容错（按环境动态调节冗余强度）的必要性——这正是本课题的核心命题，是非常有力的动机引用。
- 代码：无
- 论文：https://sigops.org/s/conferences/hotos/2021/papers/hotos21-s01-hochschild.pdf

#### D6-3. JEDEC JESD89A / JESD89-3B `[必引]`
- 来源 / 年份：JEDEC JESD89A（2006，"Measurement and Reporting of Alpha Particle and Terrestrial Cosmic Ray-Induced Soft Errors in Semiconductor Devices"）；JESD89-3B（2021 最新版，"Test Method for Beam Accelerated Soft Error Rate"）。
- 做什么：规定加速辐照 SER 测试的标准方法：束流类型（准单能 QMN vs 散裂全谱）、通量测量与归一化（以纽约市海平面中子通量为参考）、E_min=10 MeV 的定义、FIT/Mb 的计算方式、最少能量点数（QMN 至少 4 点）等；相关设施为 LANSCE、TRIUMF、ChipIR。IEC TS 62396 为航电对应标准。
- 与本课题关系：本课题不做辐照实验，但**必须用它来给注入实验的故障率做量级锚定**：引用标准定义的 FIT/bit 量级（SRAM 约 10²–10³ FIT/Mb 量级，具体值须引具体器件报告），结合目标器件的位数与任务时长，推算"一次任务内期望发生多少次 SEU"，从而说明 one-fault-per-run 假设在何种暴露时间下成立、何时必须转向多故障模型。这一段计算是把"注入实验"与"空天场景"挂钩的唯一桥梁，强烈建议补上 `[风险]`。
- 代码：无
- 文档：https://www.jedec.org/standards-documents/docs/jesd-89-3a ；LANL 高能中子测试手册 https://cdn.lanl.gov/files/radiation-effect-testing-handbook-laur-19-30813_ec677.pdf

#### D6-4. 辐照实验与注入的交叉验证实践（Rech / Quinn 系）`[必引]`
- 代表作者 / venue：Paolo Rech 等（ChipIR/LANSCE 束流实验 + 架构级注入交叉验证，发表于 DSN / TNS / SC 系列）；Heather Quinn（LANL，JESD89 测试标准解读与 FPGA/COTS 辐照报告）。
- 做什么：同一程序同时做（a）加速中子束流实验与（b）架构级/软件级注入，比较两者给出的 SDC/DUE 比例与排序，量化注入方法的保真度；Quinn 的 LANL 系列文档给出 COTS 器件辐照测试与 SEU/MBU/SEFI/SEL 速率计算的操作细节。
- 与本课题关系：用来支撑一个关键论点——注入实验在**相对排序**上与束流实验一致，在绝对率上不一致。这正是本课题应该采取的结论口径。同时 Quinn 的 MBU 数据可为 D4-5 的多比特实验提供"现实中 MBU 占 SEU 的比例"这一参数（随工艺节点缩小而上升），使多比特实验不是凭空设定。
- 代码：无
- 文档：https://www.osti.gov/servlets/purl/1524359 （Quinn, JESD89 测试标准讲义）

---

### D.7 基准集

#### D7-1. PolyBench/C 4.2.1 `[已采纳，需注意]`
- 维护者：Louis-Noël Pouchet, Tomofumi Yuki（OSU/Colostate）。30 个静态控制流数值计算 kernel（线性代数、图像处理、物理仿真、动态规划、统计）。
- 特性：单文件可调插桩（`polybench.c`，支持 kernel 前 cache flush、设置实时调度以减少 OS 干扰）、非零数据初始化、live-out 数据 dump、语法构造防止 kernel 被死代码消除、参数化问题规模（MINI/SMALL/MEDIUM/LARGE/EXTRALARGE）。
- 与本课题关系：对本课题近乎理想（规则循环、输出可逐元素比较、便于定义质量度量）。需注意：(1) 必须用 `POLYBENCH_DUMP_ARRAYS` 的 live-out 输出做结局判定，而不是只看返回码；(2) 规模选择会显著改变故障空间与剪枝率（参见 D2-9 中 Blackscholes 小 footprint 剪枝率低的现象），需固定并报告 dataset 规模；(3) PolyBench 的 `polybench_flush_cache` 与实时调度会影响 PMU 计数，做性能/MWTF 测量时需说明是否启用 `[风险]`。
- 代码：https://github.com/MatthiasJReisinger/PolyBenchC-4.2.1 ；官方页 https://www.cs.colostate.edu/~pouchet/software/polybench

#### D7-2. KissFFT `[已采纳]`
- 作者：Mark Borgerding。轻量、单精度/双精度可切换的 FFT 库（BSD-3）。
- 与本课题关系：FFT 是 OBPMark 明确列为星载"处理构建块"的算法（FIR 滤波与 FFT），因此用 KissFFT 有空天代表性。方法学上注意 FFT 的输出误差分布极不均匀（能量集中在少数 bin），逐元素相对误差阈值会对低能量 bin 过于严格，应改用能量归一化误差（如 SNR 或相对 L2），并在论文中给出该选择的理由。
- 代码：https://github.com/mborgerding/kissfft

#### D7-3. PARSEC（Black-Scholes）`[已采纳]`
- 第一作者 / venue：Christian Bienia, PACT 2008。13 个多线程共享内存工作负载。
- 与本课题关系：Blackscholes 是 FI 文献中的事实标准负载（Relyzer、Approxilyzer、AxBench、gem5-Approxilyzer 均含），便于与既有结果对比，这是保留它的最佳理由。注意 gem5-Approxilyzer 报告它在小输入下 footprint 极小，等价类小、剪枝率最低——意味着其故障空间小、统计上更容易打满，但也更不具代表性；建议至少使用 `simmedium` 输入并报告输入规模 `[风险]`。
- 代码：https://parsec.cs.princeton.edu/ ；镜像 https://github.com/bamos/parsec-benchmark
- 论文：https://parsec.cs.princeton.edu/publications/bienia08characterization.pdf

#### D7-4. ACCEPT suite `[已采纳]`
- 第一作者 / venue：Adrian Sampson 等，UW 技术报告 UW-CSE-15-01（2015），"ACCEPT: A Programmer-Guided Compiler Framework for Practical Approximate Computing"。
- 做什么：基于 Clang/LLVM 的近似编译器框架，含程序员注解（`APPROX` 类型限定）、自动近似机会搜索（autotuner）、安全性分析（污点式信息流，确保近似不污染控制流与内存安全），以及一套近似 C/C++ 应用基准。
- 与本课题关系：本课题的近似 TMR 与 ACCEPT 的近似安全分析在机制上互补——ACCEPT 的"精确/近似类型分离与污点传播"正是决定"哪些变量可以用宽阈值投票、哪些必须精确比较"的现成静态分析框架，应作为本课题 LLVM Pass 设计的直接参照与引用（详见 Part B/C）。
- 代码：https://github.com/uwsampa/accept ；文档 https://sampa.cs.washington.edu/accept

#### D7-5. AxBench `[建议补充]`
- 第一作者 / venue：Amir Yazdanbakhsh / Hadi Esmaeilzadeh（Georgia Tech ACT Lab），IEEE Design & Test 2017；谱系自 MICRO 2012 的 Parrot/NPU 工作。
- 做什么：近似计算基准套件（C++），含 Black-Scholes、FFT、inversek2j、jmeint、JPEG encoder、K-means、Sobel 七个应用，每个自带**质量度量（QoR）定义**与近似注解、编译流程。
- 与本课题关系：最值得补充的一套基准，原因是它为每个应用**预定义了质量度量**。本课题阈值投票的核心难题正是"质量阈值怎么定"，直接采用 AxBench 的 QoR 定义可以避免自造度量被质疑。另外它与现有基准有重叠（Black-Scholes、FFT），可平滑迁移。
- 代码：https://github.com/he-actlab/AxBench
- 论文：https://ieeexplore.ieee.org/document/7314826

#### D7-6. Rodinia `[建议补充]`
- 第一作者 / venue：Shuai Che, IISWC 2009。
- 做什么：覆盖 Berkeley dwarfs 的异构计算基准（OpenMP/CUDA/OpenCL 多版本），含 srad、hotspot、lud、nn、bfs、particlefilter 等。
- 与本课题关系：提供**不规则控制流与指针密集**的负载，正好补足 PolyBench 过于规整的缺陷。不规则负载是 DUE（越界崩溃）的主要来源，对评估"自适应容错在控制流敏感代码上的表现"必不可少；只用 PolyBench 会系统性低估 DUE 比例 `[风险]`。
- 代码：https://github.com/yuhc/gpu-rodinia ；官方 http://lava.cs.virginia.edu/Rodinia
- 论文：https://ieeexplore.ieee.org/document/5306797

#### D7-7. MiBench / mibench-riscv `[建议补充]`
- 第一作者 / venue：Matthew R. Guthaus, WWC-4 / IISWC 2001。
- 做什么：嵌入式基准套件，六大类（automotive/industrial、consumer、office、network、security、telecomm），含 qsort、susan、CRC32、SHA、FFT、dijkstra 等；代码小、依赖少，极易交叉编译到 RISC-V。
- 与本课题关系：本课题需要 RISC-V 侧可跑的小基准；MiBench 的 automotive（basicmath/bitcount/qsort/susan）与 telecomm（FFT/CRC32/ADPCM）两类与星载控制与遥测处理场景贴近，且在 FI 文献中被广泛使用，便于横向对比。
- 代码：https://github.com/embecosm/mibench （及 vhive/mibench 等镜像）
- 论文：https://ieeexplore.ieee.org/document/990739

#### D7-8. Embench-IoT `[建议补充（RISC-V 侧必备）]`
- 维护者 / venue：Embench 工作组（David Patterson、Jeremy Bennett 等倡议），2019 起；明确定位为"替代 Dhrystone/CoreMark 的现代嵌入式基准"。
- 做什么：22 个纯 C、无 OS 依赖、无 libc 依赖（可裸机运行）的小基准，自带几何平均打分与相对 Cortex-M4 基线的归一化方法；另有 Embench-RTOS（中断延迟/上下文切换）与 DSP/浮点扩展套件。
- 与本课题关系：RISC-V 侧的最佳选择，理由有三：(1) 裸机可运行 → golden run 完全确定（无 OS 调度与 ASLR 噪声），这是 FI 实验的硬需求；(2) 规模小 → 故障空间小，可实现接近穷尽的注入覆盖，从而摆脱采样误差争议；(3) 官方给出打分归一化方法，便于报告 TMR 的性能开销。强烈建议把 Embench 作为"确定性 golden run + 高覆盖注入"的子实验平台 `[必须采纳]`。
- 代码：https://github.com/embench/embench-iot
- 站点：https://www.embench.org

#### D7-9. CoreMark / CoreMark-Pro `[可选]`
- 来源：EEMBC，2009 起；CoreMark-Pro 2015。
- 做什么：行业标准嵌入式 CPU 打分基准（链表、矩阵、状态机、CRC），CoreMark-Pro 加入浮点与较大工作负载。
- 与本课题关系：价值在于**性能开销的可比性**——RISC-V 社区普遍以 CoreMark/MHz 报告性能，用它报告 TMR 的 overhead 更容易被嵌入式/星载读者理解。作为可靠性基准则不理想（输出为单一校验值，无法定义质量阈值，Benign 类恒为空）。
- 代码：https://github.com/eembc/coremark

#### D7-10. TACLeBench `[建议补充]`
- 第一作者 / venue：Heiko Falk, WCET 2016 (OASIcs)，TACLe COST Action 产出。
- 做什么：面向**最坏情况执行时间（WCET）分析**的开源基准集合，全部自包含、无外部依赖、含流事实（flow facts）标注，分 sequential/parallel/kernel/app 等类别。
- 与本课题关系：空天计算通常是硬实时系统，容错机制的**最坏情况时间开销**比平均开销更重要。TACLeBench 的自包含性与 flow-fact 标注使得可以分析 TMR + 投票插桩对 WCET 上界的影响，这是现有 harness（只测平均执行时间与 PMU 计数）完全缺失的一个维度，也是审稿人对"空天"定位最可能追问的点 `[风险]`。
- 代码：https://github.com/tacle/tacle-bench
- 论文：https://drops.dagstuhl.de/entities/document/10.4230/OASIcs.WCET.2016.2

#### D7-11. OBPMark / OBPMark-Kernel (GPU4S Bench) / OBPMark-ML `[必须采纳]`
- 来源 / 年份：ESA + Barcelona Supercomputing Center，v0.3.x，2021 起（Zenodo 规范文档 2021），ESA GSP 资助；联系人 OBPMark@esa.int。
- 做什么：专为**航天器星上数据处理**定义的开源计算性能基准集，五大类：(1) 图像处理流水线；(2) 标准压缩算法（CCSDS 121.0 / 122.0 / 123.0 并给出推荐参数）；(3) 标准加密算法；(4) 处理构建块（FIR 滤波、FFT 等）；(5) 机器学习推理（云检测 cloud screening、船只检测 ship detection，提供浮点与量化整数预训练模型）。提供 C / OpenMP / OpenCL / CUDA 多实现；OBPMark-Kernel 源自 GPU4S Bench 的优化算法构建块；OBPMark-ML 为 ML 推理子套件。
- 与本课题关系：**这是本课题从"通用基准"升级为"空天代表性基准"的关键缺口**。目前基准集（PolyBench/KissFFT/Blackscholes/ACCEPT）没有一个是空天领域的，而论文标题是"面向空天计算"，这是选题与评测之间最刺眼的不一致 `[风险]`。建议至少移植 2–3 个 OBPMark 基准（CCSDS 121 压缩、图像处理流水线、FFT 构建块）到 harness：三者都是纯 C、输出可逐元素比较、且 ESA 提供了验证数据（`data/` 目录含输入与校验输出），接入成本低而论文说服力提升巨大。注意其状态为 "Public Beta"，引用时应说明版本号。
- 代码：https://github.com/OBPMark/OBPMark ；Kernel 版 https://github.com/OBPMark/OBPMark-Kernel ；ML 版 https://gitlab.bsc.es/obpmark/obpmark-ml
- 规范/论文：https://zenodo.org/records/5638577 ；站点 https://obpmark.github.io

#### D7-12. SPEC CPU 2017 `[可选]`
- 来源：SPEC，2017。
- 与本课题关系：FI 文献常用（尤其 AVF/PVF 类研究），但单次运行时间长、授权受限、输出为复杂文本，难以定义质量阈值；对 LLVM Pass 类课题而言其价值主要在"证明 pass 能编过大型真实程序"（鲁棒性证据）而非可靠性评测主战场。若时间有限，可只用 `5xx.*_r` 中 2–3 个做编译鲁棒性与性能开销验证。
- 站点：https://www.spec.org/cpu2017

---

### D.8 对本课题现有 harness 的方法学改进建议

以下 7 条按"投入产出比 × 被审稿人追问概率"排序，每条均给出可引用依据与可执行动作。

**(1) 给注入次数一个统计学正当性，并给所有比例加置信区间。** 依据：Leveugle, DATE 2009（D4-1）。动作：对每个 benchmark × 配置，先用 profiling 算出故障空间大小 `N = 动态注入点数 × 候选比特数`，再按 `n = z²p(1-p)/e² / (1 + ...)` 反推在目标误差界 e（建议 ±2%）与置信度（95%）下所需 n；论文表格中所有 SDC/Masked 比例一律写成 `p̂ ± CI`（小 p 时用 Wilson 区间而非正态近似）。对"有 TMR vs 无 TMR"的差异，用两比例检验给出 p 值或差值的置信区间，而不是比较点估计。这一条是成本最低、收益最高的改进。

**(2) 修正"故障空间随加固而膨胀"带来的比较偏差。** 依据：Schirmeier, DSN 2015（D4-4）；Reis, ISCA 2005（D5-3）。动作：近似 TMR 使执行时间增长 2–3 倍、内存驻留数据增加约 3 倍，故障空间同比膨胀。若两版本各注入相同次数并直接比较 SDC 率，等于隐含假设"故障率按运行归一"而非"按时间归一"，会高估加固收益。应 (a) 以 MWTF（按完成的工作量归一）为主指标，分别报告 MWTF_SDC 与 MWTF_DUE；(b) 额外报告按故障空间加权的绝对失效概率 `P_fail = SDC率 × 故障空间大小`，并在论文中明示两种归一方式的结论是否一致。两者不一致时必须讨论。

**(3) 补多比特与空间相关故障实验——这是近似 TMR 的生死问题。** 依据：Sangchoolie, DSN 2017 / TDSC 2020（D4-5）；Ayatolahi, SAFECOMP 2013；Quinn 的 MBU 现场数据（D6-4）。动作：TMR 的失效条件本质是"两个以上副本同时被污染"，而单故障模型从定义上就排除了这一事件，因此当前 harness **在结构上无法观测 TMR 的主要失效模式** —— 这是最严重的效度问题。至少补三组：(a) 同一寄存器双比特/相邻比特翻转；(b) 同一次运行内两次独立注入（跨副本），用于直接测量 TMR 击穿率；(c) burst 故障（连续若干比特）。并用 JESD89 系文献给出的 MBU/SEU 比例为权重合成一个"现实混合故障模型"下的结果。

**(4) 弥合 IR 级与机器级的效度差距。** 依据：Wei, DSN 2014（D4-3）；Georgakoudis REFINE, SC 2017（D1-7）；Cho, DAC 2013（D4-2）。动作（按投入递增）：(a) 最低成本——在 threats-to-validity 中明确声明结论为相对比较而非绝对 SER，并引用上述三篇；(b) 中等成本——在 x86_64 上用 PINFI（或 Pin/DynamoRIO 自写 ~200 行工具）对 2–3 个 kernel 做 1000 次对照注入，报告 IR 级与机器级结局分布的差异幅度；(c) 高成本但最彻底——按 REFINE 的路径把注入点下移到 LLVM 后端（MachineIR），一次改造同时覆盖 x86_64 与 RISC-V 两个 target，且顺带获得对寄存器溢出、地址计算、标志位的注入能力。考虑到课题需跑双 ISA，(c) 的边际成本其实不高。

**(5) 剔除死值掩蔽造成的 Masked 率系统性高估。** 依据：Mukherjee ACE 分析中的 un-ACE / 动态死指令（D5-1）；Relyzer 的 def-use 剪枝（D4-7）；FAIL* 的全故障空间 def/use 区间分析（D2-6）。动作：当前 harness 若在 IR 指令上均匀采样注入点，会把大量"注入到此后再未被读取的值"算作 Masked，使掩蔽率虚高、并相应压低 TMR 的表观收益。改法：在 profiling 阶段记录每个 IR 值的 def-use 区间，只对"存在后继 use"的值注入（或者保留但在结果中把这类位点单独归为 "un-ACE / trivially masked" 一类分开报告）。后者更稳妥——既不改变采样分布，又能让读者看到真实的有效掩蔽率。

**(6) 保证 golden run 的确定性与每次注入的可重放性。** 依据：Schirmeier, DSN 2015（确定性 golden run 是正确分类的前提）；NVBitFI 的三步参数文件范式（D3-2）。动作：(a) 关闭 ASLR（`setarch -R`）、固定 CPU 亲和性与频率（禁用 turbo / 设 performance governor）、固定所有 PRNG 种子、消除时间戳与未初始化内存对输出的影响；(b) 为每次注入持久化一条完整记录元组：`{随机种子, 动态指令序号, IR 指令 ID, 源码位置, 值类型, 寄存器/值宽度, 比特位, 注入时刻, 结局, 质量度量值, PMU 计数}`，使任意一次注入可被单条命令重放（调试 SDC 个案时这是唯一可行的办法）；(c) hang 的判定用相对 golden 墙钟时间的固定倍数 k（建议 k=10）并在论文中报告 k。注意 PolyBench 的 cache flush 与实时调度选项会干扰 PMU 读数，需固定配置并声明。

**(7) 按数据类型与比特位置分层报告，并补齐空天代表性基准。** 依据：GoldenEye, DSN 2022（数值格式决定脆弱性，D3-5）；Approxilyzer 的质量分级（D4-8）；OBPMark（D7-11）。动作：(a) 把结局按"浮点符号位 / 指数位 / 尾数高位 / 尾数低位 / 整数 / 指针"分层统计——这是阈值投票器的设计依据所在（阈值能吸收尾数误差，吸收不了指数位翻转），均匀混合报告会把该机制的真实行为完全掩盖；(b) Benign 的质量阈值必须独立于投票器阈值定义，且取值须有应用层依据（引用 AxBench 的 QoR 定义或 OBPMark 的验证容差），避免用机制参数去判定机制有效性的循环论证；(c) 至少移植 2–3 个 OBPMark 基准（CCSDS 121 压缩 / 图像处理流水线 / FFT 构建块）与 Embench-IoT 全套（裸机确定性 + 小故障空间可近穷尽覆盖），前者解决"空天代表性"缺口，后者解决"采样误差"争议。

**（可选第 8 条）补 WCET 维度。** 依据：TACLeBench, WCET 2016（D7-10）。空天系统多为硬实时，自适应容错的最坏情况时间开销与其平均开销同等重要。现有 harness 只有平均时间与 PMU 计数，建议至少在 TACLeBench 子集上报告 TMR 插桩对执行时间上界（实测最大值或静态 WCET 分析上界）的影响。

---

### D.9 引用优先级速查

| 优先级 | 条目 | 用途 |
|---|---|---|
| P0 必引且必须采纳 | Leveugle DATE'09；Schirmeier DSN'15；Sangchoolie DSN'17；Reis ISCA'05；Mukherjee MICRO'03 | 统计正当性、公平比较、多比特、MWTF、结局分类 |
| P0 必引 | LLFI QRS'15；Wei DSN'14；REFINE SC'17；Cho DAC'13；Sridharan HPCA'09 | 方法继承 + 效度缺口 |
| P1 强烈建议采纳 | Approxilyzer MICRO'16 + gem5-Approxilyzer DSN'19；OBPMark；Embench-IoT；AxBench | 质量分级分类法、空天基准、确定性基准、QoR 定义 |
| P1 必引（动机） | Dixit arXiv'21；Hochschild HotOS'21；JESD89-3B | SDC 现实性、f/V/T 依赖性支撑"自适应"、故障率量级锚定 |
| P2 工具备选 | LLTFI；VULFI（向量/RVV）；FIES/ARCHIE（QEMU，RISC-V 可移植）；InjectV/CHAOS（gem5 RISC-V）；FAIL*（故障空间剪枝） | 按需扩展注入能力 |
| P2 可引（历史与补充） | Xception TSE'98；FERRARI TC'95；GOOFI DSN'01；GemFI DSN'14；GeFIN IISWC'15；Trident DSN'18；ePVF DSN'16；Relyzer ASPLOS'12 | 谱系、工具间效度、分析式脆弱性预测 |

> 链接核对说明：D2-10（InjectV）与 D2-11（CHAOS）为较新预印本，arXiv 编号与发表状态请在引用前再次核对；D2-1/D2-2/D2-3（Xception / FERRARI / GOOFI）无公开代码仓库，仅作谱系引用。
