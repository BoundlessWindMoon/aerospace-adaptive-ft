## E. RISC-V 与空天辐射容错：平台、开源核、周期级仿真与标准

> 本章定位：为《面向空天计算的自适应容错框架研究》提供**平台正当性（为什么选 RISC-V rv64gcv）**、**硬件侧基线（已有的锁步/ECC/TMR 做到什么程度、软件层还剩什么空间）**、**信号来源的真实性依据（mcause / ECC 错误率计数器在真实 RISC-V 上是否可得）**、以及**答辩时如何回答"你只有 user-mode QEMU，没有 cycle 数"这一必问问题**。
> 检索日期：2026-11-12。链接均为检索时可访问的官方页面/仓库/论文页。
> 关系标注使用四类标签：`[平台正当性]` `[基线/对比]` `[未来工作]` `[周期级证据路径]`。

---

### E.1 抗辐射 / 容错 RISC-V 核与 SoC 平台

#### E1. NOEL-V（含 FT 配置）
- **名称 / 机构**：NOEL-V RISC-V 处理器族 / Frontgrade Gaisler（原 Cobham Gaisler，瑞典），ESA 资助
- **venue+year**：工业 IP（VHDL），GRLIB 持续发布（最新手册 GRIP, Oct 2025, v2025.2）；学术引用多见于 DSD/DASIA/SEFUW
- **是什么**：面向空间的可配置 RISC-V RV64GCH 核，可双发射，支持 AHB/AXI、L2、虚拟化（两级 MMU/IOMMU），**容错特性（cache/寄存器文件 ECC、校验、错误注入与报告）可在所有配置下打开**，官方明确"无需锁步即可获得容错"。
- **与本项目关系**：`[平台正当性]` 这是"RISC-V 已是欧洲空间主流处理器"的最硬证据，直接支撑论文"为什么把自适应容错框架做在 RISC-V 上而不是 SPARC/PowerPC"。`[基线/对比]` NOEL-V FT 把 ECC/校验做在硬件里，本项目的 LOW/HIGH 切换正好消费这类硬件暴露出的纠错事件率——可在论文里写成"硬件提供检测与计数，软件层提供可调冗余"的分层论证。
- **code**：https://www.gaisler.com/grlib-ip-library （GPL 版含源码与 FPGA bitfile）
- **paper/doc**：https://www.gaisler.com/products/noel-v ，手册 https://download.gaisler.com/products/GRLIB/doc/grip.pdf

#### E2. 开源 Lockstep NOEL-V（SELENE SoC 内）
- **名称 / 作者**：Open-source Lockstep NOEL-V / BSC + UPC（Alcaide、Hernández、Abella 等），ISOLDE/SELENE 资助
- **venue+year**：DATE/DSD 系列及 UPCommons 技术报告，2023
- **是什么**：首个开源 RISC-V **硬件锁步核**实现：在 Gaisler NOEL-V 上做双核锁步 + staggering（延迟错位以抵抗共因故障 CCF），并集成进可在 FPGA 上综合的 SELENE SoC。
- **与本项目关系**：`[基线/对比]` 代表"硬件全冗余"端点：面积/功耗 ~2x 且恒定不可调。本项目的核心卖点（按 criticality 选择性保护 + 运行时 LOW/HIGH）正是针对锁步"不可调、恒定开销"的缺陷，这篇是最合适的对照物。`[未来工作]` 若未来做硬件协同，可在该 SoC 上接入本框架的模式切换信号。
- **code**：https://gitlab.bsc.es/selene （SELENE hardware；NOEL-V 本体在 GRLIB）
- **paper**：https://upcommons.upc.edu/server/api/core/bitstreams/def43000-8180-41c6-8ef6-a67e0ec5954b/content

#### E3. SELENE（H2020 871467）
- **名称 / 机构**：SELENE — Self-monitored Dependable platform for High-Performance Safety-Critical Systems / BSC、Cobham Gaisler、Ikerlan、Siemens、UPV、Airbus D&S、CAF Signalling
- **venue+year**：EU H2020（2019–2022）；平台论文 Hernández et al., **Euromicro DSD 2020**, pp. 370–377, DOI 10.1109/DSD51259.2020.00066
- **是什么**：基于多核 NOEL-V 的安全关键"自监控"平台，含 NoC、AI 加速器、Jailhouse hypervisor、诊断与监控 IP；用例包含卫星与空间机器人。
- **与本项目关系**：`[平台正当性]` "self-monitored / 平台自身在线监测并据此调整"正是本项目自适应思想在系统级的先例，可作为理念出处引用。`[基线/对比]` 其监控靠专用硬件 IP，本项目靠软件读 CSR/计数器，成本更低、可移植性更强。
- **code**：https://gitlab.bsc.es/selene
- **paper/项目结果**：https://cordis.europa.eu/project/id/871467/results

#### E4. De-RISC（H2020 EIC-FTI 869945）
- **名称 / 机构**：De-RISC — Dependable Real-time Infrastructure for Safety-critical Computer / fentISS、Cobham Gaisler、BSC、Thales R&T
- **venue+year**：EU H2020 FTI（2019–2022），DASIA/SEFUW 系列报告
- **是什么**：把 Gaisler 多核 RISC-V SoC 产品化，并把 XtratuM/XNG 时空隔离 hypervisor 移植到 NOEL-V，形成完整空间计算平台（硬件+hypervisor）。
- **与本项目关系**：`[平台正当性]` 证明 RISC-V 在空间域已有完整混合关键性（mixed-criticality）软件栈。本项目的 criticality-driven 选择性保护与 hypervisor 分区隔离是互补层次：分区给空间隔离，本框架给数据流冗余。
- **code**：无完全开源（XNG 商业）；NOEL-V 在 GRLIB
- **paper/项目**：https://derisc-project.eu

#### E5. GR765（ESA 下一代八核）
- **名称 / 机构**：GR765 SoC / Frontgrade Gaisler（ESA 多份合同）
- **venue+year**：开发中（2026 年状态：开发阶段，产品页公开规格；RISC-V Summit Europe 2026 "RISC-V in Space" 专场有介绍）
- **是什么**：抗辐射容错八核微处理器，**启动时可选 LEON5FT（SPARC V8）或 NOEL-V（RV64GCH）**；每核自带 Packed SIMD 扩展、FPU/MMU、32 KiB L1；4 MiB L2；最高 800 MHz / 2600 DMIPS per core；内嵌 32k LUT eFPGA；SpaceWire/CAN FD/MIL-STD-1553B/SpaceFibre/PCIe Gen3；后量子安全启动；官方描述"advanced fault tolerance features, **no need for lock-step processing**"。
- **与本项目关系**：`[平台正当性]` 这是"RISC-V 将成为欧洲空间主力 CPU"的最强前瞻依据，也是本项目目标硬件画像（RV64 + SIMD/向量 + 硬件 FT + 多核）。`[未来工作]` 论文可写"框架可直接部署于 GR765 的 NOEL-V 模式，利用其 FT 计数器驱动模式切换"。
- **code**：N/A（商业 IP）
- **paper/doc**：https://www.gaisler.com/products/gr765 ；行业背景报道 https://www.eetimes.com/space-industry-is-standardizing-on-risc-v

#### E6. GR740（LEON4FT 四核，在轨主力）
- **名称 / 机构**：GR740 / Frontgrade Gaisler，ESA NGMP
- **venue+year**：已量产；2026-02 被选为 ESA **RAMSES** 任务单板机处理器
- **是什么**：四核 LEON4FT（SPARC V8），250 MHz，ECC + FT cache/寄存器文件，空间抗辐射。NASA HPSC 文档把它当作 **SpaceBench 的 1.0 基线**。
- **与本项目关系**：`[基线/对比]` 当论文需要说明"空天平台算力有多稀缺、因此冗余开销必须可调"时，GR740 的 250 MHz / 1700 DMIPS 是最常被引用的量化参照（HPSC 对比表中 GR740=1.0）。
- **code**：N/A
- **paper/doc**：https://www.gaisler.com/products/gr740

