# 相关工作速查(2026-11-16 检索)

每条:**一句话这个工作做了什么** + **对我们的启发**。
已在 `lit_*.md` 四册里收过的不重复(EDDI/SWIFT/nZDC 开销基线、RFT、Relax/BRAINIAC、
FT-BLAS 的存在性、半环硬度、幂等恢复)。标 `[未读全文]` 的只看了摘要/二手引用,
写进参考文献前必须核对。

---

## A. 进程级 / 系统级冗余 —— 对标我们新的"粗粒度层",最该正面处理

**PLR: Process-Level Redundancy** — Shye et al., DSN 2007 / IEEE TDSC 6(2) 2009
一句话:把应用**整进程**复制成 N 份,让 OS 自由调度到多核,比对输出判错;
不需要重编译、不需要改 OS,4 路 SMP 上 SPEC2000 检错开销 **16.9%**。
**启发**:我们的"粗粒度整内核复制 + 多核回落 ~1×"在概念上已被 PLR 占了,而且它的
16.9% 是真实 SMP + SPEC2000,比我们的 ~1× 更有说服力。差异化只能落在三处:
(a) 我们是**编译期自动判层**,进程级冗余只是"搜不到便宜机制时的兜底层";
(b) 它之上还有 ABFT / SIMD 两层更便宜的机制;
(c) PLR 明确**不支持共享内存与多线程程序**,我们的 fork-TMR 同样受限 —— 这是共同边界,
不是我们的缺陷,但必须写明。

**Romain / ASTEROID 副本感知协同调度** — Rambo et al., 2020(TU Dresden)`[未读全文]`
一句话:在微内核上做 OS 级副本管理,并为混合关键性系统做副本感知的协同调度。
**启发**:**副本放置与调度是一个独立研究维度,我们完全没碰** —— 现在只是 `fork()` 出来
交给 Linux 调度。"把副本放到哪个核/哪个隔离域"可以成为一个干净的增量。

**EXPERTISE** — So et al., ACM TACO 2022
一句话:软件级冗余多线程,优化同步开销与静默存储检查。
**启发**:线程级比进程级轻但要处理同步;我们若要报多核数字,对标对象是它而不是 PLR。

**SRT / CRT 与能效一族** — Reinhardt & Mukherjee ISCA 2000;Madan & Balasubramonian
TPDS 2007;RECVF, DSN 2010
一句话:冗余多线程的性能与**能量**账单:SRT 性能开销 20–30%、**能量 1.5–1.6×**;
RECVF 把能量压到 **1.26×**、性能开销 1.2%,比 DMR 省 37% 能量。
**启发**:多核冗余"墙钟 ~1×"**不等于免费**。空天的硬约束是功耗(见 E 册 HPSC),
所以把自适应的代价函数从"时间开销"换成"能量开销"是一个有文献支撑的学术增量。

---

## B. 评测方法学 —— 决定我们现有数字可信度,第二重要

**Quantifying the Accuracy of High-Level Fault Injection** — Wei, Thomas, Li,
Pattabiraman, DSN 2014
一句话:用 LLFI(LLVM IR 级)对比 PINFI(x86 汇编级),结论是
**IR 级注入对 SDC 概率准确,对崩溃(crash)概率不准确**(IR 里有 load 而汇编里是 mov,
注入点集合不同)。
**启发**:我们的 SDC 表有文献背书;**我们的 DUE 表没有**。所有 DUE 结论——包括
"粗粒度冗余 DUE 升高"——都必须标注这个限制,或改用二进制级/微架构级注入重测。

**Improving the Accuracy of IR-level Fault Injection** — Palazzi, Li, Fang,
Pattabiraman, IEEE TDSC 2020
一句话:查清了"IR 级准不准"两派矛盾的根因(PINFI 的位采样被改过,混淆了
activated / unactivated fault),重做后确认 IR 级对 SDC 与汇编级同等准确、对崩溃不然。
**启发**:给了"怎么把 IR 注入做对"的具体口径——**采样只能取已激活的故障**。
要核对我们的注入器是否踩了同一个坑。

