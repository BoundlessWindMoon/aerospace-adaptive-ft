## H. 故障注入与可靠性评测中的「真实时间维度」：速率型注入、故障累积—修复周期、时变 λ(t) 与束流归一化

> 本篇专门回答一个问题：**既有工作如何把"时间"真正引入故障注入 / 可靠性评测**——不是"第几条动态指令"，而是"任务进行到第几秒、此刻环境强度多大、单位时间来几个故障、修复周期多长"。
> 现有 harness 的基线（见 part_D 开头）是 **one-fault-per-run + 均匀随机单比特翻转 + 按 run 统计 SDC%**，其时间语义仅为"动态指令序号"，**没有到达率（arrival rate）、没有任务时长（mission time）、没有修复周期（repair interval）**。本篇收集的就是能补上这三样东西的文献。
> 每条给出：作者 / 精确 venue + 年份 / **时序模型是什么** / **实际用到的参数值**（若原文给出）/ 代码或数据是否公开 / `机制` 与 `对本课题关系` 两组 bullet。
> 标注：`[必引]` 论文方法学章节应直接引用；`[可用]` 有公开代码/工具可复用；`[风险]` 指出现有 harness 的效度缺口；`[存疑]` 该条某项信息未能独立核实，已逐条标明。
> 条目总数：**29**（H.1 速率型/多故障 campaign 6 条、H.2 故障累积与修复周期 8 条、H.3 CTMC/随机模型与 λ(t) 9 条、H.4 束流归一化 5 条、H.5 轨道环境时间序列 1 条＋明确否定结论），末尾为 **H.6 可直接落地的时序实验设计候选**。

### H.0 与 part_D / part_E 的关系（本篇只写增量）

**本篇扩展（extend）的既有条目**：

| 既有条目 | 本篇如何扩展 |
|---|---|
| D4-1 Leveugle DATE'09（样本量—置信度） | → H.1 全节。Leveugle 解决的是"抽多少个样本"，完全不涉及"每单位时间来几个故障"；H1/H2 给出把样本量换算成**速率与任务时长**的做法 |
| D4-4 Schirmeier DSN'15（故障空间加权） | → **H2**。part_D 只引用了"故障空间随加固而变"这一结论；本篇补上该文真正的时序桥梁：`λ = g·w`、`g = 0.057 FIT/Mbit`、Poisson 多故障概率表 |
| D5-3 MWTF | → **H24 / H26 / H27**。补上 MWTF 在束流文献中的**原始定义式**及其与 cross-section 的关系 |
| D6-3 JESD89A / JESD89-3B | → **H24 / H28**。补上 σ→FIT→MTBF→MEBF 的完整公式链与 13 n/cm²/h 归一化基准 |
| D6-4 Rech / Quinn 交叉验证 | → **H6 / H25 / H26 / H28**。补上精确 venue、卷期与实际通量数值（3.5×10⁶ n/cm²/s、等效 57 万年） |
| E8 / E35 Klessydra 动态 DMR→TMR | → **H23**。明确指出它**没有**在时变故障率下评估，这是本项目可以占的空位 |
| E36 TMR-MER / RUSH | → **H10 / H11 / H14**。补上其**闭式可靠性函数与 CTMC 参数取值**，并纠正 RUSH 缩写 |
| E37 Xilinx SEM IP | → **H13**。补上它作为"按速率注入 + 按周期修复"实验引擎的具体用法与实测数据 |
| E38 辐射环境时变性（SPENVIS / AE9-AP9） | → **H.3 全节 + H.5**。part_E 只把它当"动机引用"；本篇给出**真正把 λ(t) 算出来并喂进可靠性模型的工程链条**（SGP4 + IGRF + CREME96） |

**完全新增的主题**：速率型 campaign 的报告范式（H.1）、scrub 周期 / 修复率闭式 MTTF（H.2）、分段任务（phased-mission）非齐次 Markov（H.3）、束流恒定通量与真实时变环境的解耦论证（H.4）。

---

### H.1 速率型 / 多故障 campaign：按"每单位时间若干故障"注入，而非 one-fault-per-run

#### H1. RTL Fault Injection of a Deployed GNN Trigger for Belle II —— proportional-rate vs fixed-rate 双 campaign `[必引]`
- **作者 / venue**：Georgios Sotiropoulos, Marc Neu, Tanja Harbaum, Torben Ferber, Jürgen Becker（Karlsruhe Institute of Technology）；**IEEE System-on-Chip Conference (SOCC) 2026**（预印本标注 "Accepted to IEEE SOCC 2026"）；arXiv:2607.03153v2，2026-07-17。
- **时序模型**：**不是 one-fault-per-run**。每个 run 是一段连续 RTL 仿真，SEU 按**固定时间间隔速率**注入（单位：**ns/fault**），一个 run 内注入成百上千个故障；结局不报 per-run SDC%，而报 **MTTF（μs）** = 有效仿真时间 / 失效次数。设计两种互补 campaign：
  - *proportional-rate*：每个子模块的注入速率**与其信号数占全设计的比例成正比**，模拟"均匀辐射环境下各模块按面积受击"；
  - *fixed-rate*：所有子模块都用**与全设计相同的绝对速率**，使各模块 MTTF 可直接横向比较（曝露量被拉平，隔离出"每故障敏感度"）。
- **实际参数值**：全设计 **1 846 ns/fault**；proportional-rate 下 Preprocessing 8 141 ns/fault、GNN Dataflow Accelerator 3 357 ns/fault、Postprocessing 64 885 ns/fault、Belle2Link MAC 9 515 ns/fault；fixed-rate 下全部 1 846 ns/fault（小模块速率被放大最多 **35×**）。合计 **1 442 840 个 SEU 注入到 211 245 个信号**，run 数 2 711–7 123/配置。MTTF：全设计 44.1 μs；Postprocessing 486.4 μs（proportional）vs 59.5 μs（fixed）。每个 MTTF 附 **95% 置信边际 ±1.16%–±1.88%**。失效分类为 deadlock / timeout / packet-integrity violation（后者被定位在 SDC 与 DUE 之间）。
- `机制`
  - 用"ns/fault"而不是"第 k 条指令"做注入调度，**指标自动以时间为分母**（MTTF），而样本量/置信区间工具（D4-1）照旧可用。
  - 显式论证两种速率策略的效度差异：proportional-rate 混合了"被击中频率"与"每故障敏感度"；fixed-rate 把曝露拉平。**两者给出相同的加固优先级排序**，这是速率型方法稳健性的实证。
  - 附带发现：**只看输出 vs 看级间活性，MTTF 估计差异高达 78.7%**——观测点选择对时序指标的影响远大于对 per-run 分类的影响。
- `对本课题关系`
  - **本篇最直接可照搬的实验设计模板** `[必引]`。它证明把注入调度从"指令序号"换成"时间间隔"是一次低成本、高回报的范式升级。
  - 对本项目的近似 TMR 尤其关键：fixed-rate campaign 是 Schirmeier DSN'15（D4-4）"故障空间随加固而变"问题的一种**干净解法**——不比较"每版本各注入 1000 次"，而比较"相同绝对故障率下的 MTTF"。这同时正面回应了"TMR 把运行时间拉长 2–3 倍，比较不公平"的质疑。
  - 可直接借用的表格列：{Runs, Rate(ns/fault), Faults injected, 各类结局 %, MTTF, MTTF deviation, 95% margin of error}。
- **代码/数据**：未声明公开（被测 RTL 为 Belle II 实际触发固件）`[存疑：仅据预印本判断]`。
- **论文**：https://arxiv.org/abs/2607.03153

#### H2. Schirmeier 的 FIT → Poisson → 故障空间加权（D4-4 的时序内核）`[必引][必须采纳]`
- **作者 / venue**：Horst Schirmeier, Christoph Borchert, Olaf Spinczyk，*Avoiding Pitfalls in Fault-Injection Based Comparison of Program Susceptibility*，**IEEE/IFIP DSN 2015**。
- **时序模型**：**把"一次注入"换算成"每比特每纳秒故障率"的标准推导**。取现场 DRAM 软错误率 **g = 0.057 FIT/Mbit**，换算得 `g ≈ 1.6 × 10⁻²⁹ /(ns·bit)`；定义故障空间 `w = Δt · Δm`（Δt = 运行周期数，Δm = 占用内存比特数）；一个 run 被 k 个独立故障命中的概率服从 Poisson `P_λ(k) = λ^k e^{−λ} / k!`，其中 **λ = g · w**。以 Δt = 1 s（1 GHz ⇒ 10⁹ cycles）、Δm = 1 MiB = 2²³ bit 为例列出 k = 0,1,2,… 的概率表，结论是**真实速率下 k ≥ 2 可忽略**，故 `P(Failure) ≈ P(Failure|1 fault) · P(1 fault)`，其中 `P(Failure|1 fault) = F / w`（F = 完整故障空间扫描中的失效数）。
- `机制`
  - 给出了 **one-fault-per-run 假设成立条件的精确表述**：当 `g · Δt · Δm ≪ 1` 时成立。一旦任务时长拉到小时/天级，或环境通量上升数个数量级（SAA 穿越、SPE 事件），λ 逼近或超过 1，单故障范式失效。
  - 同时给出"按故障空间加权比较两个程序版本"的正确做法：比 `F / w` 而不是 `F / n`。
- `对本课题关系`
  - **这是把现有 harness 从"指令时间"接到"物理时间"的最短路径，成本几乎为零**：只需记录每个 benchmark 的 Δt 与 Δm，乘一个引自 JESD89A / CREME96 的 g，就能报出"一次任务期望发生多少次故障"。
  - 更重要的是它给出了**本项目自适应机制的定量门槛**：论文可以明确写出"当 λ = g·w 超过某阈值时 one-fault-per-run 范式失效、必须启用多故障模型"，并把 LOW/HIGH 切换判据与该阈值挂钩——把一个方法学缺陷转化为课题动机 `[风险→机会]`。
  - part_D 的 D4-4 只引用了其"陷阱"结论，**未引用这段 Poisson 推导**，这是本篇相对 part_D 的纯增量。
- **代码**：https://github.com/danceos/fail （FAIL*，公开）`[可用]`
- **论文**：https://ess.cs.uni-osnabrueck.de/static/papers/Schirmeier-2015-DSN.pdf

#### H3. F-SEFI / P-FSEFI / CHASER（LANL 系 QEMU 概率触发注入器）`[可用]`
- **作者 / venue**：
  - Qiang Guan, Nathan DeBardeleben, Sean Blanchard, Song Fu，*F-SEFI: A Fine-Grained Soft Error Fault Injection Tool for Profiling Application Vulnerability*，**IEEE IPDPS 2014**, pp. 1245–1254, DOI 10.1109/IPDPS.2014.128。
  - Guan, DeBardeleben, Wu, Eidenbenz, Blanchard, Monroe, Baseman, Tan，*Design, Use and Evaluation of P-FSEFI: A Parallel Soft Error Fault Injection Framework for Emulating Soft Errors in Parallel Applications*，**EAI SIMUTOOLS 2016**, pp. 9–17。
  - Guan, Hu, Grove, Fang, Jiang, Yin, DeBardeleben，*CHASER: An Enhanced Fault Injection Tool for Tracing Soft Errors in MPI Applications*，**IEEE/IFIP DSN 2020**, pp. 355–363, DOI 10.1109/DSN48063.2020.00051。
  - 相关：DECAF-FSEFI，**SELSE 2017**（UC Riverside + LANL），提供 just-in-time 注入与传播追踪。
- **时序模型**：基于 QEMU / TEMU 动态二进制插桩，注入由**"对指定指令类型按概率在线触发"**驱动，而非"预先选定第 k 条动态指令"。因此天然支持**单 run 内注入任意多个故障**，P-FSEFI 还支持**跨 MPI 进程并发注入**（多个 F-SEFI 实例串联，模拟多节点同时受击）。
- `机制`
  - 把"注入时机"从离线采样改为**在线概率触发**，这是实现速率型 campaign 的工程原语。
  - P-FSEFI 的多实例并发是**空间多故障**（多副本/多节点同时）的实现范例——正是击穿 TMR 的关键故障模式。
- `对本课题关系`
  - `[可用]` 代码公开，可直接参考其概率触发实现，移植到本项目的 LLVM 插桩版：把 `inject_if(counter == target)` 改为 `inject_if(bernoulli(p))` 或定时器 / PMU 溢出驱动。
  - 代价提示：F-SEFI 系报告的开销约为**原生 QEMU 的 300×**，不宜直接做大规模 campaign；本项目 fork-per-trial 的 LLVM 注入器在速率型模式下开销远低于此，这是可以写进论文的工程优势。
  - **局限（必须说明）**：F-SEFI 的"速率"是**每指令概率**，并未与物理 FIT 挂钩，也不报 MTTF——它提供的是机制而非方法学。方法学仍须取自 H1 / H2 / H8。
- **代码**：https://github.com/lanl/PFSEFI （LANL，公开）
- **论文**：IPDPS 2014 / DSN 2020（IEEE Xplore）

