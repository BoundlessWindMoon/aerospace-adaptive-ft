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

- [A. 选择性软件容错与 SDC 感知保护](survey/part_A_selective_swft.md)
- [B. 近似计算编译器与误差界标定](survey/part_B_approx_compilers.md)
- [C. 近似冗余与松弛表决：创新性查新](survey/part_C_approx_redundancy.md)
- [D. 故障注入方法学与基准集](survey/part_D_fault_injection.md)
- [E. RISC-V 与空天辐射容错](survey/part_E_riscv_space.md)
- [F. 脆弱性预测、权重学习与运行时自适应](survey/part_F_learning_adaptive.md)
- [G. SIMD/RVV 打包开销优化](survey/part_G_simd_packing.md)

（本文件即合订本；分册原文见 `survey/` 目录）

## A. 选择性软件容错与 SDC 感知保护（Selective Software Fault Tolerance / SDC-Aware Protection）

Scope note: entries are grouped by role in the project pipeline (full-redundancy baselines → criticality/SDC-driven selection → SDC analysis & prediction infrastructure → control-flow checking → open LLVM/space toolchains → recent 2022–2026 / RISC-V work). Each entry gives first author, venue/year, mechanism, why it matters to this project, code URL if any (verified or marked to-verify), and paper URL/DOI.

### A.1 Full-duplication / full-TMR baselines（"software TMR" 版本的对比基线）

**1. EDDI — Error Detection by Duplicated Instructions in Super-scalar Processors**
- First author: Nahmsuk Oh (with P. Shirvani, E. J. McCluskey), Stanford CRC
- Venue/year: IEEE Transactions on Reliability 51(1), 2002 (~815 citations)
- Mechanism: pure-software duplication of every data-processing instruction into a shadow register/memory space, with comparison instructions inserted before stores and branches; shadow memory uses an address offset so a single upset cannot corrupt both copies.
- Why it matters: the canonical "protect everything" data-flow baseline. The per-instruction criticality score is a selection function over EDDI's candidate set; EDDI = that scoring function saturated to 1.0. Report overhead relative to it.
- Code: none original; faithful LLVM reimplementations exist in ASPIS (entry 41) and COAST (entry 40).
- Paper: https://ieeexplore.ieee.org/abstract/document/994913 (DOI 10.1109/24.994913)

**2. ED4I — Error Detection by Diverse Data and Duplicated Instructions**
- First author: Nahmsuk Oh
- Venue/year: IEEE Transactions on Computers 51(2), 2002
- Mechanism: instead of bit-identical duplicates, the second version computes on data multiplied by a diversity factor k (AN-code style); the comparison is x2 == k*x1. Detects errors that affect both copies identically (common-mode, opcode errors).
- Why it matters: direct intellectual ancestor of "approximate TMR with tolerance delta" — ED4I already breaks the bit-exactness assumption of voting. Cite it so reviewers do not call delta-voting unprecedented; differentiate by arguing delta tolerates *approximation error*, not a known algebraic factor.
- Paper: https://dl.acm.org/doi/10.1109/12.980007