#### E7. Ibex + Dual-Core Lockstep（DCLS）与寄存器文件 ECC
- **名称 / 机构**：Ibex / lowRISC（起源于 PULP 的 zero-riscy），用于 OpenTitan RoT
- **venue+year**：开源持续维护；RISC-V Summit Europe 2023 介绍（G. Chadwick）；形式化故障安全分析见 **IACR ePrint 2024/247**（Fault-Resistant Partitioning of Secure CPUs）
- **是什么**：小型可配置 RV32IMCB 核，提供**影子核锁步（DCLS）**、寄存器文件 ECC/EDC、ICache 完整性与地址染色、double-fault 本地/全局升级告警。OpenTitan 的设计文档逐条列出了这些对抗机制（`RV_CORE_IBEX.LOGIC.SHADOW`、`DATA_REG_SW.INTEGRITY` 等）。
- **与本项目关系**：`[基线/对比]` 工业级"硬件检测 + 告警升级"的参考实现；其 **alert/escalation 两级响应**与本项目 LOW/HIGH 两级保护在概念上同构，可作为"分级响应"设计的工程先例。`[未来工作]` 若做 RTL 侧注入实验，Ibex 是最易上手的 FT 核。
- **code**：https://github.com/lowRISC/ibex
- **paper/doc**：https://lowrisc.org/ibex ；https://opentitan.org/book/hw/top_earlgrey/ip_autogen/rv_core_ibex/doc/interfaces.html ；https://eprint.iacr.org/2024/247.pdf

#### E8. Klessydra-fT03：IMT RISC-V 核上的动态 DMR→TMR
- **名称 / 作者**：Barbirotta, Cheikh, Mastrandrea, Menichelli, Ottavi, Olivieri / Sapienza University of Rome（DSP Lab）
- **venue+year**：**IEEE DFT 2021**（"A Fault Tolerant soft-core obtained from an Interleaved-Multi-Threading RISC-V microprocessor design"）；扩展版 **JLPEA 13(1):2, 2022**（"Evaluation of Dynamic Triple Modular Redundancy in an Interleaved-Multi-Threading RISC-V Core"）
- **是什么**：利用交错多线程（IMT）流水线中天然存在的硬件线程做冗余：**平时跑 DMR，检测到失配时动态切换到 TMR**。RTL 级 SEU 注入显示 FFT 失效率从 62% 降到 13%，FIR 从 60% 降到 10%。
- **与本项目关系**：**本章与本项目最相关的一篇**。`[基线/对比]` 它是"运行时在 DMR/TMR 之间切换"的硬件版；本项目是编译器+运行时的软件版，且触发信号来自 ECC 错误率而非失配本身。答辩时可明确定位差异：(1) 它的切换粒度是线程/硬件，本项目是函数/变量级 criticality；(2) 它的触发是"已发生失配"（事后），本项目是"错误率上升"（前瞻）；(3) 它需改微架构，本项目零硬件修改。
- **code**：https://github.com/klessydra/pulpino-klessydra （Klessydra 核族，PULPino 兼容）
- **paper**：https://www.mdpi.com/2079-9268/13/1/2 ；DOI 10.1109/DFT52944.2021.9568297

#### E9. SHAKTI-F / IRIS（印度 ISRO 空间控制器）
- **名称 / 作者**：Gupta, Gala, Madhusudan, Kamakoti / IIT Madras RISE group；IRIS 为 IIT Madras + ISRO IISU 合作
- **venue+year**：Shakti-F: A Fault Tolerant Microprocessor Architecture, **IEEE ATS 2015**, pp. 163–168（DOI 10.1109/ATS.2015.35）；IRIS 芯片 2025-02 成功启动
- **是什么**：SHAKTI 开源 RISC-V 家族的容错变体（F-class，面向 ISO 26262/航天）；IRIS = Indigenous RISC-V Controller for Space Applications，64 位，SHAKTI 基线 + **容错内部存储器**、CORDIC、看门狗、多启动模式。
- **与本项目关系**：`[平台正当性]` 非欧美路线的独立证据：开源 RISC-V 正被多国航天机构直接采用作为星载控制器基线；可用于论文引言的"趋势"段落。
- **code**：https://gitlab.com/shaktiproject/cores （BSD-3）
- **paper/新闻**：https://www.iitm.ac.in/happenings/press-releases-and-coverages/iit-madras-isro-jointly-developed-and-successfully-booted

#### E10. RT PolarFire SoC FPGA（Microchip）
- **名称 / 机构**：RT PolarFire SoC（RTPFS160ZT/RTPFS460ZT）/ Microchip Technology
- **venue+year**：发布 2024-05；2025-07 工程样片与 QML 扩展；ESA **SEFUW 2025**（第 6 届 Space FPGA Users Workshop）有专题报告
- **是什么**：业界首个基于 RISC-V 的抗辐射 SoC FPGA：1×E51（RV64IMAC）+ 4×U54（RV64GC）@625 MHz，2 MB 可配置 L2（cache/LIM/scratchpad），非 SRAM 配置工艺带来 **零配置位翻转**，QML Class Q 已获、Class V/Y 在路线图上。
- **与本项目关系**：`[平台正当性]` 本项目的 rv64gc 目标与 U54 的 RV64GC 完全对齐——可直接声明"编译产物可在真实空间级 RISC-V 硬件上运行"（注意：U54 无 V 扩展，向量路径需放在 HPSC/GR765/带 RVV 的核上讨论）。`[未来工作]` 这是最现实的"买得到的板子"落地目标，可写入未来工作的硬件验证计划。
- **code**：N/A（Libero SoC + Mi-V 生态）
- **paper/doc**：https://www.microchip.com/en-us/products/fpgas-and-plds/radiation-tolerant-fpgas/rt-polarfire-soc ；规格书 https://ww1.microchip.com/downloads/aemDocuments/documents/FPGA/ProductDocuments/ProductBrief/Radiation-Tolerant-PolarFire-SoC-FPGA-Product-Overview-DS00005395.pdf

#### E11. METASAT 平台（NOEL-V + SPARROW + Vortex GPU）
- **名称 / 机构**：METASAT（Horizon Europe）/ BSC、UPV、Gaisler 等
- **venue+year**：arXiv 2502.21027（2025），"A RISC-V Multicore and GPU SoC Platform with a Qualifiable Software Stack for Safety Critical Systems"
- **是什么**：面向未来空间系统的 RISC-V 原型：多核 NOEL-V + SPARROW AI 加速器 + Vortex RISC-V GPU，配可认证软件栈；论文同时报告 **QEMU 模拟与 SystemC/TLM 的精度/速度对比**（QEMU 对执行时间的估计"相当准确且足够快"，GPU 的 TLM 仿真慢到 90924 s）。
- **与本项目关系**：`[周期级证据路径]` **极其有用的引用**：一篇 2025 年空间域论文明确承认"用 QEMU 做时间评估是可接受的工程折中，而周期级/TLM 仿真代价高到不可行"。这正是为本项目"目前仅 user-mode QEMU 功能验证"做防守的权威依据。
- **code**：https://gitlab.bsc.es/metasat
- **paper**：https://arxiv.org/html/2502.21027v1

---

### E.2 RISC-V 向量扩展微架构与周期级仿真基础设施

> 本节直接服务于"如何把 rv64gcv 上的近似 TMR 开销从指令数变成真实周期数"。

#### E12. Ara / Ara2
- **名称 / 作者**：Cavalcante, Perotti, Benini 等 / ETH Zürich + Università di Bologna（PULP 平台）
- **venue+year**：Ara: **IEEE TVLSI 2020**（22nm FD-SOI, 1 GHz+）；Ara2: **IEEE Transactions on Computers 73(7), 2024**（"Exploring Single- and Multi-Core Vector Processing Acceleration"，DOI 见 computer.org/csdl）
- **是什么**：开源 RVV 1.0 向量单元（与 CVA6 标量核耦合），多 lane、多精度浮点、可综合 RTL，有完整面积/功耗/频率数据。
- **与本项目关系**：`[周期级证据路径]` 若要给 RVV 近似 TMR 一个有物理意义的开销模型，Ara2 的 lane 数/VRF 带宽/chaining 行为是最常被引用的参数来源；也可用 Verilator 跑 Ara 得到周期数。`[未来工作]` 向量寄存器文件是巨大 SEU 截面，"RVF 的 ECC + 软件近似表决"是自然的后续课题。
- **code**：https://github.com/pulp-platform/ara
- **paper**：https://www.computer.org/csdl/journal/tc/2024/07/10500752/1Warb1R886Y