**Evaluating Compiler IR-Level Selective Instruction Duplication** — Chang, Li,
Erez, FTXS @ SC 2019
一句话:用门级与二进制级注入当 ground truth 检验 IR 级选择性复制的 SDC 覆盖率,
并发现**二进制级单比特翻转的结果分布与 RTL 级高度一致**。
**启发**:二进制级注入是性价比最高的 ground truth,**不必上 RTL** 就能给我们的
SDC/DUE 表做一次校验。这是我们最便宜的一次可信度升级。

**FIERA: FPGA 仿真注入** — GOMAC 2024 `[未读全文]`
一句话:在 FPGA 上做 RTL 级注入,比 RTL 仿真快 **10⁴×**,预测 time-to-failure 与
**质子束实测相差 2.9–14.3%**;并给出注入手段的误差/速度阶梯
(beam ≫ FPGA ≈ RTL ≫ software,其中 software FI 建模误差标为 High)。
**启发**:明确了我们现在处在"建模误差 High"那一档。若要一个真正可信的档位,
FPGA CRAM 注入性价比最高;中间档是 gem5 微架构注入。

**TMR 软核的残余失效** — Wilson & Wirthlin 一族(JRERE 2021;IEEE TNS 2021;
BYU PhD 2025)`[未读全文]`
一句话:对 TMR 保护后的 RISC-V 软核做 CRAM 注入 + **中子束**,发现 TMR 之后
**仍存在残余单点失效**,补 SEU-aware placement 与存储 ECC 才做到 95% 覆盖、MTBF 20×。
**启发**:"SDC 全零"这类结论在真实注入下站不住。我们应预期自己的 0.00 会退化,
并提前准备残余失效分析,而不是等答辩被问。

**COAST 多平台篇** — Bohman et al., IEEE TNS 66(1) 2019;TNS 67(1) 2020
一句话:LLVM Pass 自动加 DWC/TMR,跨 MSP430 / ARM / RISC-V 多平台,
注入 + 束流实测 MWTF 提升 **1.2–36×**;而**关掉 cache(把故障限制在核内)后
MWTF 只有 1.2×**,作者解释是"**同步表决代码本身包含了大部分单点失效**,
它访问的值就在寄存器堆里"。
**启发**:这是对我们最直接的警告——表决器既是开销大头,又是真实注入下的 SPOF。
同时也给了一个可做的点:**表决器自身的加固**(自校验表决 / 表决器三模)。

**软件实现硬件容错的编译器技术综述** — Baroffio et al., ACM CSUR 2024 `[未读全文]`
一句话:系统综述编译器层面的软件实现硬件故障检测/容错技术。
**启发**:投稿前必读的查新基线,用来确认"分层 + 自动判层"是否已被覆盖。

---

## C. 开销下压的具体技术 —— 回答"为什么做一点容错就慢几倍"

**ESoftCheck: Removal of Non-vital Checks** — Yu, Garzarán, Snir, CGO 2009
一句话:三类 check 删除/外提——寄存器文件有 parity/ECC 时大批 check 非必要可删、
被后支配的冗余 check 可删、循环不变量与归纳变量的 check 可提到循环外;
并指出 x86 上全复制 2.16× 的主因是**寄存器压力**(32 位 x86 只有 8 个可用寄存器),
此前报告更低开销的工作用的都是 Itanium / PowerPC。
**启发**:(a)"威胁模型是否假设 RF 有 parity"是我们**尚未声明**的口径选择,声明了就能
合法删掉一大批表决点;(b) check 外提与合并是现成的、有引用的开销下压手段;
(c) 它也解释了为什么文献里的 1.4× 在我们这里复现不出来。

**FT-BLAS / FT-GEMM 的融合策略** — Wu et al., ICS 2021 / IEEE TPDS 2023;
FT-GEMM, ICS 2023(UCR)
一句话:把 ABFT 的校验和计算**融进 GEMM 汇编内核**(复用寄存器里已有的 C 元素),
开销从非融合的 15% 降到 **2.94%**;论文明说"reference checksum 构造是 ABFT 开销的大头",
非融合可达融合的 **5.35×**;GPU 版 FT-GEMM 每分钟注入数百错误仍只 8.89% 开销。
**启发**:我们的 ABFT 层是**非融合**写法(单独起循环扫数组算 rowsum/colsum,纯 memory-bound)。
`pb_gemm` 的 1.82× 里大头应该就是这个。融合是现成的 5× 级杠杆,**不动任何保护语义**,
SDC/DUE 应逐位不变——这是整个项目里最干净、性价比最高的一个否证实验。