#### H4. TMR 被"故障累积"击穿的实证：alpha 诱发 SEU 累积 `[必引]`
- **作者 / venue**：A. Manuzzato, S. Gerardin, A. Paccagnella, L. Sterpone, M. Violante，*Effectiveness of TMR-based techniques to mitigate alpha-induced SEU accumulation in commercial SRAM-based FPGAs*，**9th European Conference on Radiation and Its Effects on Components and Systems (RADECS) 2007**, pp. 98–104。`[存疑：页码来自二手引用（Keller & Wirthlin, TNS 68(5), 2021 的参考文献表），未见原文首页]`
- **时序模型**：**不做 scrubbing**，让 SEU 在配置存储中随时间**累积**，测量 TMR 设计累积到第几个 upset 时失效。时间维度体现为"累积计数"，无量纲化后即 λ·T。
- `机制`
  - 实证了 FPGA 文献的核心时序命题：**TMR 的有效性不是设计属性，而是 λ 与修复率 μ 之比的函数**；无修复时 TMR 的 MTTF 只有单模块的 5/6，长期运行必然失效（与 H7 的闭式结果一致）。
- `对本课题关系`
  - 给本项目的"近似 TMR + 阈值投票"提供最硬的反面约束 `[风险]`：若本项目没有任何重同步机制，任务时间尺度上 TMR 的收益会被累积故障吃掉。**论文必须显式回答"本项目的 μ 是什么"**。
  - 最自然的答案也是一个有力论点：软件 TMR 的副本在每次进入受保护区域时从源数据重新初始化，等价于 μ ≈ 1 / 区域执行时间（量级 10³–10⁶ s⁻¹），**这是软件 TMR 相对无修复硬件 TMR 的结构性优势**，值得单独成段并用 H7 的 I_TMR ≈ μ/(6λ) 量化。
  - 反向推论同样重要：**跨函数、跨迭代持久化的副本（如全局数组的三份拷贝）其 μ 骤降**，是真正的脆弱点，值得专门做一组"副本驻留时间 vs MTTF"的实验。
- **代码**：无
- **论文**：RADECS 2007 proceedings（IEEE Xplore）

#### H5. Brosser & Milh：scrub 速率 / SEU 速率比值扫描 `[参考]`
- **作者 / venue**：Fredrik Brosser, Emil Milh，*SEU Mitigation Techniques for Advanced Reprogrammable FPGA in Space*，**Chalmers University of Technology 硕士学位论文，2014**。`[存疑：学位论文，非同行评审，引用力度有限]`
- **时序模型**：显式把 **f_scrub / f_SEU 比值**作为自变量扫描，取值 **{0.01, 0.1, 0.5, 1, 2, 5, 10}**。最低比值 ⇒ 两次 scrub 之间平均注入 100 个故障；最高比值 ⇒ 每两次故障之间 scrub 10 次。SEU 间隔固定为 **1 SEU/min**。输出为可用性（availability）与**停机原因分解**（失效停机 vs scrub 本身占用的停机）。
- `机制`
  - 关键发现：**scrub 越频繁不一定越好**——scrub 自身占时造成停机，存在**内点最优 scrub 速率**；文中给出"给定 SEU 速率下的最优 scrub 速率"曲线，且不同 TMR 实现的最优点不同。
- `对本课题关系`
  - 这正是本项目"自适应"要优化的目标函数形状：保护强度（≈ μ）与开销之间存在**内点最优**，而非单调"越强越好"。可借用其"扫描 μ/λ 比值"的实验设计，把横轴换成"TMR 覆盖率 / 投票频率"、纵轴换成 MWTF。**有了这条曲线，"自适应"才有存在理由：最优点随 λ 移动。**
  - 正式引用建议改用 H.2 节的同行评审文献（Ostler TNS'09、Sari/Psarakis TETC'20），本条只作实验设计灵感。
- **代码**：无
- **论文**：https://publications.lib.chalmers.se/records/fulltext/202966/202966.pdf

#### H6. 束流 campaign 本质上就是速率型 campaign（方法学观察）`[必引]`
- **代表作 / venue**：Fernando Fernandes dos Santos, Angeliki Kritikakou, Olivier Sentieys, Paolo Rech 等，*Experimental evaluation of neutron-induced errors on a multicore RISC-V platform*，**IEEE IOLTS 2022**, DOI 10.1109/IOLTS56730.2022.9897448（开放版 HAL hal-03697265 / arXiv:2206.08639）。
- **时序模型**：束流实验**天生是速率型**：器件在恒定通量 Φ 下连续运行成千上万次程序，故障按 Poisson 过程到达；时间维度是真实秒数，报告单位是 cross-section（cm²）与 **MEBF**。实验设计上**刻意把通量压低**，使"每次执行至多一个故障"成立（见 H23 的论证）。
- **实际参数值**：平台 **GAP8**（8 核 RISC-V ASIC，PULP 派生）；可用中子通量 **≈3.5 × 10⁶ n/(cm²·s)**，比海平面自然通量高 **约 8 个数量级**；实测错误率**低于 1 error / 2 000 次执行**；累计束流时间等效 **> 570 000 年**的平台自然环境运行。结果：计算密集型 CNN 的错误率比平均高 **3.2×**，但 **96.12%** 的 CNN 错误不导致误分类；hang 是导致异常中断的主因。
- `机制`
  - 束流实验 = "恒定高通量 + 长时间"的**加速速率型 campaign**。注入实验要与之可比，就该也是速率型。
- `对本课题关系`
  - 为"为什么应该做速率型注入"提供最强的方法学论据：**地面真值（束流）本来就是速率型的；per-run SDC% 是注入侧单方面引入的简化** `[风险]`。
  - 提供可直接引用的具体数字（8 个数量级加速、57 万年等效）来量化"注入 campaign 与束流在时间语义上的缺口"。
  - "96.12% 的错误不导致误分类"与本项目的近似容错立场高度一致，可作为近似 TMR / 阈值投票合理性的外部证据。
- **代码/数据**：无公开代码，原始事件日志未公开。
- **论文**：https://inria.hal.science/hal-03697265/file/paper.pdf

---

### H.2 故障累积与修复周期：FPGA SEU 文献的 λ / μ 闭式 MTTF 模型

> 本节是整篇报告中**公式最硬、最可直接搬用**的部分。FPGA 界从 2006 年起就把问题写成"upset 以速率 λ 到达、scrubbing 以周期 T（修复率 μ = 1/T）修复"，MTTF 只依赖 λ 与 μ 之比。这正是现有 harness 完全缺失的那一层。

#### H7. McMurtrey / Morgan / Pratt / Wirthlin：TMR + 修复的经典 CTMC 闭式 MTTF `[必引]`
- **作者 / venue**：Daniel McMurtrey, Keith S. Morgan, Brian Pratt, Michael J. Wirthlin，*Estimating TMR Reliability on FPGAs Using Markov Models*，**Brigham Young University, ECE Dept.（BYU ScholarsArchive Faculty Publications No. 149），发表日期标注 2008-12-11**，被引约 48。`[存疑：Ostler TNS'09 的参考文献把它标为 2006 年，BYU 档案标为 2008。建议引用写 "BYU tech. rep., 2008"，或改引 Ostler TNS'09 中的转述]`
- **时序模型**：三状态连续时间 Markov 链（CTMC）
  - S0（3 模块健康）→ S1（1 模块故障）：转移率 **3λ**（λ = 单模块故障率）
  - S1 → S2（系统失效）：转移率 **2λ**
  - S1 → S0（scrub + 重同步修复）：转移率 **μ = 1 / scrub 周期**
  - 闭式解：**MTTF = (5λ + μ) / (6λ²)**；相对单模块（MTTF = 1/λ）的改进因子 **I_TMR ≈ μ / (6λ)**
  - 该文还给出 availability 与功耗模型，并覆盖 **FMER / MER / blind scrubbing / 无修复** 四种修复策略。
- `机制`
  - 这一个公式把"TMR 值不值得"归约为**纯粹的 μ/λ 比值问题**：μ → 0（不修复）时 MTTF → 5/(6λ)，**比单模块还差**；μ ≫ λ 时改进因子线性于 μ/λ。
- `对本课题关系`
  - **本课题应把这个公式作为"理论预期"写进评测章节** `[必引]`。软件 TMR 的 μ 来自"每次进入受保护区域时副本重新初始化"，量级 10³–10⁶ s⁻¹；空间 λ 量级 10⁻⁵–10⁻² s⁻¹。于是 μ/λ ≈ 10⁷–10¹¹，I_TMR 极大——**这是"软件 TMR 在任务时间尺度上远优于无修复硬件 TMR"的定量论据**，也是把近似 TMR 的开销正当化的核心论点。
  - 它同时指出本项目必须量化的一个新量：**副本状态的驻留时间**（= 1/μ）。这是一个在现有 part_A–G 中完全没有出现过的设计维度。
- **代码**：无（公式实现十行）
- **论文**：https://scholarsarchive.byu.edu/facpub/149

#### H8. Ostler et al.：按 scrub 周期做 Poisson 多故障条件概率 → 轨道相关 MTTF `[必引][必须采纳]`
- **作者 / venue**：Patrick S. Ostler, Michael P. Caffrey, Derrick S. Gibelyou, Paul S. Graham, Keith S. Morgan, Brian H. Pratt, Heather M. Quinn, Michael J. Wirthlin，*SRAM FPGA Reliability Analysis for Harsh Radiation Environments*，**IEEE Transactions on Nuclear Science, Vol. 56, No. 6, Dec. 2009, pp. 3519–3526**, DOI 10.1109/TNS.2009.2033381。被引 185+。（LANL + BYU/CHREC 合作）
- **时序模型**（**本篇最值得整体照搬的链条**）：
  1. 单个 scrub 周期内发生 n 个 upset 的概率服从 **Poisson**：`P(n) = a^n e^{−a} / n!`，其中 **a = r_SEU × t_scrub**（r_SEU = 轨道平均 upset 率，t_scrub = scrub 周期）；
  2. 由**故障注入或加速器实验**测得"给定 n 个 upset 时的条件失效概率" **P(F|n)**（设计相关量，必须做注入才知道）；
  3. 单周期无条件失效概率 **P_F = Σ_n P(F|n) · P(n)**；
  4. 系统失效率 **λ_sys = P_F / t_scrub**，**MTTF = 1 / λ_sys**；
  5. **复合 MTTF**：按系统处于各轨道条件的时间占比 P(condition) 加权，得到 composite failure rate 与 composite MTTF。
- **实际参数值**：器件 **Xilinx Virtex-4 XQR4VSX55**，静态截面取自 **Xilinx Radiation Test Consortium (XRTC)**；轨道平均静态 SEU 率由 **CREME96** 生成（文中 Table I "CREME96 orbit-averaged static SEU rates"）；**scrub 周期固定 t = 15 ms**（文中脚注："The scrub rate used for our XQR4VSX55 system is 15 ms. For the purpose of this paper we will always assume t = 0.015 s."）；**5 种轨道**；**4 种轨道条件：solar max / worst week / worst day / peak five minutes**；**6 个设计**（3 个 TMR、3 个非 TMR）。文中明确声明模型**忽略 MBU**（空间相关多位翻转），只处理"同一 scrub 周期内的多个独立 upset"。
- `机制`
  - **把"注入实验"与"任务时间"正确分工**：注入实验只负责 `P(F|n)`（设计敏感度，与环境无关）；环境模型只负责 `a`（与设计无关）。两者在 `P_F = Σ P(F|n)·P(n)` 处相乘。这是整个领域最干净的一条方法学接缝。
  - "composite MTTF"示范了**把时变环境离散化为若干恒定条件 + 占时权重**的标准工程做法——不需要连续 λ(t)，只需要 (λ_i, p_i) 列表。
  - 作者结论句（可直接引用）："the probability of failure is design dependent and **design-specific fault injection should be performed** to estimate design reliability."
- `对本课题关系`
  - **这是本课题应当采用的主框架** `[必须采纳]`。把 FPGA 的"scrub 周期"替换为本项目的"投票 / 重同步周期"（每个 kernel 调用、每个 tile、每个 checkpoint 间隔），结构完全同构：
    - 注入实验改为测 **P(F|n) = "一个投票区间内注入 n 个故障时失效的概率"**，n = 1, 2, 3, …。相对现有 harness 只需把"注入一次"改成"注入 n 次"，**改动极小**（见 H.6 设计 A）。
    - 环境侧用 JESD89A / CREME96 的 FIT 数据给出 a = λ · t_区间。
    - 最终报 MTTF 与 composite MTTF，而不是 SDC%。
  - 它还给出了**近似 TMR 的正确失效判据**：精确 TMR 的 P(F|1) ≈ 0（单故障被掩蔽），而近似 TMR 的 **P(F|1) > 0**（阈值投票可能放过单副本误差）。这个量是本项目区别于精确 TMR 的核心参数，Ostler 框架能把它直接折算成 MTTF 损失——**这是本课题可以做出的、方法学上有新意的贡献点**。
  - 注意它忽略 MBU 的声明：本项目若要主张 MBU（D4-5 One Bit is (Not) Enough），需在 P(F|n) 中额外引入空间相关性，这是一个可声明的扩展。
- **代码/数据**：无公开代码；CREME96 在线可用（https://creme.isde.vanderbilt.edu）。
- **论文**：https://space.pitt.edu/sites/default/files/2024-10/TNS09_B3.pdf

#### H9. Cannon et al.：加入共模失效 λ_CMF 的修正 MTTF —— "修复率收益的天花板定理" `[必引]`
- **作者 / venue**：Matthew J. Cannon, Andrew M. Keller, Hayden C. Rowberry, Corbin A. Thurlow, Andrés Pérez-Celis, Michael J. Wirthlin，*Strategies for Removing Common Mode Failures From TMR Designs Deployed on SRAM FPGAs*，**IEEE Transactions on Nuclear Science, Vol. 66, No. 1, Jan. 2019, pp. 207–215**。
- **时序模型**：在 H7 的三状态 CTMC 上**增加一条 S0 → S2 的直通弧**，速率 λ_CMF（单个共模比特即可直接击穿 TMR）：
  - **MTTF = (5λ + λ_CMF + μ) / (6λ² + 5λ·λ_CMF + λ_CMF² + μ·λ_CMF)**
  - 关键结构性质：μ 只出现在分母的 `μ · λ_CMF` 项中。**只要 λ_CMF > 0，无论 scrub（修复）多快，MTTF 都有上界 ≈ 1/λ_CMF**。
