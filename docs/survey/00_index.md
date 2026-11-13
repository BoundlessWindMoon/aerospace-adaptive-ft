# 《面向空天计算的自适应容错框架研究》中期答辩文献地图

本文档是按开题报告结构组织的文献检索结果，共 **7 个专题、约 340 条条目**，按"每条给出 一作 / 会议+年份 / 机制 / 与本课题的关系 / 开源代码 / 论文链接"的格式整理。

> 链接可信度说明：条目来自联网检索，绝大多数 DOI/仓库地址来自论文页或 GitHub 页面本身，但**写进参考文献前请逐条核对**（尤其是标了「待核实」的条目、arXiv 预印本、以及 2025–2026 的新文章）。各分册内部已标注哪些条目未能确认。

## 一、七个专题与它们服务的开题报告章节

- **A. 选择性软件容错与 SDC 感知保护**（55 条）→ 开题 2.2、3.2
  EDDI / SWIFT / SWIFT-R / CRAFT+PROFiT / nZDC / NEMESIS / Shoestring / SDCTune / Relyzer / Trident / COAST / ASPIS / 控制流校验一族。这是你的 baseline 清单和"关键性评分"的学术谱系。
- **B. 近似计算编译器、精度自动调优与误差界标定**（64 条）→ 开题 3.4.2、4.4.4，直接服务**方向 B（δ 自动标定器）**
  EnerJ / ACCEPT / Rely / Chisel / DECAF / Topaz / ApproxHPVM / ApproxTuner / Green / SAGE / Precimonious / HiFPTuner / FPTuner / ADAPT / RLIBM。末尾给了 5 个可直接照搬的 δ 标定方法学模板。
- **C. 近似冗余 / 降精度冗余 / 松弛表决：查新与创新性对标**（46 条）→ 开题 3.4、3.4.4
  **这一册请优先读完**：它回答"approximate TMR + 阈值表决"是否已有先例。结论是概念层面已有 15 年文献（ATMR 一族、RPR/ANT、IDMR/ITMR 近似表决器），每条都标了 OVERLAP RISK，并给出创新性应当如何重新表述。
- **D. 故障注入方法学、评测基础设施与基准集**（53 条）→ 开题 4.3、5.3.2
  LLFI/LLTFI/PINFI/FlipIt/gem5-FI/SASSIFI 等注入器 + 统计严谨性（Leveugle 样本量、Sangchoolie 单比特是否够）+ 指标体系（AVF/PVF/MWTF）+ 基准集（含 ESA OBPMark）。末尾是对你现有 harness 的 7 条方法学改进建议。
- **E. RISC-V 与空天辐射容错：平台、开源核、周期级仿真与标准**（38 条）→ 开题 1.1、4.3、5.3.1
  NOEL-V/GR765/De-RISC/SELENE/Ibex lockstep、Ara/Vicuna/Spatz/Saturn、NASA HPSC、OBPMark、RISC-V RERI。末尾 E.7 给了 9 条"怎么补 RISC-V cycle 级证据"的路径与代价对比，E.8 给了答辩可引用的权威标准。
- **F. 脆弱性预测、权重学习与运行时自适应**（58 条）→ 开题 3.2.2、3.3.3、4.4.4
  Relyzer/Approxilyzer/Trident/ePVF/PARIS/ACE/PVF + 自适应冗余 + ECC 计数器做错误率传感（Schroeder/Meza/Sridharan 现场数据）+ 空间辐射环境模型（AE9/AP9、CREME96、SPENVIS）+ 变点检测（CUSUM/SPRT）。末尾 F.9 给了替代 (w1,w2,w3) 的 4 条路线，F.10 把你的指标 A / 指标 B 映射成统计上站得住的变点检测规则。
- **G. SIMD/RVV 打包开销优化与向量通道冗余**（41 条）→ 开题 3.4.3，直接服务**方向 C**
  用向量通道做冗余的先行工作（SIMD-based soft error detection、SKIVA、DRIFT、Warped-DMR）+ LLVM SLP/VPlan 打包代价模型（PSLP/TSLP/Look-ahead SLP/goSLP/VeGen）+ RVV codegen（RISCVInsertVSETVLI、Saturn/Ara/Vicuna 的 vector-scalar move 代价）+ 数据布局变换 + SWAR。  末尾 13 项按收益/成本排序的优化清单。

## 二、检索中发现的、会直接影响答辩口径的 6 件事