**3. SWIFT — Software Implemented Fault Tolerance**
- First author: George A. Reis (Princeton Liberty group, with D. I. August)
- Venue/year: CGO 2005 (~1100 citations)
- Mechanism: compiler-level instruction duplication that *drops* memory duplication by assuming ECC memory (reclaiming EDDI's biggest cost), moves checks to stores/branches, and folds in enhanced control-flow checking via signatures; exploits idle ILP slots.
- Why it matters: the standard baseline in every comparable paper, and its "assume ECC for memory, protect only the datapath" assumption is exactly the assumption that the runtime ECC-rate monitor makes *dynamic* rather than static. Framing opportunity: SWIFT fixes the assumption at compile time; this project adapts it online.
- Paper: https://liberty.princeton.edu/Publications/cgo05_swift.pdf , https://ieeexplore.ieee.org/document/1402092

**4. SWIFT-R / TRUMP / MASK — Automatic Instruction-Level Software-Only Recovery**
- First author: Jonathan Chang / George A. Reis
- Venue/year: DSN 2006 (pp. 83–92); extended as IEEE Micro 27(1):36–47, 2007 (Top Picks)
- Mechanism: SWIFT-R triplicates compute instructions into three register sets and inserts in-register majority voting at sync points (software TMR with correction, not just detection). TRUMP replaces one replica with an AN-coded (x3) computation so only two streams are needed. MASK enforces program invariants cheaply.
- Why it matters: **closest baseline for "software TMR", and also the strongest threat to the novelty of "approximate TMR"**: TRUMP is already "1 exact + 1 cheaper-but-not-identical replica". The contribution must be framed as (a) *approximation-aware* delta-voting on numeric kernels, plus (b) runtime version switching. Reported: SWIFT-R cuts SDC to ~0.81%, SEGV to ~1.93%.
- Paper: https://liberty.princeton.edu/Publications/dsn06_trump.pdf ; IEEE Micro: https://liberty.princeton.edu/Publications/micro07_trump.pdf

**5. CRAFT + PROFiT — Software-Controlled Fault Tolerance**
- First author: Jonathan Chang (with Reis, August)
- Venue/year: ACM TACO 2(4), 2005
- Mechanism: CRAFT = hybrid HW/SW schemes (checking store buffer, load value queue) that patch SWIFT's unprotected windows; PROFiT uses profiling to choose, per program region, which protection level to apply, trading reliability against performance.
- Why it matters: **PROFiT is the historical origin of "configurable reliability / multi-version code"** — the thing being rebuilt here with modern LLVM plus a runtime ECC-rate signal. Must-cite; the delta over PROFiT is online adaptation plus approximation as a third version.
- Paper: https://dl.acm.org/doi/10.1145/1113841.1113843 (also https://www.academia.edu/9018951/)

**6. nZDC — A Compiler Technique for near Zero Silent Data Corruption**
- First author: Moslem Didehban (ASU MPS Lab, with A. Shrivastava)
- Venue/year: DAC 2016
- Mechanism: duplication at the *machine-code* (post-register-allocation) level that also protects "critical" instructions (loads, stores, branches) by loading written values back and comparing, plus an architecture-specific control-flow check. Closes the SDC holes EDDI/SWIFT leave in memory-access and branch instructions.
- Why it matters: shows that IR-level duplication (what an LLVM-IR pass does) has an intrinsic coverage ceiling because backend register allocation and optimization break redundancy. Must be addressed explicitly — see also entry 46 (compiler-induced reliability loss).
- Code: not released; TUM's COMPAS reimplements nZDC for RISC-V (entry 42).
- Paper: https://dl.acm.org/doi/10.1145/2897937.2898103 ; full detail in thesis: https://mpslab-asu.github.io/publications/papers/Didehban2018THESIS.pdf

**7. NEMESIS — A Software Approach for Computing in Presence of Soft Errors**
- First author: Moslem Didehban
- Venue/year: ICCAD 2017 (pp. 297–304)
- Mechanism: triple-stream (M/R/D) execution with nZDC-style load-back store detectors; on mismatch it runs a diagnosis routine plus majority voting and memory restoration, achieving forward recovery without full checkpoint restart.
- Why it matters: direct competitor to the "software TMR version with recovery"; its diagnosis-on-detection structure is a cheaper alternative to always-on triplication and is worth adding as a *fourth* protection level (detect-then-vote instead of always-vote).
- Paper: https://dl.acm.org/doi/10.5555/3199700.3199740

**8. InCheck — An In-Application Recovery Scheme for Soft Errors**
- First author: Moslem Didehban (with S. R. D. Lokam, A. Shrivastava)
- Venue/year: DAC 2017
- Mechanism: lightweight in-application checkpointing co-designed with nZDC detectors; on detection it rolls back to a fine-grained register/memory checkpoint rather than restarting the task.
- Why it matters: if the framework needs recovery (not only detection) for the "none"/"approximate" versions, InCheck is the cheap complement to TMR and a natural ablation point ("TMR vs detect+rollback at the same observed error rate").
- Paper: https://dl.acm.org/doi/10.1145/3061639.3062272

**9. Generic Soft Error Data and Control Flow Error Detection by Instruction Duplication**
- First author: Moslem Didehban (with Hwisoo So, Prudhvi Gali, A. Shrivastava, Kyoungwoo Lee)
- Venue/year: IEEE TDSC 21(1):78–92, 2023
- Mechanism: makes nZDC's control-flow checking ISA-independent by duplicating compare/branch computation and verifying branch direction generically; implemented as late backend passes in LLVM-or1k so that standard optimizations cannot remove the redundancy.
- Why it matters: the most recent "full duplication, near-complete coverage" reference point, and an explicit treatment of *where in the pass pipeline* to insert redundancy — a decision this project must make for both x86_64 and rv64gcv.
- Paper: https://mpslab-asu.github.io/publications/papers/Didehban2023TDSC.pdf (DOI 10.1109/TDSC.2023.3245842)

### A.2 Selective / criticality-driven protection（本课题核心）

**10. Shoestring — Probabilistic Soft Error Reliability on the Cheap**
- First author: Shuguang Feng (with S. Gupta, A. Ansari, S. Mahlke, University of Michigan)
- Venue/year: ASPLOS 2010 (pp. 385–396)
- Mechanism: relies on symptom-based detection (traps, segfaults) for most errors and uses compiler analysis to find the small set of "high-value, symptom-free" instructions whose corruption would silently reach output (short distance to a store/output, no masking consumer); duplicates only those.
- Why it matters: **the archetype of the distance-to-output heuristic.** Shoestring's safe-vs-vulnerable classification based on consumer analysis and proximity to program output is essentially the v0 of "semantic criticality". Position the weighted model (distance-to-output + loop depth + liveness) as a generalization and quantify what the extra features buy. Results: 33.9% of symptom-escaping errors recovered at ~15.8% overhead.
- Code: none released (an LLVM implementation is described but not published).
- Paper: https://dl.acm.org/doi/10.1145/1735970.1736063 ; LLVM pubs mirror: https://llvm.org/pubs/2010-03-ASPLOS-Shoestring.html

**11. SDCTune — A Model for Predicting the SDC Proneness of an Application for Configurable Protection**
- First author: Qining Lu (UBC, with K. Pattabiraman, M. S. Gupta, J. A. Rivers / IBM)
- Venue/year: CASES 2014
- Mechanism: heuristic plus regression-tree model over static/dynamic instruction features (instruction type, masking operations such as AND-0/SHR, comparison operations, **nested loop depth**, data width, distance to stores) to rank instructions by SDC-proneness, then duplicates the top-ranked set under an overhead budget.
- Why it matters: **the single most directly overlapping prior work on the scoring model.** SDCTune already reports that comparison instructions in inner loops are less SDC-prone than in outer loops — i.e. the loop-depth feature planned here. Treat it as (a) the baseline scoring function to beat and (b) the main novelty threat; differentiation must come from the multi-version + runtime-adaptive layer, not the feature set alone.
- Paper: https://dl.acm.org/doi/10.1145/2656106.2656111 ; model slides: https://blogs.ubc.ca/dependablesystemslab/files/2014/10/SDCTune-cases-presentation.pdf

**12. Configurable Detection of SDC-causing Errors in Programs**
- First author: Qining Lu (with Guanpeng Li, K. Pattabiraman, M. S. Gupta, J. A. Rivers)
- Venue/year: ACM TECS 16(3), Article 88, 2017
- Mechanism: journal extension of SDCTune adding SDCAuto (an automatically learned regression-tree model, no manual heuristics) and a full cost/benefit selection procedure: choose the duplication set that maximizes SDC coverage per unit of performance overhead.
- Why it matters: the formal statement of "selective instruction duplication cost/benefit optimization" and "configurable reliability" that the runtime switcher is the dynamic version of. Borrow its evaluation protocol (coverage-vs-overhead Pareto curves) verbatim — reviewers in this field expect that plot.
- Paper: https://dl.acm.org/doi/10.1145/2930667

**13. IPAS — Intelligent Protection Against Silent Output Corruption in Scientific Applications**
- First author: Ignacio Laguna (LLNL, with M. Schulz, D. F. Richards, J. Calhoun, L. Olson)
- Venue/year: CGO 2016 (pp. 227–238)
- Mechanism: defines *silent output corruption* (SOC) as the only SDC that matters, trains an ML classifier (features collected by LLVM passes, labels from fault injection with a user-provided verification routine) to predict which instructions cause SOC, then duplicates only those via an LLVM pass.
- Why it matters: canonical "application-level acceptability drives protection" paper; its user-supplied quality/verification routine is precisely the semantics needed to justify an *approximate* replica with tolerance delta. Also the cleanest template for an ML-vs-static-heuristic comparison in the evaluation.
- Code: not publicly released as far as could be verified (LLNL internal); workflow fully described in the paper.
- Paper: https://lukeo.cs.illinois.edu/files/2015_LaScRiCaOl_ipas.pdf ; record: https://experts.illinois.edu/en/publications/ipas-intelligent-protection-against-silent-output-corruption-in-s

**14. Low-Cost Prediction-Based Fault Protection Strategy**
- First author: Sunghyun Park (with Shikai Li, Ze Zhang, Scott Mahlke, University of Michigan)
- Venue/year: CGO 2020
- Mechanism: rather than duplicating a computation, it builds a cheap *predictor* of the value a protected instruction should produce and checks the real value against the prediction, falling back to recomputation on mismatch — an always-on protection cheaper than duplication.
- Why it matters: the closest existing idea to "approximate replica + tolerance threshold": a cheap approximate computation used as a checker. **Principal novelty threat to the approximate-TMR contribution** — read carefully and differentiate (they predict/check 1-of-1; this project votes 1 exact + 2 approximate with delta, enabling *correction*, not only detection).
- Paper: https://dl.acm.org/doi/pdf/10.1145/3368826.3377920 ; program: https://2020.cgo.org/accepted-papers

**15. Trace-and-Brace (TAB) — Bespoke Software Countermeasures against Soft Errors**
- First author: Yousun Ko (Yonsei, with Alex Bradbury / lowRISC, B. Burgstaller, R. Mullins / Cambridge)
- Venue/year: LCTES 2022 (co-located with PLDI 2022)
- Mechanism: traces execution to identify intrinsically robust vs. vulnerable code regions, then "braces" only the vulnerable ones with tailored (bespoke) data-flow and control-flow countermeasures; reports 99.82% data-flow error detection.
- Why it matters: recent, RISC-V-flavored (lowRISC/Cambridge authorship), region-granularity selective hardening — i.e. the *basic-block-level* criticality granularity used here. Evidence that BB granularity is publishable, and a baseline for the BB score.
- Paper: https://dl.acm.org/doi/10.1145/3519941.3535070 ; PDF: https://dl.acm.org/doi/pdf/10.1145/3519941.3535070

**16. Evaluating Compiler IR-Level Selective Instruction Duplication (validity study)**
- Venue/year: FTXS workshop at SC 2019
- Mechanism: compares selective duplication driven by LLVM-IR fault models against RTL and binary-level fault injection; concludes IR-based selective duplication is a cost-effective way to hit a target SDC coverage under an overhead budget, but should *not* be used where SDCs must be eliminated entirely (0.25% of RTL faults escape even full IR duplication).
- Why it matters: **the single most important validity citation for this thesis.** It licenses the IR-level approach for "good-enough reliability under a budget" while stating exactly which claim cannot be made. Use it in the threats-to-validity section.
- Paper: https://sc19.supercomputing.org/proceedings/workshops/workshop_files/ws_ftxs105s2-file1.pdf

**17. Deploying Lightweight Input-Aware Selective Instruction Duplication in HPC Applications**
- First author: Md Hasanur Rahman (with Guanpeng Li, University of Iowa / University of Florida)
- Venue/year: SC 2025 (pp. 1099–1112)
- Mechanism: makes selective duplication *input-aware* — a program's SDC profile shifts with input, so the protection set is chosen with lightweight dynamic profiling rather than one static profile, with deployment cost explicitly accounted for.
- Why it matters: 2025 state of the art in exactly this subfield and the most recent competitor. Conceptual parallel worth stating in the abstract: they adapt to *input*, this project adapts to *environment (ECC/soft-error rate)*.
- Paper: https://dl.acm.org/doi/10.1145/3712285.3759822 ; DBLP: https://dblp.org/db/conf/sc/sc2025

**18. ApproxDup — An Approximate Instruction Duplication Mechanism for Efficient SDC Detection in GPGPUs**
- First author: Xiaohui Wei (Jilin University, et al.)
- Venue/year: IEEE TCAD 43(3):1051–…, 2024 (online 2023)
- Mechanism: duplicates SDC-vulnerable instructions with *approximate* duplicates (reduced-precision / simplified replicas) and compares results with a slack, covering 92.51% of SDCs at much lower overhead than exact selective duplication.
- Why it matters: **the strongest direct threat to the "approximate TMR with tolerance delta" claim** — approximate duplication with threshold comparison already exists in a top journal. Cite it and differentiate on (i) TMR/correction vs duplication/detection, (ii) CPU + RISC-V vector targets vs GPGPU, (iii) runtime version switching.
- Code/artifact: Zenodo artifact record https://zenodo.org/records/8220533 (1.4 GB)
- Paper: https://dl.acm.org/doi/abs/10.1109/TCAD.2023.3330821 ; abstract: https://ui.adsabs.harvard.edu/abs/2024ITCAD..43.1051W/abstract

**19. A Study on Selective Implementation Approaches for Soft Error Detection (S-SWIFT-R)**
- First author: Mahsa Nikseresht (KU Leuven, with J. Vankeirsbilck, D. Pissoort et al.)
- Venue/year: Electronics (MDPI) 11(20):3380, 2022; related conference paper "A Selective Soft Error Protection Method for COTS Processor-based Systems" (2021) and a 2025 PhD chapter
- Mechanism: systematically compares *criteria* for choosing which variables/registers to protect with selective SWIFT-R (register liveness, runtime-monitored memory interaction, criticality ranking) on COTS embedded processors.
- Why it matters: an existing explicit study of exactly the criteria weighted here (notably **variable liveness**), on COTS parts in a dependability setting close to space. Use it to justify feature weights instead of asserting them.
- Paper: https://www.mdpi.com/2079-9292/11/20/3380 (open access) ; conference version: https://lirias.kuleuven.be/3643872 ; PDF: https://lirias.kuleuven.be/retrieve/640290 ; 2025 thesis chapter: https://lirias.kuleuven.be/retrieve/3038e39c-b5ff-4c4d-9d1c-6e61adf36c60

### A.3 SDC analysis, vulnerability modeling and prediction（criticality 标签的来源）

**20. Relyzer — Exploiting Application-Level Fault Equivalence to Analyze Application Resiliency to Transient Faults**
- First author: Siva Kumar Sastry Hari (UIUC, with S. V. Adve, H. Naeimi, P. Ramachandran)
- Venue/year: ASPLOS 2012
- Mechanism: prunes the fault-injection space using fault-equivalence classes (control equivalence, store equivalence, def-use pruning), then injects into a small set of representatives to obtain near-exhaustive per-instruction resiliency profiles.
- Why it matters: provides statistically sound per-instruction ground truth without millions of injections — exactly what is needed to validate the criticality score. Methodological ancestor of everything else in this section.
- Paper: https://dl.acm.org/doi/10.1145/2150976.2150990 ; discussed in: http://rsim.cs.illinois.edu/Pubs/16-MICRO-approxilyzer.pdf

**21. Approxilyzer — Towards a Systematic Framework for Instruction-Level Approximate Computing and its Application to Hardware Resiliency**
- First author: Radha Venkatagiri (UIUC, with A. Mahmoud, S. K. S. Hari, S. V. Adve)
- Venue/year: MICRO 2016
- Mechanism: extends Relyzer with an output *quality* metric: for virtually every operand bit of every dynamic instruction it classifies the outcome as masked / detected / output-corrupted, and for corruptions it quantifies quality degradation against an acceptable threshold.
- Why it matters: **the exact formal machinery this project needs** to decide which instructions may run approximately and what delta is acceptable. Approximation potential and resiliency are computed in one framework; the "none / approximate / TMR" three-way decision is an instantiation of it. Must-read.
- Paper: http://rsim.cs.illinois.edu/Pubs/16-MICRO-approxilyzer.pdf

**22. gem5-Approxilyzer — An Open-Source Tool for Application-Level Soft Error Analysis**
- First author: Radha Venkatagiri (with K. Ahmed, A. Mahmoud, S. Misailovic, D. Marinov, C. W. Fletcher, S. V. Adve)
- Venue/year: DSN 2019 (pp. 214–221)
- Mechanism: open-source reimplementation of Relyzer + Approxilyzer on gem5 for x86 (original used proprietary Simics/SPARC), with static and dynamic analysis plus heuristics giving 3–6 orders of magnitude reduction in injection runtime.
- Why it matters: **open source + x86_64 + instruction-level SDC labels** — the most usable ground-truth generator for the x86 target, and extensible to RISC-V inside gem5, which would itself be a defensible engineering contribution of the thesis.
- Code (verified): https://github.com/rsimgroup/gem5-approxilyzer
- Paper: http://rsim.cs.illinois.edu/Pubs/19-DSN-gem5Approxilyzer.pdf

**23. Minotaur — Adapting Software Testing Techniques for Hardware Errors**
- First author: Abdulrahman Mahmoud (UIUC, with Venkatagiri, Ahmed, Misailovic, Marinov, Fletcher, Adve)
- Venue/year: ASPLOS 2019
- Mechanism: treats resiliency analysis as software testing — applies test-suite minimization, prioritization and reduction to error-injection campaigns on top of Approxilyzer; both faster and finds *more* SDC-causing PCs (SDC-PCs).
- Why it matters: if the criticality model is trained or validated by injection, Minotaur is how the campaign becomes affordable in a master's timeline. Also supplies the "SDC-PC" vocabulary reviewers use.
- Code: not separately released (built on the Approxilyzer / gem5 line above).
- Paper: https://ma3mool.github.io/publication/minotaur2019.html ; PDF: https://dijkstra.eecs.umich.edu/kleach/eecs481/sp20/readings/minotaur.pdf

**24. Trident — Modeling Soft-Error Propagation in Programs**
- First author: Guanpeng Li (UBC, with K. Pattabiraman, S. K. S. Hari, M. Sullivan, T. Tsai)
- Venue/year: DSN 2018 (best-paper recognized)
- Mechanism: a three-level analytical model (static instruction-level propagation, control-flow, memory) that predicts per-instruction and whole-program SDC probability *without* fault injection, within a few percent of FI results.
- Why it matters: **the strongest alternative to the static weighted scoring model** — Trident derives propagation probabilities analytically instead of weighting hand-picked features. Either adopt it as the scorer (defensible, strong) or justify why a lighter weighted model suffices. Open source and LLVM-based, so it drops into the toolchain.
- Code (verified): https://github.com/DependableSystemsLab/Trident
- Paper: https://research.nvidia.com/sites/default/files/pubs/2018-06_Modeling-Soft-Error/18-DSN-Li-Trident_0.pdf

**25. vTrident — Modeling Input-Dependent Error Propagation in Programs**
- First author: Guanpeng Li (with K. Pattabiraman)
- Venue/year: DSN 2018 (companion paper)
- Mechanism: simplifies Trident's memory sub-model to *bound* how a program's SDC probability varies across multiple inputs, far faster than per-input FI or full Trident; also identifies the input that yields the median SDC rate.
- Why it matters: runtime switching must be robust across inputs; vTrident is the cheap way to show the criticality ranking is input-stable (or is not). Ships in the Trident repository under test/vTrident.
- Code (verified, same repo): https://github.com/DependableSystemsLab/Trident
- Paper: https://dl.acm.org/doi/10.1109/DSN.2018.00028 (see repo README for both paper links)

**26. GPU-Trident — Efficient Modeling of Error Propagation in GPU Programs**
- First author: Abdul Rehman Anwer (UBC, with M. Sullivan, G. Li, T. Tsai, K. Pattabiraman, S. K. S. Hari)
- Venue/year: SC 2020 (preliminary version SELSE 2019)
- Mechanism: ports Trident to GPU kernels, handling thread-level parallelism and shared-memory effects; predicted 35.67% vs FI-measured 33.73% mean SDC probability, instruction-level Pearson correlation ~0.83.
- Why it matters: shows how an analytical propagation model must be re-derived when the execution model changes — directly instructive for extending the scorer to **RVV vector instructions** (lanes, masking, strip-mining), an open gap this thesis can claim.
- Paper: https://www.mbsullivan.info/attachments/papers/anwer2020gpu.pdf ; SELSE version: https://d1qx31qr3h6wln.cloudfront.net/publications/SELSE2019_GPUTrident.pdf

**27. ePVF — An Enhanced Program Vulnerability Factor Methodology for Cross-Layer Resilience Analysis**
- First author: Bo Fang (UBC, with Q. Lu, K. Pattabiraman, M. Ripeanu, S. Gurumurthi)
- Venue/year: DSN 2016 (pp. 168–179)
- Mechanism: refines ACE/PVF analysis by filtering *crash-causing* bits out of the vulnerable set so the remainder approximates true SDC vulnerability; requires a detailed dynamic data-dependence graph, which limits it to roughly a million dynamic instructions.
- Why it matters: the pre-Trident analytical baseline, useful for the related-work taxonomy (ACE/PVF → ePVF → Trident → ML/GNN) and a reminder that crash is not SDC — a distinction the criticality score must encode, since in a watchdog-protected spacecraft a crash is often the *benign* outcome.
- Paper: https://store.computer.org/csdl/pds/api/csdl/proceedings/download-article/12OmNAY79gf/pdf (DOI 10.1109/DSN.2016.24)

**28. Hamartia — A Fast and Accurate Error Injection Framework**
- Venue/year: DSN-W (workshop) 2018 (NVIDIA / UBC collaboration)
- Mechanism: uses RTL gate-level fault injection to show that the ubiquitous single-bit-flip model misses many realistic hardware faults (single-event multi-bit / multi-location effects), and provides a faster injector supporting richer error models.
- Why it matters: **threat-model hygiene.** If switching decisions are tuned under a single-bit-flip assumption, this is the citation reviewers will use against the work. State the fault model explicitly, and note that delta-threshold voting is arguably *more* robust to multi-bit corruption than exact comparison.
- Paper: https://research.nvidia.com/publication/2018-06_hamartia-fast-and-accurate-error-injection-framework ; PDF: https://d1qx31qr3h6wln.cloudfront.net/publications/dsn-w_Hamartia_Framework.pdf

**29. GATPS — An Attention-Based Graph Neural Network for Predicting SDC-Causing Instructions**
- First author: J. Ma (et al.)
- Venue/year: 2021 (IEEE)
- Mechanism: predicts SDC-causing instructions by learning an embedding for each instruction through attention-based aggregation over the program graph.
- Why it matters: the first clear GNN instance of the criticality scorer. If a hand-weighted linear model is presented in 2026, a reviewer will ask why not a GNN; have the answer ready (interpretability, compile-time cost, no need for FI labels).
- Paper: https://ieeexplore.ieee.org/iel7/9440994/9440995/09441056.pdf

**30. Efficient Instruction Vulnerability Prediction With Heterogeneous SDC Propagation Knowledge Graph**
- First author: Bao Wen (NUAA, with Jingjing Gu, Dazhong Shen, Qiang Zhou, Fuzhen Zhuang et al.)
- Venue/year: IEEE TDSC 23(1):1173–1190, Jan–Feb 2026 (online 2025), DOI 10.1109/TDSC.2025.3614343
- Mechanism: builds a multi-layer heterogeneous program knowledge graph of instruction entities and relations that encodes plausible error-propagation paths, combines a GNN with a Transformer to model intra- and inter-basic-block context, and predicts vulnerable instructions at low overhead; trained on 1,144,070 LLFI fault injections across 22 programs.
- Why it matters: the current SOTA accuracy bar for SDC prediction, from a Chinese institution the committee will likely know. Its intra/inter-basic-block modeling competes directly with the BB-level score here, and it is a ready-made source of feature definitions.
- Paper: https://www.computer.org/csdl/journal/tq/2026/01/11179941/2aiMtZyBi0w ; IEEE: https://ieeexplore.ieee.org/document/11179941

**31. GVR — A Unified Graph Neural Framework for Instruction-Level Soft Error Vulnerability Prediction and Ranking**
- First author: Mohammad Reza Pirayesh (with Mohsen Raji et al., Shiraz University)
- Venue/year: Journal of Systems Architecture, vol. 181, 2026 (in press)
- Mechanism: a single graph representation fusing static program graphs with dynamic execution information and enriched instruction features, used both to *predict* vulnerability and to *rank* instructions for selective protection.
- Why it matters: "predict + rank for selection" is exactly pipeline stage 1 → stage 2 of this project; the newest entry in the field, so cite it to demonstrate currency.
- Paper: https://www.sciencedirect.com/science/article/pii/S138376212600353X

**32. LLFI / LLTFI — LLVM-level fault injection**
- First author: UBC DependableSystemsLab (LLFI: Thomas & Pattabiraman; LLTFI extends to ONNX/TensorFlow/PyTorch)
- Venue/year: LLFI ~2013 (DSN-W / ISSRE line); LLTFI 2022
- Mechanism: injects faults into LLVM IR at selected program points with traceability back to source code; LLTFI adds ML-model injection through onnx-mlir.
- Why it matters: **the most likely experimental harness for this thesis**, and the de facto standard against which numbers will be compared (the TDSC 2026 work above used LLFI for >1.1M injections). Keep the IR-vs-RTL fidelity caveat from entry 16 in mind.
- Code (verified): https://github.com/DependableSystemsLab/LLFI , https://github.com/DependableSystemsLab/LLTFI , GPU variant https://github.com/DependableSystemsLab/LLFI-GPU , invariant-based variant https://github.com/DependableSystemsLab/LLFI-IPA
- Paper: see PaperLLFI.bib in the repository

**33. ZOFI — Zero-Overhead Fault Injection Tool for Fast Transient Fault Coverage Analysis**
- First author: Vasileios Porpodas (Intel; previously Cambridge, DRIFT co-author)
- Venue/year: arXiv 1906.09390 (2019), tool paper
- Mechanism: injects transient faults into an unmodified *binary* at runtime with negligible overhead (ptrace-style register corruption), enabling fast coverage campaigns on real hardware.
- Why it matters: allows evaluation on real x86_64 silicon with no simulator, complementing LLFI; cheap enough for the many configurations a multi-version framework generates.
- Code: https://github.com/vporpo/zofi (repo named in the paper; to-verify)
- Paper: https://arxiv.org/html/1906.09390v4

### A.4 Control-flow checking（CFG 级对应机制，空天场景性价比很高）

**34. CFCSS — Control-Flow Checking by Software Signatures**
- First author: Nahmsuk Oh (with P. Shirvani, E. J. McCluskey)
- Venue/year: IEEE Transactions on Reliability 51(1):111–122, 2002
- Mechanism: assigns a compile-time signature to each basic block; a runtime signature register is XOR-updated on each edge and checked on block entry, detecting illegal inter-block jumps. Branch-fan-in nodes need an adjusting signature.
- Why it matters: the mandatory CFC baseline, already implemented in two open toolchains that can be reused (COAST and ASPIS), so it comes nearly free in the multi-version matrix.
- Implementations: COAST documentation with algorithm and modifications: https://coast-compiler.readthedocs.io/en/latest/cfcss.html ; ASPIS: https://github.com/HEAPLab/ASPIS
- Paper: https://ieeexplore.ieee.org/document/994926 ; summary: https://www.semanticscholar.org/paper/eca217cfa0c878c9685235ab1ca39ead34946b63

**35. CEDA — Control-Flow Error Detection Using Assertions**
- First author: Ramtilak Vemu (with Jacob A. Abraham, UT Austin)
- Venue/year: IEEE Transactions on Computers 60(9):1233–1245, 2011
- Mechanism: splits signature updates into two parts per basic block (entry and exit) with values chosen so that both inter-block and many intra-block control-flow errors are caught, at lower overhead and higher coverage than CFCSS.
- Why it matters: the standard "better than CFCSS" comparison point; implementing only CFCSS invites the CEDA/RASM question at defense.
- Paper: https://dl.acm.org/doi/10.1109/TC.2011.101 ; IEEE: https://ieeexplore.ieee.org/document/5871589

**36. RASM — Random Additive Signature Monitoring for Control Flow Error Detection**
- First author: Jens Vankeirsbilck (KU Leuven, with N. Penneman, H. Hallez, J. Boydens)
- Venue/year: IEEE Transactions on Reliability 66(4):1178–1192, 2017
- Mechanism: additive (not XOR) signature updates using *random* values with optimally placed validity checks — two gradual updates and one verification per basic block, so skipping any update makes the runtime signature unrecoverable. Higher detection ratio than the CFCSS/CEDA family.
- Why it matters: the modern CFC of choice, already implemented in ASPIS and in TUM's RISC-V backend — i.e. the CFC to ship in the framework's high-protection level.
- Paper: https://ieeexplore.ieee.org/iel7/24/8124127/08067656.pdf ; record: https://ui.adsabs.harvard.edu/abs/2017ITR....66.1178V/abstract

**37. RACFED — Random Additive Control Flow Error Detection**
- First author: Jens Vankeirsbilck (with N. Penneman, H. Hallez, J. Boydens)
- Venue/year: SAFECOMP 2018 (LNCS, pp. 220–234)
- Mechanism: extends RASM with intra-block instruction monitoring so that both inter-block and *intra*-block CFEs are detected by one mechanism, avoiding the cost of stacking two techniques (e.g. RASM + RSCFC/SIED).
- Why it matters: if the criticality model distinguishes intra-BB from inter-BB control errors, RACFED is the technique to borrow; it also covers the SIED line of work through its comparisons.
- Paper: https://lirias.kuleuven.be/retrieve/c61d5f98-a5a6-4a5b-9743-5b9fd3fd0348

**38. PaSS — Path Sensitive Signatures for Control Flow Error Detection**
- First author: Ze Zhang (University of Michigan, with Sunghyun Park, Scott Mahlke)
- Venue/year: LCTES 2020
- Mechanism: partitions the CFG into intervals, continuously updates a path-memorizing signature, and uses a single check per interval to catch both intra-interval (short-distance) and inter-interval (long-distance) CFEs; adds low-cost inter-procedural protection. 99.0% fault coverage at 19.1% overhead.
- Why it matters: the best overhead/coverage point in the recent compiler CFC literature and a *selective* (interval-granularity) scheme — the control-flow analogue of BB-level criticality. Strong candidate to adopt rather than reinvent.
- Paper: https://cccp.eecs.umich.edu/papers/ze-lctes20.pdf ; ACM: https://dl.acm.org/doi/abs/10.1145/3372799.3394360

**39. DRIFT — Decoupled Compiler-Based Instruction-Level Fault Tolerance**
- First author: Konstantina Mitropoulou (Cambridge, with V. Porpodas, T. M. Jones)
- Venue/year: LCPC 2013 (Springer LNCS; frequently cited as the 2014 chapter)
- Mechanism: observes that checking code in EDDI/SWIFT-style duplication serializes against control flow; DRIFT *decouples* redundant data-flow from control-flow checks, batching and delaying checks to recover ILP — up to ~29% faster than the state of the art at equal coverage.
- Why it matters: a pure *performance* optimization applicable to all three code versions, most valuable in the triplicated version. See also the companion COMET (CASES 2016) on communication-optimized multithreaded detection.
- Paper: https://link.springer.com/chapter/10.1007/978-3-319-09967-5_2 ; chapter record: https://www.researchgate.net/publication/303542655 (cited within the nZDC references)

### A.5 开源 LLVM 容错工具链与空天应用（实现参考与基线）

**40. COAST — COmpiler-Assisted Software fault Tolerance (BYU CCL)**
- First author (tool paper): Matthew Bohman / Benjamin James (with M. Wirthlin, H. Quinn, J. Goeders)
- Venue/year: "Microcontroller Compiler-Assisted Software Fault Tolerance", IEEE Transactions on Nuclear Science 66(1):223–232, 2019; follow-ups on multiple processor platforms (2019) and FreeRTOS (2021)
- Mechanism: open LLVM IR passes implementing DWC (duplication) and **TMR with inserted voters**, with a configurable *scope of replication* (per-function and per-variable annotations, pointer-crossing handling) plus CFCSS; validated by fault injection (QEMU) **and neutron-beam radiation testing on a TI MSP430**, and demonstrated on ARM and RISC-V.
- Why it matters: **the most important engineering baseline for this thesis.** It is open source, LLVM-based, TMR-capable, space-motivated, multi-ISA (including RISC-V) and radiation-validated. The contribution must be expressible as a clean delta over COAST: per-instruction criticality scoring, approximate replicas with delta-voting, and runtime multi-version switching (COAST's selection is manual annotation and static).
- Code (verified): https://github.com/byuccl/coast ; docs: https://coast-compiler.readthedocs.io
- Paper: https://space.pitt.edu/sites/default/files/2024-10/Microcontroller-Compiler-Assisted-Software-Fault-Tolerance.pdf ; multi-platform: https://ccl.byu.edu/assets/Applying_Compiler-Automated_Software_Fault_Tolerance_to_Multiple_Processor_Platforms_2019.pdf ; RTOS: https://byuccl.github.io/assets/Automated_Software_Compiler_Techniques_to_Provide_Fault_Tolerance_for_Real-Time_Operating_Systems_2021.pdf

**41. ASPIS — Automatic Software-based Protection and Integrity Subsystem**
- First author: Davide Baroffio (Politecnico di Milano HEAPLab, with F. Reghenzani, W. Fornaciari)
- Venue/year: ACM TODAES 29(5), September 2024 ("Enhanced Compiler Technology for Software-based Hardware Fault Detection"); earlier ACM CF 2023 paper
- Mechanism: an LLVM-IR pass suite implementing EDDI for data flow plus CFCSS and RASM for control flow, with overhead-reduction techniques including **use-def-chain-driven selective duplication** that exploits SSA form; language- and architecture-independent by construction.
- Why it matters: the most modern, actively maintained open LLVM SIHFT framework, and its use-def-chain selective duplication is essentially a liveness-based criticality heuristic — one of the three planned features, already published. Build on it rather than from scratch, and benchmark against it.
- Code (verified): https://github.com/HEAPLab/ASPIS ; project page: https://heaplab.github.io/ASPIS
- Paper: https://dl.acm.org/doi/10.1145/3660524

**42. COMPAS — Compiler-Assisted Software-Implemented Hardware Fault Tolerance for RISC-V**
- First author: Uzair Sharif (TU Munich, with D. Mueller-Gritschneder, U. Schlichtmann)
- Venue/year: MECO 2022 (11th Mediterranean Conference on Embedded Computing); companion talk "Extending the RISC-V LLVM Backend to Support Fault-tolerant Computing"
- Mechanism: open-source LLVM **backend** (late, post-optimization) passes for RV32/RV64 implementing EDDI, SWIFT, nZDC, CFCSS, RASM and their own REPAIR scheme, with support for **selective hardening of critical functions**.
- Why it matters: **the RISC-V baseline, and the work that overlaps this project's stated scope most directly** (rv64, LLVM, selective hardening, multiple schemes). Read before finalizing the novelty claim; the remaining differentiators are criticality *scoring* (vs function-level manual selection), approximate TMR, and runtime adaptation. Its backend placement also answers the "optimizations delete my redundancy" problem.
- Code (verified): https://github.com/tum-ei-eda/compas-ft-riscv
- Paper: https://ieeexplore.ieee.org/document/9797144 ; talk abstract: https://www.edacentrum.de/en/node/1660

**43. CompaSeC — A Compiler-assisted Security Countermeasure to Address Instruction Skip Fault Attacks on RISC-V**
- First author: Johannes Geier (TU Munich, with D. Mueller-Gritschneder, U. Sharif, U. Schlichtmann)
- Venue/year: ASP-DAC 2023
- Mechanism: reuses the COMPAS SIHFT passes (duplication plus signature monitoring) as a countermeasure against *instruction-skip* fault-injection attacks on RISC-V, combining selective data- and control-flow hardening.
- Why it matters: shows that one compiler infrastructure serves both radiation and attack threat models; the natural place to discuss RISC-V CFI / soft-error variants if the thesis wants a security-adjacent paragraph.
- Paper: https://www.aspdac.com/aspdac2023/archive/pdf/8C-3.pdf ; ACM: https://dl.acm.org/doi/10.1145/3566097.3567925

**44. REDDI — Hardening On-Board Software: a Low-Overhead Compiler-based Approach**
- First author: Emilio Corigliano (Politecnico di Milano, with HEAPLab and Argotec)
- Venue/year: EDHPC 2025 (2nd European Data Handling & Data Processing Conference, ESA; IEEE Xplore document 11325983)
- Mechanism: a *selective* SIHFT technique applied at compilation time, targeting satellite on-board software with substantially reduced overhead compared with full EDDI-style duplication; part of the ESA-funded ASTRAEUS line and built in the ASPIS ecosystem.
- Why it matters: **the closest paper in the world to this thesis topic in 2025** — compiler-based selective hardening explicitly for spacecraft on-board software, ESA-funded. Read it first: it shows what the European space community considers novel and which overhead numbers are credible.
- Paper (open PDF): https://re.public.polimi.it/bitstream/11311/1298225/1/2025_EDHPC_REDDI.pdf ; IEEE: https://ieeexplore.ieee.org/document/11325983 ; session: https://indico.esa.int/event/552/contributions/11091

**45. ASTRAEUS — Software Techniques for Soft Error Resilience: the ASTRAEUS Project**
- First author: Federico Reghenzani (PoliMi, with D. Baroffio, E. Corigliano, W. Fornaciari, G. S. Gajani, P. Maffezzoni, plus Argotec)
- Venue/year: DSD 2025 (28th Euromicro Conference on Digital System Design, pp. 138–144); ESA Contract 4000147102/25/NL/RA, ARTES 4.0
- Mechanism: project overview of SIHFT-based radiation mitigation for COTS devices in space telecom payloads — modifying a conventional compilation toolchain to identify transient faults and apply fault-tolerant responses, validated by fault injection and radiation testing.
- Why it matters: supplies the authoritative framing paragraph for "COTS in space + compiler-based mitigation", including the validation standards (ESA/ECSS radiation-effects mitigation handbook) a 空天计算 thesis should cite.
- Paper (open PDF): https://re.public.polimi.it/bitstream/11311/1298452/1/2025___DSD___Astraeus.pdf ; record: https://re.public.polimi.it/handle/11311/1298452

**46. Quantifying Compiler-Induced Reliability Loss in Software-Based Fault Tolerance**
- First author: Davide Baroffio (with J. Geier et al.; PoliMi / TUM)
- Venue/year: ASP-DAC 2026 (preprint on mediaTUM)
- Mechanism: measures how standard LLVM optimization passes *erode* redundancy inserted at IR level (CSE, DCE, GVN removing duplicated computation), quantifying the resulting coverage loss for ASPIS/COAST-style tools.
- Why it matters: **a must-cite implementation hazard.** It decides whether to insert redundancy at IR level (portable but optimizable-away) or at backend level (robust but per-ISA work doubled for x86_64 and RV64). Expect this question at the defense.
- Paper (preprint): https://mediatum.ub.tum.de/doc/1836695/pp6qy0y8uhnyf7z0srwwm4m2g.baroffiogeier_aspdac26_preprint.pdf

**47. Characterizing Runtime Performance Variation in Error-Detecting (EDDI-hardened) Programs**
- Venue/year: ISSRE 2023
- Mechanism: an empirical study of why EDDI-protected binaries exhibit large, hard-to-predict runtime variation (ILP, execution-port pressure, cache effects) instead of the textbook ~2x slowdown.
- Why it matters: the runtime switcher's cost model needs *predictable* per-version overheads; this paper shows they are not predictable and demonstrates how to measure honestly. Also a WCET concern for real-time space workloads.
- Paper: https://store.computer.org/csdl/proceedings-article/issre/2023/159400a730/1RKjmKdNTna

**48. Software Fault Tolerance in Real-Time Systems: Identifying the Future Research Questions**
- First author: Federico Reghenzani (with Zhishan Guo)
- Venue/year: ACM Computing Surveys 55(14s), July 2023 (DOI 10.1145/3589950)
- Mechanism: surveys SIHFT in real-time contexts and proposes a joint scheduling/failure analysis model, showing how redundancy interacts with timing guarantees.
- Why it matters: switching versions at runtime creates a schedulability problem (each version has a different WCET). This survey is both the related-work anchor and the source of the mixed-criticality vocabulary a 空天 thesis committee expects.
- Paper: https://zguo32.wordpress.ncsu.edu/files/2023/04/Reghenzani-and-Guo-2023-Software-Fault-Tolerance-in-Real-Time-Systems-Ide.pdf

**49. EXPERT — Effective and Flexible Error Protection by Redundant Multithreading**
- First author: Hwisoo So (Yonsei, with M. Didehban, Y. Ko, A. Shrivastava, K. Lee)
- Venue/year: DATE 2018
- Mechanism: redundant multithreading in which the redundancy level is *flexible*, trading coverage against thread resources instead of enforcing lock-step full RMT.
- Why it matters: the multicore alternative to intra-thread TMR. If the target platform has spare cores, reviewers will ask why instructions are triplicated rather than threads; have the comparison ready (RMT needs cores and inter-thread communication; instruction TMR survives on a single hardened core).
- Paper: https://ieeexplore.ieee.org/document/8342067

**50. Duplication-Based Fault Tolerance for RISC-V Embedded Software**
- Venue/year: 2024 (venue not fully verified; indexed on ResearchGate, cites COMPAS)
- Mechanism: evaluates duplication-based SIHFT variants specifically on RISC-V embedded software and quantifies the coverage/overhead trade-off on real cores.
- Why it matters: a recent concrete RISC-V coverage/overhead dataset to compare rv64gcv numbers against. **Verify the venue on DBLP before citing.**
- Link: https://www.researchgate.net/publication/383768042_Duplication-Based_Fault_Tolerance_for_RISC-V_Embedded_Software

**51. Using RISC-V Extensions to Support Software Fault Tolerance (Xpdetector / P-DETECTOR)**
- First author: De Blaere (with J. Vankeirsbilck et al., KU Leuven)
- Venue/year: recent (2023–2025; Semantic Scholar record)
- Mechanism: proposes a proof-of-concept RISC-V extension (Xpdetector) with custom instructions and hardware support that implement the P-DETECTOR SIHFT scheme, cutting the software overhead of duplication-based detection.
- Why it matters: the HW/SW co-design escape hatch for duplication overhead on RISC-V; useful for the future-work section and to pre-empt "why not add an instruction?".
- Link: https://www.semanticscholar.org/paper/c00c1cacc0371aaa834e975c6e65a0624cf6e52d

**52. Reliability and Performance Evaluation of a RISC-V Vector Extension Unit for Vector Multiplication**
- First author: C. Imianosky (with D. A. Santos, D. R. Melo, F. Viel, L. Dilillo)
- Venue/year: IEEE DFT 2024
- Mechanism: RTL fault-injection campaigns on the vector execution unit of HARV-SoC, comparing scalar and RVV-intrinsic implementations for both speedup (up to 28.69x) and reliability across vector parameter configurations.
- Why it matters: **the empirical basis for extending the criticality model to RVV.** Vector width / LMUL choices change the vulnerability profile, so one vector instruction is not "one instruction" for criticality purposes. See also RISC-V Summit Europe 2025 posters describing the first full RTL instruction-replication implementation in an HPC-class vector processor (Vitruvius+): ~75% reduction in non-silent failures at ~7.5% area overhead.
- Paper: https://hal.science/hal-04715405v1/file/2024___DTFS___Reliability_and_Performance_Evaluation_of_a_RISC_V_Vector_Extension_Unit_for_Vector_Multiplication%20%281%29.pdf ; posters: https://riscv-europe.org/summit/2025/posters

**53. Approximate TMR (ATMR) voters and reduced-precision redundancy**
- Representative paper: "A Fault Tolerant Voter for Approximate Triple Modular Redundancy", Electronics (MDPI) 8(3):332, 2019; see also X-Rel (Vafaei et al., 2023) for precision-relaxed voters, reduced-precision redundancy (RPR) work by Junsangsri et al. (2022/2025), and the 2026 TMR survey
- Mechanism: ATMR's defining correctness rule is that *only one* approximate module may differ from the exact one for any given input vector, so a majority of two always exists; the paper designs a voter tolerant to faults at its own inputs and introduces a Quality-of-Circuit metric. X-Rel relaxes voter precision to compare only the most significant bits.
- Why it matters: **hardware ATMR has already formalized the correctness condition the software approximate-TMR scheme needs.** delta-threshold voting is sound only if each replica's approximation error is bounded such that two of three always agree within delta — borrow the ATMR condition and state it as an explicit constraint. X-Rel's precision-relaxed voting is the hardware analogue of delta.
- Paper: https://www.mdpi.com/2079-9292/8/3/332 ; TMR survey: https://arxiv.org/html/2603.14411v1 ; RPR: https://www.techrxiv.org/doi/pdf/10.36227/techrxiv.21350274

**54. Processor Design for Soft Errors: Challenges and State of the Art (survey)**
- Venue/year: ACM Computing Surveys 49(3), Article 57, November 2016
- Mechanism: broad survey of hardware and compiler-based soft-error techniques, including the SWIFT-R / TRUMP recovery line and their cost structures.
- Why it matters: the taxonomy backbone for the related-work chapter, and a source of accepted overhead figures for each classical scheme without re-deriving them.
- Paper: https://li3tuo4.github.io/pub/li-csur.pdf

**55. SDC-causing Error Detection Based on Lightweight Vulnerability Prediction (and the ML-SDC lineage)**
- First author: Yunfei Liu (et al.)
- Venue/year: ACML 2019 (PMLR vol. 101)
- Mechanism: lightweight feature-based ML prediction of per-instruction SDC vulnerability without exhaustive injection, using masking operations, transfer/branch features and loop information; its related-work section maps the whole ML-for-SDC field (SVM: Laguna 2016, Yang & Wang 2019; regression trees: Lu 2014/2017; NN/LSTM: Bronevetsky 2009, Liu 2018 "Instruction SDC Vulnerability Prediction Using LSTM"; random forests: Liu 2019 KSII).
- Why it matters: cite this to cover the entire ML-prediction cluster economically, and to source the exact feature list (mask instructions, loop depth, transfer/branch) that the weighted model should at minimum include.
- Paper (open): https://proceedings.mlr.press/v101/liu19c/liu19c.pdf

### A.6 未能完全核实的条目（引用前务必在 DBLP 核对）

- **"PARIS" / "SDCPredictor"** as standalone named tools: the ML-SDC-prediction cluster was found (entries 29, 30, 31, 55) but no paper titled *PARIS* for SDC prediction at PRDC/DSN could be verified. Laguna's SVM-based work in this space is IPAS (entry 13). Drop the names or verify directly on DBLP.
- **SIED**: consistently referenced as an intra-block CFE technique in the RASM/RACFED papers, but the original publication record was not retrieved. Cite it through Vankeirsbilck's comparisons until verified.
- **"ASTERISC"**: the name collides with (a) AsteRISC, a flexible multi-cycle RISC-V core for design-space exploration (https://asterisc.readthedocs.io) and (b) ethereum-optimism/asterisc, a RISC-V fault-*proof* VM (https://github.com/ethereum-optimism/asterisc). Neither is an SIHFT compiler. A reference to "ASTERISC" for software fault tolerance is most likely a garbled reference to **ASPIS** or **ASTRAEUS** (entries 41, 45).
- **"FT-LLVM"**: no distinct project found. The real open LLVM duplication/TMR passes are COAST, ASPIS and compas-ft-riscv (entries 40–42); note that `compas-ft-riscv` is literally the "ft" LLVM RISC-V repository.
- **Code availability summary**: Minotaur, Shoestring, IPAS, SDCTune and nZDC have no public code. The open-code set in this topic is COAST, ASPIS, compas-ft-riscv, Trident/vTrident, gem5-approxilyzer, LLFI/LLTFI/LLFI-GPU, ZOFI (to-verify) and the ApproxDup Zenodo artifact.

### A.7 Must-read top 10（针对本课题排序）

1. **REDDI (EDHPC 2025)** — selective compiler hardening for spacecraft on-board software; the nearest living neighbor to this thesis. Read first.
2. **COAST (TNS 2019 + GitHub)** — open LLVM DWC/TMR for space, multi-ISA, radiation-validated. The implementation baseline and the yardstick for what remains to contribute.
3. **COMPAS (MECO 2022 + tum-ei-eda/compas-ft-riscv)** — open LLVM *backend* SIHFT for RV32/RV64 with selective hardening of critical functions. The RISC-V baseline and the largest novelty-overlap risk.
4. **SDCTune / Configurable Detection of SDC-causing Errors (CASES 2014 / TECS 2017)** — the published version of criticality scoring plus cost/benefit selection (loop depth, masking, distance to stores). Must be beaten or clearly differentiated.
5. **Approxilyzer (MICRO 2016)** + **gem5-Approxilyzer (DSN 2019, open source)** — the only framework unifying instruction-level approximability and resiliency under an output-quality threshold. The formal justification for delta and for the approximate version.
6. **SWIFT-R / TRUMP (DSN 2006 / IEEE Micro 2007)** — software TMR with in-register majority voting plus AN-coded cheap replicas. Mandatory baseline and the closest prior art to "1 exact + non-identical replicas".
7. **Trident (DSN 2018, open source)** — analytical per-instruction SDC probability without fault injection. Either the scorer or the scorer's strongest competitor; either way it belongs in the experiments.
8. **Shoestring (ASPLOS 2010)** — the origin of "protect only instructions close to output that symptoms will not catch". Ancestor of the distance-to-output feature; cite prominently and generalize honestly.
9. **ApproxDup (TCAD 2024)** + **Low-Cost Prediction-Based Fault Protection (CGO 2020)** — the two papers that already pair a cheap/approximate replica with a slack comparison. Read together; the approximate-TMR novelty statement must survive both.
10. **ASPIS (TODAES 2024, open source)** + **Quantifying Compiler-Induced Reliability Loss (ASP-DAC 2026)** — the modern open IR-level toolchain plus the proof that IR-level redundancy leaks coverage through the optimizer. Settle IR-vs-backend insertion before writing pass code.

Honorable mentions to skim rather than study: PaSS (LCTES 2020) for the cheapest effective CFC; FTXS@SC'19 for the validity limits of IR-level selection; Reghenzani & Guo (CSUR 2023) for the real-time/schedulability consequences of switching versions at runtime; the ATMR voter line (Electronics 2019 / X-Rel 2023) for the correctness condition a delta-voter must satisfy.
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
## G. SIMD/RVV 打包开销优化与向量通道冗余（服务方向 C）

**Scope**：方向 C 的工程问题是——LLVM pass 把两个近似副本打包进 `<2 x float>`，让 approximate TMR 的两个近似副本在一条 SIMD 指令里执行；但每个外部输入/常量都要付 `fptrunc` + 2×`insertelement`（再加 `shufflevector`/`extractelement` 还原），动态指令数被打包序列吃掉。HIGH 档 4.91x 对 SWIFT 5.22x、LLTFI-SID 4.79x 几乎没有优势，目标是压到 3.0–3.5x。目标平台 x86_64（SSE/AVX）与 RISC-V rv64gcv（RVV 1.0）。

本节覆盖四条技术线：(1) 已有的"用向量通道做冗余"先行工作（含它们各自怎么处理打包开销）；(2) LLVM SLP/VPlan 系的打包代价建模与消除技术；(3) RVV 1.0 codegen 的具体代价点（`vsetvli`、`vmv.v.x`/`vfmv.v.f`、LMUL、segment load、`vrgather`）；(4) 规避打包的替代方案（AoS→SoA、SWAR、低精度 double-pumping）。最后给出按收益/成本排序的 LLVM 级优化清单。

---

### G.0 先把成本账算清楚（这决定了后面哪条优化值得做）

当前每个"进入冗余域"的标量值的打包序列（IR 层）：

```llvm
; 当前（每个输入值 4 条 IR，x86 实测 3-5 条机器指令）
%h    = fptrunc float %x to half          ; 若近似副本用 fp16
%v0   = insertelement <2 x float> undef, float %a0, i32 0
%v1   = insertelement <2 x float> %v0,   float %a1, i32 1
...                                        ; 使用
%r0   = extractelement <2 x float> %res, i32 0
%r1   = extractelement <2 x float> %res, i32 1
```

关键观察（后面所有优化都从这三条派生）：

1. **两个近似副本的输入在绝大多数情况下是同一个标量**。即需要的不是任意 2-lane 打包，而是 **splat（广播）**。splat 在 x86 上是 1 条 `vbroadcastss`/`shufps`（甚至 0 条，若由 `vmovddup`/内存操作数折叠掉），在 RVV 上是 1 条 `vfmv.v.f`。两条 `insertelement` 是 LLVM 在 `undef` 基础上逐 lane 构造时的默认 lowering，**只要把 IR 写成 `shufflevector <2 x float> %vec, poison, <i32 0, i32 0>` 或 `insertelement`+zeroinitializer splat idiom，InstCombine/DAGCombine 会直接识别成 broadcast**。这是最大的单笔收益来源。
2. **常量输入根本不需要运行时指令**。`<2 x float> <float C, float C>` 是 `ConstantDataVector`，由常量池 load 或 `vmv.v.i`/立即数 splat 实现，可被 LICM 完全提到循环外。当前每个常量付 2×`insertelement` 说明 pass 在 IR 构造时没有走常量折叠路径。
3. **打包/解包应该在"冗余域边界"做，而不是每条指令做**。这就是 SLP 文献里的 pack/unpack cost 摊销问题（Larsen & Amarasinghe 已明确提出），也是 DRIFT/COMET 里"解耦同步点"的思想。

---

### G.1 用 SIMD/向量通道做冗余：直接先行工作

#### G1. SIMD-Based Soft Error Detection
- **First author / venue**：Zhi Chen（A. Nicolau, A. V. Veidenbaum, UC Irvine）— **ACM International Conference on Computing Frontiers (CF) 2016**, pp. 45–54
- **Mechanism**：这是与方向 C **最直接对标**的工作。编译器（LLVM）把原始标量指令与其副本打包进同一条 SIMD 指令的不同 lane：原始值放 lane 0，副本放 lane 1，一条 `addps`/`mulps` 同时算两份，在 store/branch 前用 `cmpps`+`movmskps` 做 lane 间比较。核心代价与方向 C 完全相同——**lane 填充（pack）与结果提取（unpack）**。论文的处理办法：(a) 对 load 来的值用 **broadcast（`movddup`/`pshufd`）而非两条 insert**；(b) 把 pack 点上推到基本块入口、unpack 点下压到 SoR 边界，使一次 pack 服务多条指令；(c) 对已被自动向量化的循环直接利用剩余空闲 lane（VLEN 未用满时冗余"免费"）。
- **Applicability（对 fptrunc+insertelement 的直接意义）**：**最高**。这篇是必引 baseline，也是方向 C "新颖性"的最大威胁，同时又是打包开销优化的现成配方。可直接照搬的三点：broadcast 替代双 insert、pack/unpack 边界外提、空闲 lane 复用。差异点（可辩护的创新）：该工作是**精确**副本 + 位级相等比较，没有 fp16 近似副本、没有 δ 阈值、没有分级恢复，也未做 RVV。
- **Code**：无公开仓库（CECS TR 15-04 为技术报告版）
- **URL**：https://dl.acm.org/doi/10.1145/2903150.2903457 ；TR：https://www.cecs.uci.edu/publication/simd-based-soft-error-detection

#### G2. Software Fault Tolerance for FPUs via Vectorization
- **First author / venue**：Zhi Chen（R. Inagaki, A. Nicolau, A. V. Veidenbaum）— **SAMOS 2015**（Int. Conf. on Embedded Computer Systems: Architectures, Modeling and Simulation）
- **Mechanism**：G1 的前身，专门针对**浮点单元**：把 FP 标量运算与其副本向量化到 `<2 x float>`/`<2 x double>`，用 SIMD FPU 一次算两份。明确讨论了"标量→向量"的提升代价，并指出当标量代码本身占用的是 SIMD FPU 的 scalar 子通道时，冗余执行**不增加功能单元占用**，开销几乎只剩 pack/unpack 和比较。
- **Applicability**：方向 C 的 `<2 x float>` 打包正是这篇的拓扑。它给出的论证（SSE/AVX 标量 FP 指令本就跑在向量 FPU 上，所以 2-lane 冗余近乎免费）可直接用来论证"为什么 2-lane 近似 TMR 的*执行*开销不是瓶颈，瓶颈全在 pack"。
- **URL**：https://doi.org/10.1109/SAMOS.2015.7363684 ；作者列表见 https://scholar.google.com/citations?user=jMFV0wcAAAAJ

#### G3. CAMFAS: A Compiler Approach to Mitigate Fault Attacks via Enhanced SIMDization
- **First author / venue**：Zhi Chen（J. Shen, A. Nicolau, A. Veidenbaum, N. F. Ghalaty, R. Cammarota）— **FDTC 2017**（Workshop on Fault Diagnosis and Tolerance in Cryptography）, pp. 57–64
- **Mechanism**：在 LLVM 中增强 SIMDization，使**冗余副本自动落入未被占用的向量 lane**，并把 pack/unpack 开销通过"增强 SIMD 化"（扩大向量化区域、合并打包序列）压到最低。对数据故障使用 intra-instruction redundancy (IIR)，支持"直接冗余"与"互补冗余"两种 lane 布局。
- **Applicability**：**高**。其 "enhanced SIMDization" 的具体手段——把更多标量代码一起向量化以让 pack 序列被多条指令摊销——正是把 4.91x 往下压的主路径。另外它的"互补冗余"（lane 1 存按位取反的副本）可在不增加 pack 成本的前提下提高对**同模故障**（如整条向量寄存器被同一 SEU 影响）的覆盖率，对空天场景有直接价值。
- **URL**：https://doi.org/10.1109/FDTC.2017.13 ；https://www.researchgate.net/publication/321664261

#### G4. Lightweight Fault Attack Resistance in Software Using Intra-Instruction Redundancy (IIR)
- **First author / venue**：Conor Patrick（B. Yuce, N. F. Ghalaty, P. Schaumont, Virginia Tech）— **SAC 2016**（Selected Areas in Cryptography, LNCS 10532, pp. 231–244）
- **Mechanism**：**"intra-instruction redundancy"一词的来源**。把算法 bitslice 化后，一个 32-bit 寄存器的不同 bit-slice 承载同一计算的多个冗余实例（空间冗余 Rs ∈ {1,2,4}）；冗余比较退化为寄存器内的掩码-异或-测试，**完全不需要 insertelement，因为"打包"是数据表示本身**。
- **Applicability**：**概念级关键**。它指出一条绕开 pack 的根本路径：如果冗余副本的 lane 布局是**数据表示的一部分**（从源头就以 packed 形式存在、以 packed 形式在函数间传递），pack 指令就不是"每次使用付一次"而是"程序入口付一次"。对方向 C 的直接映射：把冗余域的 ABI 改成 **packed `<2 x float>` 作为函数参数/返回值类型**（LLVM 里就是重写 function signature + 在 IR 层做 interprocedural 的"vector-of-replicas" 类型传播），这样跨函数调用不再解包再打包。
- **URL**：https://doi.org/10.1007/978-3-319-69453-5_13

#### G5. SKIVA / Architecture Support for Bitslicing（指令集支持的 intra-instruction redundancy）
- **First author / venue**：Pantea Kiaei（P. Schaumont 等）— **Cryptology ePrint 2021/1236**；SKIVA 原始工作见 Kiaei & Schaumont, "SKIVA: Flexible and Modular Side-channel and Fault Countermeasures"（IEEE **TCAD/TCHES** 系）；Skiva-V 为 RISC-V 版本
- **Mechanism**：为 SPARC V8 / RISC-V 增加 ISE，支持 (a) **direct redundancy**（lane 存相同副本）与 (b) **complementary redundancy**（lane 存取反副本）的**一条指令内比较**，以及用于在 slice 之间做转置/交织的专用指令（`subrot` 等），把"重排 lane 布局"从多条 shuffle 压成 1 条。
- **Applicability**：**中-高**。两点可用：(1) 它证明"lane 布局重排"是冗余方案的主要开销，值得专门指令——这反过来支持方向 C 在 RVV 上用 `vrgather`/`vslideup` 而不是逐 lane insert；(2) 若论文允许提"轻量 ISA 扩展"作为 future work，Skiva-V 是现成的 RISC-V 先例（一条 `vpack2.f32` 完成 fptrunc+splat）。
- **Code**：https://github.com/Secure-Embedded-Systems （Skiva 相关 artifact）
- **URL**：https://eprint.iacr.org/2021/1236

#### G6. Usuba: High-Throughput and Constant-Time Ciphers, by Construction
- **First author / venue**：Darius Mercadier（P.-É. Dagand, Sorbonne/LIP6）— **PLDI 2019**
- **Mechanism**：bitslicing 编译器，用 "slicing types" 把逻辑数据宽度与物理寄存器/向量宽度解耦，编译器自动决定 slice 布局（含 SWAR 与 SSE/AVX 后端），并自动生成 transpose/pack 代码。与 SKIVA 结合时，部分 slice 用于冗余、部分用于掩码份额。
- **Applicability**：**中**。方法论上非常有用：Usuba 的"slicing type"正是方向 C 需要的类型系统——在 IR 类型里把"这是 N 个副本的打包"显式化，让编译器而不是人来放 pack/unpack 并自动做常量折叠。可作为"类型驱动的冗余打包"设计论证来源。
- **Code**：https://github.com/usubalang/usuba
- **URL**：https://dl.acm.org/doi/10.1145/3314221.3314636 ；博士论文（含 SKIVA 章节）：https://dariusmercadier.com/assets/documents/thesis.pdf

#### G7. DRIFT: Decoupled CompileR-Based Instruction-Level Fault-Tolerance
- **First author / venue**：Konstantina Mitropoulou（V. Porpodas, M. Cintra, Univ. of Edinburgh）— **LCPC 2013**（LNCS 8664, pp. 217–233）
- **Mechanism**：传统 instruction-level 冗余（EDDI/SWIFT）在每个 store/branch 前插入比较，导致"原始指令—副本指令—比较"链式依赖，ILP 被破坏。DRIFT **解耦**：把多个检查点的比较聚合、延迟到基本块或更大区域末尾批量执行，用一个累积的 "mismatch mask" 代替逐点分支，恢复 ILP，开销从 1.29x 降到 1.06x（相对未保护）。
- **Applicability**：**高（结构性）**。对方向 C 的意义不在 pack 本身，而在 **compare/vote 的指令数**：如果当前 4.91x 里有相当比例是"每次 store 前的 δ 阈值比较 + 分支"，DRIFT 的批量化方案可以把 N 次 `fcmp`+`br` 压成 N 次 `fsub`/`fcmp` 的位或累积 + 1 次分支。在向量形式下更便宜：`vmfgt` 产生 mask，多点 mask 用 `vmor` 累积，区域末尾一条 `vcpop`/`vfirst` 判定。
- **URL**：https://link.springer.com/chapter/10.1007/978-3-319-09967-5_12 ；书：https://link.springer.com/book/10.1007/978-3-319-09967-5

#### G8. Exploiting Idle Hardware to Provide Low Overhead Fault Tolerance（+ COMET）
- **First author / venue**：Konstantina Mitropoulou（V. Porpodas, M. Cintra）— **ACM TACO** 13(4), 2016, Art. 41；配套 **COMET: Communication-Optimised Multi-Threaded Error-Detection**, **PACT/CASES 2016**
- **Mechanism**：把冗余线程/副本调度到空闲的功能单元与空闲 issue slot；COMET 优化原始线程与检测线程之间的通信（队列、批量传输），使通信而非计算成为被优化对象。
- **Applicability**：**中**。"把冗余放进空闲资源"是方向 C 在 x86 上的核心论据（标量 FP 指令只用了 SIMD FPU 的 1/4~1/8 通道）。TACO 2016 提供了可引用的量化方法学（如何证明冗余副本"免费"）。
- **URL**：https://dl.acm.org/doi/10.1145/3001935 ；COMET：https://dl.acm.org/doi/10.1145/2968455.2968508

#### G9. Warped-DMR: Light-weight Error Detection for GPGPU
- **First author / venue**：Hyeran Jeon（M. Annavaram, USC）— **MICRO 2012**, pp. 37–47
- **Mechanism**：利用 GPU warp 中**未被激活的 SIMT lane**（因分支分歧或 warp 未满）机会性地重执行活跃线程的计算（intra-warp DMR），并用 temporal DMR 补齐全活跃 warp 的情况。96% 错误覆盖率、最坏 16% 性能开销，**无需额外执行单元，也无需程序员介入**。
- **Applicability**：**中-高（论证用）**。这是"空闲 lane 冗余是免费的"最被引用的证据。方向 C 可据此论证：在 VLEN=128/256 的 RVV 上，`<2 x float>` 只占 LMUL=1 的极小一部分，若把冗余粒度提升到 `<2*VL x float>`（即一个向量里既做数据并行又做冗余），pack 成本被 VL 摊薄到接近 0。
- **URL**：https://dl.acm.org/doi/10.1109/MICRO.2012.13 ；https://www.semanticscholar.org/paper/c15a6798258dcdd8f25752719cd0adeb96f8abd5

#### G10. Warped-RE: Low-Cost Error Detection and Correction in GPUs
- **First author / venue**：Mohammad Abdel-Majeed（W. Dweik, H. Jeon, M. Annavaram, USC）— **DSN 2015**, pp. 331–342
- **Mechanism**：Warped-DMR 的扩展，保证**每条** warp 指令都被冗余验证：利用 lane 内的 value similarity（同 warp 内线程值相同则一次计算即可）腾出 lane 做冗余，检测到错误后重执行纠正，形成检测+纠正统一框架。
- **Applicability**：**中**。其 "value-similarity 腾 lane" 思路对方向 C 的映射：若两个近似副本的输入相同（必然），则**不需要两份输入存储**，只需要一次 splat；而若近似副本的精度降级使两 lane 可以用更窄元素，可在同宽度向量里塞更多副本。
- **URL**：https://ieeexplore.ieee.org/document/7266862

#### G11. Real-World Design and Evaluation of Compiler-Managed GPU Redundant Multithreading
- **First author / venue**：Jack Wadden（A. Lyashevsky, S. Gurumurthi, V. Sridharan, K. Skadron）— **ISCA 2014**
- **Mechanism**：首个在真实 GPU 上实现的编译器管理 RMT：Intra-Group RMT（同 wavefront 内复制 work-item，原始与冗余在**同一 SIMD 单元的不同 lane** 锁步执行）与 Inter-Group RMT。明确界定 SoR 覆盖 VRF + 全部 SIMD 功能单元。
- **Applicability**：**中-高**。它的 SoR 论证可直接套用：**当两个副本占据同一向量寄存器的不同 lane 时，VRF 与向量 ALU 都在保护域内，但指令取指/译码不在**——这正是方向 C 必须在论文里诚实声明的覆盖边界。
- **URL**：https://www.cs.virginia.edu/~skadron/Papers/wadden_gpurmt_isca2014.pdf

#### G12. Compiler Techniques to Reduce the Synchronization Overhead of GPU Redundant Multithreading
- **First author / venue**：Manish Gupta（UCSD / AMD Research）— **DAC 2017**
- **Mechanism**：指出 Intra-Group RMT 的主开销是**同步/通信**（55%，Inter-Group 可达 1726%），而非冗余计算本身；提出编译器优化减少跨副本通信与比较点（LDS 通信批量化、比较点上移、冗余的"按需"同步）。
- **Applicability**：**高（方法学直接可移植）**。把"同步开销 ≫ 计算开销"这一结论换成方向 C 的语言就是"**pack/unpack/compare 开销 ≫ SIMD 冗余计算开销**"。这篇给出的三类减法（批量化、点上移、按需）就是优化清单里 #3/#5/#7 的文献支撑。
- **URL**：https://cseweb.ucsd.edu/~m7gupta/papers/dac17.pdf

#### G13. Optimizing Software-Directed Instruction Replication for GPU Error Detection (SInRG)
- **First author / venue**：Abdulrahman Mahmoud（S. K. S. Hari, M. B. Sullivan, T. Tsai, S. W. Keckler, UIUC + NVIDIA）— **SC 2018**
- **Mechanism**：纯软件（编译器）指令复制 + 选择性保护，在 GPU 上通过**只复制"高 SDC 贡献"指令链**与优化比较点位置，把开销从全复制的 2x+ 降到显著更低；提供复制粒度/边界的系统性设计空间探索。
- **Applicability**：**高**。与方向 A（selective hardening）交叉：把 4.91x 压到 3.0–3.5x 的最直接方式之一就是"**少打包**"——只对 SDC 贡献最高的指令子图建立 packed 冗余域，其余不打包。SInRG 给出了选择准则与 ROI 曲线。
- **Code**：相关工具链 https://github.com/NVlabs/nvbitfi （故障注入侧）
- **URL**：http://rsim.cs.illinois.edu/Pubs/18-SC-SInRG.pdf

#### G14. S-SETA / Chielle 系列（软件冗余的开销削减）
- **First author / venue**：Eduardo Chielle（F. L. Kastensmidt 等, UFRGS）— **IEEE TNS** 62(6), 2015（S-SETA）；**IEEE TNS** 63(4), 2016（Reliability on ARM Processors Against Soft Errors Through SIHFT）；"Overhead Reduction in Data-Flow Software-Based Fault Tolerance Techniques"（Springer 章节）
- **Mechanism**：系统化的 SIHFT 开销削减：选择性断言插入、控制流签名合并、数据流副本的签名合并、不变量检查替代完整复制。TNS 2016 给出 ARM 上含 NEON 的实测开销分解。
- **Applicability**：**中-高**。这是方向 C 在"开销对标"章节必须引用的一组数据（SWIFT 5.22x 的那条线就来自这一谱系）。"Overhead Reduction" 那章提供了指令数分解方法（复制 / 比较 / 控制流检查各占多少），方向 C 应该用同一分解法说明 4.91x 里 pack 占多少——这是把问题量化的前提。
- **URL**：https://ieeexplore.ieee.org/document/7322301 （S-SETA）；https://doi.org/10.1109/TNS.2016.2525735

#### G15. Using Approximate Computing and Selective Hardening to Reduce Overheads in SIHFT
- **First author / venue**：Alexander Aponte-Moreno（F. Restrepo-Calle, C. Pedraza, UNAL）— **Electronics** 8(12):1539, 2019
- **Mechanism**：把近似计算（降精度、循环穿孔）与选择性加固结合，在软件层用"近似副本"替代精确副本以降低 SIHFT 的时间/空间开销；给出可靠性-开销 Pareto。
- **Applicability**：**高（与课题主张最接近的软件层近似冗余）**。与方向 C 的差异：没有 SIMD 打包、没有 δ 阈值表决器、没有 RVV。但它已经提出"近似副本省开销"，所以方向 C 的创新必须落在**"打包进 SIMD lane 使近似副本的执行开销趋零" + "分级恢复"**上，而不是"近似副本本身"。
- **URL**：https://www.mdpi.com/2079-9292/8/12/1539

---

### G.2 LLVM 向量化与打包代价：可直接复用的算法

#### G16. Exploiting Superword Level Parallelism with Multimedia Instruction Sets（SLP 原始论文）
- **First author / venue**：Samuel Larsen（S. Amarasinghe, MIT）— **PLDI 2000**
- **Mechanism**：SLP 的定义与四步算法：找同构无依赖语句对 → 建 pack → 扩展 pack → 调度。**论文本身就把 "additional operations to pack the initial data into the source registers and unpack the results" 列为 SLP 的固有代价**，并给出用"已存在的向量形式操作数优先配对"来避免新建 pack 的启发式。
- **Applicability**：**基础必引**。方向 C 的 `<2 x float>` 冗余打包在形式上就是一个人为构造的 SLP pack（两个"同构语句"恰好是原指令和它的近似副本）。因此**整个 SLP 的 pack-cost 削减文献全部可以直接移植**——这是本节最重要的结论。
- **URL**：https://dl.acm.org/doi/10.1145/349299.349320 ；作者页（含 PDF）：https://www.cag.lcs.mit.edu/~slarsen ；导读：https://www.cs.cornell.edu/courses/cs6120/2020fa/blog/slp

#### G17. PSLP: Padded SLP Automatic Vectorization
- **First author / venue**：Vasileios Porpodas（A. Magni, T. M. Jones, Cambridge）— **CGO 2015**
- **Mechanism**：对**非同构**指令序列，通过插入**冗余的 padding 指令**（计算结果不被使用的"假"指令）把序列补成同构，从而可向量化；最高 63% 提升。注意：**它主动插入冗余指令以换取向量化**。
- **Applicability**：**非常高（思路同源）**。两点：(1) PSLP 证明"插入冗余指令使之可向量化"是正收益的——这正是 approximate TMR 打包的合法性论证；(2) **反向使用**：方向 C 的近似副本本质是"免费 padding"，即原本 PSLP 要插入的 dummy 指令可以用近似副本来充当，于是**冗余与向量化互相补贴**。LLVM 里有过把 PSLP 并入 SLPVectorizer 的讨论（见 llvm-dev 线程），代码路径明确。
- **Code**：LLVM 中的讨论/补丁：https://lists.llvm.org/pipermail/llvm-dev/2020-October/145578.html
- **URL**：https://ieeexplore.ieee.org/document/7054199 ；PDF：https://www.cl.cam.ac.uk/~tmj32/papers/docs/porpodas15-cgo.pdf

#### G18. Throttling Automatic Vectorization: When Less Is More (TSLP)
- **First author / venue**：Vasileios Porpodas（T. M. Jones）— **PACT 2015**
- **Mechanism**：SLP 的"全有或全无"决策会在 pack/unpack 代价超过向量化收益时产生负优化。TSLP 在 SLP 图上做**部分向量化**：对每个子图独立评估"向量化到哪一层最优"，必要时在中途停止（保留标量尾部），避免昂贵的 pack。LLVM 的 VPlan 文档明确把 TSLP 列为 "Plan Step 2.b" 的对照物。
- **Applicability**：**极高，这是优化清单的核心算法**。方向 C 需要的恰恰是"**打包域边界的最优选择**"：对一个指令子图，决定在哪里 pack 进入、在哪里 unpack 退出，使 (pack 条数 + unpack 条数) 最小而被摊销的指令数最大。TSLP 的收益模型可以几乎逐字搬过来（把"向量化收益"替换成"冗余保护收益"，把"pack 代价"保留原样）。
- **URL**：https://ieeexplore.ieee.org/document/7429331 ；https://scholar.google.com/citations?user=5AHPuNQAAAAJ

#### G19. Look-ahead SLP: Auto-vectorization in the Presence of Commutative Operations
- **First author / venue**：Vasileios Porpodas（R. C. O. Rocha, L. F. W. Góes）— **CGO 2018**, pp. 163–174
- **Mechanism**：标准 SLP 的操作数重排（operand reordering）是贪心、逐 lane、无回溯的，容易因局部选择导致下游 opcode 不匹配而被迫插入 shuffle。LSLP 在重排时**向前看多层** def-use 链，选择能让更深层保持同构的操作数排列，从而减少 shuffle/pack。
- **Applicability**：**高**。方向 C 一旦让两 lane 的内容不再完全相同（例如 lane 0 精确、lane 1 近似，或两个不同的近似变体），lane 对齐就会产生 shuffle。LSLP 的多层向前看正是用来消除这类 shuffle 的；其实现已在 LLVM `SLPVectorizer.cpp` 的 `reorderInputsAccordingToOpcode`/`VLOperands` 中（look-ahead 启发式已上游）。
- **Code**：上游 LLVM `llvm/lib/Transforms/Vectorize/SLPVectorizer.cpp`（`LookAheadHeuristics` 类）
- **URL**：https://dl.acm.org/doi/10.1145/3179541.3168807 ；https://www.researchgate.net/publication/323507137

#### G20. VW-SLP: Auto-vectorization with Adaptive Vector Width
- **First author / venue**：Vasileios Porpodas（R. C. O. Rocha, L. F. W. Góes）— **PACT 2018**, Art. 12
- **Mechanism**：SLP 固定向量宽度的限制导致图在宽度不匹配处断裂。VW-SLP 在 SLP 图内**自适应改变向量宽度**（宽度可在节点间增减），用更少的 shuffle 把不同宽度的区域缝合起来。
- **Applicability**：**高（RVV 尤其）**。方向 C 的"自适应"容错框架里，HIGH 档可能是 3 副本、MEDIUM 档 2 副本、LOW 档 1 副本——这正是**同一程序内不同宽度的向量区域**。VW-SLP 提供了在同一 SLP 图里混合宽度并最小化过渡 shuffle 的算法，对应到 RVV 上就是 LMUL/`vl` 的区域性切换。
- **URL**：https://dl.acm.org/doi/10.1145/3243176.3243189

#### G21. Super-Node SLP: Optimized Vectorization for Code Sequences Containing Operators and Their Inverse Elements
- **First author / venue**：Vasileios Porpodas（R. C. O. Rocha, E. Brevnov, L. F. W. Góes, T. Mattson, Intel）— **CGO 2019**, pp. 206–216
- **Mechanism**：把 `+`/`-`（以及 `*`/`/`）视为同一"super-node"，允许混合加减的序列被向量化而不需要额外 shuffle 或 `addsub` 指令，扩大了可向量化区域。
- **Applicability**：**中**。对方向 C 的意义：δ 阈值比较里的 `a-b`、`b-a`、`|a-b|` 这类混合符号序列可以被打包进同一向量操作，减少表决器的指令数。
- **URL**：https://dl.acm.org/doi/10.1109/CGO.2019.8661192

#### G22. PostSLP: Cross-Region Vectorization of Fully or Partially Vectorized Code
- **First author / venue**：Vasileios Porpodas（P. Ratnalikar, Intel）— **LCPC 2019**, pp. 15–31
- **Mechanism**：在常规 SLP **之后**运行的 pass，跨区域（跨基本块/跨已向量化片段）合并向量代码，**把分散的 pack/unpack 序列合并与消除**——即专门针对"已经向量化但过渡代码冗余"的情形。
- **Applicability**：**极高，这是最贴题的一篇**。方向 C 的 IR 在冗余打包 pass 之后正是"部分向量化、中间塞满 insertelement/extractelement"的形态。PostSLP 的目标函数就是消除这些过渡指令。实现上对应"在冗余打包 pass 之后再跑一遍 SLPVectorizer + InstCombine + 自定义的 pack-merging pass"。
- **URL**：https://link.springer.com/chapter/10.1007/978-3-030-72789-5_2 ；条目见 https://scholar.google.com/citations?user=5AHPuNQAAAAJ

#### G23. goSLP: Globally Optimized Superword Level Parallelism Framework
- **First author / venue**：Charith Mendis（S. Amarasinghe, MIT）— **PACT/OOPSLA 2018**（Proc. ACM Program. Lang. 2(OOPSLA):110）
- **Mechanism**：用**整数线性规划 (ILP)** 在整个函数范围内求解 statement packing 问题（成对最优），并用动态规划求解最优的向量置换（vector permutation）选择，显式把 **pack/unpack/shuffle 成本编码进目标函数**。相对 LLVM SLP 在 SPEC2017 上有 7.58% 的额外加速。
- **Applicability**：**极高**。方向 C 的打包决策问题（哪些值在哪里被 pack、哪些结果在哪里 unpack、哪些 pack 可共享）就是 goSLP 的目标函数的一个实例，只需把"向量化带来的指令减少"换成"冗余保护的覆盖收益"。若要做出一个有分量的编译器贡献，**"把近似冗余打包形式化为 ILP 并求解"** 是最可发表的切口。
- **Code**：https://github.com/charithmendis/goSLP （基于 LLVM 的实现）
- **URL**：https://dl.acm.org/doi/10.1145/3276480 ；MIT 论文版：https://dspace.mit.edu/bitstreams/d2d6d641-f1eb-41fc-a757-98241970c62a/download

#### G24. VeGen: A Vectorizer Generator for SIMD and Beyond
- **First author / venue**：Yishen Chen（C. Mendis, M. Carbin, S. Amarasinghe, MIT）— **ASPLOS 2021**
- **Mechanism**：提出 **Lane Level Parallelism (LLP)** 模型（比 SIMD 更一般，覆盖 `addsubpd`、VNNI 这类非 SIMD 向量指令），并自动从指令的文档语义生成 vectorizer，联合做向量化与向量指令选择。x265 `idct4` 上相对 LLVM 向量器 3–3.4x。
- **Applicability**：**高**。两个用法：(1) LLP 模型给"两个 lane 做不同的事（一个精确、一个近似）"提供了形式化框架，这在纯 SIMD 模型里是表达不出来的；(2) VeGen 能自动发现"一条指令完成 fptrunc+广播"这类复合指令（x86 的 `vcvtps2ph`+`vbroadcast` 组合、RVV 的 `vfncvt`+`vrgather`），即**自动找出替代当前 4 条 IR 序列的最短机器指令序列**。
- **Code**：https://github.com/ychen306/vegen
- **URL**：https://dl.acm.org/doi/10.1145/3445814.3446692 ；https://www.asplos-conference.org/asplos2021/abstracts/asplos21-paper28-extended_abstract.pdf

#### G25. All You Need Is Superword-Level Parallelism: Systematic Control-Flow Vectorization with SLP
- **First author / venue**：Yishen Chen（C. Mendis, S. Amarasinghe, MIT）— **PLDI 2022**
- **Mechanism**：把控制流向量化（if-conversion、循环向量化）统一归约为 SLP：引入 "SuperVectorization" 框架，用 λ-SLP 式的统一表示同时处理跨基本块、跨循环迭代的打包，使 pack 决策在更大的作用域内全局优化。
- **Applicability**：**高**。方向 C 的冗余域往往跨多个基本块（含分支），当前实现很可能在每个基本块边界重新 pack。SuperVectorization 提供"跨控制流保持 packed 形式"的算法，等价于把 pack 提升到支配树的最近公共支配块（这正是优化清单 #2）。
- **Code**：https://github.com/ychen306/supervec
- **URL**：https://dl.acm.org/doi/10.1145/3519939.3523701

#### G26. Vectorization-Aware Loop Unrolling with Seed Forwarding
- **First author / venue**：Rodrigo C. O. Rocha（V. Porpodas, P. Petoumenos, L. F. W. Góes, Z. Wang, M. Cole, H. Leather）— **CC 2020**（Int. Conf. on Compiler Construction）
- **Mechanism**：展开因子与 SLP 可向量化性强耦合。该工作在展开时**前向传递 SLP "seed" 信息**，让展开决策以"展开后能形成多少低成本 pack"为目标，而不是以展开收益为目标。
- **Applicability**：**高（实操性强）**。对方向 C：**对冗余域所在循环做 2x/4x 展开，使多个迭代的 splat 共享一次 pack**，并让 LICM 把不变部分提出循环。这是把 pack 成本从 "O(指令数)" 降到 "O(指令数 / 展开因子)" 的最廉价手段。
- **URL**：https://dl.acm.org/doi/10.1145/3377555.3377890 ；PDF：https://www.pure.ed.ac.uk/ws/files/133438723/Vectorization_Aware_Loop_ROCHA_DOA23122019_AFV.pdf

#### G27. Autovesk: Automatic Vectorized Code Generation from Unstructured Static Kernels Using Graph Transformations
- **First author / venue**：Hayfa Tayeb（B. Bramas, M. Faverge, 等, Inria）— **ACM TACO** 21(1), 2023/2024
- **Mechanism**：把标量计算图直接当作图变换问题，自动插入/消除 shuffle、blend、broadcast，为非结构化 kernel 生成向量代码；对 pack 序列做显式的图级代价最小化。
- **Applicability**：**中-高**。提供"把 insertelement/extractelement 作为图节点统一优化"的实现范式，尤其适合方向 C 这种人为构造的、非自然同构的向量图。
- **Code**：https://gitlab.inria.fr/bramas/autovesk
- **URL**：https://dl.acm.org/doi/10.1145/3631709 ；arXiv：https://arxiv.org/pdf/2301.01018

#### G28. LLVM VPlan（Vectorization Plan）基础设施
- **First author / venue**：Ayal Zaks / Florian Hahn 等（Intel/Mobileye + Apple）— LLVM 官方设计文档 + LLVM Dev Meeting 2023 技术报告
- **Mechanism**：三步法 Legal → Plan（构建多个 VPlan、**计算代价**、VPlan-to-VPlan 变换、裁剪高代价方案）→ Execute（只有这一步修改 IR）。关键是 **VPlan 把"打包/解包"显式建模为 recipe（`VPWidenRecipe`、`VPScalarIVStepsRecipe`、`VPReplicateRecipe`、`VPInstruction::Broadcast`）**，代价在 `VPCostContext` / `TargetTransformInfo` 里查询。
- **Applicability**：**极高（实现落点）**。方向 C 若把冗余打包实现为一个 **VPlan-to-VPlan transform** 而不是直接改 IR，就能：(1) 复用 `TTI::getShuffleCost(SK_Broadcast)` / `getVectorInstrCost(Instruction::InsertElement)` 做真实代价决策；(2) 自动享受 VPlan 既有的 "hoist invariant recipe / sink scalar" 优化；(3) 同一套 pass 在 x86 与 RVV 上自动用各自代价模型选择不同打包策略。**这应当是方向 C 的工程主线重构方向。**
- **Code**：`llvm/lib/Transforms/Vectorize/VPlan*.cpp`、`llvm/lib/Analysis/TargetTransformInfo.cpp`
- **URL**：https://llvm.org/docs/VectorizationPlan.html ；路线图 slides：https://llvm.org/devmtg/2023-10/slides/techtalks/Hahn-VPlan-StatusUpdateAndRoadmap.pdf ；自动向量化总览：https://llvm.org/docs/Vectorizers.html

#### G29. LLVM 中的 splat / 常量向量惯用法（工程参考，非论文）
- **来源**：LLVM LangRef + InstCombine/DAGCombine 实现
- **Mechanism（必须掌握的四个事实）**：
  1. **splat 的规范 IR 形式**是 `insertelement <2 x float> poison, float %x, i64 0` 后跟 `shufflevector <2 x float> %t, poison, <2 x i32> zeroinitializer`。InstCombine 的 `foldInsSequenceIntoSplat` / `SimplifyDemandedVectorElts` 会识别它，`X86ISelLowering` 的 `lowerBuildVectorAsBroadcast` 把它降成 **1 条 `vbroadcastss`**（AVX）或 `shufps $0`（SSE）；RISCV 降成 **1 条 `vfmv.v.f`**。**用 `poison` 而不是 `undef` 很重要**，否则部分折叠会被阻断。
  2. 常量向量用 `ConstantDataVector::getSplat(2, ConstantFP::get(...))` 直接构造 → 落到 `.rodata` 常量池，运行时 **0 条指令**（x86 可被折叠为内存操作数；RVV 用 `vmv.v.i`/`vmv.v.x` + `li`，LICM 后在循环外）。
  3. `llvm.vector.insert` / `llvm.vector.extract` 内在函数用于 **scalable↔fixed 向量子向量插取**，在 RVV 上降为 `vslideup`/`vslidedown`（1 条），比逐 lane insert 便宜；`llvm.experimental.vector.splice`、`llvm.vector.interleave2`/`deinterleave2` 则直接对应 RVV 的 segment load/store 与 `vzip`-类操作。
  4. **IRBuilder 的现成 API**：`IRBuilder::CreateVectorSplat(unsigned NumElts, Value *V)` 一次生成规范 splat 序列（内部就是 insert+shuffle-zeroinitializer）；`CreateShuffleVector(V, PoisonValue, ZeroMask)`。**只要把当前两条 `insertelement` 换成 `CreateVectorSplat(2, X)`，机器指令数立刻从 2–3 条降到 1 条甚至 0 条。**
- **Applicability**：**这是零成本、立刻可做、收益确定的第一步**。
- **URL**：https://llvm.org/docs/LangRef.html#llvm-vector-insert-intrinsic ；https://llvm.org/doxygen/classllvm_1_1IRBuilderBase.html

---

### G.3 RISC-V RVV 1.0 codegen 的具体代价点

#### G30. LLVM RISC-V Vector Extension 后端建模与 `RISCVInsertVSETVLI`
- **来源**：LLVM 官方文档 "RISC-V Vector Extension" + `llvm/lib/Target/RISCV/RISCVInsertVSETVLI.cpp`
- **Mechanism**：RVV 用 **scalable vector type** `<vscale x n x ty>` 建模，`n` 与 `ty` 分别控制 **LMUL 与 SEW**。`RISCVInsertVSETVLI` 在**向量寄存器分配之后**运行，做数据流分析以发射**尽可能少的 `vsetvli`**，并在只需改 `vtype` 而不需改 `vl` 时发射更便宜的 `vsetvli x0, x0, ...`。随后 `RISCVExpandPseudoInsts` 展开 `PseudoVSETVLI`。
- **Applicability（方向 C 在 RVV 上的最大坑）**：`<2 x float>` 在 RVV 上会被映射到 **SEW=32, LMUL=1/4（或 mf2 等 fractional LMUL），`vl=2`**。如果冗余域里标量代码和向量代码交替出现，**每次切换都可能触发一条 `vsetvli`**——这在动态指令数里是纯开销，而且是最容易被忽略的那部分。对策：(a) 让整个冗余域使用**同一个 SEW/LMUL 配置**，使 `RISCVInsertVSETVLI` 的数据流分析能合并；(b) 避免在冗余域内部混用 fp16（SEW=16）与 fp32（SEW=32），否则每次 `fptrunc` 都附带一次 `vtype` 切换——**这可能是 4.91x 里一个被严重低估的来源**；(c) 用 `vsetivli` 立即数形式（`vl` 为小常数 2 时适用）。
- **URL**：https://llvm.org/docs/RISCV/RISCVVectorExtension.html

#### G31. Improvements to RISC-V Vector Code Generation in LLVM（2025 最新后端状态）
- **First author / venue**：Luke Lau（Igalia）— **RISC-V Summit Europe 2025**（Paris, May 2025），Paper P1.4.03
- **Mechanism**：报告 LLVM RVV 后端近两年的改进：(1) **`vsetvli` 插入 pass 移到寄存器分配之后**，使调度器可以跨 `vl`/`vtype` 变化移动向量指令（此前被阻止）；(2) `memcmp`/`memcpy` 的向量内联；(3) **widening bf16/fp16 会占用 2 倍 LMUL，需在代价模型中体现对吞吐与寄存器压力的影响**；(4) 明确列出"RVV 代价模型的缺口会导致开启 RVV 比不开启更慢"为待修项。
- **Applicability**：**极高且时效性最强**。第 (3) 点直接命中方向 C：**用 fp16 做近似副本在 RVV 上并不省，因为 widening 操作吃 2×LMUL**。第 (1) 点意味着应当使用 **LLVM 19+ / 20+**，否则 `vsetvli` 周围的调度障碍会额外抬高指令数。第 (4) 点说明方向 C 必须自己测量而不能相信默认 RVV 代价模型。
- **URL**：https://riscv-europe.org/summit/2025/media/proceedings/2025-05-15-RISC-V-Summit-Europe-P1.4.03-LAU-abstract.pdf

#### G32. The Saturn Vector Unit（RVV 1.0 微架构代价的开源参考）
- **First author / venue**：Jerry Zhao（D. Grubb, M. Rusch, T. Wei, K. Anderson, B. Nikolić, K. Asanović, UC Berkeley）— **UCB/EECS-2024-215 技术报告**, Dec 2024（配套 Chisel RTL 生成器）
- **Mechanism**：完整合规的参数化 RVV 1.0 向量单元，面向"架构向量长度 ≈ 2× 数据通路宽度"的**短向量**场景；支持 indexed/strided/**segmented** load/store，并实现 **streaming segment buffer**——一个 2D 微架构结构，在写回时对从内存流入的数据做**流式转置**，使 RGB 这类 AoS 结构能被一条 segment load 直接拆进不同向量寄存器。
- **Applicability（两条非常具体）**：
  1. **segment load 可以免费完成"打包"**：如果把两个副本所需的输入在内存里按 `{x, x}` 或 `{x_exact, x_approx}` 交织存放，一条 `vlseg2e32.v` 就同时完成 load + 分通道，**彻底取代 load + fptrunc + 2×insertelement**。Saturn 的 streaming segment buffer 说明这在真实短向量微架构上是高效的、不是纸面优化。
  2. 提供可综合的 RTL，可用来**实测** `vfmv.v.f`、`vrgather`、`vslideup` 与 segment load 的真实周期数，为方向 C 的 RVV 代价模型提供可信数据（而不是引用厂商手册）。
- **Code**：https://github.com/ucb-bar/saturn-vectors （Chisel RTL，BSD-3，159+ stars；需配合 Chipyard）
- **URL**：https://www2.eecs.berkeley.edu/Pubs/TechRpts/2024/EECS-2024-215.html ；手册 PDF：https://www2.eecs.berkeley.edu/Pubs/TechRpts/2024/Archive/EECS-2024-215.pdf

#### G33. Ara / AraXL：长向量 RVV 处理器（LMUL 与向量-标量搬移代价）
- **First author / venue**：Matheus Cavalcante（F. Schuiki, F. Zaruba, M. Schaffner, L. Benini, ETH Zürich）— **IEEE TVLSI** 28(2):530–543, 2020（Ara）；"A 'New Ara' for Vector Computing"（RVV 1.0 版, arXiv 2210.08882）；AraXL（超宽长向量）
- **Mechanism**：CVA6 的 RVV 协处理器，lane 数可配（最多 16 lanes，4096-bit VLEN），split VRF。文档明确指出：**长向量能摊销 "V 指令的 setup time"（含 `vsetvli` 与标量-向量搬移），短向量则不能**；FPU 利用率在大问题规模下 >98%。
- **Applicability**：**高（决定 RVV 上的打包粒度）**。核心结论对方向 C：**在长向量 RVV 实现上，`vl=2` 的冗余打包是最糟糕的配置**——setup 开销（`vsetvli` + `vfmv.v.f`）完全无法摊销。正确做法是把冗余维度与数据并行维度**合并**：对长度 N 的循环，用 `vl = 2N'` 的向量，前 N' 个 lane 放精确/副本 A、后 N' 个放副本 B（或用奇偶交织），这样一次 `vfmv`/一次 `vsetvli` 服务 N' 个元素的冗余。
- **Code**：https://github.com/pulp-platform/ara ；https://github.com/pulp-platform/AraXL
- **URL**：https://doi.org/10.1109/TVLSI.2019.2950087 ；RVV1.0 版：https://arxiv.org/html/2210.08882v2

#### G34. Vicuna: A Timing-Predictable RISC-V Vector Coprocessor
- **First author / venue**：Michael Platzer（P. Puschner, TU Wien）— **ECRTS 2021**, LIPIcs vol. 196, pp. 1:1–1:18
- **Mechanism**：32-bit 整数 RVV（`Zve32x`）协处理器，**无时序异常（timing-anomaly free）**，可做紧致 WCET 分析；通过 CORE-V-XIF 接入 CV32E40X/Ibex；向量寄存器宽度、流水线数量/布局、内存接口宽度均可配。
- **Applicability**：**中-高（空天场景特别相关）**。方向 C 面向空天计算，**WCET 可分析性**比平均性能更重要。Vicuna 是唯一提供"向量冗余执行的时序可预测性"论证基础的开源 RVV 实现，可用来回答审稿人"自适应容错框架在实时约束下如何保证最坏情况"这一问题。同时它是 FPGA 可部署的，适合做辐射测试平台的目标核。
- **Code**：https://github.com/vproc/vicuna ；文档：https://vicuna.readthedocs.io/
- **URL**：https://doi.org/10.4230/LIPIcs.ECRTS.2021.1 ；https://repositum.tuwien.at/handle/20.500.12708/55611

---

### G.4 数据布局变换：从源头避免打包

#### G35. Memory Layout Transformations: AoS → SoA（Intel 官方方法学）
- **来源**：Intel Developer Zone 技术文章（A. K. Sharp 编），ID 659847
- **Mechanism**：把 Array-of-Structures 改为 Structure-of-Arrays，使对同一字段的跨元素访问变成**连续访问**，消除 gather/scatter，并让编译器能向量化外层循环。文档明确强调：**即使硬件有 gather/scatter，这个变换仍然必要**，因为 gather 的带宽与延迟远高于连续 load。还介绍 AoSoA（tiled，块大小 = SIMD 宽度）作为折中。
- **Applicability**：**高（方向 C 的"结构性"解法）**。把它翻译成冗余语言：当前方向 C 的布局是 "AoS"（每个值在使用点被打包成一个 2-lane 结构），应该改成 **"SoA of replicas"**——在内存里维护两份（或 1.5 份）数组，副本数组与原数组各自连续，于是**冗余域入口只需两条连续 load，零 insertelement**。对于常量/只读输入，这个预处理可以在程序初始化时一次完成。
- **URL**：https://www.intel.com/content/www/us/en/developer/articles/technical/memory-layout-transformations.html ；概念参考：https://en.wikipedia.org/wiki/AoS_and_SoA

#### G36. Compiler Support for Semi-Manual AoS-to-SoA Conversions with Data Views
- **First author / venue**：（Durham / Weinzierl 组）— **arXiv:2405.12507**, 2024（C++ attribute 语言扩展 + 编译器扩展）
- **Mechanism**：通过 C++ attribute 让开发者声明"这段循环内希望用 SoA"，编译器在循环 **prologue 自动把 AoS 转成 SoA、epilogue 转回**，循环体内所有字段访问被重定向到 SoA，后续优化 pass 因而看到的是 SoA。可以只转换字段子集以缩小拷贝开销。
- **Applicability**：**高（实现模板）**。这正是方向 C 需要的"区域性布局变换"框架：在冗余域入口一次性建立 packed-replica 布局（prologue），在出口折回（epilogue），域内所有访问都是已打包形式。**这把 pack 开销从 O(每次使用) 降到 O(每个区域)**，是把 4.91x 压到 3.x 的最大结构性杠杆。
- **URL**：https://arxiv.org/html/2405.12507v1

#### G37. Polly / 多面体数据布局变换
- **First author / venue**：Tobias Grosser（A. Größlinger, C. Lengauer）— "Polly – Performing Polyhedral Optimizations on a Low-Level Intermediate Representation", **Parallel Processing Letters** 22(4), 2012；LLVM 官方子项目
- **Mechanism**：在 LLVM IR 上做多面体调度与**数据布局/数组展开（array expansion）、数据打包（`-polly-pattern-matching-based-opts` 的 packed array 生成）**；LLVM VPlan 文档明确把 Polly 的 RegionInfo 与 VPlan 的 H-CFG 并列对比。
- **Applicability**：**中**。对规则循环嵌套，Polly 可自动生成"副本维度"：把数组 `A[i]` 扩展为 `A'[i][r]`（r 为副本索引，最内维），则最内维连续即天然 packed，`<2 x float>` load 直接得到两个副本的输入。缺点是只适用于 SCoP（静态控制部分），不规则代码不适用。
- **Code**：https://github.com/llvm/llvm-project/tree/main/polly
- **URL**：https://polly.llvm.org/

#### G38. ispc: A SPMD Compiler for High-Performance CPU Programming
- **First author / venue**：Matt Pharr（W. R. Mark, Intel）— **InPar 2012**（Innovative Parallel Computing）
- **Mechanism**：SPMD-on-SIMD 编程模型：程序以标量语义书写，编译器把"program instances"映射到 SIMD lane，并自动管理 **varying/uniform 类型区分**——`uniform` 值在所有 lane 共享（标量寄存器，无需 splat），`varying` 值才真正占用 lane。
- **Applicability**：**高（类型系统启示）**。`uniform` vs `varying` 的区分正是方向 C 缺失的东西：**两个近似副本的大多数输入是 "uniform"（相同值），只有需要被独立破坏/独立计算的中间结果才是 "varying"**。若在 IR 层维护这个区分（LLVM 里可用 metadata 或自定义 analysis），则所有 uniform 值**根本不需要进入向量域**，可以保持标量形式直到真正需要 lane 分化的那条指令——`vfmadd` 等指令支持 **scalar/immediate 操作数形式**（x86 的 `vbroadcastss` 可折叠进内存操作数；RVV 有 `.vf`/`.vx` 变体，**`vfmul.vf v8, v8, fa0` 直接吃标量浮点寄存器，完全不需要 splat**）。**这是本节第二大收益来源，见优化清单 #1。**
- **Code**：https://github.com/ispc/ispc
- **URL**：https://ispc.github.io/papers.html ；https://doi.org/10.1109/InPar.2012.6339601

---

### G.5 替代 lane 打包的方案：SWAR / 低精度 double-pumping

#### G39. Compiling for SIMD Within a Register (SWAR)
- **First author / venue**：Randall J. Fisher（H. G. Dietz, Purdue）— **LCPC 1998**（LNCS 1656, pp. 290–304）；另见 Fisher 博士论文 "General-Purpose SIMD Within A Register", Purdue 2003
- **Mechanism**：**SWAR 一词的来源**。在普通通用寄存器（GPR）里用普通整数指令做分段并行运算：把 N 个窄字段塞进一个字，用掩码、进位隔离（carry barrier）、修正常量来实现分段加/减/比较。奠基工作还有 Lamport, "Multiple Byte Processing with Full-Word Instructions", **CACM** 18(8):471–475, 1975。
- **Applicability（方向 C 的"无 SIMD 依赖"后备方案）**：**中-高**。具体映射：两个 **fp16** 近似副本可以塞进一个 **64-bit GPR 的两个 32-bit 半**（或 bf16 塞进 32-bit GPR）。打包代价 = 1 条 `shl` + 1 条 `or`（或在 RV64 上 1 条 `pack` 指令，Zbkb 扩展），**比 SIMD 的 insertelement 更便宜且不触发 `vsetvli`**。代价是算术必须用 SWAR 风格实现（浮点 SWAR 很难，所以这条路只对**定点/整数近似副本**可行）。诚实结论：对 fp 近似副本不可行，但若方向 C 的近似副本改成**定点近似**（这在空天 DSP 场景完全合理），SWAR 是绕开全部向量化开销的路径。
- **URL**：https://link.springer.com/chapter/10.1007/3-540-48319-5_19 ；https://dl.acm.org/doi/10.5555/645676.663771 ；工程手册：https://aggregate.org/SWAR ；Fisher 论文：见 https://en.wikipedia.org/wiki/SWAR 参考文献

#### G40. 低精度 double-pumping / packed-FP16 冗余（现状评估，非单篇论文）
- **来源**：NVIDIA Mixed Precision 文档；"Rethinking Floating Point Overheads for Mixed Precision DNN Accelerators"（**MLSys 2021**）；x86 `vcvtps2ph`/`vcvtph2ps`（F16C）、AVX512-FP16、RVV `Zvfh`
- **Mechanism**：FP16 相对 FP32 在同样向量宽度里塞 2 倍元素，从而"同样的向量指令算两倍的工作"。MLSys 2021 那篇指出 FP16 的实际硬件开销主要在对齐逻辑（至少需 8 bit 对齐逻辑才能保持推理精度），并给出降低 FP 算术开销的架构优化。
- **Applicability（重要的负面结论，必须写进论文）**：
  - **x86**：F16C 的 `vcvtps2ph`/`vcvtph2ps` 是**显式转换指令**，没有"算 fp16 乘加"的能力（除非有 AVX512-FP16，Sapphire Rapids+ 才有）。因此 `fptrunc float→half` 在主流 x86 上就是**真实的 1 条指令**，而且还要转回去才能算 —— **当前实现里的 `fptrunc` 很可能是净亏损**。建议：近似副本改用 **fp32 但做算法级近似**（截断尾数位：`and` 掉低 mantissa bits，1 条 `vandps`，比 `fptrunc`+`fpext` 的 2 条更便宜且不改元素宽度、不触发 SEW 切换）。
  - **RVV**：`Zvfh` 下 fp16 算术原生支持，但如 G31 所述 **widening 到 fp32 吃 2×LMUL**，混用 SEW 还触发 `vtype` 切换。
  - **结论**：**"用更低精度换更多副本"在当前目标平台上得不偿失；应把 fptrunc 整体删掉，改为 fp32 lane + 位掩码近似。** 这单项就能消掉每个输入的 1 条指令。
- **URL**：https://proceedings.mlsys.org/paper_files/paper/2021/file/e96c462898b47600c6f0b1c40979e8ca-Paper.pdf ；https://docs.nvidia.com/deeplearning/performance/mixed-precision-training/index.html

#### G41. Reduced Precision Redundancy（RPR / ANT）——与本节交叉的精度-冗余权衡
- **First author / venue**：Byonghyo Shim（N. R. Shanbhag）— **IEEE TVLSI** 2004（RPR）；Hegde & Shanbhag, **ISLPED 1999** / **TVLSI** 2001（ANT）
- **Mechanism**：主模块全精度、冗余模块降精度；比较两者之差与阈值，超阈值时用降精度结果替换。详见 Part C 的 B1/B2 条目。
- **Applicability（在本节的角度）**：它提供了"**副本精度可以不等**"的正当性，于是方向 C 可以让 lane 0 = fp32 精确、lane 1/2 = 位掩码近似的 fp32，**三者共享同一 SEW，不需要任何精度转换指令**。这是把 fptrunc 从热路径删除的理论依据。
- **URL**：https://doi.org/10.1109/TVLSI.2004.830976

---

### 把 4.91x 压到 3.0-3.5x 的候选优化清单（按预期收益/实现成本排序）

> 基线假设：当前每个"进入冗余域"的标量值付 4 条 IR（1 `fptrunc` + 2 `insertelement` + 出口 1–2 `extractelement`）。若冗余域内平均每个输入值只服务 2–3 条计算指令，则打包序列占总动态指令数的比例可达 40–55%，这与 "4.91x 中约 2.0x 来自打包" 的推断一致。下表的节省量以**相对 native 的倍数**给出。

| # | 优化 | 机制 | LLVM pass / API | 预期节省 | 文献支撑 |
|---|---|---|---|---|---|
| 1 | **删掉 `fptrunc`，近似副本改为"fp32 + 尾数位掩码"** | 近似不靠降精度格式，而靠 `and` 掉低 k 位 mantissa（1 条 `vandps` / `vand.vx`，且可对整个向量一次完成，甚至对常量在编译期完成）。全部 lane 同 SEW，RVV 不再有 `vtype` 切换，x86 不再有 F16C 往返 | `InstCombine` 友好的 IR：`bitcast`+`and`+`bitcast`，或直接用 `llvm.fabs`-风格的位操作；在打包 pass 里生成 | **−0.4x ~ −0.7x**（每输入省 1 条 `fptrunc`，RVV 上额外省掉伴随的 `vsetvli`） | G31（widening 吃 2×LMUL）、G40（F16C 无 fp16 算术）、G41（副本精度可不等） |
| 2 | **用规范 splat 取代两条 `insertelement`** | 两个副本的输入是同一标量 → 需要的是 broadcast 不是逐 lane 构造。`CreateVectorSplat` 生成 insert+`shufflevector <zeroinitializer>`，被 `lowerBuildVectorAsBroadcast` 降为 **1 条 `vbroadcastss`**；RVV 降为 **1 条 `vfmv.v.f`** | `IRBuilder::CreateVectorSplat(2, V)`；务必用 `PoisonValue` 而非 `UndefValue` | **−0.5x ~ −0.9x**（2–3 条 → 1 条，且可被内存操作数折叠掉） | G29（LLVM splat 惯用法）、G1（Chen CF'16 用 broadcast 替代双 insert） |
| 3 | **对常量输入走 `ConstantDataVector`** | 常量的 packed 形式在编译期完全确定，落常量池，运行时 **0 条指令**；x86 可折叠为 `vaddps xmm, xmm, [rip+C]` 的内存操作数 | `ConstantDataVector::getSplat()` / `ConstantVector::get()`；禁止对 `isa<Constant>` 的值走 insertelement 路径 | **−0.2x ~ −0.4x**（取决于常量输入占比；科学计算 kernel 里常量占比常 15–30%） | G29、G16（SLP 对"已是向量形式的操作数"优先配对） |
| 4 | **把标量操作数保持标量，用 `.vf`/`.vx`/内存广播形式** | 真正需要 lane 分化的只是被保护的中间结果；"uniform" 输入可以一直是标量。RVV 的 `vfmul.vf`、`vfadd.vf`、`vfmacc.vf` 直接吃 `f` 寄存器；x86 的 broadcast 可折叠进内存操作数 | 新增一个 uniform/varying analysis（可用 `llvm::Metadata` 或 `DivergenceAnalysis` 的思路）；ISel 侧依赖 `RISCVISelLowering` 已有的 `splat_vector` → `.vf` 模式匹配 | **−0.6x ~ −1.0x**（把一大类输入的 pack 成本直接归零） | G38（ispc 的 uniform/varying）、G31（RVV `.vf` 形式） |
| 5 | **pack/unpack 边界外提到支配块 / 冗余域边界** | 一次 pack 服务整个区域而非每条指令；unpack 只在 SoR 出口（store/branch/call）做一次。等价于在支配树上把 pack 提到最近公共支配块、把 unpack 沉到最近公共被支配块 | `LICM`（`MachineLICM` + IR 层 `LICMPass`）、`GVN`/`EarlyCSE` 消除重复 splat、`CodeGenPrepare` 的 sink；自定义 pass 做区域内的 pack 唯一化 | **−0.5x ~ −0.8x** | G1（pack 点上推/unpack 下压）、G12（Gupta DAC'17：同步开销占 55%，批量化是主解）、G25（跨控制流保持 packed） |
| 6 | **δ 阈值比较的批量化（DRIFT 式）** | 把每个 store 前的 `fsub`+`fcmp`+`br` 换成：区域内累积 mask（RVV `vmfgt` 产 mask，`vmor` 累积），区域末尾一条 `vcpop`/`vfirst` + 1 次分支。消除 N−1 次分支与随之而来的依赖链 | 自定义 pass + `llvm.vector.reduce.or` / RVV `vcpop.m` intrinsic；x86 用 `vcmpps` + `vmovmskps` + `or` 累积 | **−0.3x ~ −0.6x** | G7（DRIFT，1.29x→1.06x）、G8（COMET）、G12 |
| 7 | **TSLP 式的"打包域"代价驱动决策** | 不是对所有指令都打包：对每个候选子图，用真实 TTI 代价比较"打包冗余"vs"标量重执行冗余"vs"不保护"，只在摊销比 > 阈值时打包。本质是在 SLP 图上做部分向量化 | 实现为 **VPlan-to-VPlan transform**，代价来自 `TTI::getShuffleCost(SK_Broadcast)`、`getVectorInstrCost(InsertElement)`、`getArithmeticInstrCost`；参考 `SLPVectorizer` 的 `getTreeCost()` | **−0.4x ~ −0.8x**，且**同时提升 x86/RVV 两个目标的可移植性** | G18（TSLP）、G28（VPlan 代价模型）、G13（SInRG 的选择性复制 ROI） |
| 8 | **循环展开 + 展开感知的 pack 共享** | 对冗余域所在循环做 2x/4x 展开，使多个迭代共享一次 splat / 一次 `vsetvli`；展开因子以"展开后能形成多少低成本 pack"为目标而非以循环收益为目标 | `LoopUnrollPass` + `-unroll-count` 启发式改造；`SLPVectorizer` 的 seed forwarding | **−0.2x ~ −0.5x**（RVV 上收益更大，因为 `vsetvli` 被摊销） | G26（Vectorization-Aware Loop Unrolling, CC 2020） |
| 9 | **"SoA of replicas" 区域性布局变换** | 在冗余域 prologue 一次性建立 packed-replica 布局（或维护一份交织数组），域内所有访问直接是 packed load/store，epilogue 折回。pack 成本从 O(每次使用) 降到 O(每个区域) | 参考 arXiv:2405.12507 的 prologue/epilogue 重定向实现；对规则循环可用 Polly 的 array expansion（副本维作最内维） | **−0.5x ~ −1.0x**（结构性杠杆，最大单项，但实现成本也最高） | G35（Intel AoS→SoA）、G36（data views）、G37（Polly） |
| 10 | **RVV：用 segment load/store 直接完成打包** | 内存中按 `{x_A, x_B}` 交织存放，一条 `vlseg2e32.v` 同时 load 并分通道；写回用 `vsseg2e32.v`。完全取代 load+splat/insert。Saturn 的 streaming segment buffer 证明其微架构高效 | `llvm.vector.interleave2` / `llvm.vector.deinterleave2` intrinsics → RVV segment 指令；或直接用 `llvm.riscv.vlseg2` | **−0.3x ~ −0.6x（仅 RVV）** | G32（Saturn segment buffer）、G29（interleave intrinsics） |
| 11 | **RVV：把冗余维与数据并行维合并，提高 `vl`** | 不用 `vl=2`（最差配置，setup 无法摊销），而用 `vl=2N'`：前 N' lane 放副本 A、后 N' 放副本 B（或奇偶交织）。一次 `vsetvli` + 一次 `vfmv` 服务 N' 个元素的冗余；lane 间比较用 `vslidedown` 半向量后一条 `vmfgt` | 把打包 pass 从 SLP 式（straight-line）改为 Loop-Vectorizer 式（`VPlan` 的 VF 选择里把冗余因子乘进 VF）；`RISCVInsertVSETVLI` 会自动合并同配置区域 | **−0.6x ~ −1.2x（仅 RVV，循环代码）** | G33（Ara：长向量摊销 setup）、G9（Warped-DMR：空闲 lane 冗余近乎免费）、G20（VW-SLP 自适应宽度） |
| 12 | **冗余域 ABI：函数签名直接用 packed 类型** | 跨函数调用不再 unpack→传标量→重新 pack；让 `<2 x float>` 成为参数/返回值类型，packed 形式是数据表示本身（IIR 思想） | 过程间的类型传播 pass（参考 `ArgumentPromotion` / `DeadArgumentElimination` 的 signature rewriting 机制）+ `ABIArgInfo` 检查（x86-64 SysV 下 `<2 x float>` 走 XMM，RVV 下走向量寄存器需 `vector_calling_convention`） | **−0.2x ~ −0.5x**（调用密集代码收益高） | G4（IIR：打包是表示而非操作）、G6（Usuba 的 slicing types） |
| 13 | **打包 pass 之后再跑一遍 PostSLP 式清理** | 冗余打包后的 IR 正是"部分向量化 + 过渡指令冗余"形态；跨区域合并向量片段、消除配对的 `insertelement`/`extractelement` | pass pipeline 顺序：`[冗余打包] → InstCombine → EarlyCSE → SLPVectorizer → VectorCombine → DAGCombine`；`VectorCombine` 专门折叠 insert/extract/shuffle 链 | **−0.2x ~ −0.4x**（零算法成本，只是调 pipeline 顺序） | G22（PostSLP）、G27（Autovesk 图级 shuffle 消除） |

**建议的落地顺序（按"收益 ÷ 工作量"）**：

1. **第一周可完成、收益确定**：#2（规范 splat）+ #3（常量向量）+ #1（删 fptrunc 改位掩码）+ #13（调 pass pipeline）。这四项都是局部改动，合计预期 **−1.3x ~ −2.4x**，单靠它们就可能把 4.91x 打到 **3.0–3.6x**，直接达成目标。
2. **第二阶段（1–2 个月）**：#4（uniform/varying 分析）+ #5（边界外提）+ #6（比较批量化）。这三项需要新 analysis，但不需要重构架构，预期再 **−0.5x ~ −1.0x**，并且 #6 对 SDC 覆盖率无损。
3. **第三阶段（论文贡献点）**：#7（代价驱动的打包域决策，做成 VPlan transform）+ #11（RVV 冗余维/数据维合并）。这两项是**可发表的编译器贡献**，尤其 #7 若用 goSLP 式 ILP 形式化（G23），可以写成 "cost-model-driven approximate-replica packing"，这是方向 C 相对 G1（Chen CF 2016）最清晰的技术增量。
4. **可选 / 风险较高**：#9（布局变换，收益最大但侵入性强，可能破坏指针别名假设）、#10（segment load，需要改内存布局）、#12（ABI 改动，与外部库交互受限）。

**两条必须写进论文的诚实限定**：

- **指令数不等于时间**：4.91x 的动态指令数在 x86 乱序核上可能只对应 1.8–2.5x 的时间开销（因为 2-lane SIMD 冗余指令与标量指令延迟相同、`vbroadcastss` 可被 move elimination 消掉）。应同时报告指令数与周期数，否则"指令数 4.91x" 这个指标本身会被审稿人质疑为不合适的 proxy（G2、G8 提供了这一论证的文献基础）。
- **覆盖域边界**：当两个副本位于同一向量寄存器的不同 lane 时，向量寄存器文件与向量 ALU 在 SoR 内，但**取指/译码/标量地址计算不在**；且一次击中整个向量寄存器的 SEU 会同时污染两个 lane（空间相关故障）。Wadden ISCA 2014（G11）给出了标准表述；SKIVA 的 complementary redundancy（G5）是针对这一弱点的现成缓解手段，且**不增加 pack 成本**——建议把它作为 HIGH 档的默认 lane 布局。

---

**本节条目数：41（G1–G41）**