- **实际参数值**：TMR 使中子截面降低 **80×**；对比 striped 与 PCMF 两种布局策略的注入结果（Table III）与辐照结果（Table IV）；关键观察："for every failure for both the striped and PCMF designs, **the scrubber logged multiple upset bits**"——即失效总是伴随**累积**，而非单次事件。
- `机制`
  - 这是 λ/μ 模型族里最重要的"天花板定理"：**存在任何共模路径时，提高修复率的收益饱和**。
- `对本课题关系`
  - 对本项目有直接对应物：软件 TMR 的共模路径 = **未被三重化的共享代码与状态**（阈值投票器自身、循环归纳变量、指针与地址计算、调用栈与返回地址、以及 part_G 讨论的 SIMD 打包后共享的向量寄存器）。
  - **SIMD 打包副本尤其危险** `[风险]`：三个副本打包进同一个向量寄存器后，一次 MBU 可同时损坏多个副本，即 λ_CMF 显著不为零，而 part_G 的性能收益论证里没有计入这一项。建议把 Cannon 公式写进 part_G 的 threats-to-validity，并用 D4-5 的 MBU 比例数据估计 λ_CMF。
  - 建议写法：用这个公式论证"本项目必须报告 λ_CMF 的来源与量级，并说明阈值投票器本身是不是单点失效"。这是答辩极可能被问到的点，提前用 Cannon 公式回应最有力。
- **代码**：BYU BL-TMR / SpyDrNet 工具链公开（https://github.com/byuccl）
- **论文**：https://www.nsf-shrec.org/sites/default/files/2024-03/Strategies-for-Removing-Common-Mode-Failures-From-TMR-Designs-Deployed-on-SRAM-FPGAs.pdf

#### H10. TMR-MER（Nguyen / Diessel / UNSW）：模块级修复的闭式可靠性函数与"修复率可调度" `[必引]`（扩展 E36）
- **作者 / venue**：
  - Nguyen Tran Huu Nguyen, Dimitris Agiakatsikas, Ediz Cetin, Oliver Diessel，*Fine-Grained Module-Based Error Recovery in FPGA-Based TMR Systems*，**ACM TRETS, Vol. 11, No. 1, March 2018, Article 4, pp. 4:1–4:23**, DOI 10.1145/3173549。
  - Nguyen, Agiakatsikas, Cetin, Diessel，*Dynamic scheduling of voter checks in FPGA-based TMR systems*，**IEEE FPT 2016**, Xi'an, pp. 169–172。
  - Nguyen Tran Huu Nguyen et al.，*Scheduling configuration memory error checks to improve the reliability of TMR–MER systems*，**IET Computers & Digital Techniques, 2019**, DOI 10.1049/iet-cdt.2018.5001。
  - Gong, Wu, Nguyen, Agiakatsikas, Zhao, Cetin, Diessel，*A programmable configuration controller for fault-tolerant applications*，**IEEE FPT 2016**, pp. 117–124。
  - 综合来源：Nguyen 的 UNSW 博士论文（公开 PDF）。
- **时序模型**：模块级 TMR + 修复的**闭式可靠性函数**（不止 MTTF，而是完整 R(t)）：
  - 无修复：`R_TMR(t) = 3R²(t) − 2R³(t)`
  - 带 MER 修复：`R_TMR+Recovery(t) = e^{−at/2} · [ a·sinh(bt/2) + b·cosh(bt/2) ] / b`
    其中 **a = 5λ_m + μ_m**，**b = √(λ_m² + 10λ_mμ_m + μ_m²)**
  - 修复率分解：**μ_m = 1/t_repair = 1/(t_d + t_c + t_sync) ≈ 1/(t_d + t_c)**，t_d = 平均错误**检测**时间、t_c = **纠正**时间、t_sync = 重同步时间（案例中可忽略）。
  - **核心创新：t_d 由"投票器检查调度"决定**——round-robin 轮询 vs **Variable-Rate Voter Checking (VRVC)**（对不同模块以不同频率检查）。于是"修复率"从常数变成**可调度的设计变量**。
- **实际参数值**：示例曲线用 **λ = 0.001, μ = 0.1**（μ/λ = 100），t ∈ [0, 3000] s；博士论文还引用 **TAOS 任务的真实在轨 SEU 发生序列**（Fig. 2.1 "Occurrence of SEUs in the TAOS mission"）作为环境输入。
- `机制`
  - 证明**按模块重要性差异化检查频率（VRVC）优于均匀轮询**——在总检查带宽固定时，把检查时间多分给关键模块能提高系统 R(t)。
- `对本课题关系`
  - **这是本项目"按 criticality 差异化保护"在时间维度上的直接同构先例** `[必引]`：它们差异化的是**检查频率**（时间资源），本项目差异化的是**保护强度**（空间 / 算力资源）。相关工作章节应明确这个对偶关系，并把 VRVC 的闭式 R(t) 作为对比基线。
  - 可直接借用的论证链：μ_m = 1/(t_d + t_c) ⇒ "提高投票频率" = "提高 μ" ⇒ 收益斜率由 H7 的 μ/(6λ) 给出、上限由 H9 的 1/λ_CMF 封顶。**H7 + H9 + H10 三条公式连起来就是一套完整的理论分析章节**，不需要本项目自己推公式。
  - TAOS 在轨 SEU 序列是"用真实事件时间序列驱动可靠性评估"的先例——比本课题能拿到的任何东西都更接近 H.5 想要的东西 `[可借用]`。
- **代码**：无公开仓库
- **论文**：https://www.cse.unsw.edu.au/~odiessel/papers/phdnguyen.pdf

#### H11. FMER（Agiakatsikas et al.）：SoC 级 CTMC 与 λ_device 的实际取值 `[必引]`
- **作者 / venue**：Dimitris Agiakatsikas, Nguyen Tran Huu Nguyen, Ediz Cetin, Oliver Diessel 等，*FMER: An Energy-Efficient Error Recovery Methodology for FPGA-based SoCs*，**IEEE Transactions on Aerospace and Electronic Systems (TAES), 2018**。`[存疑：卷 / 期 / 页码未核实，仅见 UNSW 预印本文件名 "taes18agiakatsikas.pdf"。引用前请在 IEEE Xplore 核对]`
- **时序模型**：对整个 SoC 建 CTMC，分别给出 TMR&Scrub 与 TMR&MER 两种配置的 λ_m 与 μ：
  - 模块失效率 **λ_m = λ_device / (3K)**（K = TMR 组件数；器件级故障率均摊到各冗余模块）；
  - **scrub** 修复率 **μ_s = (F_D/2 × t_F + w)⁻¹**，F_D = 器件配置帧数、t_F = 单帧重配置时间、w = 等待开销（平均要扫半个器件）；
  - **MER** 修复率 **μ_m = (F_M × t_F)⁻¹**，F_M = 该模块的帧数（**只重配置出错模块，故 μ_m ≫ μ_s**）。
  - 重要方法学声明：作者发现**用简化的乘积型可靠性表达式（其 Eq.(4)）与完整 Markov 模型给出相同结果**，但前者"evaluate 起来直接得多"——对独立可修复组件，可以绕过 CTMC 数值求解。
- **实际参数值**：**K = 20，λ_device = 0.1 SEUs/dev/s**（注意这是**刻意取高的加速速率**，用于让可靠性曲线在可视时间尺度内分开），λ_m = λ_device/(3K) ≈ 1.67 × 10⁻³ s⁻¹。
- `机制`
  - 给出"把器件级速率摊到模块级"的标准做法：λ_module = λ_device / (模块数 × 冗余度)。
  - 给出"修复率随修复粒度缩小而成比例提高"的定量关系：**μ ∝ 1 / 修复范围**。
- `对本课题关系`
  - 直接对应本项目的**选择性 / 部分 TMR**：只保护部分变量 ⇒ 被保护的"模块"更小 ⇒ 修复范围更小 ⇒ μ 更高。**"选择性保护不仅省开销，还提高修复率"是本项目可以主张的一个新论点**，而 FMER 提供了它的公式依据。
  - λ_device = 0.1 SEU/s 的"刻意加速"做法，是本项目设计速率型 campaign 时的现实先例：**不必用真实的 10⁻⁵ s⁻¹，可以用加速速率并在论文中显式声明加速因子**（与束流实验同一逻辑，见 H23）。这解决了"真实速率下跑不出失效事件"的工程死结。
  - "简化表达式 == Markov 模型"的声明也很有用：本项目不必引入 CTMC 求解器，可以直接用闭式。
- **代码**：无
- **论文**：http://www.cse.unsw.edu.au/~odiessel/papers/taes18agiakatsikas.pdf

#### H12. Sari / Psarakis 系列：scrub 周期 × 实时调度的联合优化 `[必引]`
- **作者 / venue**：
  - Aitzan Sari, Mihalis Psarakis，*Scrubbing-based SEU mitigation approach for Systems-on-Programmable-Chips*，**IEEE FPT 2011**。
  - Sari, Psarakis, Dimitris Gizopoulos，*Combining Checkpointing and Scrubbing in FPGA-based Real-Time Systems*，**IEEE VLSI Test Symposium (VTS) 2013**, pp. 1–6。
  - Sari, Psarakis，*Scrubbing-Aware Placement for Reliable FPGA Systems*，**IEEE Transactions on Emerging Topics in Computing (TETC), Vol. 8, No. 3, Jul.–Sep. 2020, pp. 564–576**, DOI 10.1109/TETC.2017.2757978。
  - Psarakis, Sari，*A scrubbing scheduling approach for reliable FPGA multicore processors with real-time constraints*，**IEEE DFT 2017**, pp. 1–4。
  - Vlagkoulis, Sari, Psarakis, Furano, Poivey, Ferlet-Cavrois 等（ESA 合作），*Configuration Memory Scrubbing of SRAM-Based FPGAs Using a Mixed 2-D Coding Technique*，**IEEE TNS, Vol. 69, 2022, pp. 871–882**（会议版：RADECS 2019, pp. 31–34）。
- **时序模型**：把 scrub 周期当作**实时任务**与应用任务一起用 EDF 调度；可靠性模型给出"在截止期内完成且未失效"的概率；并提出**按模块 criticality 分优先级的 scrub 调度**（prioritized scrubbing scheduling，TETC'20 的第三个案例：JPEG 编码器按模块关键度排 scrub 优先级）。后续文献（RASA, IEEE Access 2021）报告其 EDF-scrubbing 在 60% 利用率下达到约 50% 任务成功率，scan 开销约 **10 ms/次**。
- `机制`
  - 把"修复周期"从可靠性参数升级为**实时调度资源**：提高 μ 必须和应用任务争 CPU / ICAP 带宽，于是存在真正的 trade-off 曲线而非免费收益。
- `对本课题关系`
  - 本项目的"投票 / 重同步"同样消耗 CPU 时间并与应用争算力——**问题结构与 Sari/Psarakis 完全相同**。若论文想扩展到"自适应保护强度在实时约束下的可调度性"，这一系列是最合适的引用与对比基线 `[未来工作]`。
  - "按模块 criticality 分 scrub 优先级"是本项目 criticality-driven 选择性保护的**时间版先例**，与 E37 的 XAPP538 prioritized essential bits、H10 的 VRVC 并列，构成"差异化保护"的三条独立先例线——答辩时可用来证明该思想在硬件、配置存储、时间调度三个层面都已被独立验证。
- **代码**：无
- **论文**：TETC 2020 https://www.computer.org/csdl/journal/ec/2020/03/08053788/13rRUyhaIkF

#### H13. Xilinx / AMD SEM IP 作为"按速率注入 + 按周期修复"的实验引擎 `[可用]`（扩展 E37）
- **来源**：AMD/Xilinx Soft Error Mitigation (SEM) IP 与 TMR SEM IP；应用案例见 *Analysis of the SEU Tolerance of an FPGA-Based Time-to-Digital Converter Using Emulation-Based Fault Injection*，**Electronics (MDPI), Vol. 14, No. 11, 2025, Article 2176**。
- **时序模型**：SEM IP 通过 ICAP **实时、不中断应用功能**地向配置存储注入故障，同时提供检测 / 纠正，因此可以构造"注入速率 λ 固定 × 纠错开启/关闭"的对照实验，**直接观察故障累积的效应**。
- **实际参数值**（MDPI 2025 案例）：累计注入 **3 000 000 个错误**；结果 TDC 的平均 bin width 仅退化 **0.8%**，但积分非线性 INL 退化 **8.8%**，且退化**随累积 SEU 数单调上升**；开启 SEM 纠错后退化被抑制。
- `机制`
  - 工业 IP 直接提供"速率型注入 + 周期型修复"的硬件原语，且 **SEU 事件计数通过 AXI-Lite 暴露给片上处理器**。
- `对本课题关系`
  - `[平台正当性]` 两点：(1) 证明"运行时读取 SEU 事件速率"在真实系统中是标准做法——本项目的 λ 在线估计器不是空想；(2) 该案例演示了**"累积效应只在某些指标上显现"**：bin width 几乎不变而 INL 大幅退化。
  - 第 (2) 点对本项目的近似计算场景是一个**很好的新实验点子**：在速率型 campaign 中跟踪**输出质量指标随累积故障数的轨迹**（而不是只报终态的二元 SDC 分类）。累积故障可能不改变"是否 SDC"的判定，却持续侵蚀质量——这正是近似容错最需要测量的东西，而 one-fault-per-run 范式结构上无法测。