#### E13. Vicuna / Vicuna 2.0
- **名称 / 作者**：Michael Platzer, Peter Puschner / TU Wien；Vicuna 2.0 由 Jones 等延续
- **venue+year**：**ECRTS 2021**, LIPIcs vol.196, 1:1–1:18（DOI 10.4230/LIPIcs.ECRTS.2021.1）；Vicuna 2.0 见 **RISC-V Summit Europe 2025** (P2.1.05)
- **是什么**：**时序可预测**的 RVV 协处理器（SystemVerilog，Zve32x 子集），与 Ibex/CV32E40X 经 OpenHW XIF 接口耦合，**无 timing anomaly**，可用 Verilator 直接仿真，支持 VREG_W / VLANE_W 等配置。论文明确指出 Ara 和 Hwacha 因动态仲裁/乱序写回而存在 timing anomaly，而 Vicuna 没有。
- **与本项目关系**：`[周期级证据路径]` **性价比最高的 cycle 级证据来源**：RTL + Verilator，无需 FPGA、无需 gem5 补丁，且时序可预测意味着"冗余副本的周期开销"可以被确定性地归因，非常适合论文中"LOW vs HIGH 模式的周期开销"对照实验。缺点：32 位、Zve32x 子集，与本项目 rv64gcv 不完全一致，需在论文中说明外推假设。
- **code**：https://github.com/vproc/vicuna
- **paper**：https://drops.dagstuhl.de/storage/00lipics/lipics-vol196-ecrts2021/LIPIcs.ECRTS.2021.1/LIPIcs.ECRTS.2021.1.pdf

#### E14. Spatz
- **名称 / 作者**：Perotti, Cavalcante, Zhang, Benini 等 / ETH Zürich（PULP）
- **venue+year**：**ICCAD 2022**（"Spatz: A Compact Vector Processing Unit for High-Performance and Energy-Efficient Shared-L1 Clusters"）；后续 TCAD/TC 扩展
- **是什么**：紧凑型 RVV 单元，面向共享 L1 的多核簇（Snitch 集群），强调能效与小面积。
- **与本项目关系**：`[未来工作]` 代表"星载 AI/DSP 加速用小向量单元"的方向；若框架未来覆盖向量簇上的选择性保护，Spatz 的 L1-共享结构会改变冗余副本的数据放置代价。
- **code**：https://github.com/pulp-platform/spatz
- **paper**：https://doi.org/10.1145/3508352.3549367

#### E15. Saturn Vector Unit
- **名称 / 作者**：Jerry Zhao, Daniel Grubb, Miles Rusch, Tianrui Wei, Kevin Anderson, Borivoje Nikolić, Krste Asanović / UC Berkeley
- **venue+year**：UCB/EECS-2024-215 技术报告（**2024-12**，微架构手册）；Chipyard 1.12+ 已集成
- **是什么**：参数化 Chisel RVV **1.0 完整 application-profile** 向量单元：V / Zve64d / Zve64x / Zvfh / Zvbb，Zvl64–1024 可配 VLEN，SIMD 数据路径宽度 64/128/256/512+，indexed/strided/segmented 访存、虚拟内存与精确陷阱、零死区 chaining。与 Rocket / Shuttle 核集成。
- **与本项目关系**：`[周期级证据路径]` **当前唯一"RVV 1.0 完整 + 可进 Chipyard/FireSim 流程"的开源向量单元**，是给 rv64gcv 近似 TMR 做周期级评测的最权威路径（Rocket+Saturn → Verilator 周期精确，或 FireSim → FPGA 加速周期精确）。精确陷阱的支持对"检测到失配后回滚/重算"的实现尤其重要。
- **code**：https://github.com/ucb-bar/saturn-vectors （BSD-3）
- **paper**：https://www2.eecs.berkeley.edu/Pubs/TechRpts/2024/EECS-2024-215.html ；手册 PDF https://www2.eecs.berkeley.edu/Pubs/TechRpts/2024/Archive/EECS-2024-215.pdf ；集成文档 https://chipyard.readthedocs.io/en/1.12.1/Generators/Saturn.html

#### E16. Chipyard / Rocket Chip / BOOM
- **名称 / 机构**：Chipyard（Amid et al., **IEEE Micro 2020**）、Rocket Chip（Asanović et al., UCB/EECS-2016-17）、BOOM（Celio et al., UCB/EECS-2015-167）
- **是什么**：RISC-V SoC 生成与评测的统一框架：Rocket（有序标量）、Shuttle（超标量）、BOOM（乱序）、Saturn（向量）、Verilator/VCS 周期精确仿真、FireSim 上板。
- **与本项目关系**：`[周期级证据路径]` 这是通往 cycle 级数据的"主干道"；论文的"未来工作/实验扩展计划"一节应明确写：`Chipyard(Rocket + Saturn) → Verilator` 作为第一步，`FireSim` 作为规模化第二步。
- **code**：https://github.com/ucb-bar/chipyard ；https://github.com/chipsalliance/rocket-chip ；https://github.com/riscv-boom/riscv-boom
- **paper**：https://ieeexplore.ieee.org/document/9099108

#### E17. FireSim
- **名称 / 作者**：Sagar Karandikar 等（Krste Asanović 组）/ UC Berkeley
- **venue+year**：**ISCA 2018**；入选 IEEE Micro Top Picks 2018 与 "ISCA@50 25-year Retrospective"
- **是什么**：FPGA 加速的 **cycle-exact** 全系统仿真平台，从可综合 RTL 自动生成模型，含 FASED DRAM 时序模型（FPGA 2019），支持云端（AWS F1）与本地 FPGA。
- **与本项目关系**：`[周期级证据路径]` 保真度最高、能跑完整 Linux + 真实 workload 的路径；代价是 FPGA 资源与数周的工程投入。论文可诚实表述为"在硕士论文周期内不可行，但是明确的后续验证路线"。
- **code**：https://github.com/firesim/firesim
- **paper**：https://fires.im/publications

#### E18. gem5 的 RVV 支持现状（2026）
- **名称 / 机构**：gem5 社区；RVV 分支主要来自 Cristóbal Ramírez Lazo 的 fork 与 PCLTLAB fork
- **venue+year**：gem5 自 **v23.1.0.0** 起 RVV 可在 SE 与 FS（OpenSBI+Linux）模式下运行；社区讨论 gem5 Discussion #523（最新追问到 2026-08 仍在确认 VLEN 限制）；gem5 官方 Architecture Support 页（最后编辑 2026-10-21）对 RISC-V 向量支持的描述仍很简略
- **是什么**：事件驱动的 CPU 时序模型（AtomicSimple/TimingSimple/MinorCPU/O3），非 RTL，速度快但微架构保真度依赖模型标定。
- **与本项目关系**：`[周期级证据路径]` 中等投入、中等保真的选项。**重要风险点（必须在论文中写明）**：社区长期存在 **VLEN > 512 bit 不支持**（因向量访存实现方式）的限制，且 O3 向量时序模型尚未被广泛标定/验证，因此 gem5 给出的 RVV 周期数更适合做**相对比较**（LOW 模式 vs HIGH 模式），而不适合当作绝对性能数。
- **code**：https://github.com/gem5/gem5 ；RVV fork https://github.com/PCLTLAB/gem5-rvv （参见 fprox 教程）
- **doc**：https://github.com/orgs/gem5/discussions/523 ；https://www.gem5.org/documentation/general_docs/architecture_support ；教程 https://fprox.substack.com/p/performance-simulation-of-risc-v-vector

#### E19. Spike / Dromajo / Banshee / QEMU plugin —— 功能级与"指令计数"工具带
- **名称 / 机构**：Spike（RISC-V International 官方 ISS）、Dromajo（Esperanto Technologies，RTL 协同仿真 checker）、Banshee（ETH Zürich，基于二进制翻译的 PULP 集群快速模拟器，**ICCAD 2021**）、QEMU TCG plugins（`libinsn.so` 等）
- **是什么**：全部是**功能级/指令级**工具。Spike 可做 ISA 黄金模型与简单 cycle 近似；Dromajo 常与 BOOM/Rocket RTL 做 lockstep co-simulation 验证；Banshee 对 PULP 集群能给出不错的性能估计；QEMU plugin 能统计执行指令数/访存数，但**没有流水线、cache、乱序与向量 chaining 模型**。
- **与本项目关系**：`[周期级证据路径]` 这是本项目当前所处的层级。诚实的表述方式是：**QEMU 指令计数 = 代码膨胀与动态指令开销的可靠度量；≠ 周期开销**。论文应把现有数据明确标注为"instruction-count overhead / 功能正确性"，并把周期级留给 E.7 的路线。
- **code**：https://github.com/riscv-software-src/riscv-isa-sim ；https://github.com/chipsalliance/dromajo ；https://github.com/pulp-platform/banshee ；https://www.qemu.org/docs/master/devel/tcg-plugins.html
- **paper**：Banshee, ICCAD 2021, DOI 10.1109/ICCAD51958.2021.9643546