**Rethinking ABFT with a Cooperative Software-Hardware Approach** — SC 2013
一句话:把 ABFT 开销拆成 **checksum 构造** 与 **验证** 两块,简化验证阶段得到
6.0–12.2% 的性能提升。
**启发**:我们的 ABFT 层现在只报一个总数;照这个拆法画开销构成图,才能说清
"1.82× 里哪部分是可去的"。

**AABFT: Automatic Algorithm-Based Fault Tolerance** — Narmour & Derrien(Inria)`[未读全文]`
一句话:提出一套编译方案,**自动分析循环巢并生成 checksum 计算**。
**启发**:**这是离我们代数校验层最近的工作,可能直接威胁新颖性,列为必读第一位。**
必须搞清它的可约性判据与我们的 `max_array_dims + n_stage < nest_depth` 是不是同一个东西。

**SKIVA: 子字冗余** — Didehban et al.(ASU)`[未读全文]`
一句话:把宽寄存器切成子字,低半放原件、高半放副本,周期性 rotate+xor 比对;
可配置空间/时间冗余,AVX 上能塞 3 份做一条指令内的 TMR。
**启发**:我们的 SIMD-slack 方向对、**宽度做错了**——当前默认是 `<2 x float>`
(`-atft-simd-f64` 默认 off),而 x86 上 `<2 x float>` 吞吐等同标量 double,等于没省。
正确做法是 `<2 x double>` 并**整条链保持打包、只在 store 拆一次**。

**SIMD 的 bookkeeping 税** — SIGARCH "SIMD Instructions Considered Harmful";
MMX extended-subwords 一族
一句话:x86 AVX / MIPS MSA 的 SIMD 代码里 **2/3 到 3/4 是 pack/unpack 与 fringe 的
bookkeeping**;单用 MRF 或单用 extended subwords 的加速比分别 <1 和 1.15。
**启发**:SIMD 车道内冗余不是稳赢。否证条件要写成"**打包/拆包指令占比 < 20%**,
否则这条路作废",而不是只看算术指令数。

**nZDC / gZDC** — Didehban & Shrivastava, DAC 2016;Didehban et al., IEEE TDSC 2023
一句话:把校验从"store 之前查寄存器"改成"**store 之后 load 回来查结果**",
覆盖率更高、开销 2.53–3.74×;TDSC 的表 3 按基本块形态给出每类块的额外指令数。
**启发**:可以直接引它的表当**代价模型**,替掉我们自己拍的常数(对应"消除拍脑袋常数")。

**同步点延迟比对 + XOR 哈希** — SWIFT, CGO 2005;Chen & Mueller, TR-2007-10
一句话:"在同步点集中比对,显著低于每次计算后都比对";函数参数用所有参数的
XOR 哈希传一个值,代替逐个复制。
**启发**:内层循环不表决、累一个签名、循环出口比一次。代价是检错延迟拉长到整个循环,
且**同一 bit 位在不同值上翻两次会互相掩蔽**——要量,不能只声明。

**SInRG: GPU 指令复制** — SC 2018(NVIDIA / UIUC)
一句话:GPU 上线程内指令复制平均 69% 开销,靠"牺牲错误包含域"换性能。
**启发**:"把冗余藏进空闲资源"是通用杠杆,但在任何平台上都不免费。

---

## D. 分层选择与自适应 —— 补 `lit_adaptive_prior.md` 没收的

**PROFiT / Software-Controlled Fault Tolerance** — Reis, Chang, Vachharajani,
Rangan, August, Mukherjee, ACM TACO 2(4) 2005
一句话:用 profile 量**每个程序区域**的脆弱性与性能代价,据此决定各区域开/关冗余,
并可分别在"全部可靠性选项 / 仅软件 / 仅软硬混合"三个集合里择优。
**启发**:**这是我们"分层 + 下包络选优"的直接先行工作,必须正面引用。**
我们的差异只能落在:(a) 候选集合里含 **ABFT 这种降阶机制**,他们只有指令级与软硬混合;
(b) 判据是**可约性(代价降阶 + 环结构)**,不是 profile 脆弱性;
(c) 切换发生在**运行时**(SPRT),不是编译期一次定选。这三条要在相关工作里逐条写清。