- **文档**：https://www.xilinx.com/products/intellectual-property/sem.html ；案例 https://www.mdpi.com/2079-9292/14/11/2176

#### H14. RUSH 在轨实验：TMR-MER vs SEM scrubbing 的真实 A/B 对照 `[必引]`（扩展 E36，含勘误）
- **来源 / venue**：UNSW / CUAVA（澳）**CUAVA-1 3U CubeSat**（2021-08 由 SpaceX CRS 送抵 ISS 后从 ISS 释放，设计任务期 12 个月），4 个实验载荷之一为 **RUSH**。载荷描述见 Xiaofeng Bai 等，*The CUAVA-1 CubeSat — A Pathfinder Satellite for Remote Sensing and Space Weather*（标题据 USU digitalcommons 记录），**AIAA/USU Small Satellite Conference (SmallSat) 2021**。
- **勘误 `[风险]`**：part_E 的 E36 把 RUSH 展开为 "Rapid Upset in Space Hardware"。SmallSat 2021 的 CUAVA-1 载荷描述原文为 **"the RUSH (Reconfigurable Systems for Space) payload, which aims to demonstrate an in-orbit reconfigurable FPGA system"**。两种展开在文献中并存。**建议 part_E 修正，或引用时只写 "RUSH payload (CUAVA-1)" 不展开缩写**。
- **时序模型**：在轨真实 λ(t)（LEO，含 SAA 穿越）下，**同时运行 TMR-MER 与 TMR + Xilinx SEM 控制器 scrubbing 两种配置**并对比失效率——这是检索范围内**唯一一个"模块级自适应修复 vs 固定周期 scrub"的真实在轨对照实验**。
- **实际参数值**：`[存疑]` 未找到公开的在轨结果数据表（CUAVA-1 的在轨运行状态与载荷数据回传情况未核实）。**引用时只引用其实验设计，不要引用结果数字。**
- `机制`
  - 方法论价值：A/B 对照 + 真实时变环境 = 对"自适应策略优于固定策略"最强的证据形式。
- `对本课题关系`
  - **这是本项目论文"实验设计正当性"一节最值得模仿的范式** `[必引]`：不要只报"自适应版本 SDC% 更低"，而要报"在同一条 λ(t) 轨迹下，自适应 vs 固定-LOW vs 固定-HIGH 三者的 MTTF 与完成工作量（MWTF）"。这是 H.6 设计 C 的直接来源。
- **论文**：https://digitalcommons.usu.edu/cgi/viewcontent.cgi?article=5112&context=smallsat

---

### H.3 连续时间 Markov / 随机模型与时变 λ(t)：评估动态与自适应冗余

> 本节回答问题 3。核心结论先说：**真正在"时变故障率"下评估自适应冗余的工作，集中在 FPGA / 空间计算界（佛罗里达大学 CHREC → 匹兹堡大学 SHREC 的 RFT / HARFT 系列），而不在软件容错界**。而被问到的 Klessydra 动态 DMR→TMR、X-Rel、ATMR 变体**都没有**在时变 λ 下评估——这是本项目可以占的空位（见 H22）。

#### H15. Jacobs / George / Cieslewski：RFT —— 分段任务（phased-mission）Markov 模型 + 轨道 λ(t) `[必引][必须采纳]`
- **作者 / venue**：Adam Jacobs, Alan D. George, Grzegorz Cieslewski，*Reconfigurable Fault Tolerance: A Framework for Environmentally Adaptive Fault Mitigation in Space*，**IEEE International Conference on Field Programmable Logic and Applications (FPL) 2009, pp. 199–204**, DOI 10.1109/FPL.2009.5272313（NSF CHREC, University of Florida）。
- **时序模型**：**phased-mission（分段任务）Markov 模型**——
  - 系统被建模为若干 **phase**，每个 phase 内 **RFT 模式、失效率 λ、修复率 μ 全部恒定**；phase 长度由任务与轨道共同决定（例如"进入 SAA"= 一个 phase 的边界）。
  - 在每个 phase 边界，**把前一个 Markov 模型的末态概率映射为后一个模型的初态概率**（因为冗余拓扑改变，状态空间也改变）。文中 Fig. 2 给出 TMR（3 Up / 2 Up / Failed，转移率 3λ、2λ、μ1、μ2）在 t1 切到 SCP（2 Up / Failed，转移率 2λ、μ2）、t2 再切回 TMR 的示例。
  - 主指标是 **performability**（Meyer 的可性能性：可靠性 × 吞吐的联合度量），报瞬时与平均两种；用 MATLAB 迭代求解器解。
  - 数学依据明确引自：**Kim & Park, "Phased-mission system reliability under Markov environment", IEEE Trans. Reliability 43(2), June 1994**；**Smotherman & Zemoudeh, "A non-homogeneous Markov model for phased-mission reliability analysis", IEEE Trans. Reliability 38(5), Dec. 1989**；Meyer, "Closed-form solutions of performability", IEEE Trans. Computers C-31(7), 1982。
- **实际时间剖面（本篇最想要的东西）**：
  - 案例 1 **ISS 轨道**：圆轨道，**高度 400 km、倾角 51.65°、周期 92 分钟**。文中 Fig. 3 给出**一个轨道周期内每小时 upset 数的曲线**：**最大峰出现在穿越南大西洋异常区（SAA）时，另有两个较小峰出现在最接近两极时**。
  - 案例 2：更恶劣的**大椭圆轨道（HEO）**。
  - 自适应策略：**低 upset 期用 RFT-HP（高性能模式），高 upset 期（SAA 与极区）重构为 RFT-TMR（高可靠模式）**，并在 TMR 模式下做 scrubbing。
  - 作者坦承 upset 率数值"基于 Xilinx Virtex 器件的已有实验数据"，并声明"trends 具有代表性，但偏保守（overly pessimistic）"。
- `机制`
  - **这是把"轨道时间 → 时变 λ(t) → 分段恒定 λ_i → 冗余模式切换 → performability"全链条打通的最早工作**。
  - 关键技术点：**不需要求解非齐次 CTMC**，只需"分段齐次 + 边界概率映射"，工程上极其可行。
- `对本课题关系`
  - **本课题与它的关系是"同一思想的软件版"，必须正面引用并明确差异** `[必引]`：
    1. 它切换的是 FPGA 比特流（需要部分重构，秒级代价）；本项目切换的是 LLVM 生成的多版本函数 / 运行时标志（纳秒–微秒级代价）⇒ **本项目的 phase 粒度可以比它细几个数量级**，这是一个可主张的新贡献。
    2. 它的 λ(t) 来自离线轨道模型（开环）；本项目可以用在线错误率信号（闭环）。
    3. 它的指标是 performability；本项目可以沿用（performability = MWTF 的连续时间推广），这为 D5-3 的 MWTF 提供了更严谨的理论出身。
  - **ISS 400 km / 51.65° / 92 min + SAA 主峰 + 双极区次峰** 这组参数是本课题可以直接拿来做 λ(t) 时间剖面的现成模板（见 H.6 设计 C）。
- **代码**：无
- **论文**：https://www.nsf-shrec.org/sites/default/files/2024-03/FPL09_F6.pdf

#### H16. Jacobs et al.：RFT 完整框架 —— SGP4 + IGRF + CREME96 生成任意轨道的 λ(t) `[必引][必须采纳]`
- **作者 / venue**：Adam Jacobs, Grzegorz Cieslewski, Alan D. George, Ann Gordon-Ross, Herman Lam，*Reconfigurable Fault Tolerance: A Comprehensive Framework for Reliable and Adaptive FPGA-Based Space Computing*，**ACM Transactions on Reconfigurable Technology and Systems (TRETS), Vol. 5, No. 4, Article 21, Dec. 2012, pp. 21:1–21:30**, DOI 10.1145/2392616.2392619。
- **时序模型 —— "RFT fault-rate model"，三个公开模型串联**：
  1. **SGP4**（NORAD 简化通用摄动模型）：输入卫星 **TLE（两行轨道元素）**，输出在用户指定时间段内的**逐点经度 / 纬度 / 高度时间序列**；
  2. **IGRF**（IAGA 国际地磁参考场，每 5 年更新；NASA Goddard 提供 FORTRAN 实现）：把每个轨道位置映射为 **McIlwain L 参数**（即该点所在的磁壳）；
  3. **CREME96**（Tylka et al. 1997）：输入轨道参数 + 航天器物理特性 + 器件工艺参数，输出 SEU 率。**关键技巧：CREME96 默认只给"轨道平均"故障率（把数百圈平均掉），但它也能按 L 参数区间给出分段故障率——于是用很窄的 L 参数分段跑多次 CREME96，就得到"L → 故障率"映射表**；
  4. 把 (1) 的位置时间序列经 (2) 转成 L 参数时间序列，再查 (3) 的映射表，**得到逐时刻的 Segmented Fault Rates，即 λ(t)**。
  - 实现：C++ 程序把三个模型串起来；轨道可用 JSatTrak 开源工具可视化。
- **实际参数值**：文中沿用 H15 的 ISS 与 HEO 案例；明确指出轨道有一段穿过内 Van Allen 带的低高度区域即 **SAA**；SEU 率随太阳天气条件变化。
- `机制`
  - **这是"从轨道参数生成时变注入速率"的唯一完整、可复现的公开工程配方**，且三个组件全部公开可得（SGP4 参考代码、IGRF FORTRAN、CREME96 Web 界面）。
  - 它的 L 参数分段技巧解决了一个真实障碍：环境工具默认输出的是**轨道平均值**，直接用会把时变性抹平。
- `对本课题关系`
  - **问题 5 的答案主体就在这里** `[必须采纳]`：若本课题要做"环境时间序列驱动的 λ(t) 注入"，不必自己发明方法——**照抄 RFT fault-rate model，把输出接到注入器的速率参数上即可**。
  - 工程量评估：SGP4 有现成 Python 实现（`sgp4` / `skyfield` 包），IGRF 有 `pyIGRF` / `ppigrf`，CREME96 需在线交互但可**离线缓存成 (L, rate) 查找表**（Jacobs 原文就是这么做的）。**总工程量约 200–400 行 Python + 一次性的 CREME96 查表**，完全在硕士论文一章的预算内。
  - 可引用的差异化主张：Jacobs 用 λ(t) 驱动的是**解析可靠性模型**（Markov），**不是故障注入 campaign**。把 λ(t) 直接驱动注入 campaign（而不是只喂给公式）在检索范围内**未见先例**——这是本课题可以明确主张的新颖点（见 H29 与 H.6 设计 C）。
- **代码**：RFT 框架本身未开源；三个底层模型公开（SGP4 参考实现、IGRF @ NASA Goddard、CREME96 @ Vanderbilt）。
- **论文**：https://space.pitt.edu/sites/default/files/2024-10/Jacobs_TRETS12.pdf

#### H17. Jacobs / Wulf / George：动态故障率环境下的任务调度 `[参考]`
- **作者 / venue**：Adam Jacobs, Nicholas Wulf, Alan D. George，*Task Scheduling for Reconfigurable Systems in Dynamic Fault-Rate Environments*，**IEEE High Performance Extreme Computing Conference (HPEC) 2013**（Waltham, MA, Sept. 10–12, 2013）。
- **时序模型**：在 RFT 的 λ(t) 之上做**调度**：根据当前 upset 率选择每个任务使用哪种容错方法（TMR / 部分冗余 / 无保护），目标是在满足可靠性约束下最大化吞吐。核心论点（原文）："since SEU rates vary based on orbital position and the majority of orbital positions experience **relatively low upset rates**, a system designed for the worst-case upset-rate scenario contains processing resources that are **wasted during the frequent low-upset-rate periods**."
- `机制`
  - 把"自适应冗余"从"系统级模式切换"下推到"**任务级 / 内核级**的容错方法选择"。
- `对本课题关系`
  - **这是本项目在粒度上的最近邻**：本项目的 LLVM Pass 做的正是"按函数 / 按变量选择保护强度"，只是载体从 FPGA 任务换成了编译期多版本函数。
  - 它那句"最坏情况设计在大部分时间浪费算力"是本项目动机段**可以直接引用的英文表述**，比自己写更有分量。
- **代码**：无
- **论文**：https://ieee-hpec.org/2013/index_htm_files/34-2876289.pdf

#### H18. HARFT（Wilson / Sabogal / George / Gordon-Ross）：混合 SoC 上的环境自适应框架 `[必引]`
- **作者 / venue**：Christopher Wilson, Sebastian Sabogal, Alan D. George, Ann Gordon-Ross，*Hybrid, Adaptive, and Reconfigurable Fault Tolerance*，**IEEE Aerospace Conference 2017, Big Sky MT, pp. 1–11**（NSF CHREC, Univ. of Florida / Univ. of Pittsburgh）。
- **时序模型**：HARFT = Hybrid Adaptive Reconfigurable Fault Tolerance，面向 Xilinx Zynq-7000 / Zynq UltraScale+ 这类"硬核 CPU + FPGA"混合 SoC。架构包含 **CRAM Scrubber + Reconfigure Monitor + Configuration Manager**，用部分重构（PR）在运行时在不同加速器 / 不同冗余配置之间切换；环境输入取自 **CRÈME**（Vanderbilt CREME 在线工具）。该框架后续被 ReCoN（Sabogal et al., IEEE Space Computing Conference）复用来做可重构 CNN 加速器的容错。
- `对本课题关系`
  - `[基线/对比]` 这是 RFT 思想在 **COTS 混合 SoC** 上的工程落地版，与本项目的目标平台（RISC-V SoC + 软件容错）在系统层级上最接近。
  - 可借用的架构分工图：**Software 层（Application + libaccel + Configuration Manager）/ FPGA 层（PR region + TMR + CRAM Scrubber）**——本项目可以画一张对应图，把"Configuration Manager"换成"自适应保护策略运行时"，"CRAM Scrubber"换成"ECC / 错误计数器读取"。