---

### E.3 RISC-V 可靠性机制：错误上报、RAS 规范与选择性加固研究

> 本节是本项目"运行时信号来源"的合法性依据：LOW/HIGH 切换由 ECC 错误率驱动，必须说明这个信号在 RISC-V 上**不是虚构的**。

#### E20. RISC-V RAS Error-Record Register Interface（RERI）
- **名称 / 机构**：RISC-V International，RERI Task Group（主要推动者 Greg Favor, Ventana Micro Systems）
- **venue+year**：**2024 年批准（ratified）**，v1.0；收入 RISC-V Ratified Specifications Library
- **是什么**：标准化的 RAS 错误记录寄存器接口：内存映射的 error bank（每 bank 最多 63 条 error record），定义**错误严重性分级、标准错误码、已纠正错误计数（corrected error counting）、错误记录的优先覆盖规则、以及用于测试 RAS handler 的错误注入**。明确覆盖从服务器到嵌入式，最小实现一个 bank/一条 record 仅占 128 字节地址空间。
- **与本项目关系**：**本项目最关键的规范级依据**。`[平台正当性]` "corrected error counting" + "错误严重性分级" 恰好就是本框架 LOW/HIGH 模式切换所需的输入：可纠正错误（CE）速率上升 → 升到 HIGH；而不可纠正错误（UE）→ 立即进入安全响应。论文应把运行时信号接口明确对齐到 RERI，而不是只写"读某个硬件计数器"，这会显著提升设计的可信度与可移植性。`[未来工作]` 在真实硬件上把 runtime 的信号采集层实现为 RERI 读取器。
- **code**：https://github.com/riscv-non-isa/riscv-ras-eri
- **paper/spec**：https://docs.riscv.org/reference/ras-eri/v1.0/reri_intro.html ；概览讲解 https://www.youtube.com/watch?v=jWdFGRanAN0

#### E21. mcause / 机器陷阱 CSR 与 HPM 计数器
- **名称 / 机构**：RISC-V Privileged Architecture（Volume II），RISC-V International
- **venue+year**：批准规范，持续修订
- **是什么**：`mcause` 编码陷阱原因（含 instruction/load/store access fault，可被实现用于报告不可纠正的存储错误）；`mhpmcounter3..31` + `mhpmevent*` 提供实现自定义的硬件性能事件计数（厂商可映射 ECC 纠错事件）；`mcounteren`/`mcountinhibit` 控制访问与开关。Smcntrpmf/Sscofpmf 提供计数器溢出中断（可用于"速率超阈值即中断"）。
- **与本项目关系**：`[平台正当性]` 支撑论文"错误率信号可由 mcause 陷阱 + HPM 计数器获得"的 ISA 级依据。**必须注意并在论文中声明的两点**：(1) 可纠正 ECC 事件在 RISC-V 中**不是**由 `mcause` 报告的（它不产生陷阱），而应走 RERI/厂商 MMIO 计数器；`mcause` 对应的是不可纠正/访问错误；(2) HPM 事件编码是 implementation-defined，因此框架应设计成可插拔的"信号后端"（QEMU 模拟后端 / RERI 后端 / 厂商计数器后端）。这一澄清本身就是论文可写的工程贡献点。
- **doc**：https://docs.riscv.org/reference/ （Privileged ISA）
- **code**：N/A

#### E22. AIA（Advanced Interrupt Architecture）与 ACLINT
- **名称 / 机构**：RISC-V International；ACLINT 由 RISC-V platform runtime TG 推动
- **venue+year**：AIA v1.0 已批准；ACLINT 规范（riscv/riscv-aclint）
- **是什么**：AIA 定义 IMSIC/APLIC，支持 MSI 式中断与虚拟化中断投递；ACLINT 定义 MTIMER/MSWI/SSWI，取代传统 CLINT。
- **与本项目关系**：`[未来工作]` 自适应框架需要"低延迟、可虚拟化的错误告警通道"：RERI 的错误信号最终要以中断形式送到 RAS handler，而在 AIA 平台上这意味着 MSI 投递与优先级配置。论文可在"系统集成"一节说明模式切换响应延迟由 AIA 中断路径决定。
- **code**：https://github.com/riscv/riscv-aia ；https://github.com/riscv/riscv-aclint
- **spec**：https://docs.riscv.org/reference/

#### E23. RISC-V 流水线隐藏寄存器故障注入与选择性加固
- **名称 / 作者**：Laurent, Beroulle, Deleuze, Bruguier 等（Rocket 隐藏寄存器）；另有多篇 CVA6 故障注入工作（Thales Silicon Security, Coulon；RISC-V Summit Europe 2025 的 CVA6 PMP FIA 研究）
- **venue+year**：**DATE 2019**（"Fault Injection on Hidden Registers in a RISC-V Rocket Processor"）；JAIF 2022（CVA6 威胁与对策全景）；RISC-V Summit Europe 2025 (P1.1.02)
- **是什么**：在 RTL 级注入故障，证明**流水线隐藏寄存器**（非架构可见的暂存器）是显著的脆弱点与泄漏面，并给出软件侧对策；CVA6 工作系统化列出"控制寄存器冗余、流水线完整性保护、cache 数据/代码完整性校验、寄存器文件冗余字段、CFI"等对策清单。
- **与本项目关系**：`[基线/对比]` **本项目威胁模型的边界说明**：软件层（LLVM pass + 运行时）无法保护架构不可见的隐藏寄存器与控制通路。诚实地引用这组工作，可以把"为什么仍需硬件 ECC/锁步、本框架只覆盖数据流与可见状态"写成一个有文献支撑的作用域声明，而非缺陷。
- **paper**：https://www.date-conference.com/proceedings-archive/2019/pdf/0384.pdf ；https://jaif.io/2022/media/JAIF2022_Coulon.pdf

#### E24. 流水线 RISC-V 核的可靠性分析与"优先加固控制路径"结论
- **名称 / 机构**：HARV-SoC 相关工作（LATS 2026 论文，Zenodo 公开）
- **venue+year**：**IEEE LATS 2026**（"Design and Reliability Analysis of a Pipeline RISC-V Processor Core"）
- **是什么**：对流水线 RISC-V 核做故障注入与脆弱性分析，结论是**应优先对流水线控制路径做选择性加固**，并推荐奇偶校验等轻量机制而非全面 TMR。
- **与本项目关系**：`[基线/对比]` 直接呼应本项目的 criticality-driven 思路：硬件界的研究结论同样是"选择性 > 全面冗余"。可作为"选择性保护是跨层共识"的论据。
- **paper**：https://zenodo.org/records/18703492/files/2026_LATS_HARV_SoC_Pipeline_HAL_Version.pdf

---

### E.4 空天计算工程实践：平台、基准与在轨经验

#### E25. NASA HPSC（High Performance Spaceflight Computing）
- **名称 / 机构**：NASA JPL + Microchip Technology（SiFive IP）
- **venue+year**：项目自 2021；CDR 2024；**tape-out 2025 年中**，同年首批芯片下线；**2026-02 首次点亮并发出 "Hello Universe"**；2026-03 起进入功耗/性能/可靠性/抗辐射测试；NASA 投入 $80M。参考文献：Powell, "NASA HPSC", cFE/cFS Symposium, 2025-01-29（NTRS 20250000356）；PESTO 2024（NTRS 20240012768）
- **是什么**：HPSC 1000RH：**8× SiFive X280（RV64 带向量，面向边缘 AI）+ 2× SiFive S21**（共 10 核，部分材料写 12 核含系统/安全控制器），1 GHz，多级 L3，240G TSN 以太网交换、10GbE、PCIe/CXL、RDMA；安全区（后量子算法）；**容错特性包括 DCLS（双核锁步）、Split-Mode、WorldGuard 分区**；向量性能 256 GFLOPs。对比表：SpaceBench 相对 GR740 加速 **1343×**，28130 DMIPS @1 GHz（GR740 = 1700 DMIPS @250 MHz）。目标形态 SpaceVPX S3C，由 Microchip 商业供货。
- **与本项目关系**：**本项目最强的平台正当性来源**。`[平台正当性]` (1) 未来十年 NASA 旗舰星载计算机就是 **RV64 + 向量**，与本项目 rv64gcv 目标完全一致；(2) HPSC 提供 **DCLS / Split-Mode / 分区** 这类"可配置的冗余模式"——这正是"运行时在性能模式与冗余模式之间切换"的硬件级印证，本项目的软件 LOW/HIGH 切换可定位为"在不支持或不愿牺牲一半算力做 DCLS 的场合提供细粒度替代"；(3) 1343× 算力跃升恰好带来"算力富余可用于按需冗余"的新设计空间，这是论文动机段的核心论据。
- **code**：N/A
- **paper/doc**：https://www.nasa.gov/game-changing-development-projects/high-performance-spaceflight-computing-hpsc ；https://ntrs.nasa.gov/api/citations/20250000356/downloads/Powell-cFS-2025-HPSC_for2025Jan29.pdf ；https://ntrs.nasa.gov/api/citations/20240012768/downloads/Powell-PESTO-2024-HPSC%2010032024a.pdf