**Design and Evaluation of Hybrid Fault-Detection Systems (CRAFT)** — Reis et al., ISCA 2005
一句话:系统评估软硬混合检错的设计空间。
**启发**:设计空间表的组织方式可借用,用来摆我们的"层 × 开销 × 覆盖"三维表。

**Software-only Triple Diverse Redundancy on GPUs** — Alcaide, Kosmidis,
Hernandez, Abella, DSN 2020
一句话:在 GPU 上做纯软件的三重**多样性**冗余——副本不只是复制,还做多样化以破共模。
**启发**:**多样性冗余我们完全没做**,而它正是"共模故障怎么办"的标准答案,
也正好是我们 fork-TMR 那个"单事件只命中第 0 份副本"假设的补丁。
这是一个真实可做、且有空天出处的新增创新点。

---

## E. 平台与空天锚点

**NASA HPSC / Microchip PIC64-HPSC** — 2022 选型,2024 发布
一句话:**8 核 SiFive X280 RISC-V + 向量扩展**,1.2 GHz / 26K DMIPS / 12nm FinFET,
辐射容限,int8 2 TOPS;JPL 的设计约束原文是"用与现有硬件**相当的电量**处理更多数据"。
**启发**:一个锚点同时立住"为什么多核、为什么向量、为什么能量预算"。
把方法定位成"面向 X280 类多核向量空天处理器的自适应软件容错",比 x86 叙事硬一个档次。

**NOEL-V / GR765** — Frontgrade Gaisler
一句话:空间级 RISC-V,双发射,ECC 缓存 / SEU 纠错 / 可配 lockstep-TMR,已在轨验证。
**启发**:把我们的软件容错定位成"硬件 lockstep 的**软件替代/补充**",
立论站在真实空间处理器上。经典对照另有 LEON3/4、GR740。

**XS-GEM5** — OpenXiangShan
一句话:唯一严格对齐高性能 RTL(香山南湖/昆明湖)的开源 RISC-V 周期级模拟器,
SPEC CPU2006 相关性 **>95%**,带 TopDown 分析;但主路径是全系统 + 专有 checkpoint(GCPT),
SE 模式受限(不启内核、不接 difftest、不支持 fast-forward),速度约 100 KIPS。
**启发**:当"可信度背书 + 机理归因"用,**不当主平台**。

**gem5 校准研究** — Akram & Sawalha(SC 2019 workshop);Butko et al.;
parti-gem5(IJPP 2026)
一句话:未校准的 gem5 在 execution microbenchmark 上 IPC 误差可达 **458%**,
校准后均值误差 6%;ARM O3 的文献值为总执行时间误差 2–16%。
**启发**:模拟器不是自动可信。我们只报**比值**不报绝对 IPC,正好规避大部分误差——
这个论证要写进方法学一节。

**InjectV / CHAOS** — Polito 2026;arXiv 2026 `[未读全文]`
一句话:gem5 SimObject 形式的微架构故障注入器,分别注寄存器堆、物理内存、L1D/L1I/L2。
**启发**:不上 FPGA 的前提下能拿到的最好一档注入。

**McPAT / McPAT-Calib** — MICRO 2009;IEEE TCAD 2023
一句话:用 gem5 的 stats 估能量;McPAT 原生建模误差公认 **20–40%**,
McPAT-Calib 针对 RISC-V BOOM 做回归校准。
**启发**:能量**只报比值,不报绝对瓦数**;若被追问,备选是 McPAT-Calib。

**OBPMark / GPU4S** — ESA + BSC(Steenari, Kosmidis)
一句话:ESA 主导的星上处理开源基准套件(图像/雷达/压缩/信号/ML),
有 C / OpenMP / OpenCL / CUDA 多实现,专为填补"空天缺公开基准"的空白。
**启发**:我们只用了 `obpmark_image_proc` 一个。**扩到 OBPMark 的多个 block 是
最便宜的"评测更好"升级**,而且直接提升空天说服力。

**Mixed Software/Hardware-based Fault-tolerance(BSC 博士论文,TDX 2022)** `[未读全文]`
一句话:在 OBPMark 上评估混合软硬件容错,并做 COTS 多核/GPU 空天平台选型。
**启发**:"OBPMark + 混合容错 + 空天平台"这个组合已有系统性工作,查新必读。