- **代码**：无
- **论文**：https://space.pitt.edu/sites/default/files/2024-10/Hybrid-Adaptive-and-Reconfigurable-Fault-Tolerance-HARFT.pdf

#### H19. Wilson / George / Klamm：SmallSat 计算机可靠性估计方法学（截面 → 轨道速率 → 系统可靠性）`[必引]`
- **作者 / venue**：Christopher Wilson, Alan D. George（UF/CHREC）, Ben Klamm（**NASA Ames Research Center**，任务设计中心辐射环境分析师），*A Methodology for Estimating Reliability of SmallSat Computers in Radiation Environments*，**IEEE Aerospace Conference 2016, pp. 1–12**, DOI 10.1109/AERO.2016.7500605。
- **时序模型**：面向"没有预算做辐照试验"的 SmallSat 设计者，给出**从器件截面数据 + 轨道环境模型推出系统级可靠性**的方法学，可混合处理 rad-hard 与 COTS 部件在同一系统中的情形。
- `对本课题关系`
  - `[必引]` 这是把 H.4 的束流归一化链条与 H.3 的轨道模型**缝合成系统级可靠性数字**的参考方法学，且有 NASA 作者背书，用于答辩时证明"本项目的可靠性论证方式符合航天工程规范"。
  - 本项目的现实处境与该文设定完全一致：**没有束流预算，只有注入数据 + 公开环境模型**。该文等于给出了这种处境下的合规做法。
- **代码**：无
- **论文**：https://space.pitt.edu/sites/default/files/2024-10/A-Methodology-for-Estimating-Reliability-of-SmallSat-Computers-in-Radiation-Environments.pdf

#### H20. 分段任务可靠性的数学基础：非齐次 Markov 与"Markov 环境" `[必引]`
- **作者 / venue**：
  - Mark Smotherman, Kamal Zemoudeh，*A Non-Homogeneous Markov Model for Phased-Mission Reliability Analysis*，**IEEE Transactions on Reliability, Vol. 38, No. 5, Dec. 1989, pp. 585–590**。被引 187。
  - Kyungmee O. Kim, Kyung S. Park，*Phased-Mission System Reliability Under Markov Environment*，**IEEE Transactions on Reliability, Vol. 43, No. 2, June 1994, pp. 301–309**。
  - Mark K. Smotherman, Robert M. Geist，*Phased mission effectiveness using a nonhomogeneous Markov reward model*，**Reliability Engineering & System Safety, Vol. 27, No. 2, 1990, pp. 241–255**。
  - J. Bechta Dugan，*Automated analysis of phased-mission reliability*，**IEEE Trans. Reliability, Vol. 40, No. 1, 1991, pp. 45–52**。
- **时序模型**：
  - Smotherman & Zemoudeh：**非齐次 Markov 链**，转移率本身是时间函数 λ(t)——这是"时变故障率"的严格数学处理，不需要分段近似。
  - Kim & Park："**Markov 环境**"模型：环境本身是一条 Markov 链（例如 {安静期, SAA, SPE} 三状态，带转移率），系统故障率取决于当前环境状态 ⇒ 这是**随机时变 λ**（而非确定性剖面）。
  - Smotherman & Geist：在非齐次 Markov 上加 **reward**，得到 performability / mission effectiveness。
- `机制`
  - 这三篇给出了三种处理时变性的层级：**(a) 确定性剖面 λ(t)（非齐次 CTMC）；(b) 随机环境（环境-系统耦合 CTMC）；(c) 分段恒定 + 边界映射（H15 的工程近似）**。
- `对本课题关系`
  - **本项目若要在论文里给出"自适应优于固定"的解析论证，这是正确的引用出处** `[必引]`。尤其 **Kim & Park 的"Markov 环境"模型是本项目最贴切的理论框架**：把轨道环境建成 {LOW, HIGH} 两状态 Markov 链（转移率由 SAA 占时比反推），系统在 LOW 下用轻保护、HIGH 下用重保护，就能解析地算出自适应策略的 MTTF 并与两种固定策略比较。**这个解析对照只需几十行代码，却能让实验结论有理论锚点。**
  - 注意区分：这些是 1989–1994 年的可靠性工程文献，**软件容错/编译器社区几乎从不引用**，引用它们本身就是本课题跨界贡献的一部分。
- **代码**：无（经典工具链 SHARPE / SPNP / HARP 可用）
- **论文**：IEEE Xplore（TR 38(5):585–590；TR 43(2):301–309）

#### H21. 动态故障树与随机 Petri 网：工具层 `[参考]`
- **作者 / venue**：
  - Joanne Bechta Dugan, Salvatore J. Bavuso, Mark A. Boyd，*Dynamic Fault-Tree Models for Fault-Tolerant Computer Systems*，**IEEE Transactions on Reliability, Vol. 41, No. 3, Sept. 1992, pp. 363–377**。
  - Dugan, Bavuso, Boyd，*Fault trees and Markov models for reliability analysis of fault-tolerant systems*，**Reliability Engineering & System Safety, Vol. 39, 1993, pp. 291–307**。
  - 工具：**Galileo / DIFtree**（动态故障树模块化求解，把子树按静态/动态分类，分别用 BDD 与 Markov 求解）；**SHARPE**、**SPNP**（Trivedi 组，随机 Petri 网 / Markov reward）；**HARP**（Dugan, Trivedi, Smotherman, Geist, AIAA J. Guidance Control & Dynamics 9(3), 1986）。
- **时序模型**：动态故障树（DFT）在静态故障树上增加**顺序依赖门**（功能依赖 FDEP、冷/温/热备件门 SPARE、优先与门 PAND），能表达"失效顺序与时间相关"的行为；可自动展开为 Markov 链求解。
- `对本课题关系`
  - `[参考]` 如果本项目想把"检测 → 切换 → 重执行"这种**有顺序依赖的恢复流程**建模（例如"先 DMR 检测到失配，再升级 TMR，再重算"），DFT 是标准表达工具，SPNP / Galileo 是现成求解器，不必手写 CTMC。
  - 优先级低于 H20：本项目的状态空间很小（2–3 种保护模式 × 2–4 个故障状态），手写闭式或小型 CTMC 更直接。列在此处是为了答辩时能回答"为什么不用 DFT/SPN"——答：状态空间小，闭式更透明。
- **代码**：SPNP / SHARPE 需向 Duke 申请许可；开源替代：Storm、PRISM（概率模型检验器，支持 CTMC 与 reward）。
- **论文**：IEEE TR 41(3):363–377

#### H22. 自适应 SEU 缓解 + 片上粒子传感器：唯一的"在线测 λ 并据此改 μ"硬件先例 `[必引]`
- **作者 / venue**：Robert Glein, Benjamin Schmidt, Florian Rittner, Jürgen Teich, Daniel Ziener，*A Self-Adaptive SEU Mitigation System for FPGAs with an Internal Block RAM Radiation Particle Sensor*，**IEEE 22nd Annual International Symposium on Field-Programmable Custom Computing Machines (FCCM) 2014, pp. 251–258**（Erlangen / Fraunhofer IIS）。
- **时序模型**：在 FPGA 内部划出一块 **BRAM 当作辐射粒子传感器**（故意不加保护，统计其比特翻转率即为在线 λ 估计），然后**按实测 λ 自适应调整 SEU 缓解强度（scrub 频率 / 冗余级别）**。这是**闭环**系统：λ 不是离线模型给的，而是在线测的。
- `对本课题关系`
  - **这是本项目"运行时根据错误率信号切换保护强度"最直接的硬件先例，必须引用** `[必引]`。它证明：(1) 在线估计 λ 是可行的；(2) 按实测 λ 调整 μ 是有收益的。
  - 本项目的对应物：用 ECC 纠错计数器 / `mcause` 陷阱计数 / RERI 错误记录（见 E20/E21）作为 λ 估计器，取代 Glein 的"牺牲 BRAM"。可以主张本项目的信号源**不需要牺牲存储资源**，是一个工程改进。
  - 一个必须讨论的统计问题：**λ 很小时在线估计的方差极大**（泊松计数，观测到 k 次事件的相对标准差 ≈ 1/√k）。要在 SAA 穿越的 ~10 分钟窗口内可靠地检出 λ 上升，需要 λ·T 足够大。**这是本项目自适应触发器设计的核心约束，建议专门做一组"检测延迟 vs 误报率"的实验**（见 H.6 设计 D）。
- **代码**：无
- **论文**：FCCM 2014（IEEE Xplore）；被 *A Self-Adaptive SEU Mitigation Scheme for Embedded Systems in Extreme Radiation Environments*（Univ. of Essex 仓库，`[存疑：venue 未核实]`）引用。该后续工作给出的参数：仿真 SEU 注入速率 **1 × 10⁴ 与 6.25 × 10⁴ bit/s**，对应纠错率 99.9% / 99.97%；中子辐照实测通量 **5 × 10⁶ cm⁻²s⁻¹**，未加固 RAM 错误率 **1.2 bit/(kB·h)**，常规 ECC RAM 约 **4.3 × 10⁻⁴ bit/(kB·h)**。

#### H23. 明确的否定结论：动态 DMR→TMR / X-Rel / ATMR 都**没有**在时变 λ 下评估 `[必引（作为空位论证）]`
- **被检查的工作**：
  - **Klessydra-fT03 动态 DMR→TMR**：Marcello Barbirotta, Abdallah Cheikh, Antonio Mastrandrea, Francesco Menichelli, Mauro Olivieri 等（Sapienza University of Rome），**IEEE DFT 2021**（DOI 10.1109/DFT52944.2021.9568297）；扩展版 **JLPEA (MDPI), Vol. 13, No. 1, Article 2, 2022/2023**，"Evaluation of Dynamic Triple Modular Redundancy in an Interleaved-Multi-Threading RISC-V Core"。见 part_E 的 E8 / E35。
    - **其评估方式**：RTL 级 SEU 注入，**固定速率 / one-fault 式**；报告 FFT 失效率 62% → 13%、FIR 60% → 10%。**没有任务时长、没有 λ(t)、没有 MTTF**。切换触发是"已发生 DMR 失配"（事后、局部），不是"环境速率上升"（前瞻、全局）。
  - **X-Rel**：Javad Vafaei 等，*X-Rel: Energy-Efficient and Low-Overhead Approximate Reliability Framework for Error-Tolerant Applications Deployed in Critical Systems*，**IEEE Transactions on VLSI Systems (TVLSI), 2023**（IEEE Xplore 文档 10125036；预印本 arXiv:2306.09037）。用 ILP 决定每个模块截断多少 LSB，并把投票器精度放松到仅比较高位；评估指标是 **EDAP（最高 3.39× 改善）与图像质量 MSSIM（最高 +114%）**。**完全没有时间维度**：没有故障率、没有 MTTF，错误是以"注入后图像质量"衡量的。
  - **ATMR 变体综述**：Lukas Flad, Mark Leyer, Felix Sebastian Nitz, Tobias Krawutschke，*A Comprehensive Survey of Redundancy Systems with a Focus on Triple Modular Redundancy (TMR)*，**arXiv:2603.14411（2026-03-15，投稿 ACM Computing Surveys，未经同行评审）**。该综述自己得出的研究空白清单包含："**the absence of high-level toolchains for dynamic reconfiguration**" 与 "the need for ... **open-source tools for adaptive fault tolerance**"。
- `对本课题关系`
  - **这一条是本课题最重要的空位论证，应当写进 Introduction 的贡献声明** `[必引]`：
    > "现有的自适应 / 动态冗余工作（动态 DMR→TMR、ATMR、X-Rel）均在**恒定、隐含的故障率**下评估；真正在时变故障率下评估自适应冗余的工作集中在 FPGA 重构领域（RFT / HARFT / TMR-MER），且其 λ(t) 仅用于驱动**解析可靠性模型**，而非驱动故障注入 campaign。"
  - 换言之，本课题的空位是两个维度的交集：**（软件/编译器层的自适应容错）× （由时变 λ(t) 驱动的注入 campaign）**。这个交集在检索范围内是空的。
  - 引用 Flad et al. 2026 综述的"研究空白"段落是最省力的佐证，但**必须标注它是未经同行评审的 arXiv 预印本** `[存疑]`。
- **论文**：JLPEA https://www.mdpi.com/2079-9268/13/1/2 ；X-Rel https://arxiv.org/pdf/2306.09037 ；综述 https://arxiv.org/abs/2603.14411

---

### H.4 束流 / 辐照实验作为时间维度的"地面真值"：σ → FIT → 每任务失效数