#### E26. OBPMark / OBPMark-ML
- **名称 / 机构**：ESA + Barcelona Supercomputing Center（ESA GSP 资助，源自 GPU4S）
- **venue+year**：首发 2021-06（v0.1），当前 **v0.3.1（2023-07）**，仍为 "Public Beta"；规范见 Zenodo record 5638577
- **是什么**：**空间域唯一开放、权威的星载处理基准套件**。类别：1) 图像处理流水线（校正标定、雷达成像）；2) 标准压缩（CCSDS 121.0 / 122.0 / 123.0 高光谱）；3) 加密（AES）；4) 信号处理构件（FIR、FFT）；5) ML 推理（云检测 cloud screening、船舶检测 ship detection，提供浮点与量化模型）。实现有 C（顺序）、OpenMP、OpenCL、CUDA。OBPMark-ML 独立仓库。
- **与本项目关系**：**本项目实验设计的首选 benchmark**。`[平台正当性]` 用 OBPMark 而非 SPEC/PolyBench，可直接声明"评测负载是 ESA 定义的真实星载工作负载"，极大提升论文说服力。`[基线/对比]` 其 ML 推理与图像压缩 kernel 天然具备"输出可容忍微小偏差"的性质，是**近似 TMR 阈值表决**最合理的实验对象——阈值表决的容差可以用 benchmark 自带的 verification data 与质量指标来量化。强烈建议把 1.1 图像校正、2.2 CCSDS 122.0、OBPMark-ML 云检测三者作为主实验集。
- **code**：https://github.com/OBPMark/OBPMark ；https://github.com/OBPMark ；ML: https://gitlab.bsc.es/obpmark/obpmark-ml
- **paper/spec**：https://obpmark.github.io ；https://zenodo.org/records/5638577

#### E27. NASA SpaceCube（v3.0 / v3.0 Mini / Mini-Z）与 SpaceCube AI 协处理器
- **名称 / 机构**：NASA GSFC Science Data Processing Branch（Geist, Brewer, Crum, Petrick, Wilson 等）
- **venue+year**：SpaceCube v3.0 系列（2019 起）；SpaceCube AI 协处理器与 **STP-H9-SCENIC（ISS 实验）**，2023
- **是什么**：NASA 的"混合处理"路线：抗辐射器件 + COTS（Kintex UltraScale FPGA、Zynq SoC）组合，配软件/硬件容错缓解，在 1U CubeSat 尺寸内提供 10×–100× 算力提升；v3.0 Mini 是 GSFC MARES CubeSat 总线的高性能处理器；明确支持部分重构（Partial Reconfiguration）。
- **与本项目关系**：`[平台正当性]` 这是"用 COTS 换算力、用容错软件补可靠性"这一整体技术路线的官方背书——与本项目"不改硬件、用编译器+运行时提供可调冗余"的立场完全一致。`[基线/对比]` SpaceCube 的缓解主要在 FPGA 层（TMR + scrubbing + PR），本项目在处理器软件层，可形成互补叙事。
- **doc**：https://ntrs.nasa.gov/api/citations/20200000976/downloads/20200000976.pdf ；https://ntrs.nasa.gov/api/citations/20190028775/downloads/20190028775.pdf ；https://technology.nasa.gov/patent/GSC-TOPS-241

#### E28. RAD750 / RAD5545 与在轨历史平台（含 Mars 2020 / Ingenuity）
- **名称 / 机构**：BAE Systems（RAD750、RAD5500 系列）；NASA JPL（Perseverance 使用 RAD750；Ingenuity 旋翼机使用 **Qualcomm Snapdragon 801 COTS + Linux**，软件容错为主）
- **venue+year**：RAD750 约 2001 起在轨；RAD5545 约 2015 起
- **是什么**：传统抗辐射单核/四核 PowerPC。HPSC 对比表给出量化画像：RAD750 200 MHz / 400 DMIPS / **无硬件向量引擎**；RAD5545 466 MHz / 4 核 / 5592 DMIPS。Ingenuity 则是另一极端：用消费级 COTS SoC 在火星飞行 70+ 次，靠软件冗余与可重启策略兜底。
- **与本项目关系**：`[平台正当性]` 两段对比构成论文引言的经典张力：一边是算力极度受限的 rad-hard 器件（因此恒定 TMR 不可承受），一边是 Ingenuity 证明"COTS + 软件容错"在真实深空任务中可行。本项目正位于二者之间：COTS/高性能 RISC-V + 可调软件冗余。
- **doc**：RAD750 https://www.baesystems.com/en-us/product/radiation-hardened-electronics ；HPSC 对比表见 E25 的 NTRS 链接

#### E29. SpaceVPX（VITA 78）与星载计算机标准形态
- **名称 / 机构**：VITA Standards Organization（VITA 78 / SpaceVPX、SpaceVPX Lite VITA 78.1）
- **venue+year**：VITA 78 发布 2015，持续修订；HPSC 路线图明确采用 **SpaceVPX S3C form factor**
- **是什么**：面向空间的 OpenVPX 衍生标准，定义带冗余（dual-redundant）供电、系统管理（SpaceUM）、背板拓扑，使"单点故障可切换到备份模块"。
- **与本项目关系**：`[平台正当性]` 说明空天系统的容错是**分层的**：板级/模块级冗余由 SpaceVPX 提供，芯片级由 ECC/锁步提供，**任务/数据流级的可调冗余正是本项目填补的层次**。这个"三层分工图"是答辩中定位贡献的好工具。
- **doc**：https://www.vita.com/SpaceVPX

#### E30. 星载 AI 与 COTS 可靠性现状综述
- **名称 / 作者**：G. Furano, G. Meoni, A. Dunne, D. Moloney, V. Ferlet-Cavrois, A. Tavoularis, M. Psarakis 等（ESA ESTEC + 合作方）
- **venue+year**：**IEEE Aerospace and Electronic Systems Magazine 35(12):44–56, 2020**（"Towards the Use of Artificial Intelligence on the Edge in Space Systems: Challenges and Opportunities"）；另有 Furano et al., **DFT 2020**（"AI in Space: Applications Examples and Challenges"）；在轨 COTS AI 架构的失效路径分析见 IJASS 2026（DOI 10.1007/s42405-026-01130-w）
- **是什么**：ESA 官方视角的星载边缘 AI 可靠性综述：COTS 加速器的辐射敏感性、缺乏器件级鉴定、需要软件/系统级缓解；并指出神经网络推理对部分误差有内在容忍度。
- **与本项目关系**：`[平台正当性]` 近似容错（approximate TMR / 阈值表决）在空间 AI 负载上的**权威动机引用**：ESA 自己承认 AI 负载可容忍部分数值偏差且必须用系统级手段缓解。这几乎是为本项目"阈值表决而非位精确表决"量身定制的论据。
- **paper**：https://doi.org/10.1109/MAES.2020.3008468 ；DFT 2020 DOI 10.1109/DFT50435.2020.9250908

#### E31. NASA cFS（core Flight System）
- **名称 / 机构**：NASA GSFC
- **venue+year**：开源（Apache 2.0），cFE/cFS Symposium 年会；HPSC 的软件栈演示即在 cFS 社区发布（E25 的 2025 文献）
- **是什么**：可重用的星载飞行软件框架：消息总线（SB）、事件服务（EVS）、表服务、健康与安全（HS）应用、看门狗与故障响应。
- **与本项目关系**：`[未来工作]` 本框架的运行时需要一个"错误率上报 + 模式决策 + 日志"宿主；cFS 的 HS/EVS 正是真实任务中承担这件事的组件。论文可写"将 runtime 的模式控制器封装为 cFS 应用"作为工程化路径，这比停留在裸机 runtime 更有说服力。
- **code**：https://github.com/nasa/cFS
- **doc**：https://cfs.gsfc.nasa.gov