1. **"近似 TMR + 阈值表决"不是新概念，必须改口径。** 最危险的先行工作是 Shim & Shanbhag 的 **Reduced Precision Redundancy（IEEE TVLSI 2004）**：1 个精确块 + 1 个降精度副本，`|y_main − y_RPR| > T_h` 就采用降精度副本的结果——与你"次关键代码用近似结果替代"的判定规则同构；该组 2006 年的 TVLSI 论文已把 RPR 直接用于**软错误**。另有 ATMR 一族（Sierawski TNS 2006 → Sánchez-Clemente TNS 2016）和 Chen/Han/Lombardi 的近似表决器（IEEE TC 2017），以及 **Arifeen 的 ATMR 综述（IEEE Access 2020）**——评委读过这篇就会说"这方向有十五年文献"。还有一篇 2025/26 的 *Journal of Supercomputing* 论文在软件层用混合精度副本 + 阈值检测软错误。应对方式见 C 册末尾小节：创新性要落在"编译器自动化 + 关键性驱动的分级 + 运行时环境自适应"的组合上，而不是"近似副本 + 阈值"本身。
2. **SWIFT-R 的同门 TRUMP 已经是"1 精确 + 1 非同构副本"**（DSN 2006 / IEEE Micro 2007 Top Picks），用 AN 码替代第三条流。你对比 SWIFT 的指令倍率时，评委很可能问 TRUMP。
3. **PROFiT（ACM TACO 2005）是"多版本代码 + 按区域选保护等级"的历史源头**，你的多版本框架需要相对它说清增量（在线自适应 + 把近似当作第三档）。
4. **corrected ECC 事件不会通过 `mcause` 上报。** RISC-V 的正确标准入口是 **RERI（RAS Error-record Register Interface）v1.0，2024 年已批准**。开题 3.3.3.1 里"用 mcause 观测 ECC"的写法需要改成 RERI + HPM 计数器，这反而把你自己写的"mcause 粒度过粗"风险点变成了有标准依据的解法。
5. **gem5 的 RVV 支持仍有 VLEN ≤ 512 bit 限制**；目前唯一集成进 Chipyard/FireSim 的开源 RVV 1.0 application-profile 向量单元是 **Saturn（UCB EECS-2024-215）**。E.7 给出 9 条补 cycle 级证据的路径和代价，短期最现实的是"静态指令构成 + 已发表向量单元代价模型"的估算口径，并在答辩时按 E.7 给的模板明确说明口径边界。
6. **ASPIS（Politecnico di Milano HEAP Lab，不是 Torino）是最新的 LLVM 软件容错 Pass 集合**，而 ASP-DAC 2026 有工作专门指出"编译器后续优化会削掉冗余"（compiler-induced reliability loss）——这直接威胁 IR 层插冗余的有效性，和 nZDC 当年的论点一致。你的 Pass 在 pipeline 中的插入位置需要一个明确的、可引用的说法。

## 三、跨册必读 20 篇（按答辩被问到的概率排序）

| # | 文献 | 册 | 为什么必须读 |
| :-- | :-- | :-- | :-- |
| 1 | Shim & Shanbhag, **RPR**, TVLSI 2004（+ 2006 软错误版） | C | 对你创新性威胁最大的先行工作 |
| 2 | Arifeen et al., **Approximate TMR: A Survey**, IEEE Access 2020 | C | 一篇就能概览 ATMR 全谱系 |
| 3 | Reis et al., **SWIFT**, CGO 2005 | A | 全场最常被引的 baseline |
| 4 | Chang/Reis, **SWIFT-R / TRUMP**, DSN 2006 + IEEE Micro 2007 | A | 软件 TMR 与"非同构副本"先例 |
| 5 | Chang et al., **CRAFT + PROFiT**, TACO 2005 | A | 多版本 + 可配置可靠性的源头 |
| 6 | Feng et al., **Shoestring**, ASPLOS 2010 | A | 你"距离输出"启发式的原型 |
| 7 | Lu et al., **SDCTune**, CASES 2014 | A | 和你的加权特征模型最像 |
| 8 | Hari et al., **Relyzer**, ASPLOS 2012 / **Approxilyzer**, MICRO 2016 | A/F | 等价类剪枝 + 近似可容忍错误的量化 |
| 9 | G. Li et al., **Trident**（*Modeling Soft-Error Propagation in Programs*）, DSN **2018** | A/F | 不做注入就预测 SDC，替代你的启发式 |
| 10 | Didehban & Shrivastava, **nZDC**, DAC 2016 | A | IR 层冗余的覆盖率天花板论证 |
| 11 | Sharif et al., **ApproxTuner**, PPoPP 2021 | B | δ 自动标定器的首选方法学模板 |
| 12 | Sharif et al., **ApproxHPVM**, OOPSLA 2019 | B | 端到端质量指标 → 逐算子精度预算 |
| 13 | Baek & Chilimbi, **Green**, PLDI 2010 | B | "calibration phase"这一机制的祖先 |
| 14 | Misailovic et al., **Chisel**, OOPSLA 2014 | B | 把手调变成 ILP 求解 |
| 15 | Rubio-González et al., **Precimonious**, SC 2013 | B | FP64→FP32 降级的标准工具与 baseline |
| 16 | Achour & Rinard, **Topaz**, OOPSLA 2015 | B/C | 离线学习结果分布 → 在线离群判定 → 精确重算，与你三段式同构 |
| 17 | Leveugle et al., **统计故障注入样本量**, DATE 2009 | D | 你 1000 次注入的置信区间必须有依据 |
| 18 | Sangchoolie et al., **One Bit is (Not) Enough**, DSN 2017 | D | 单比特模型的效度边界 |
| 19 | Bohman/James/Wirthlin/Quinn/Goeders, **COAST**, IEEE TNS'19 | A/E | 空天领域编译器容错的权威对标（注：Keller & Wirthlin 是 FPGA CRAM TMR 那条线，勿混引） |
| 20 | ESA/BSC, **OBPMark** | D/E | 你当前四套基准没有一个来自空天领域，这是唯一现成的补法 |