#### H24. 标准公式链与标准报告单位（本节的"公式卡片"）`[必引][必须采纳]`
- **规范来源**：**JEDEC JESD89A (2006)**，*Measurement and Reporting of Alpha Particle and Terrestrial Cosmic Ray-Induced Soft Errors in Semiconductor Devices*；**JESD89-3B (Sept. 2021)**，*Test Method for Beam Accelerated Soft Error Rate*。另见 Heather Quinn（LANL）的标准解读报告 **LA-UR-19-25066**。
- **完整公式链**（各符号单位已标明）：
  1. **截面**：`σ [cm²] = N_errors / Φ`，其中 **Φ [n/cm²] = 平均通量 [n/(cm²·s)] × 有效执行时间 [s]**（注意：是**有效**时间，不含 setup / readback 等不受保护的时段）。σ 的物理含义是"被击中即产生输出错误的等效电路面积"。**σ 与通量无关**——这是整个归一化链条的关键：σ 是器件+程序的内禀属性。
  2. **归一化基准**：JESD89A 规定 **以纽约市海平面为参考点**；常用的高能中子自然通量为 **13 n/(cm²·h)**（部分文献用 12 或 14，取决于版本与能量下限约定）。
  3. **FIT**：`FIT = σ [cm²] × flux [cm⁻²h⁻¹] × 10⁹`，**1 FIT = 10⁹ 器件小时中 1 次失效**。按密度归一化时报 **FIT/Mb 或 FIT/bit**。
  4. **MTBF**：`MTBF = 1 / (σ × flux_environment)`。
  5. **MEBF（Mean Executions Between Failures）**：`MEBF = MTBF / t_execution` —— 即"平均能完成多少次正确执行后才遇到一次失效"。**这是 part_D 的 D5-3 中 MWTF 的束流版同义指标**。
  6. **MWTF**（BYU COAST 系的定义，见 H26）：`MWTF = 完成的工作量 / 遇到的错误数 = (raw error rate × AVF × execution time)⁻¹`。
  7. **置信区间**：JESD89A 规定用 **卡方（χ²）统计量**按观测事件数计算 95% 置信限；极低事件数（如 SEL）用特殊形式的 χ²。至少取 **4 个能量点**（QMN 束）；**E_min = 10 MeV**（见 H27 的警告）。
  8. **能量谱**：QMN（准单能）vs spallation（全谱，LANSCE / TRIUMF / ChipIR）；全谱结果比 QMN 的 FIT **高约 45%–70%**（Infineon/Cypress AN54908 的实测对比）。
- `对本课题关系`
  - **这张卡片应当原样进入本课题论文的方法学章节**，因为它是把注入实验结论挂到工程量级的唯一规范途径 `[必须采纳]`。
  - 具体用法（本课题可执行的三行计算）：
    1. 由注入实验得到**每比特每次执行的失效概率** p_f = F/n；
    2. 由 JESD89A/CREME96 得到目标环境的**每比特 FIT**；
    3. 乘以程序驻留比特数 × 任务时长 ⇒ **"一次任务期望发生多少次失效"**。
  - 这同时给出了"SDC% 到工程量"的换算：SDC% 本身没有单位含义，**乘上 λ 与任务时长后才是"每任务失效数"**。part_D 的 D6-3 指出了这个缺口，本条给出了填法。
- **文档**：https://www.jedec.org/standards-documents/docs/jesd-89-3a ；Quinn 解读 https://www.osti.gov/servlets/purl/1524359 ；Infineon AN54908（公式与 χ² 置信限的工业实现）

#### H25. 束流通量恒定、真实环境时变 —— 社区如何处理这个矛盾 `[必引]`
- **代表性论证来源**：Santos / Rech 等 IOLTS 2022（见 H6）；Oliveira, Rech 等，*Experimental and Analytical Study of Xeon Phi Reliability*，**ACM/IEEE SC 2017**（Supercomputing）；A. E. Wilson & Wirthlin 系列（见 H26）；Ostler TNS 2009 的 composite MTTF（见 H8）。
- **三段式处理方法（这是对问题 4 最后一问的直接回答）**：
  1. **把环境依赖项从测量中剥离**：束流测的是 **σ（cm²）**，它是 `N_errors / Φ`，**分母已经把通量除掉了**。因此 σ 与束流强度无关，也与真实环境强度无关——它是"器件 + 程序"的内禀敏感度。**恒定通量不是问题，因为通量在测量阶段就被归一化掉了。**
  2. **把通量压低以保持单故障语义**：实验刻意选择通量，使"一次程序执行内发生 ≥2 个故障"的概率可忽略。Santos et al. 的原文表述：*"Since the terrestrial neutron flux is low, it is improbable to see more than a single corruption during program execution in a realistic application. We have carefully designed the experiments to maintain this property (observed error rates were lower than **1 error per 2 000 executions**)."* 即**束流实验主动维持 one-fault-per-run 语义**，以便 σ 可线性外推。
  3. **时变性在下游通过"多组环境通量 / 占时加权"恢复**：把同一个 σ 分别乘上不同环境的通量，得到不同的 FIT / MTBF；若环境随时间变化，就按占时比加权——这正是 Ostler TNS'09 的 **composite MTTF**（solar max / worst week / worst day / peak five minutes 四种条件 + P(condition) 权重）。
- `机制`
  - 关键洞见：**"恒定通量"与"时变环境"的矛盾是通过"把时间维度推迟到后处理"解决的**，而不是通过在束流里模拟时变通量（那是做不到的，束流强度不可能按 SAA 剖面调制）。
- `对本课题关系`
  - **这给本课题的注入实验提供了一条干净的、与束流实践一致的设计路线** `[必须采纳]`：
    - **第一阶段（与环境解耦）**：保留现有 one-fault-per-run 范式测 `P(F|1)`，甚至扩展到 `P(F|n)`（H8）。这一步的产物是**与环境无关的敏感度曲线**，不需要知道 λ。
    - **第二阶段（引入时间）**：在后处理阶段把 λ(t) 乘进去，输出 MTTF / composite MTTF / 每任务失效数。**不需要在注入器里实现任何时变逻辑。**
    - 只有当要验证"自适应控制器在时变 λ 下的动态行为"（切换时机、检测延迟、误报）时，才**必须**在注入器里实现时变速率（H.6 设计 C/D）。
  - **这是本篇最重要的工程结论**：问题 1–4 的大部分需求可以靠"保留单故障注入 + 后处理乘 λ(t)"满足；只有问题 5（自适应控制器的闭环动态）必须改注入器。
- **论文**：IOLTS 2022（见 H6）；SC 2017 Xeon Phi `[存疑：SC'17 论文的卷/页码未核实，仅据作者主页与 Google Scholar 条目确认 venue 与年份]`

#### H26. BYU：RISC-V 软核 TMR 的截面 → MWTF/MWBF 归一化链（最完整的公开数据）`[必引]`
- **作者 / venue**：
  - Andrew E. Wilson, Michael Wirthlin，*Neutron Radiation Testing of Fault Tolerant RISC-V Soft Processor on Xilinx SRAM-based FPGAs*，**IEEE Space Computing Conference (SCC) 2019, July 2019, pp. 25–32**。
  - A. E. Wilson, S. Larsen, C. Wilson, C. Thurlow, M. Wirthlin，*Neutron Radiation Testing of a TMR VexRiscv Soft Processor on SRAM-Based FPGAs*，**IEEE Transactions on Nuclear Science, Vol. 68, No. 5, 2021, pp. 1054–1060**。
  - A. E. Wilson, M. Wirthlin, N. G. Baker，*Neutron Radiation Testing of RISC-V TMR Soft Processors on SRAM-Based FPGAs*，**IEEE Transactions on Nuclear Science, Vol. 70, 2023, pp. 603–**（ADS: 2023ITNS...70..603W；IEEE 文档 10012379）。
  - A. E. Wilson, C. Thurlow, M. Wirthlin，*Fault injection testing of fault tolerant RISC-V soft processors on Xilinx SRAM-based FPGAs*，**Journal of Radiation Effects Research and Engineering (JRERE), Vol. 39, April 2021, pp. 317–322**。
- **时序模型与归一化**：全部用 **σ_pp（per-processor cross section）** 报告，再折算 **MWBF（Mean Work Between Failures）**，并**显式声明 MWBF 是"归一化的、与具体环境无关的"**（normalized，见 TNS'23 第 VI-B 节），从而避免把结论绑死在某一个环境通量上。
- **实际参数值（可直接引用的数字）**：
  - **SCC 2019（Taiga RISC-V, Kintex UltraScale KCU105, Dhrystone, LANSCE 中子束）**：未加固设计 **fluence 1.15 × 10¹⁰ n/cm²**，观测 52 次失效，σ = **2.27 × 10⁻¹⁰ cm²/processor**；TMR 设计 **fluence 2.00 × 10¹¹ n/cm²**，观测 27 次失效，σ = **6.76 × 10⁻¹² cm²/processor**；**截面降低 33×**，**Fmax 下降 27%**，**MWTF 改善 24×**，资源开销 **5.6×**。同时记录 CRAM upsets（2 527 / 52 139）。恢复策略：检出后先补一次 scrub + 状态轮询，仍错则整器件重配置（**全读回或重配置耗时约 4 秒**）。
  - **TNS 2023（五种软核：Taiga / VexRiscv / PicoRV32 / NOEL-V / MicroBlaze）**：每个 TMR 软核的 σ_pp **至少降低 20×**，MWBF **至少改善 18×**；**NOEL-V 的 MWBF 改善最大，达 61.55×**；整体最高 **75× 可靠性改善**，代价是最多 **4.8× 资源**与平均 **12.4% Fmax 下降**。
  - 注入实验与束流实验的一致性：Taiga 的注入结果给出 **32.5×** 改善，束流给出 **33×** —— **这是"注入与束流在相对排序和倍数上一致"的最硬证据**（直接支撑 part_D 的 D6-4）。
- `对本课题关系`
  - **这是本课题方法学章节必须引用的交叉验证证据** `[必引]`，而且它恰好在 RISC-V 平台上，与 part_E 的平台正当性论证天然衔接。
  - **报告格式可直接模仿**：表格列 = {Fluence, Observed Failures, Cross Section（带 ±95% 置信）, Reduction, CRAM Upsets}；再加一张 {σ_pp, Fmax, 资源, MWBF 改善} 的综合表。本项目的对应物是 {注入次数, 失效次数, P(F|1)（带 Wilson 区间）, 相对改善, 动态指令数, MWTF 改善}。
  - **"MWBF 归一化、与环境无关"这一声明值得照抄**：它让结论不依赖于某个具体轨道，同时又保留了"把工作量折进来"的时间语义。
- **代码**：BYU BL-TMR / COAST / SpyDrNet 公开（https://github.com/byuccl）；辐照原始数据未公开。
- **论文**：https://space.pitt.edu/sites/default/files/2024-10/Neutron_Radiation_Testing_of_Fault_Tolerant_RISC-V_Soft_Processor_on_Xilinx_SRAM-based_FPGAs.pdf ；TNS'23 https://par.nsf.gov/servlets/purl/10442788

#### H27. BYU COAST：编译器级软件容错的束流归一化（与本课题最同源的一条）`[必引][必须采纳]`
- **作者 / venue**：
  - Benjamin James, Michael Wirthlin, Jeffrey Goeders，*Investigating How Software Characteristics Impact the Effectiveness of Automated Software Fault Tolerance*，**IEEE Transactions on Nuclear Science, Vol. 68, No. 5, May 2021, pp. 1014–1022**。
  - Matthew Bohman, Benjamin James, Michael J. Wirthlin, Heather Quinn（LANL）, Jeffrey Goeders，*Microcontroller Compiler-Assisted Software Fault Tolerance*，**IEEE Transactions on Nuclear Science, Vol. 66, No. 1, 2019, pp. 223–232**（页码据原文页眉 "BOHMAN et al.: MICROCONTROLLER COAST 229/231"）。
  - Benjamin James 等，*Automated Software Compiler Techniques to Provide Fault Tolerance for Real-Time Operating Systems*，**2021**（NSF SHREC 发布）`[存疑：venue 未核实]`。
- **时序模型与指标定义**：
  - 截面：**`Cross Section = Errors(SDC) / Fluence`**（原文 Eq. 1）。
  - **MWTF 的原始定义式（原文 Eq. 2 / Bohman Eq. 1）**：
    **`MWTF = 完成的工作量 / 遇到的错误数 = (raw error rate × AVF × execution time)⁻¹`**
    原文给出的动机句："benchmarks which run longer have more time during which they can be upset" —— **这正是 part_D 的 D5-3 需要的严谨定义出处**。
  - **故障覆盖率**：`F_coverage = (F_detected + F_masked) / F_total = 1 − F_undetected / F_total`。
- **实际参数值**：
  - LANSCE 中子辐照，平台 **ARM Cortex-A9**，8 个 benchmark 变体：**COAST TMR 使截面降低 1.0×–54×**（James 2021，带 95% 置信误差棒）；跨架构（MSP430 / ARM 32-bit / 64-bit / RISC-V）**截面降低 4×–106×**。
  - 注入对照（**PACIFIC，QEMU 自研注入框架**）：quicksort 带 hash 校验版 **错误率降低 141×**，仅校验"是否有序"版 **429×**（因为后者本身能天然掩蔽 upset）。
  - COAST 默认检查规则的故障覆盖率：文献报告 95%，本组注入实测 **平均 98.9%**；与 CFT 工具组合重离子束流测试报告 **MWTF 提升 1.66×**。
- `机制`
  - **这是整个检索范围内"编译器插入的软件冗余 + 束流归一化 + MWTF 报告"的完整闭环**，且与本课题技术路线（LLVM Pass 插入 DMR/TMR）几乎一一对应。
  - 核心结论对本课题是警示：**同一套自动化保护在不同 benchmark 上的收益跨越 1.0×–54×（即可能完全无效）**，且差异可归因到数据规模与算法本身的"固有容错性"。