---

### E.5 面向空间的编译器/软件容错工具链（直接同行）

#### E32. BYU COAST（COmpiler-Assisted Software fault Tolerance）
- **名称 / 作者**：Matthew Bohman, Benjamin James, Michael J. Wirthlin, Jeffrey Goeders（BYU, NSF SHREC）+ Heather Quinn（LANL）
- **venue+year**：**IEEE TNS 66(1):223–232, Jan 2019**（"Microcontroller Compiler-Assisted Software Fault Tolerance"，NSREC 发表）；"Applying Compiler-Automated SWFT to Multiple Processor Platforms"（2019）；James et al. 2021（RTOS 容错）
- **是什么**：**开源 LLVM IR 级 DMR/TMR 自动插入工具**，支持按函数/数据选择保护范围（`_DEFAULT_*`/`_TMR` 注解风格）。实验：MSP430 上 MWTF 提升 4–7×；默认规则集报告 95% 故障覆盖，作者注入实验中观测到平均 98.9% 覆盖率；中子束与重离子束实测验证。
- **与本项目关系**：**本项目最直接的同行与基线**。`[基线/对比]` COAST = LLVM + 选择性 + 位精确表决 + **静态固定保护级别**。本项目的增量必须精确表述为：(1) 阈值表决的**近似**TMR（COAST 是位精确）；(2) **运行时 LOW/HIGH 切换**（COAST 编译期定死）；(3) criticality 自动推导而非手工注解；(4) RISC-V rv64gcv 目标（COAST 主要在 MSP430/ARM/LEON3）。必须在相关工作章对标，并尽量在相同 benchmark 上给出开销对比。
- **code**：https://github.com/byuccl/coast
- **paper**：https://space.pitt.edu/sites/default/files/2024-10/Microcontroller-Compiler-Assisted-Software-Fault-Tolerance.pdf ；https://ccl.byu.edu/assets/Applying_Compiler-Automated_Software_Fault_Tolerance_to_Multiple_Processor_Platforms_2019.pdf

#### E33. ASPIS（Automatic Software-based Protection and Integrity Suite）
- **名称 / 作者**：Davide Baroffio, Federico Reghenzani, William Fornaciari / **Politecnico di Milano, HEAP Lab**（注意：常被误记为 Torino，实为 Milano）
- **venue+year**：**ACM TODAES 2024**（"Enhanced Compiler Technology for Software-based Hardware Fault Detection"，DOI 10.1145/3660524）；前作 **ACM Computing Frontiers 2023**（"Compiler-Injected SIHFT for Embedded Operating Systems", pp. 337–343, DOI 10.1145/3587135.3589944）；**DAC 2024 Late-Breaking Results**（质子束实测，Corigliano & Baroffio）；**ASP-DAC 2026**（"Quantifying Compiler-induced Reliability Loss in SIHFT"）
- **是什么**：LLVM 16.0.6 **out-of-tree 插件**形式的 SIHFT 工具链：EDDI/FDSC 等数据流与控制流保护，并带多种开销削减技术；作者自述是**当前唯一仍在活跃维护的 IR 级 SIHFT 编译工具**，语言与架构无关（只要 LLVM 前后端支持）。ASP-DAC 2026 那篇还量化了**编译器优化本身会削弱 SIHFT 保护**的问题。
- **与本项目关系**：**必须对标的最新同行**。`[基线/对比]` 架构无关的 IR 级实现正是本项目选择 LLVM pass 的技术路线背书；其 DAC 2024 质子束实验是"软件容错在真实辐射下有效"的最新证据。`[重要风险提示]` ASP-DAC 2026 关于"编译器优化导致可靠性损失"的结论直接影响本项目：**必须在论文中说明 pass 的插入位置与 -O2/-O3 的相对顺序，并讨论冗余副本被 CSE/GVN 合并消除的风险及对策**（如 `optnone`、volatile、opaque barrier、pass 顺序锚定）。这是审阅者极可能追问的点。
- **code**：https://github.com/HEAPLab/ASPIS
- **paper**：https://heaplab.github.io/ASPIS ；https://core.ac.uk/download/pdf/614054742.pdf ；ASP-DAC 2026 预印本 https://mediatum.ub.tum.de/doc/1836695/pp6qy0y8uhnyf7z0srwwm4m2g.baroffiogeier_aspdac26_preprint.pdf

#### E34. 经典 SIHFT 技术谱系：SWIFT / EDDI / CFCSS / nZDC / COMPAS / VAR3
- **名称 / 作者**：SWIFT（Reis, Chang, Vachharajani, Rangan, August, **CGO 2005**）；EDDI（Oh, Shirvani, McCluskey, **IEEE TR 2002**）；CFCSS（Oh et al., **IEEE TR 2002**）；nZDC（Didehban & Shrivastava, **DAC 2016**）；COMPAS（2022）；VAR3 规则集（Chielle et al.）
- **是什么**：软件实现硬件容错的方法学基石：指令复制 + 校验点（EDDI/SWIFT）、签名式控制流检查（CFCSS）、零数据损坏复制（nZDC）、数据流保护规则集分类（Chielle 的 G/D/C 规则，COAST 默认采用 VAR3）。
- **与本项目关系**：`[基线/对比]` 本项目的"选择性 + 近似 + 自适应"三要素需要在这个谱系中定位；特别是 Chielle 的规则集分类法可直接用来描述本项目 LOW 与 HIGH 模式各自启用了哪些检查规则，使"模式"有严格定义而非含糊的"保护强弱"。
- **paper**：SWIFT DOI 10.1109/CGO.2005.34 ；nZDC DOI 10.1145/2897937.2898086

---

### E.6 动态/自适应辐射感知重构（与本项目自适应机制最同源）

#### E35. 动态 TMR（Klessydra 系列，见 E8 的自适应角度）
- 已在 E8 详述。此处强调其**自适应判据**：以 DMR 失配为触发，运行时升级到 TMR——"按需升级冗余"的硬件先例。`[基线/对比]`

#### E36. TMR-MER：基于动态部分重构的 TMR 模块化错误恢复（含在轨实验）
- **名称 / 作者**：Nguyen, Diessel 等 / UNSW；在轨实验载荷 **RUSH**（Rapid Upset in Space Hardware，与 UNSW/澳方 CubeSat 合作）
- **venue+year**：博士论文与系列论文（FPL/FPT/TRETS 等）；RUSH 在轨实验设计用于对比 **TMR-MER vs TMR + Xilinx SEM 控制器 scrubbing**
- **是什么**：TMR 结合 FPGA 动态部分重构，按模块粒度修复配置存储器错误并重新同步表决模块；对照组用 Xilinx SEM IP 做持续扫描/擦洗。文中还批评 HWICAP 资源开销大、SEM 控制器不支持用户自定义/选择性擦洗。
- **与本项目关系**：`[基线/对比]` FPGA 界"运行时调整冗余与修复策略"的代表，并且有**真实在轨对照实验**——这是极好的方法论参照（如何设计 A/B 对照来证明自适应策略优于固定策略）。`[未来工作]` 若本项目扩展到 FPGA/SoC 混合平台，TMR-MER 是直接对接点。
- **paper**：https://www.cse.unsw.edu.au/~odiessel/papers/phdnguyen.pdf

#### E37. AMD/Xilinx SEM IP 与 TMR SEM IP
- **名称 / 机构**：AMD（Xilinx）Soft Error Mitigation (SEM) IP；TMR SEM IP（Versal/UltraScale 的三模冗余版本）；相关应用笔记 XAPP538（"Soft error mitigation using prioritized essential bits"）
- **是什么**：通过 ICAP 持续扫描配置 RAM，检测并纠正配置位翻转；可经 AXI-Lite 或 UART 与片内处理器交互，使软件能**读取 SEU 事件与纠错统计**；XAPP538 提出**按"essential bits"优先级**做差异化缓解。
- **与本项目关系**：`[平台正当性]` 两点直接可引用：(1) 工业界早已把"SEU 事件计数通过 AXI 暴露给处理器"作为标准做法——证明本项目"运行时读取错误率信号"在真实系统中是可实现的；(2) XAPP538 的 **prioritized essential bits** 是"按重要性差异化保护"的工业先例，与本项目的 criticality-driven 选择性保护同构，可作为思想源头之一引用。
- **doc**：https://www.xilinx.com/products/intellectual-property/sem.html ；XAPP538；介绍文 https://www.adiuvoengineering.com/post/microzed-chronicles-tmr-sem-ip