## 四、按你自己列的三个短板找文献

**短板 1：HIGH 模式 4.91× 指令开销偏高（方向 C）** → 读 G 册
- 先看 G.0 的成本账拆解和末尾 13 项清单。三个已经可以直接动手的结论：
  - 当前的 `fptrunc` 在 x86（F16C 只做转换、无 fp16 算术）和 RVV（widening 吃 2×LMUL + SEW 切换）上都是净亏损，应改为 fp32 + 尾数位掩码；
  - 两条 `insertelement` 换成 `IRBuilder::CreateVectorSplat`，可降为 1 条 `vbroadcastss` / `vfmv.v.f`；
  - RVV 上 `vl=2` 是最差配置，应把"冗余维"并进数据并行维（即向量化循环后在通道里放副本，而不是为每个标量建 2 通道向量）。
- 对标文献：Chen 等 SIMD-Based Soft Error Detection（CF'16）、SKIVA、DRIFT、Warped-DMR；LLVM 侧 PSLP（CGO'15）、Look-ahead SLP（CGO'20）、goSLP、VeGen。

**短板 2：(w1,w2,w3) 与 δ 经验给定（方向 B）** → 读 B 册末尾 5 个模板 + F.9 四条路线
- δ：ApproxTuner 的"单点误差剖面 + 误差组合模型 + 预测式调优"最贴近你已有的 `pure-approx` 离线表征数据（你已经测出 dgemm 1.21e-7、fft 5.39e-5，这就是 Green 意义上的 calibration 数据）；要 sound 的误差界就上 FPTuner / Satire / Daisy 一路。
- 权重：F.9 给了四条——(a) 用 Trident/ePVF 的 SDC 概率做回归目标反解权重；(b) 用 AxBench 预标注的可近似区域做监督信号；(c) Chisel 式 ILP 直接绕过权重（选版本而非打分）；(d) blame analysis / ADAPT 的梯度归责分数替代人造特征。

**短板 3：RISC-V 只有 QEMU 功能验证，无 cycle 级数据** → 读 E.7
- 9 条路径（P0–P8）给了 effort / fidelity 对比，并附答辩时的口径模板（明确说"RISC-V 侧为静态指令构成与功能/SDC 验证，x86 侧为硬件 PMU 实测"比含糊其辞更安全）。

## 五、建议的阅读顺序（约一周，每天 1.5 小时）

1. **Day 1 — C 册全读**（查新与创新性，最高优先级，直接决定答辩讲法与论文第 3 章表述）。
2. **Day 2 — A 册 A 节 + B 节**（baseline 谱系：EDDI → SWIFT → SWIFT-R/TRUMP → CRAFT/PROFiT → nZDC；Shoestring → SDCTune）。
3. **Day 3 — B 册**（δ 标定器方法学；边读边决定方向 B 要照搬哪一个模板）。
4. **Day 4 — D 册 D.4 + D.8**（统计严谨性与对现有 harness 的改进建议；这部分最容易在答辩现场被问"1000 次够不够""为什么只注单比特"）。
5. **Day 5 — E 册 E.7 + E.8**（RISC-V 口径与空天权威依据，顺手把开题 3.3.3.1 的 mcause 写法改成 RERI）。
6. **Day 6 — F.9 + F.10**（权重学习与变点检测，给答辩留一个"下一步工作"的漂亮说法）。
7. **Day 7 — G 册末尾清单**（挑 2–3 项当场能做的优化）。

## 六、分册文件

- [A. 选择性软件容错与 SDC 感知保护](part_A_selective_swft.md)
- [B. 近似计算编译器与误差界标定](part_B_approx_compilers.md)
- [C. 近似冗余与松弛表决：创新性查新](part_C_approx_redundancy.md)
- [D. 故障注入方法学与基准集](part_D_fault_injection.md)
- [E. RISC-V 与空天辐射容错](part_E_riscv_space.md)
- [F. 脆弱性预测、权重学习与运行时自适应](part_F_learning_adaptive.md)
- [G. SIMD/RVV 打包开销优化](part_G_simd_packing.md)

单文件合订本（含本索引 + 全部七册）：`../LITERATURE_SURVEY.md`