- `对本课题关系`
  - **本课题的最近同行，必须作为主要对比基线引用** `[必引]`（part_E 的 E32 已收录 COAST 工具本身，但**未收录这两篇给出 MWTF 定义式与截面数据的论文**，这是本篇的纯增量）。
  - 直接可采纳的三点：
    1. **MWTF 定义式**（修正 part_D D5-3 的定义严谨性问题）；
    2. **"按 benchmark 报告收益区间而非单一均值"**的报告纪律——本课题的 PolyBench/KissFFT/Black-Scholes 之间的收益差异很可能同样跨一个数量级，预先声明比被审稿人指出要好；
    3. **"固有容错性"作为混淆变量**必须控制：若某 benchmark 本身会掩蔽 upset（如排序、迭代收敛算法），TMR 的测量收益会被系统性低估——这对近似容错课题尤其危险 `[风险]`。
- **代码**：COAST 开源 https://github.com/byuccl/coast ；PACIFIC 注入框架 `[存疑：是否公开未核实]`
- **论文**：https://ccl.byu.edu/assets/Investigating_How_Software_Characteristics_Impact_the_Effectiveness_of_Automated_Software_Fault_Tolerance_2021.pdf ；https://space.pitt.edu/sites/default/files/2024-10/Microcontroller-Compiler-Assisted-Software-Fault-Tolerance.pdf

#### H28. Quinn（LANL）对 JESD89A 的解读与 E_min 陷阱 `[必引]`（扩展 D6-4）
- **作者 / 来源**：Heather Quinn（Los Alamos National Laboratory, ISR-3 Space Data Systems），**LA-UR-19-25066**（JESD89A 测试标准讲义，2019，OSTI 公开）；另有 LANL Microprocessor / FPGA Test Guidelines 与 JPL Microprocessor / SoC Test Guideline。
- **时序 / 归一化要点**：
  - **E_min 问题**：JESD89A 规定 **E_min = 10 MeV**。若被测器件实际阈值 E₀ < 10 MeV，低能事件被计入错误数但其通量**未被计入分母** ⇒ σ 被高估；若 E₀ > 10 MeV，分母通量**被过计** ⇒ σ 被低估。根因是宽谱中子源无法给出 E₀ 信息，于是标准只能固定 E₀ = 10 MeV。
  - **适用范围限制**：宽谱中子源（LANSCE / TRIUMF / ChipIR）的结果**只对 (n, Si) 反应有效**，不能用于预测 (n, ¹⁰B) 反应或 alpha 诱发的错误率。
  - 还需考虑 **F 因子**（器件几何、封装、屏蔽）；若测试与最终产品使用同一金属-介质叠层，屏蔽因子可相消。
  - JESD89A 的规范性附录：**Annex A（地面中子通量测定）、Annex B（计数统计）、Annex C（实时测试统计）**。
- `对本课题关系`
  - `[必引]` 用于**界定本课题引用 FIT 数值时的误差条**：不能声称"我们的 MTTF 精确到某个数值"，只能声称"在引用的 FIT 量级下 MTTF 处于某个量级区间"。这是正当的、也是审稿人期待的表述方式。
  - 同时提供了"为什么本课题只做相对比较而不做绝对 SER 预测"的权威依据——与 part_D 的 D4-2 / D4-3 形成同一条结论口径。
- **文档**：https://www.osti.gov/servlets/purl/1524359

---

### H.5 由轨道 / 环境时间序列驱动注入速率 λ(t)：**软件容错社区无人做此事**

#### H29. 明确否定结论 + 最接近的四个候选 `[必引（作为贡献声明依据）]`
- **结论（请在论文中按此表述）**：
  > 在本次检索范围内，**软件实现容错（SIHFT / 编译器级冗余）社区没有任何工作把 SPENVIS / AE9-AP9 / CREME96 / OMERE 输出的环境时间序列转换为时变注入速率 λ(t) 来驱动故障注入 campaign**。环境模型在该社区中只被用作"动机引用"（指出 SAA/SPE 会使 SEU 率上升数个数量级），从不进入实验设计。
- **最接近的四个候选（按接近程度排序）**：
  1. **H16 Jacobs et al., ACM TRETS 5(4):21, 2012**（最接近）—— 已经做出了 **SGP4 → IGRF → McIlwain L → CREME96 → Segmented Fault Rates = λ(t)** 的完整链条，并用它驱动**分段任务 Markov 模型**。缺的最后一步：λ(t) **没有**用来驱动故障注入 campaign，只喂给解析模型。**本课题要做的就是补上这一步。**
  2. **H8 Ostler et al., IEEE TNS 56(6), 2009** —— 用 **CREME96 轨道平均 SEU 率 + 4 种轨道条件（solar max / worst week / worst day / peak five minutes）+ 占时权重** 得到 composite MTTF。时变性被离散成 4 档而非连续时间序列；注入实验只用来提供 P(F|n)。
  3. **H19 Wilson, George, Klamm, IEEE Aerospace 2016** —— NASA Ames 合作的 SmallSat 可靠性估计方法学，把器件截面 + 轨道环境接起来，但同样是解析链条，不驱动注入。
  4. **航天工程侧的最新同类（非 SW-FT 社区）**：*Monte Carlo and Bayesian-based reliability modeling of spaceborne separation control systems under radiation environment*，**Chinese Journal of Aeronautics, 2026**（ScienceDirect S1000936126001147）。该文用 **CRÈME96 参数 + 加速器测试数据**构建高保真 SEU 模拟器，**显式引入时间相关失效率 λ(t) 与 k-bit upset 事件**，再用 Monte Carlo + 贝叶斯网络做动态故障传播分析，评估 OTMR / CTMR 两种冗余方案（相对单模块分别提升 44.5% / 30.1%），并指出"不同故障注入强度下关键失效路径的重要度会变化"。`[存疑：卷/期/页码未核实；非软件容错社区工作，针对的是分离控制系统这类一次性不可复位系统]`
     - **这是检索到的、最接近"λ(t) 驱动的注入/仿真 campaign"的工作**，但它仿真的是硬件电路级 SEU 传播，不是程序级注入，也不涉及编译器。
  5. **H10 的 TAOS 在轨 SEU 序列** —— UNSW 博士论文用**真实在轨事件时间序列**作为可靠性模型输入，是"真实 λ(t) 数据"离本课题最近的一次使用。
- `对本课题关系`
  - **贡献声明可以写成**：
    > "本文首次把轨道环境模型输出的时变单粒子翻转率 λ(t) 直接用于驱动编译器级故障注入 campaign，从而在同一条任务时间轨迹上比较自适应保护与固定保护策略的 MTTF 与 MWTF。"
    这个声明在本次检索下成立，但**建议答辩前再做一次针对性检索**（关键词：`time-varying fault rate` / `orbit-driven fault injection` / `mission-profile fault injection` / `λ(t) injection campaign`），并保留检索记录。
  - 风险提示 `[风险]`：审稿人可能质疑"这只是把已有的环境模型和已有的注入器接起来，新颖性不足"。**预先准备的回应**：新颖性不在于接线，而在于(1) 只有时变 λ 才能暴露自适应控制器的**检测延迟与切换抖动**这两类失效模式（固定 λ 下结构性不可见）；(2) 由此得到的 MTTF 对比才是**同轨迹、同工作量**的公平对比（回应 Schirmeier DSN'15 的批评）。

---

### H.6 可直接落地的时序实验设计候选

> 前提假设：现有注入器为 **fork-per-trial 的 LLVM 插桩注入器**（父进程 fork 子进程，子进程在第 k 条动态指令处翻一个比特，父进程比对输出并分类）。下面 5 个设计按**实现代价从低到高**排序，每条明确说明"必须给注入器加什么"。
> 代价标注：`[S]` 小时级（改配置/后处理脚本）、`[M]` 天级（改注入器运行时）、`[L]` 周级（新增环境模型与控制器联调）。

---

#### 设计 A：后处理归一化 —— 一行不改注入器，先把 MTTF 报出来 `[S]`
- **理论依据**：H2（Schirmeier 的 λ = g·w）、H24（σ→FIT→MTBF 公式链）、H25（"时间维度推迟到后处理"）、H27（MWTF 定义式）。
- **做什么**：保留现有 one-fault-per-run campaign 不动。额外记录每个 benchmark × 每个配置的：动态指令数 Δt、驻留比特数 Δm（寄存器 + 栈 + 堆的加权估计）、墙钟时间。后处理脚本里：
  1. 取一个引自 JESD89A / CREME96 的 **g [FIT/bit]**（至少给三档：地面海平面、LEO 轨道平均、SAA 峰值）；
  2. 算 `λ = g · Δt · Δm`，用 Poisson 表核验 one-fault 假设成立性（并把这张表放进论文，直接回应 D4-4）；
  3. 算 `MTBF = 1/(P(F|1) · λ)`、`MWTF = (P(F|1) · λ · t_exec)⁻¹`、"每任务（如 90 天）期望失效数"。
- **必须给注入器加的东西**：**无**。只需在 trial 元数据里补记 Δt / Δm / t_exec 三个字段（多数情况下 PMU 已经在采了）。
- **产出**：论文从"SDC% 表"升级为"SDC% + MTTF + 每任务失效数"三联表，且**三档环境下的结论差异本身就是自适应必要性的论据**。
- **风险**：Δm 的估计需要说清方法（建议用 `/proc/self/statm` 峰值 RSS 或 LLVM 静态分析的活跃变量字节数，并声明这是保守上界）。

#### 设计 B：n-fault-per-run，测 P(F|n) 曲线 `[M]`
- **理论依据**：H8（Ostler 的 `P_F = Σ_n P(F|n)·P(n)`，这是整条链里**唯一必须靠注入实验提供的量**）、H4（累积击穿 TMR）、H13（累积只在质量指标上显现）。
- **做什么**：对每个 benchmark × 配置，分别做 n = 1, 2, 3, 4, 6, 8 的 campaign（n 个故障在同一次执行的不同动态指令处，按均匀或 Poisson 间隔撒点），得到 **P(F|n) 曲线**；再按目标环境的 a = λ·T 算 Poisson 权重，合成 MTTF 与 composite MTTF。
- **必须给注入器加的东西**：
  1. 子进程端支持**多个触发点**：把现在的"单个目标计数器"换成一个**已排序的触发点列表**（`std::vector<uint64_t> targets`），插桩代码里的比较从 `counter == target` 改为 `counter == targets[next_idx]`，命中后 `next_idx++`。这是最小改动。
  2. 采样器端：给定 n 与故障空间，生成 n 个**不重复**的触发点（建议两种撒点策略各做一组：均匀随机 vs Poisson 间隔，用来检验结论对撒点分布的敏感性）。
  3. 结局分类扩展：除 5 类结局外，额外记录**输出质量指标的连续值**（不只二元 SDC），以便画"质量 vs 累积故障数"的退化轨迹（H13 的点子）。
- **产出**：
  - **本设计是收益/代价比最高的一个**。它直接产出近似 TMR 的关键参数 **P(F|1) > 0**（精确 TMR 下 ≈ 0），并能量化"近似换来的开销节省值不值得"。
  - 配合设计 A 的后处理，立刻得到可发表的 MTTF 对比。
- **风险**：n 个故障的采样空间组合爆炸，样本量需求上升。应对：按 D4-1 对**每个 n 独立**计算样本量；n 大时 P(F|n) 趋近 1，不需要高精度（可以只跑 200–400 次）。

#### 设计 C：恒定速率型 campaign + MTTF 报告（照搬 H1 的双 campaign 设计）`[M]`
- **理论依据**：H1（proportional-rate vs fixed-rate，ns/fault，MTTF ± 95% margin）、H11（刻意加速的 λ_device = 0.1 SEU/s 的先例）、H14（A/B 对照范式）。
- **做什么**：把 run 定义从"执行一次 benchmark"改为"**执行固定墙钟时长 / 固定迭代轮数的长跑**"，期间以恒定速率 **R [faults/s]** 或 **[faults/Minstr]** 注入；记录首次失效时间 ⇒ MTTF。做两组：
  - *fixed-rate*：所有配置（无保护 / 精确 TMR / 近似 TMR-LOW / 近似 TMR-HIGH）用**相同绝对速率**（这是对 Schirmeier 批评的正面解决）；
  - *proportional-rate*：速率按各配置的故障空间大小成比例（模拟"均匀辐射下大程序被击中更多"），用于检验结论稳健性。
  - 速率取值建议扫 3–4 个数量级（如 10⁻¹, 10⁰, 10¹, 10² faults/s），并**显式声明加速因子**（= R / 真实环境 λ），引用 H11 与 H25 的先例为其正当性背书。
- **必须给注入器加的东西**：
  1. **从 fork-per-trial 转为"长跑 + 进程内重复注入"**：子进程不再在单点翻转后退出，而是常驻；注入由**定时器（`timer_create` + `SIGALRM`）**或 **PMU 溢出中断（`perf_event_open` 的 `PERF_EVENT_IOC_REFRESH` / 采样溢出信号）**触发。后者更好：它让"速率"以**指令数**而非墙钟计量，免受调度抖动影响，且本项目已经在用 `perf_event_open`。
  2. **信号处理器里的注入逻辑**：在 `SIGALRM`/溢出信号处理器中，对"当前正在执行的函数的某个受保护变量 / 某个寄存器"做翻转。这需要一个**运行时注册的目标表**（LLVM Pass 在每个受保护变量上生成一条注册调用，运行时维护一个"当前活跃目标"列表），信号处理器从中随机选一个。这是本设计的主要工作量。
  3. **在线失效检测**：长跑中必须能判断"此刻是否已失效"。建议每轮迭代后与黄金结果比对（或比对校验和），记录首次不匹配的时间戳；同时记录"已完成的正确迭代数"以直接得到 MEBF（H24 的指标 5）。
  4. 父进程端：超时看守 + 崩溃/挂起分类（DUE 要与 SDC 分开计时）。