#### E38. 辐射环境的时变性：SAA 穿越与太阳事件（自适应的物理依据）
- **名称 / 机构**：ESA SPENVIS / ECSS-E-ST-10-04（空间环境标准）；AP8/AP9-AE9 辐射带模型；NOAA/SWPC 太阳质子事件通报
- **venue+year**：ECSS-E-ST-10-04C（空间环境），持续修订；AE9/AP9 由 AFRL 维护
- **是什么**：量化低地轨道辐射环境的强时变性：穿越南大西洋异常区（SAA）期间质子通量可比轨道平均高 2–3 个数量级；太阳高能粒子事件（SPE）期间 SEU 率可短时上升数量级。
- **与本项目关系**：**这是"为什么必须自适应"的物理学依据，答辩必备**。`[平台正当性]` 固定保护级别要么在 SAA/SPE 期间不足，要么在安静期浪费数倍算力；本项目的 LOW/HIGH 切换正是对这一时变性的直接响应。论文动机段应给出 SAA 穿越占轨道时间比例与通量倍数的具体数字，并用 SPENVIS/AE9-AP9 做引用来源。
- **tool**：https://www.spenvis.oma.be ；https://ecss.nl

---

### E.7 如何给 RISC-V 补上 cycle 级证据：可选路径与代价对比

**问题定义**：当前只有 user-mode `qemu-riscv64` 的功能验证。QEMU 使用 TCG 动态二进制翻译，**没有流水线、cache、分支预测、向量 chaining 模型**，因此它能给出的是"动态指令数 / 访存次数 / 代码膨胀"，不是周期数。答辩时的正确姿态是：**明确区分已有证据的类型，并给出一条可执行的升级路线**，而不是把指令数包装成性能数。

#### 路径对比表

| 路径 | 保真度 | 工程投入（含学习成本） | RVV 1.0 支持 | 能跑 Linux/完整负载 | 主要风险 | 适合本论文的定位 |
|---|---|---|---|---|---|---|
| **P0. QEMU + TCG plugin 指令计数**（现状） | 功能级，无时序 | 已完成（1–2 天） | 有（功能正确） | 是（user-mode 为 syscall 转发） | 把指令数当周期数会被直接质疑 | 现有章节：**正确性 + 指令级开销**，不声称性能 |
| **P1. 静态/分析型性能模型** | 低–中（可解释） | 1–2 周 | 手工建模 | N/A | 参数来源需引用（Ara2/Saturn/X280） | 用 CPI 与向量吞吐参数把指令数折算成**周期估计区间**，并做敏感性分析 |
| **P2. Spike + Dromajo 协同** | 功能级（可做 RTL lockstep checker） | 1–2 周 | Spike 有 | 是 | 本身不产出可信周期数 | 做黄金模型与故障注入对照，不作性能证据 |
| **P3. gem5（RVV fork / v23.1+）** | 中（事件驱动，需标定） | 2–4 周 | 有，但 **VLEN ≤ 512 bit** 限制，O3 向量时序未充分验证 | 是（FS: OpenSBI+Linux） | 绝对值不可信，社区支持零散 | **性价比最高的"相对周期开销"来源**：同一配置下比较 LOW/HIGH/基线 |
| **P4. Vicuna + Ibex/CV32E40X，Verilator** | 高（RTL，时序可预测、无 timing anomaly） | 3–5 周 | **Zve32x 子集，32 位** | 否（裸机） | 与 rv64gcv 不一致，需说明外推 | 给**向量冗余的周期开销**一个确定性、可归因的 RTL 证据 |
| **P5. Chipyard（Rocket/Shuttle + Saturn），Verilator/VCS** | 高（RTL 周期精确） | 4–8 周 | **RVV 1.0 完整**（V/Zve64d/Zvfh/Zvbb，VLEN 可配，精确陷阱） | 裸机 + 可跑 Linux（较慢） | Chisel/Chipyard 环境与仿真速度 | **最权威的学术路径**；与本项目 rv64gcv 目标一致 |
| **P6. Ara/Ara2 RTL（Verilator）** | 高 | 4–8 周 | RVV 1.0（与 CVA6 耦合） | 裸机 | 存在 timing anomaly，归因较难 | 面积/功耗/频率数据引用 + 可选周期仿真 |
| **P7. FireSim（FPGA 加速，cycle-exact）** | 最高（全系统 + FASED DRAM 模型） | 2–3 个月 + FPGA/云成本 | 随 Chipyard（含 Saturn） | 是（完整 Linux + 真实负载） | 硬件/云资源、调试链路长 | 写入未来工作；若有 FPGA 资源则是最终答案 |
| **P8. 真实硬件实测**（RT PolarFire SoC / VisionFive2 等带 RVV 的 SBC） | 真实（但无法控制变量） | 板卡采购 + 2–4 周 | 取决于芯片（U54 **无** V 扩展；部分国产/商用 SBC 有 RVV 1.0） | 是 | 不是空间级器件，且无法注入辐射 | 用 `rdcycle`/`rdinstret`/HPM 做真机开销实测，补齐"非仿真"证据 |

#### 推荐组合（按硕士论文周期的现实约束）

1. **保留 P0 并严格重命名指标**：把现有数据标注为"动态指令数开销 / 代码尺寸开销 / 功能正确性与注入检出率"，在表格与正文中不使用"性能/周期/加速比"字样。这一步零成本，但能消除最大的质疑点。
2. **做 P1**：用 Ara2 / Saturn 手册 / SiFive X280 的公开参数（lane 数、VRF 带宽、向量吞吐、cache 延迟）把指令数折算为**周期估计区间**，并对关键参数做 ±50% 敏感性分析。这把"无周期数"转化为"有区间与误差分析的估计"，学术上完全可接受。
3. **做 P3 或 P4 之一（二选一，取决于时间）**：
   - 若重点是 **rv64gcv 的向量路径** → 优先 **P3 gem5**（能直接跑 rv64gcv 二进制，不必重写代码），但只报告相对比值。
   - 若重点是 **周期开销的可信归因** → 优先 **P4 Vicuna**（RTL + 时序可预测），代价是需把 kernel 降级到 32 位 Zve32x 并说明外推。
4. **把 P5/P7 明确写进未来工作**，并给出具体配置（例：`Rocket + Saturn, VLEN=512, DLEN=256, Verilator` → `FireSim on U250`），显示你清楚这条路怎么走。
5. **可选的高性价比加分项 P8**：在任意带 RVV 1.0 的真机上用 `rdcycle`/`rdinstret` 读周期，虽非空间级器件，但"真实硬件上的周期开销"这一句话在答辩中价值很高。

#### 可直接用于答辩的表述模板

> 本文的 RISC-V 侧结果定位为**功能正确性与指令级开销**，取得方式是 user-mode QEMU。周期级结果不在本文范围内，原因有二：其一，周期精确评测需要 RTL 或 FPGA 级基础设施（Chipyard+Saturn / FireSim），其工程量与本文的编译器-运行时贡献不成比例；其二，同期空间领域工作（METASAT, arXiv 2502.21027, 2025）也明确采用 QEMU 做执行时间评估，并报告 SystemC/TLM 周期级仿真慢至不可用。本文已基于 Ara2 与 Saturn 的公开微架构参数给出周期开销的估计区间与敏感性分析，并将 `Rocket + Saturn` 的周期精确评测列为明确的后续工作。

---

### E.8 真实空天平台与标准：答辩时可引用的权威依据

#### 平台类（证明"RISC-V 是空天计算的现实目标"）