- **产出**：**可以报 MTTF 与 MEBF，并且四种配置的比较是在同一绝对故障率下做的**——这是本课题最需要的那张表。
- **风险**：进程内重复注入会使"黄金运行可确定性重放"的前提变弱（D4-4 的第二条陷阱）。应对：固定随机种子、禁用 ASLR、固定 CPU 亲和性、单线程运行，并在论文中声明这些控制措施。

#### 设计 D：λ(t) 时变剖面驱动的自适应控制器闭环实验（本课题的新颖点）`[L]`
- **理论依据**：H15（ISS 400 km / 51.65° / 92 min + SAA 主峰 + 双极区次峰的 λ(t) 剖面）、H16（SGP4 + IGRF + CREME96 生成 λ(t) 的完整配方）、H20（Kim & Park 的"Markov 环境"作为解析对照）、H22（在线 λ 估计的方差问题）、H29（这是检索范围内的空位）。
- **做什么**：
  1. **离线生成 λ(t)**：按 H16 的配方，用 Python 栈（`sgp4` / `skyfield` + `pyIGRF`/`ppigrf`）生成 ISS 轨道（400 km, 51.65°, 92 min）的位置时间序列 → McIlwain L 参数序列 → 查一张**离线缓存的 CREME96 (L → SEU 率) 表** → 得到 λ(t)。若 CREME96 交互成本高，**退化方案**：直接照 H15 的曲线形状构造一个"分段常数 λ(t)"（安静期 λ₀、SAA 期 100×λ₀、极区 10×λ₀，SAA 占轨道时间约 10–15%），并在论文中声明这是基于 Jacobs FPL'09 曲线的**定性复现**而非独立计算 `[必须声明]`。
  2. **用 λ(t) 驱动设计 C 的注入器**：把恒定速率 R 换成 R(t)（时间标度可压缩，例如把 92 分钟轨道压缩成 92 秒仿真，并声明时间压缩因子）。
  3. **四臂 A/B 对照**（照 H14 的范式）：固定-无保护 / 固定-LOW / 固定-HIGH / **自适应**。四臂跑在**同一条 λ(t) 轨迹**上（同随机种子的故障到达序列），比较 MTTF、MEBF、总完成工作量、以及"高保护模式占时比"。
  4. **解析对照**：按 H20 的 Kim & Park 模型建一个 {LOW, HIGH} 两状态环境 × {健康, 单错, 失效} 系统的 CTMC，解析算出四臂的 MTTF 预期，与实验值对比。**实验与解析吻合是最强的可信度论证**。
- **必须给注入器加的东西**（在设计 C 之上）：
  1. **时变速率调度器**：一个读取 λ(t) 表的定时器重编程循环（每个 λ(t) 分段结束时重设定时器周期 / PMU 溢出阈值）。
  2. **λ(t) 表与控制器的隔离**：注入器知道真实 λ(t)，但**被测控制器不许直接读它**——控制器只能看到自己的错误计数器。这个隔离必须在代码结构上保证（两个独立的共享内存区域），否则实验无效。
  3. **控制器切换事件日志**：记录每次 LOW↔HIGH 切换的时间戳，用于分析**检测延迟**（λ 上升到控制器切换之间的滞后）与**切换抖动**（在阈值附近反复切换的次数）。**这两个量在恒定 λ 下结构性不可观测，是本设计不可替代的产出。**
- **产出**：本课题最核心的那张图——**时间轴上的三条曲线**：λ(t)、控制器所处模式、累积失效数；以及四臂 MTTF/MWTF 对比表。
- **风险**：
  - 时间压缩会同时压缩"控制器的检测窗口"，可能人为放大检测延迟问题。应对：对压缩因子做敏感性扫描（1×、10×、100×），并报告结论是否随之变化。
  - 在线 λ 估计的泊松方差（H22）：若 SAA 窗口内期望事件数 λ·T < 5，检测必然不可靠。应对：实验中显式扫 λ·T，画出"检测概率 vs λ·T"曲线，作为自适应机制的适用边界声明——**这本身就是一个有价值的负面结论**。

#### 设计 E：修复周期（μ）作为自变量 —— 副本驻留时间实验 `[M]`
- **理论依据**：H7（MTTF = (5λ+μ)/(6λ²)，I_TMR ≈ μ/(6λ)）、H9（λ_CMF 天花板）、H10（μ = 1/(t_d+t_c)，修复率可调度）、H11（μ ∝ 1/修复范围）、H5（存在内点最优）。
- **做什么**：固定 λ（用设计 C 的恒定速率），把**"副本重同步周期 T_sync"作为自变量扫描**（例如每 1 / 10 / 100 / 1000 次循环迭代重新从源数据初始化副本，或每个 tile / 每个 kernel 调用一次），测 MTTF。预期得到 H7 的线性关系 `MTTF ∝ μ/(6λ²)`，并在某个点**偏离线性**——偏离点即 λ_CMF 的天花板（H9），由此**反推本项目实现中的共模失效率 λ_CMF**。
- **必须给注入器加的东西**：
  1. **LLVM Pass 侧**：生成可配置的重同步点（一个编译期参数控制插入频率），以及一个"副本一致性强制刷新"的 runtime 调用。
  2. 注入器侧：无需新机制（沿用设计 C）。
  3. 测量侧：记录重同步带来的指令数开销，以便画 MTTF-vs-开销 的 Pareto 前沿（对应 H5 的"内点最优"）。
- **产出**：
  - **这是本课题可以做出的、最有理论含量的一张图**：MTTF vs μ 的实测曲线叠加 H7/H9 的理论曲线，用拟合得到 λ_CMF。它把"软件 TMR 的 μ 极高"这个定性优势变成定量结果。
  - 同时直接支撑一个新论点（H11）：**选择性保护缩小了修复范围 ⇒ 提高了 μ ⇒ 除了省开销还提高了可靠性**。
- **风险**：λ_CMF 的拟合需要在天花板附近有足够样本，而天花板处失效罕见，样本量需求大。应对：把 λ 刻意调高（加速，H11 的先例）使天花板出现在可观测范围内。

---

#### 代价—收益速查表

| 设计 | 代价 | 注入器改动 | 产出的新指标 | 对应问题 |
|---|---|---|---|---|
| A 后处理归一化 | `[S]` | 无（只加 3 个元数据字段） | MTTF、每任务失效数、Poisson 假设核验表 | 问题 1、4 |
| B n-fault / P(F|n) | `[M]` | 多触发点列表（约 20 行） | P(F|n) 曲线、composite MTTF、近似 TMR 的 P(F|1) | 问题 1、2 |
| C 恒定速率长跑 | `[M]` | 定时器/PMU 溢出驱动的进程内重复注入 + 在线失效检测 | MTTF、MEBF、同速率下的公平对比 | 问题 1 |
| D λ(t) 闭环 | `[L]` | 时变速率调度器 + 控制器隔离 + 切换事件日志 | 检测延迟、切换抖动、同轨迹四臂对比 | 问题 3、5 |
| E μ 扫描 | `[M]` | 可配置重同步点（Pass 侧） | MTTF vs μ 曲线、拟合出的 λ_CMF | 问题 2 |

**推荐组合（硕士论文周期的现实约束下）**：**A + B 必做**（代价低、直接补上 part_D 的两个方法学缺口 D4-4 与 D6-3）；**C 做一个 benchmark 子集**（用于支撑"公平对比"的主张）；**D 作为核心贡献做一个 benchmark**（用退化方案的分段常数 λ(t)，并诚实声明是对 Jacobs FPL'09 曲线的定性复现）；**E 列为未来工作**，除非 H7 的理论分析成为论文的卖点之一。

---

#### 本篇条目索引（共 29 条）

| 编号 | 主题 | 标注 |
|---|---|---|
| H1 | Belle II GNN trigger，proportional/fixed-rate 双 campaign，ns/fault → MTTF（SOCC 2026） | `[必引]` |
| H2 | Schirmeier 的 FIT → Poisson → 故障空间加权（DSN 2015） | `[必引][必须采纳]` |
| H3 | F-SEFI / P-FSEFI / CHASER，概率触发多故障注入（IPDPS 2014 / SIMUTOOLS 2016 / DSN 2020） | `[可用]` |
| H4 | alpha 诱发 SEU 累积击穿 TMR（RADECS 2007） | `[必引]` |
| H5 | f_scrub/f_SEU 比值扫描与内点最优（Chalmers MSc 2014） | `[参考][存疑]` |
| H6 | 束流 campaign 即速率型 campaign；GAP8 RISC-V（IOLTS 2022） | `[必引]` |
| H7 | TMR+修复的闭式 MTTF = (5λ+μ)/(6λ²)（BYU tech. rep. 2008） | `[必引]` |
| H8 | Ostler：Poisson/scrub 周期 + P(F\|n) → 轨道相关与复合 MTTF（TNS 56(6) 2009） | `[必引][必须采纳]` |
| H9 | Cannon：λ_CMF 天花板定理（TNS 66(1) 2019） | `[必引]` |
| H10 | TMR-MER 闭式 R(t) 与 VRVC（TRETS 11(1) 2018 等） | `[必引]` |
| H11 | FMER：SoC 级 CTMC、λ_device=0.1/s、μ ∝ 1/修复范围（TAES 2018） | `[必引][存疑]` |
| H12 | Sari/Psarakis：scrub 周期 × 实时调度（FPT'11 / VTS'13 / TETC 8(3) 2020） | `[必引]` |
| H13 | SEM IP 作为速率注入+周期修复引擎；累积只在质量指标显现（Electronics 14(11) 2025） | `[可用]` |
| H14 | RUSH/CUAVA-1 在轨 A/B 对照（SmallSat 2021）＋ RUSH 缩写勘误 | `[必引][风险]` |
| H15 | RFT：phased-mission Markov + ISS λ(t) 剖面（FPL 2009） | `[必引][必须采纳]` |
| H16 | RFT 完整框架：SGP4+IGRF+CREME96 → λ(t)（TRETS 5(4):21 2012） | `[必引][必须采纳]` |
| H17 | 动态故障率环境下的任务调度（HPEC 2013） | `[参考]` |
| H18 | HARFT：混合 SoC 环境自适应（IEEE Aerospace 2017） | `[必引]` |
| H19 | SmallSat 可靠性估计方法学（IEEE Aerospace 2016，NASA Ames 合作） | `[必引]` |
| H20 | 非齐次 Markov / Markov 环境 / Markov reward（TR 38(5) 1989、TR 43(2) 1994、RESS 27(2) 1990） | `[必引]` |
| H21 | 动态故障树与 SPN 工具层（TR 41(3) 1992；Galileo/SPNP/PRISM） | `[参考]` |
| H22 | 片上粒子传感器驱动的自适应 SEU 缓解（FCCM 2014） | `[必引]` |
| H23 | **否定结论**：Klessydra 动态 DMR→TMR / X-Rel / ATMR 均未在时变 λ 下评估 | `[必引]` |
| H24 | σ→FIT→MTBF→MEBF→MWTF 公式卡片与 13 n/(cm²·h) 基准（JESD89A / JESD89-3B） | `[必引][必须采纳]` |
| H25 | 恒定束流通量 vs 时变真实环境的三段式处理 | `[必引][必须采纳]` |
| H26 | BYU RISC-V 软核 TMR：σ_pp → MWBF，33×/75×，注入与束流一致（SCC'19 / TNS 68(5) 2021 / TNS 70 2023） | `[必引]` |
| H27 | BYU COAST：MWTF 原始定义式、截面 1.0×–54×（TNS 68(5) 2021 / TNS 66(1) 2019） | `[必引][必须采纳]` |
| H28 | Quinn 对 JESD89A 的解读与 E_min 陷阱（LA-UR-19-25066） | `[必引]` |
| H29 | **否定结论**：SW-FT 社区无 λ(t) 驱动的注入 campaign；最接近者为 H16 | `[必引]` |

#### 未能核实的条目汇总（引用前必须二次确认）

1. **H4** Manuzzato et al., RADECS 2007 的页码 98–104 来自二手引用。
2. **H5** Brosser & Milh 为硕士学位论文，非同行评审。
3. **H11** FMER 的 IEEE TAES 卷 / 期 / 页码未核实（仅见 UNSW 预印本）。
4. **H14** CUAVA-1 / RUSH 的在轨**结果数据**未找到公开来源；RUSH 缩写在文献中有两种展开。
5. **H22** 后续 Essex 仓库那篇 "A Self-Adaptive SEU Mitigation Scheme for Embedded Systems in Extreme Radiation Environments" 的 venue 未核实。
6. **H23** Flad et al. 的 TMR 综述为 arXiv 预印本（arXiv:2603.14411），投稿 ACM Computing Surveys，未经同行评审。
7. **H25** Oliveira/Rech 的 Xeon Phi SC 2017 论文的页码未核实。
8. **H27** PACIFIC 注入框架是否开源未核实；James et al. 的 RTOS 那篇 venue 未核实。
9. **H1** Belle II 论文为 arXiv 预印本并标注"Accepted to IEEE SOCC 2026"，**正式出版信息（页码/DOI）尚不存在**；同时其 arXiv ID（2607.03153）对应 2026 年 7 月，引用时请确认当前是否已正式出版。