| 依据 | 发布方 | 时间/状态 | 一句话用法 |
|---|---|---|---|
| **NASA HPSC 1000RH**（8×SiFive X280 RV64+向量，DCLS/Split-Mode/WorldGuard，SpaceBench 相对 GR740 1343×） | NASA JPL + Microchip | 2025 tape-out；2026-02 首次点亮"Hello Universe"；2026 在测 | "NASA 下一代星载计算机就是 RV64 + 向量 + 可配置冗余模式，本文目标平台与之一致" |
| **GR765**（LEON5FT/NOEL-V 可选，八核，800 MHz，eFPGA，官方称无需锁步的 FT） | Frontgrade Gaisler / ESA | 开发中（2026） | "欧洲下一代空间 CPU 同样走 RISC-V，且强调可配置容错而非恒定锁步" |
| **NOEL-V + GRLIB（FT 可在所有配置启用）** | Frontgrade Gaisler | 量产 IP，GRIP 2025.2 | "硬件已提供 ECC/检测与错误报告，软件层的职责是可调冗余" |
| **RT PolarFire SoC**（1×E51 + 4×U54 RV64GC @625 MHz，零配置位翻转，QML-Q） | Microchip | 2024 发布；2025 工程样片 | "买得到的空间级 RISC-V 硬件，本文产物可直接部署" |
| **GR740**（4×LEON4FT，SpaceBench 基线 1.0） | Frontgrade Gaisler / ESA | 在轨；2026-02 入选 RAMSES | "星载算力基线，用于说明恒定 TMR 的不可承受性" |
| **IRIS（SHAKTI 基线，容错内存）** | IIT Madras + ISRO IISU | 2025-02 成功启动 | "开源 RISC-V 被多国航天机构直接采用" |
| **SpaceCube v3.0 / STP-H9-SCENIC** | NASA GSFC | 2019–2023，ISS 在轨 | "NASA 官方的 COTS+容错混合路线背书" |
| **SpaceVPX（VITA 78）** | VITA | 2015 起；HPSC 采用 S3C | "板级冗余已有标准，任务级可调冗余是待填补的层次" |

#### 基准与软件栈类（证明"实验负载与工程落地是真实的"）

| 依据 | 发布方 | 时间/状态 | 一句话用法 |
|---|---|---|---|
| **OBPMark v0.3.1 / OBPMark-ML** | ESA + BSC（ESA GSP 资助） | 2021 起，当前 Public Beta v0.3.1 (2023-07) | "评测负载由 ESA 定义：图像校正、CCSDS 121/122/123 压缩、FFT/FIR、云检测与船舶检测 ML 推理" |
| **CCSDS 121.0 / 122.0 / 123.0** | CCSDS | 蓝皮书标准 | "压缩 kernel 的输出容差有标准可依，支撑阈值表决的容差设定" |
| **NASA cFS** | NASA GSFC | 开源，Apache 2.0 | "运行时模式控制器可封装为 cFS 应用，含 HS/EVS 故障响应通路" |
| **RISC-V RERI v1.0** | RISC-V International | **2024 批准** | "错误严重性分级与已纠正错误计数是标准化接口，本文的错误率信号有规范依据" |
| **RISC-V AIA v1.0 / ACLINT** | RISC-V International | 已批准 | "错误告警到模式切换的中断路径有标准可依" |

#### 标准与规范类（证明"可靠性论证方式符合行业规范"）

| 标准 | 发布方 | 版本/时间 | 本论文用法 |
|---|---|---|---|
| **ECSS-Q-ST-60-15C Rev.1** — Radiation Hardness Assurance (RHA), EEE components | ECSS / ESA | **Rev.1, 2025-03-20**（原 C 版 2012-10-01） | 定义 TID / TNID（位移损伤）/ SEE 三类效应与 RHA 流程；Rev.1 把 RHA 嵌入项目阶段 0–D 并新增 SEE 测试模板。用于论文"容错需求来源"与"SEE 是设计约束"的规范引用 |
| **ECSS-E-ST-10-04C** — Space environment | ECSS / ESA | 持续修订 | 任务辐射环境规范的来源（SAA、辐射带、SPE），是 E38 自适应物理依据的标准出处 |
| **ECSS-Q-ST-60-13C Rev.2** — Commercial (COTS) EEE components | ECSS / ESA | Rev.2 | COTS 入轨的采购与评估条件，支撑"COTS + 软件容错"路线的合规叙事 |
| **ECSS-Q-ST-80C** — Software product assurance | ECSS / ESA | Rev.1 | 若论文讨论工具链可信性（LLVM pass 对认证的影响），这是对口标准 |
| **JEDEC JESD89 系列**（JESD89B 测试方法；JESD89-1 实时实测；JESD89-2 α 粒子；JESD89-3 加速中子束） | JEDEC | JESD89B 及子标准 | 地面 SER 测量方法学的权威依据：定义 FIT/device、加速中子束测试流程。用于论文"错误率（FIT）如何被测量与外推"的方法论引用，也是阈值设定（ECC CE 速率阈值）的量纲来源 |
| **ISO 26262 / IEC 61508**（功能安全） | ISO / IEC | — | 跨域对照：ASPIS 等工作指出功能安全标准要求使用高级语言与认证编译器，因此**平台无关的 IR 级加固**是被标准偏好的路线——这是本项目技术选型的标准侧论据 |
| **MIL-STD-883 Method 1019 / 1080**（TID、SEE 测试） | DoD | — | 美方器件鉴定对照，可与 ECSS 并列引用说明"器件级鉴定与系统级容错是两个层次" |

#### 三层容错分工图（答辩定位用）

```
板级/模块级   SpaceVPX (VITA 78) 双冗余供电、SpaceUM 管理、模块切换
     ▲
芯片/微架构级  NOEL-V FT / Ibex DCLS / HPSC DCLS+Split-Mode / ECC RF & cache / SEM IP scrubbing
     ▲        → 提供：错误检测、纠正、以及 RERI 标准化的错误计数与告警
     ▲
任务/数据流级  ★ 本文：LLVM criticality 分析 + 选择性近似 TMR（阈值表决）
              + 运行时 LOW/HIGH 模式切换（消费上层硬件给出的 CE 速率信号）
              → 填补"冗余强度随辐射环境时变而可调"这一缺口
```

#### 必须预先准备的三个反问及应答要点

1. **"为什么不直接用硬件锁步？"** → 锁步开销恒定 2×（HPSC 的 DCLS 直接砍掉一半核），且不可随环境调节；E2（开源锁步 NOEL-V）与 E25（HPSC DCLS）正是本文的对照端点。GR765 官方宣传语 "no need for lock-step processing" 本身就说明工业界在寻找锁步之外的容错路径。
2. **"近似表决会不会掩盖真实错误？"** → 以 E30（ESA 官方的星载 AI 可靠性综述）作为"AI/图像负载对数值偏差有内在容忍度"的权威依据；以 OBPMark 自带 verification data 与 CCSDS 压缩的质量指标把"容差"量化为可验证的阈值，而非主观设定。
3. **"你的 ECC 错误率信号从哪来？"** → RISC-V **RERI v1.0（2024 批准）** 的 corrected-error counting 与错误严重性分级；mcause 负责不可纠正/访问错误陷阱；HPM 计数器为厂商自定义事件通路；Xilinx SEM IP 经 AXI 向处理器暴露 SEU 统计则是工业界的既有实践。同时坦承：可纠正 ECC 事件**不**经 `mcause` 上报，框架因此采用可插拔信号后端设计。

---

### 本章条目索引（共 38 条）

E1 NOEL-V FT · E2 开源 Lockstep NOEL-V · E3 SELENE · E4 De-RISC · E5 GR765 · E6 GR740 · E7 Ibex DCLS · E8 Klessydra 动态 DMR→TMR · E9 SHAKTI-F/IRIS · E10 RT PolarFire SoC · E11 METASAT · E12 Ara/Ara2 · E13 Vicuna · E14 Spatz · E15 Saturn · E16 Chipyard/Rocket/BOOM · E17 FireSim · E18 gem5 RVV · E19 Spike/Dromajo/Banshee/QEMU plugin · E20 RISC-V RERI · E21 mcause/HPM · E22 AIA/ACLINT · E23 隐藏寄存器注入与选择性加固 · E24 流水线可靠性分析 · E25 NASA HPSC · E26 OBPMark/OBPMark-ML · E27 SpaceCube · E28 RAD750/RAD5545/Ingenuity · E29 SpaceVPX · E30 ESA 星载 AI 可靠性综述 · E31 NASA cFS · E32 BYU COAST · E33 ASPIS · E34 SWIFT/EDDI/CFCSS/nZDC/COMPAS · E35 动态 TMR · E36 TMR-MER/RUSH · E37 Xilinx SEM IP · E38 SAA/SPE 环境时变性

**与本项目关系最紧的 6 条（优先精读）**：E32 COAST（最直接基线）、E33 ASPIS（最新同行 + 编译器优化削弱保护的风险）、E8 Klessydra 动态 TMR（自适应切换的硬件先例）、E20 RERI（信号来源的规范依据）、E26 OBPMark（实验负载）、E25 HPSC（平台正当性）。
