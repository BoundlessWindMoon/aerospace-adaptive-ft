# I. 故障到达过程的时变性：在轨实测、太阳质子事件、到达过程模型与时变失效率下的评测理论

> 本册为专题补充册，服务开题 3.3.3（运行时自适应档位切换）与 4.3/5.3（评测方法学）。
> 定位：**为"Poisson 速率 λ 会随时间变化"这一假设提供可引用的实测数字、过程模型与评测指标定义**，并在末尾给出可直接驱动仿真注入实验的 λ(t) 剖面。
> 全部条目按"一作 / 会议或期刊 + 年份 / 机制 / 对本课题关系 / 数据是否公开"的格式给出。标「**待核实**」的条目请在写入参考文献前逐条核对。

---

## I.0 与 F 册的衔接：本册扩展了哪些 F 条目

已读 `part_F_learning_adaptive.md` 的 F.7（AE9/AP9、CREME96、SPENVIS、OMERE、SIRE2、NASA/TM、F49 的 SAA 97.5% 结果、F50 CubeSat SAA、F51 Landsat）、F.8（CUSUM/SPRT/Page-Hinkley/FOCuS/Gamma-Poisson 贝叶斯变点）、F.9、F.10（指标 A/B → LLR 统一、滞回、非 Poisson 风险）。本册**不重复**这些内容，只做以下扩展：

| 本册节 | 扩展的 F 条目 | 增量 |
| :-- | :-- | :-- |
| I.1 在轨实测 | **扩展 F49 / F50 / F51** | F49 只有一组在轨 SRAM 数据与 SAA 集中度；本册补 12 个独立任务（UoSAT-2/3、TDRS-1、MPTB/CREDO-3、CRUX/APEX、SAMPEX、TOMS、Alsat-1、PROBA-1/SREM、Flying Laptop、RadFxSat-2/CELESTA、ISS/RHEME、MSL/RAD），每条给轨道、时长、SEU 数、**高速率期时长**与速率比 |
| I.2 太阳质子事件 | **扩展 F44（CREME96）、补 F43 的缺口（AE9/AP9 不含太阳质子）** | CREME96 三档最坏情况的**精确时间窗定义**（180 h / 18 h / 5 min）、ESP/PSYCHIC/JPL-91 的统计形式与数值、SPE 的**持续时间与增强倍数** |
| I.3 到达过程模型 | **扩展 F56（Gamma-Poisson 贝叶斯变点）** | F56 只给齐次模型；本册补 NHPP、MMPP、renewal/Weibull、负二项、Hawkes 的完整谱系与已发表用例，并指出 **MMPP 与"SAA 内/外两态"数学同构**、**Hawkes 在本问题上应当被排除**的理由 |
| I.4 HPC 故障轨迹 | **扩展 F35–F41** | 补 Schroeder & Gibson 的 LANL 轨迹与 Weibull 形状参数、**Hwang ASPLOS'12（硬错误占 65–82%，F37/F38 的量化版）**、Gupta SC'17 的 temporal recurrence parameter、Nie HPCA'16、Oles ICS'24 Summit、2025 年 Ampere 三机群的**到达间隔分布与过度分散实测值**，以及公开数据集入口（CFDR / FTA / OLCF DOI） |
| I.5 Checkpointing 理论 | **F 册完全没有** | Young/Daly 公式族、**waste 的精确定义式**、非指数/时变失效率下"周期性不再最优"的硬证据（Bougeret SC'11：同一次运行内区间从 2 984 s 变到 6 108 s）。给出本课题应采用的长任务期望有效功指标，替代 per-run SDC 率 |
| I.6 λ(t) 剖面 | **落地 F.10 的"可验证实验设计"第 1 步** | F.10 只说"用 SPENVIS+CREME96 生成 λ(t)"，没给数字。本节给轨道周期、SAA 时间占比、内外速率比的**显式算式与推荐区间**，并给可选 SPE 情节与 MMPP 生成器参数 |

---

## I.1 在轨实测的 SEU 时间序列：轨道位置、过境时长与速率比

> 本节的核心产出是**两个时间常数**：(a) SAA 过境一次持续多久 → 决定滑动窗口长度与允许的检测延迟；(b) 太阳质子事件持续多久 → 决定降档的保守等待时间。二者相差三个数量级，这是本课题控制器必须双时间尺度的根本原因。

**I1. UoSAT-2 在轨 SEU 的轨道分布（最早的 SEU—轨道位置联合统计）** — C. I. Underwood, **SSC/AIAA Small Satellite Conference, 1990**（"In-Orbit Radiation Effects Monitoring on the UoSat Satellites"，Utah State Univ. DigitalCommons）；配套 C. Underwood, E. Daly, R. Harboe-Sørensen, **Proc. ESA Space Environment Workshop, ESTEC, Oct 1990**
- 机制：UoSAT-2，1984 年发射，**700 km 近极地太阳同步轨道**；自 1988 年起把 SEU 打上**精度 8.25 分钟**的时间戳下传。结论：所有器件类型的 SEU 都**压倒性集中在南大西洋异常区（SAA）**，剩余部分**聚集在高纬（>60°）**，对应 GCR 成分；1989 年 10 月耀斑期间上翻数大幅上升。
- 对本课题：**"λ 双模"最早的实测证据**，比 F49 早 35 年；8.25 min 的时间戳分辨率本身说明当年工程判断"分钟级"是刻画 SEU 时序的合适尺度。建议与 F49 并引，构成"跨 35 年、跨国别一致结论"的论证。
- 数据：论文 PDF 公开（USU DigitalCommons）；事件级原始表未公开。

**I2. UoSAT 系列 18 年飞行经验：SAA 过境"簇"的时长** — C. I. Underwood, **ESA SP-439, 2000**（"18 Years of Flight Experience with the UoSAT Microsatellites"）
- 机制：COTS 存储器在 LEO 的 SEU 率量级 **~10⁻⁶ SEU/bit/day**；关键定量结论：**"SEU 只在 SAA 内发生——即以每轨一次、每次持续约 5–10 分钟的簇（bursts）出现"**；SAA 内捕获质子给出很高的瞬时 SEU 率。
- 对本课题：**本册最关键的单条数字**。它把 F49 的"数分钟到十几分钟"收紧为 **5–10 min / 轨**，直接定出三件事：
  - 滑动窗口 W 的物理上界：W 不应超过 5–10 min，否则 SAA 内突发被 SAA 外的长静默稀释；
  - 检测延迟预算：CUSUM/SPRT 需在 **≤2–3 min** 内完成升档判决，否则过境已过半，升档收益被吃掉大半；
  - F.10 中"ARL₀ 取为一轨一次误报"这一标定目标的物理来源。
- 数据：ESA SP 系列全文扫描可从 ADS 获取（`adsabs.harvard.edu/full/2000ESASP.439....5U`）。

**I3. 高太阳活动期低轨 SEU 行为** — R. Harboe-Sørensen, E. J. Daly, C. I. Underwood, J. Ward, L. Adams, **IEEE Trans. Nucl. Sci. 37(6):1938–1943, Dec 1990**
- 机制：1989 年 8–10 月强太阳活动期间低轨 SEU 行为的专题分析；记录了事件后**沿轨上翻剖面发生改变**，并可清晰看到**第二（新生）质子带**的影响。
- 对本课题：λ(t) 的自变量不止"轨道相位"。大事件后捕获质子带被重构，λ 的**基线**会在事件后数月保持偏高。这是降档逻辑的重要威胁效度点：**不能假设事件结束后 λ 立刻回到事件前的 λ₀**，降档必须带长时常数的确认期。
- 数据：IEEE 订阅。

**I4. 把在轨 SEU 显式分解为 GCR / SAA / 太阳耀斑三成分** — R. Harboe-Sørensen, E. J. Daly, L. Adams, **IEEE Trans. Nucl. Sci. 40(6):1498–1504, Dec 1993**（"Observation and Prediction of SEU in Hitachi SRAMs in Low Altitude Polar Orbits"）
- 机制：三颗微卫星的在轨 SEU 数据被**分离为 GCR 上翻、SAA 上翻、太阳耀斑上翻**三类，各自与预测值比对。
- 对本课题：**这是 λ(t) = λ_GCR + λ_SAA(t) + λ_SPE(t) 可加分解的文献依据**。§I.6 的剖面就应写成该三项和，并引用本文作为分解合法性来源，而不是自己拍一个两态模型。
- 数据：IEEE 订阅；IAEA INIS 有记录页。

**I5. TDRS-1：1989 年 10 月太阳质子事件中的 243 次上翻（GEO，持续约 6 天）** — D. R. Croley, H. B. Garrett, G. B. Murphy, T. L. Garrard, **IEEE Trans. Nucl. Sci. 42(5):1489–1496, 1995**（"Solar Particle Induced Upsets in the TDRS-1 Attitude Control System RAM During the October 1989 Solar Particle Events"）
- 机制：地球同步轨道 TDRS-1，姿控系统（ACS）控制处理电子学的 RAM（**8 片 Fairchild 93L422**）在**始于 1989-10-19、持续约六天**的三次大型太阳质子事件中记录到**异常的 243 次上翻**；同期 Galileo（1989-10-18 发射）两台固体探测器望远镜给出对应的重离子通量谱，用于反推截面。TDRS-1 自 1984 至 1990 年系统性记录 SEU，使"上翻—环境"相关性首次被建立；最严重的 ACS 异常被操作方评估为**威胁任务**（可能导致卫星翻滚）。
- 补充数字（NASA 异常汇编 NTRS 19960050463）：1989 年 10 月那次活动的主相期间，**含 GOES-6/-7 与 TDRS-1 在内的六颗地球同步卫星共报告 37 次 SEU**；GOES-5 中央遥测单元 1989 年全年 10 次 SEU，其中 6 次与耀斑相关。
- 对本课题：**SPE 情节的时间常数来源**：一次大事件的高速率期是**天级（~6 天）**，与 SAA 的 5–10 min 相差 **约三个数量级**。因此自适应控制器必须是**双时间尺度**的（快回路跟 SAA、慢回路跟 SPE），单一窗长 W 不可能同时覆盖两者。这是论文中值得单独写出的结构性结论。
- 数据：**公开**。NOAA/NGDC spacecraft anomalies 数据库挂有 `TDRS1_SEU.pdf`（"TDRS-1 Single Event Upsets and the Effect of the Space Environment"）与异常记录：`https://www.ngdc.noaa.gov/stp/space-weather/satellite-data/spacecraft-anomalies/`。（该站点 TLS 在部分网络下握手失败，需换客户端；**文档存在性已核实，该 NOAA 文档的作者署名待核实**。）

**I6. MPTB / CREDO-3：同一时间轴上的粒子通量与 SEU，5 分钟分箱** — C. S. Dyer 等，系列四篇
- (a) **IEEE TNS 47(3):481–485, Jun 2000** — "Radiation environment of the Microelectronics and Photonics Test Bed as measured by CREDO-3"
- (b) **IEEE TNS 47(6):2208–2217, Dec 2000** — "Radiation environment measurements from CREAM and CREDO during the approach to solar maximum"
- (c) **IEEE TNS 49(6):2771–2775, Dec 2002** — "Observation of solar particle events from CREDO and MPTB during the current solar maximum"
- (d) **IEEE TNS 51(6):3388–3393, Dec 2004** — "Observation of the solar particle events of October and November 2003 from CREDO and MPTB"（作者 C. S. Dyer, K. Hunter, S. Clucas, A. Campbell；DOI 10.1109/TNS.2004.839156）
- 机制：MPTB（Microelectronics and Photonics Test Bed）1997-11 发射进入**高偏心轨道**，载 23 块试验板 + CREDO-3 环境监测板。质子监测器同时给出 GCR 的太阳调制（与太阳活动反相关）与多次太阳质子事件；其中 **2000-07-14 "Bastille Day" 事件强度最大，可与 1989-10 事件相比**。关键图件给出 **"每 5 分钟区间的 SEU 数"与 CREDO 质子通量叠绘**（轨 1964–1975，2000-07-12 至 07-23，轨 1973 数据缺失），即**同一时间轴上的 λ(t) 与其环境驱动量**。(d) 给出 2003 年 10–11 月五次事件的质子通量、LET 谱与 SEE 率。
- 对本课题：**现有文献中最接近"λ(t) 实测时间序列"的可引用形态**：5 分钟分箱的事件计数 + 同步环境协变量。本课题仿真注入的时间轴建议照此分辨率设计（5 min bin），并把"环境协变量可用"写成 F41 式多信号融合的实证依据。
- 数据：IEEE 订阅；上述图件在 ESA 空间天气研讨会文集 `solspa1.pdf`（**公开可下载**，见 I21）中复现。

**I7. Bastille Day 事件中 MPTB 16-Mb DRAM 的实测与预测对照** — A. Campbell, S. Buchner, E. Petersen, B. Blake, J. Mazur, C. S. Dyer, **IEEE Trans. Nucl. Sci. 49(6):1340–1344, 2002**
- 机制：对 MPTB 上 16-Mb DRAM 做地面 SEU 测试 + 在轨速率计算，与 2000-07-14 大事件期间实测上翻对照。英国皇家工程院报告（I21）复现其 ADC 上翻观测图：**SEE 起始与太阳粒子到达时刻重合**，且图上可见卫星周期性穿越捕获辐射带造成的规律性凹凸。
- 对本课题：**"SPE 包络叠加在轨道周期调制之上"的实测图像**，正是 §I.6 推荐剖面的结构（周期性 SAA 脉冲 + 慢变 SPE 包络）。
- 数据：IEEE 订阅。

**I8. CRUX/APEX、SAMPEX、TOMS 多任务 SEE 飞行数据汇总（含实测 vs 预测误差）** — J. Adolphsen, J. Barth, E. G. Stassinopoulos, K. A. LaBel, C. M. Seidleck, **IEEE Trans. Nucl. Sci. 42(6):1964–1974, Dec 1995**（"Single Event Upset Rates for 1 M and 256 k Memories: CRUX Experiment on APEX"）；E. G. Mullen, K. P. Ray, R. Koga, **IEEE TNS 42:1988–1994, Dec 1995**；汇总报告见 NASA/GSFC NEPP 公开 PDF "Single Event Effect Flight Data Analysis of Multiple NASA Spacecraft and Experiments"
- 机制：CRUX（Cosmic Ray Upset Experiment）1994-08 随 APEX 发射，**轨道 362 × 2544 km、倾角 70°**，专测多型 SRAM（256 kbit–1 Mbit）与功率 MOSFET。关键定量结论：
  - **所有 CRUX SRAM 的 SEU 中 >90% 为质子致（即 SAA 致）**；
  - SAMPEX（Hitachi 256k SRAM）实测 **4.21×10⁻⁶ upsets/bit-day** vs 预测 3.832×10⁻⁶；TOMS/Meteor-3（1200 km, 82°）实测 **3.12×10⁻⁶** vs 预测 3.32×10⁻⁶ —— **飞行前速率预测与在轨实测落在 25% margin 内**；
  - 两次任务均未观测到破坏性锁定（SEL）；
  - CRUX 数据按 SAMPEX / TOMS 高度分箱后作墨卡托投影，SAA 致上翻的主导性清晰可见。
- 对本课题：(a) 提供 **λ 的绝对量级锚点**（LEO 极轨 COTS SRAM，10⁻⁶ 级 upsets/bit-day），§I.6 用它算绝对速率；(b) ">90% 质子致"与 F49 的 97.5% 相互印证；(c) **"预测落在 25% 内"是本课题实验设计合法性的关键引文**——它支撑"用 SPENVIS/CREME96 生成的 λ(t) 作为仿真真值是可接受的"。
- 数据：NASA NEPP 公开 PDF；原始事件表未公开。

**I9. Alsat-1：700 km SSO 上 COTS 存储器近八年在轨 SEU 统计** — Y. Bentoutou 等, **Advances in Space Research 48(7):1147–…, 2011**（"A follow-up of in-orbit observations of radiation-induced effects in commercial off-the-shelf memories on-board Alsat-1"，DOI 10.1016/j.asr.2011.08.011；ADS 2011AdSpR..48.1147B）
- 机制：Alsat-1（SSTL 制造，88 kg，2002-11-28 发射，**700 km 太阳同步、倾角 98°、轨道周期 98.5 min**，运行 7 年 9 个月至 2010-08）。关键定量结论：**典型 SEU 率 4.04×10⁻⁷ SEU/bit/day**；其中 **98.6% 为单比特错误、1.22% 为双字节错误**，其余为更高重数错误。
- 对本课题：**与本课题目标平台最接近的绝对速率引文**（小卫星 + COTS 存储器 + 700 km SSO + 周期 98.5 min）。§I.6 的轨道周期与 λ̄ 直接取自此条。另外"98.6% 单比特"可用来回应 D 册的"单比特模型是否够"（Sangchoolie DSN'17）：**在轨真实分布确实以单比特为主**，这是对本课题注入模型有利的证据。
- 数据：Elsevier 订阅；数值已在多处摘要公开。

**I10. PROBA-1 / SREM：唯一长期公开可下载的在轨粒子通量时间序列** — ESA/ESTEC + PSI；数据库说明见 **Proc. ICRC 2013, paper 1224**（"Standard Radiation Environment Monitor – data repository and analysis tools"，**作者署名待核实**）
- 机制：SREM 测 0.3–6 MeV 电子、8–300 MeV 质子、重离子与累积剂量；**在轨 7 台单元**分布于 PROBA-1、Rosetta、INTEGRAL、Herschel、Planck、GIOVE-B 与 STRV-1c。共同数据库自 2002 年（INTEGRAL 与 PROBA-1 发射）建立，主服务器在 PSI；库内含辐射带、太阳高能粒子事件、Forbush 下降等事件的质子/电子/宇宙线计数率与反演谱，并提供**响应矩阵与航天器质量模型**。PROBA-1 的高纬穿越使其同时能监测外带状态。
- 对本课题：**可用真实环境时间序列驱动 λ(t)，而非纯合成**。做法：取 SREM 的 >10 MeV 质子计数率时间序列 × 器件质子截面 → 得到真实 λ(t)，再按 NHPP 采样生成 ECC 事件流（§I.3 的 I24 给采样算法）。这比纯 SPENVIS 合成更有说服力，且是本课题最容易拿到的"真数据"。
- 数据：**公开**。`https://srem.psi.ch`（含原始与预处理数据）。

**I11. Flying Laptop：2017–2024 近七年 SRAM SEU 与 L 值 / 截止刚度联合分析（开放获取，且结论与 F49 相反）** — C. Noeldeke, S. Gaißer, S. Klinkner 等, **Advances in Space Research 67(6):2000–…, 2021**（ADS 2021AdSpR..67.2000N）；后续篇 J. Zink, C. M. Nöldeke, S. Gaißer, S. Klinkner, **Advances in Space Research 77(10):10520–10528, 15 May 2026, DOI 10.1016/j.asr.2026.03.051（开放获取，CC 许可）**
- 机制：斯图加特大学 Flying Laptop 小卫星，2017-07 起运行于 LEO。后续篇分析 **2017-08-15 至 2024-05-28** 的 SRAM SEU 频次与卫星位置、**McIlwain L 值**、**垂直截止刚度**的关系。关键定量结论：
  - SEU 发生率随 **L 值增大显著上升**、在**低截止刚度区**显著上升；
  - **SEU 频次与太阳活动反相关**——太阳活动越强 SEU 越少，对应 GCR 通量被太阳调制压低；作者据此判定**该星该器件上 SEU 以 GCR 为主导，而非 SEP**。
- 对本课题：**极重要的边界条件/反例引文**。它与 F49（SAA 捕获质子主导、97.5% 集中）**结论相反**，根因是器件阈值 LET 与屏蔽不同。论文中必须明写：
  - λ(t) 的**双模强弱是器件相关的**：高阈值/强屏蔽器件偏 GCR 主导（λ(t) 更平、随纬度缓变、随太阳周期反相关）；低阈值 COTS 器件偏 SAA 质子主导（λ(t) 强双模）；
  - 因此 λ_in/λ_out 必须作为**敏感性扫描参数**而不是单点取值（§I.6 已照此设计）；
  - 它同时为"用实测 ECC 事件而非太阳活动指数做切换"（F49 已给的论证）补上第二条独立证据——**在 GCR 主导器件上，"太阳活动强 ⇒ 升档"甚至是符号相反的**。
- 数据：**2026 年后续篇开放获取可下载**；事件级数据在论文图表中给出。

**I12. CubeSat/新器件代际的在轨 SEU 首测** — 两条
- (a) **RadFxSat-2（Fox-1E）** — Vanderbilt ISDE / AMSAT，**SSC/AIAA Small Satellite Conference**（USU DigitalCommons 公开 PDF；**年份待核实，约 2021–2022**）："The RadFxSat-2 Mission to Measure SEU Rates in FinFET Microelectronics"。首次给出 **FinFET 基 SRAM 的在轨 SEU 率**；结论：在质子主导的 LEO 环境中，<65 nm 器件对低能质子更敏感，但**启用 ECC 后速率仍在多数任务可接受范围内**；采用 WOD（Whole Orbit Data）型下传，通信异常时数据仍可回收。
- (b) **CELESTA**（CERN/ESA 立方星，低轨辐射监测） — A. Coronetti 等, **2024**（"The CELESTA CubeSat In-Flight Radiation Measurements"，CERN 文献库 record 2915017；**期刊/会议名待核实，疑为 IEEE TNS**）。载三片对 SEU/SEL 敏感的 SRAM。
- 对本课题：若以 RISC-V + COTS SRAM 的立方星级平台为目标，这是最新的器件代际对标。(a) 的"启用 ECC 后可接受"正是本课题分级保护的前提：**基线有 ECC，软件冗余只需覆盖 ECC 之外的残余风险**，这句话可以直接用来界定本课题保护目标的边界。
- 数据：(a) **公开 PDF**；(b) CERN 预印本公开（本次抓取 PDF 下载失败，需重试）。

**I13. 同器件 / 不同轨道的对照：RHEME 系列** — D. Alexander 等（LANL）
- (a) ISS 上 RHEME：**IEEE Xplore 文档 8584285, 2018**（"Single Event Upset Results from the Radiation Hardened Electronic Memory Experiment on the International Space Station"；**期刊卷期待核实，疑为 IEEE TNS**）
- (b) RHEME-2 on STPSat-5（**极轨**），约 2020（LANL LARO 记录）
- (c) RHEME-3 on STPSat-6（第三版）
- 机制：同一实验载荷在 **ISS（~400 km, 51.6°）** 与 **极轨** 两种轨道上重复，给出同器件在不同地磁屏蔽条件下的 SEU 率对照。
- 对本课题：**唯一能把"轨道依赖"与"器件差异"解耦的系列**。ISS 的 51.6° 倾角使其 SAA 穿越与极区暴露都弱于 SSO，正好对应 λ_in/λ_out 比值较小的一端，可作为 §I.6 敏感性扫描的下界锚点。
- 数据：IEEE / LANL，公开与订阅混合。

**I14. 深空：MSL/RAD 的剂量率时间序列（含 Juno 的空白说明）** — D. M. Hassler 等, **Science 343(6169), 24 Jan 2014**（DOI 10.1126/science.1244797）；C. Zeitlin 等, **Science 340, 2013**（巡航段）；J. Guo 等, **Astronomy & Astrophysics 577:A58, 2015**；J. Guo 等, **ApJ 810:24, 1 Sep 2015**；综述 J. Guo 等, **Astronomy and Astrophysics Review, 2021**（DOI 10.1007/s00159-021-00136-5）
- 机制：Curiosity 上的 RAD 自 2012-08-07 起连续测量。关键定量结论：
  - 火星表面 GCR 剂量率 **0.210 ± 0.040 mGy/day**（组织等效 E 探测器），剂量当量率 **0.64 ± 0.12 mSv/day**，平均品质因子 ⟨Q⟩ = 3.05 ± 0.26；巡航段（航天器内部）**0.48 ± 0.08 mGy/day**、**1.84 ± 0.30 mSv/day**，⟨Q⟩ = 3.82 ± 0.30；
  - 巡航段静默期 GCR 剂量率：硅探测器 **332 ± 23 µGy/day**、塑料闪烁体 **461 ± 92 µGy/day**；
  - **SPE 期间观测到剂量率的脉冲式增强**；巡航段 SPE 的**单次事件剂量当量 1.2–19.5 mSv/event**，而火星表面首次观测到的 SPE（sol 242）仅 **0.025 mSv**——大气屏蔽把软谱事件几乎吃光；
  - 剂量率与太阳调制势 Φ **反相关，相关系数 ≈ −0.6**；线性拟合斜率 β：硅 **−0.39 ± 0.07**、塑料 **−0.44 ± 0.04** µGy·day⁻¹·MV⁻¹；
  - 另有**火星大气热潮引起的昼夜周期性剂量率振荡**（气压驱动；250 Pa 气压变化对应 32.5 µGy/day 的剂量率差）。
- 对本课题：
  - **"λ(t) = 周期成分 + 脉冲成分 + 慢趋势"的最干净实测范例**（昼夜周期 ↔ 本课题的轨道周期；SPE 脉冲 ↔ SAA/SPE 脉冲；太阳调制慢趋势 ↔ 任务级基线漂移）。可直接作为 §I.6 三项可加模型的方法学背书，与 I4 的三成分分解互为呼应。
  - **"巡航 1.2–19.5 mSv/event vs 表面 0.025 mSv"这个约 500× 的落差**是"屏蔽/环境把 SPE 增强倍数压掉二到三个数量级"的量化证据，解释了为什么 LEO 的 SPE 增强远小于 GEO/行星际（与 I5、I21、I22 的倍数对照一致）。
- 数据：**公开**。RAD 数据集发布在 NASA PDS（Planetary Data System）；A&A 与 ApJ 论文开放获取。
- **Juno 的空白（须诚实声明）**：检索未发现 Juno 公开的 SEU 事件日志或 SEU 时间序列论文。可引用的只有工程设计参数（NASA / Lockheed Martin 公开资料）：钛制辐射屏蔽舱六面各约 1 m²、**壁厚约 1 cm、单面约 18 kg、整舱约 200 kg、内含 20 余个电子组件**；以及"**近木点每约 53 天一次、每次数小时的强辐射暴露**"这一与 SAA 过境同构的脉冲式暴露结构。若论文需要深空例子，建议用 MSL/RAD（I14）而非 Juno。

**I15. SAA 本身在 27 年间的漂移（λ(t) 的超慢变成分）** — P. C. Anderson, F. J. Rich, S. Borisov, **Journal of Atmospheric and Solar-Terrestrial Physics 177:237–246, Oct 2018**（DOI 10.1016/j.jastp.2018.03.015）
- 机制：利用 DMSP F8–F18 的 SSJ/4、SSJ/5 沉降粒子谱仪受 MeV 电子污染的信号，**1988–2015 连续 27 年**追踪 SAA 位置与强度的移动；首次用同轨道、同仪器的重叠卫星做到不间断监测。文中明确："LEO 卫星周期性穿越 SAA，**每次暴露于数分钟的强辐射**"，并复现 Underwood (1990) 的 UoSAT-2 SEU 位置图与 700 km 处 IGRF 场强等值线叠绘。
- 对本课题：
  - 为"SAA 过境数分钟"提供**第二个独立来源**（与 I2 的 5–10 min 一致）；
  - 指出 λ(t) 还有**年代际漂移成分**（SAA 西移与扩张），意味着任务初期标定的 λ₀/λ₁ 在数年任务后会失准——这是支持"在线估计而非离线固定阈值"的又一条论据，可直接写入 F.10 的动机段。
- 数据：Elsevier 订阅；DMSP SSJ 原始数据在 NOAA/NGDC **公开**。

---

## I.2 太阳质子事件：增强倍数、持续时间与工程模型

> 本节要回答的工程问题是：**SPE 期间 λ 抬高多少倍、抬高多久**，以及任务分析里 SPE 是如何"叠加"在捕获带速率之上的。

**I16. CREME96：最坏情况太阳质子环境的三档时间窗（本课题 SPE 情节的权威出处）** — A. J. Tylka, W. F. Dietrich, P. R. Boberg, **IEEE Trans. Nucl. Sci. 44(6):2150–2160, Dec 1997**（"CREME96: A revision of the Cosmic Ray Effects on Micro-Electronics code"）；配套 A. J. Tylka, F. Dietrich, P. R. Boberg, **IEEE TNS 44:2140–2149, Dec 1997**（IMP-8 重离子通量概率分布 1973–1996）；工程最佳实践见 Aerospace Corporation **TOR-2022-00016, 2022**（F43 已引，本册引其 SPE 章节）
- 机制：CREME96 把 CREME86 的 SPE 模型替换为**基于 1989 年 10 月系列事件的三档最坏情况模型**，三档的时间窗定义是精确的：
  - **"worst week" = 1989-10-19 至 10-27 共 180 小时的平均通量**；
  - **"worst day" = 1989-10-20 至 10-21 激波事件期间 18 小时区间的平均通量**；
  - **"peak 5-minute flux" = 1989-10-20 实测的最高 5 分钟平均质子通量**（注：IMP-8 上只有质子以 5 分钟节拍采样，重离子通量由更长时段的丰度比缩放得到）。
  - 三档均被论证为 **~99% 置信水平的最坏情况**；CREME96 的 SPE 模型显著比 CHIME/MACREE 严苛，因此用它算出的 SEU 率也显著更高。
- 对本课题：**SPE 情节的时间常数就应当直接取这三个数字**（180 h / 18 h / 5 min），而不是自己拍。这同时给出三层嵌套结构：5 min 峰值 ⊂ 18 h 最坏日 ⊂ 180 h 最坏周——与 SAA 的 5–10 min 刚好可比的是"5 min 峰值"，而控制器的降档等待期必须覆盖 18 h 量级。
- 数据：CREME96 在线工具公开（`https://creme.isde.vanderbilt.edu/`，F44 已引）；TOR-2022-00016 **公开 PDF**。

**I17. ESP / PSYCHIC（Xapsos 系列）：把 SPE 的发生建模为 Poisson + 极值理论** — M. A. Xapsos 等
- (a) **IEEE Trans. Nucl. Sci. 46(6):1481–1485, 1999** — "Probability Model for Worst Case Solar Proton Event Fluences"
- (b) **IEEE Trans. Nucl. Sci. 47(3):486–490, 2000** — "Probability Model for Cumulative Solar Proton Event Fluences"
- (c) **Solar Physics 183:157–164, 1998** — "Extreme Value Analysis of Solar Energetic Proton Peak Fluxes"
- (d) **IEEE TNS 47(6):2218–2223, Dec 2000** — "Characterizing solar proton energy spectra for radiation effects applications"
- (e) **NASA/MSFC SEE Program 报告, 1999（NTRS 20000017924）** — "Model for Emission of Solar Protons (ESP): Cumulative and Worst Case Event Fluences"
- (f) PSYCHIC 重离子扩展：**IEEE TNS 54(6), Dec 2007** — "Model for Cumulative Solar Heavy Ion Energy and Linear Energy Transfer Spectra"
- (g) 任务分析口径总结见 M. A. Xapsos, **"The Single Event Effects Environment of Space"（NASA NEPP / 2018 NSREC Short Course 讲义，2020 版公开 PDF）**
- 机制：**核心建模假设是"太阳质子事件的发生服从 Poisson 过程"**，事件幅度服从一个有上限的幂律型分布；再由极值理论给出任务期内最坏单事件的分布：`F_T(M) = exp{−N_tot·T·[1−P(M)]}`，其中 T 为太阳活跃年数、`F_T` 等于所需置信水平。数值示例（>30 MeV 质子）：**1% 风险水平下最坏单事件通量为 1.2×10¹⁰ cm⁻²；模型设定的最大单事件通量（设计上限）为 1.32×10¹⁰ cm⁻²**。SPENVIS 中 PSYCHIC 把 ESP 的 5.92 MeV 质子微分通量乘以 0.173–547.7 MeV/nucleon 的缩放因子得到重离子谱。Xapsos 组还对比得出：**CREME96 的 "worst week" 在统计上更接近 90% 最坏情况事件模型**（而非 99%），这是两套模型口径的差异点。
- 对本课题：
  - **"SPE 到达是 Poisson，SPE 内部的 SEU 到达是条件 Poisson"——这正是 MMPP / Cox 过程（doubly stochastic Poisson）的物理原型**（见 I25/I26）。本课题可以据此把 λ(t) 写成"两级 Poisson"：外层控制 SPE 情节的起止，内层控制 SEU 到达。这个表述有现成文献支撑，不是自创。
  - 工程分层口径："捕获带速率（AE9/AP9 + 器件截面）为基线，SPE 通过 ESP/PSYCHIC 在置信水平上叠加"——F43 已指出 AE9/AP9 不含太阳质子，本条补上缺口的标准补法。
- 数据：**ESP/PSYCHIC 在 SPENVIS 中公开可跑**（`https://www.spenvis.oma.be/`，F45 已引）；ESA Network of Models 有 ESP-PSYCHIC 条目；NTRS 报告 **公开 PDF**。

**I18. JPL-91（JPL 模型）：事件幅度对数正态 + Monte Carlo，且明确指出"大事件在时间上不均匀分布"** — J. Feynman, G. Spitale, J. Wang, S. Gabriel, **Journal of Geophysical Research 98:13281–13294, 1993**（"Interplanetary proton fluence model: JPL 1991"，DOI 10.1029/92JA02670）；更新版 J. Feynman, A. Ruzmaikin, V. Berdichevsky, **Journal of Atmospheric and Solar-Terrestrial Physics 64:1679–1686, 2002**
- 机制：用太阳周期 19–21 的数据证明**活跃期的事件幅度分布可用连续对数正态分布刻画**（从此不再需要把事件分为"普通"与"异常大"两类）；用 Monte Carlo 算任务期累积通量的置信水平；给出 >1、>4、>10、>30、>60 MeV 五个能档。**关键定性结论（SPENVIS 帮助页明确转述）：主导总通量的大事件"看起来并非在时间上随机分布，而是在某些太阳周内明显更常见"——1963–1988 的 25 年间只有一次大事件（1972），而 1957–1963 有 3–4 次、1989–1991 有 4–5 次。** 2002 年更新版的结论是 1998 年前的新观测仍落在 JPL-1991 的分布内，因此概率分布无需修改，模型可直接称为"JPL 模型"。
- 对本课题：**这是"SPE 到达过程本身也不是齐次 Poisson、而是有簇集的"的权威文献依据**（与 I17 的 Poisson 假设形成张力，应当在论文里把这一张力明确写出来）。它支持用 **MMPP 或 NHPP（λ_SPE(t) 随太阳周期调制）** 而非齐次 Poisson 来建模 SPE 情节的发生，这是本课题模型选择的直接论据。
- 数据：AGU 订阅；SPENVIS 中 JPL 模型 **公开可跑**；部分图件在 SPENVIS 帮助页公开。

**I19. SEP 事件幅度的幂律分布** — S. B. Gabriel, J. Feynman, **Solar Physics 165:337–346, 1996**（"Power-Law Distribution for Solar Energetic Proton Events"）
- 机制：SEP 事件幅度在很宽范围内服从幂律。
- 对本课题：幂律幅度 ⇒ **SPE 贡献的事件数方差极大、无特征尺度**，这是"用均值 λ̄ 单一数字描述任务期风险必然失真"的理论根源，也是 §I.5 为什么要用期望有效功/waste 而非单点速率的理由之一。
- 数据：Springer 订阅。

**I20. 极端 SPE 的量级与效应（含现代重标定的 1972/1989 通量）** — P. Jiggens, M.-A. Chavy-Macdonald, G. Santin, A. Menicucci, H. Evans, A. Hilgers, **Journal of Space Weather and Space Climate 4:A20, 2014（开放获取，DOI 见刊网）**
- 机制：ESA/ESTEC 空间环境与效应组对极端 SPE 的系统评估。关键定量结论：**>30 MeV 能档，1972-08 事件通量 5.0×10⁹ cm⁻²，1989-10 事件 4.23×10⁹ cm⁻²（Wu 等 2009 的估计）**；不同数据源（GOES/SEM 与 IMP-8 GME 交叉标定）对 1989-10 的通量估计相差约 3 倍。文中明确指出 **CREME96 所用的 1989-10 事件只是"5 MeV–100 MeV 能段过去 40 年中最大"，而对不同屏蔽/不同器件，最坏事件并不相同**。
- 对本课题：
  - 给出 SPE **通量的绝对数字**，用于把 λ_SPE 的抬升倍数算出来（而不是取一个"×10"的拍脑袋值）；
  - "最坏事件是器件相关的"这句话与 I11 的"双模强弱是器件相关的"构成同一类威胁效度论述，应当在论文的"威胁效度"一节合并处理。
- 数据：**开放获取**；ESA SEPEM 参考数据集亦公开。

**I21. 极端空间天气对工程系统的影响（权威政策级报告，含卫星异常表与增强倍数）** — P. S. Cannon 等（编），**Royal Academy of Engineering, "Extreme space weather: impacts on engineered systems and infrastructure", 2013（开放获取 PDF）**
- 机制：关键定量结论：
  - **Carrington 级事件在 250 年内发生的置信度约 95%、在 50 年内约 50%**（报告自述该数字不确定性大）；
  - 给出卫星异常年表（1985 Anik D2 / 1989-10 TDRS-1 SEE 中断 / 1991-07 ERS-1 SEE 仪器失效 / 2001-11 MAP 在 L2 的 SEE 临时中断 / 2003-10-26 SMART-1 的 SEE 导致发动机关机与星敏噪声 等），**明确哪些是 SEE 致、哪些是 ESD 致**；
  - 航空侧（Concorde CREAM 实测）：1989 年 9–10 月系列事件期间**瞬时增强达 ×10、飞行平均增强达 ×6**；按模型反算 **1956-02-23 事件在高纬度可达背景的 ×300**；
  - Dyer 等的结论：SPE 可在高纬航线上产生**高达宇宙线背景数百倍**的 SEU 率，即使在 40 000 ft 以下的亚音速高度亦然；
  - 复现 Campbell 等（I7）的 Bastille Day ADC 上翻观测图。
- 对本课题：**SPE 增强倍数的可引用数字区间**：大气/高纬环境 ×6 到 ×300（极端算例），GEO/行星际更高；而 LEO 低纬受地磁屏蔽保护，增强远小（F49 甚至在三次小型 SPE 期间未探测到 SEU）。§I.6 的 SPE 情节即据此取保守的 ×2–×10 并作敏感性扫描。
- 数据：**开放获取 PDF**（`raeng.org.uk`）。

**I22. 太阳耀斑对 SEU 环境的增强：Concorde/CREAM 与 ESA 工作组文集** — C. S. Dyer, A. J. Sims, J. Farren, J. Stephen, **IEEE Trans. Nucl. Sci. 37(6):1929–1937, Dec 1990**（"Measurements of solar flare enhancements to the single event upset environment in the upper atmosphere"）；汇总综述 C. S. Dyer, "Radiation Effects on Spacecraft & Aircraft"，**ESA Space Weather Workshop (SPW-W3) Proceedings（ESA/ESTEC，公开 PDF `solspa1.pdf`）**；另见 J. L. Barth, C. S. Dyer, E. G. Stassinopoulos, **IEEE Trans. Nucl. Sci. 50(3):466–482, Jun 2003**（"Space, Atmospheric, and Terrestrial Radiation Environments"，**卷期待核实**）
- 机制：CREAM 在 Concorde 与 Shuttle 上的长期测量，后发展为自由飞行器用的 CREDO（1990-01 首次搭载 UoSAT-3，**800 km、98.7°**，1990-04 至 1996-10 连续取得环境与上翻数据，覆盖太阳极大到极小，含 1991-03 创造新质子带的大事件）。CREAM 通道 1 的月均计数率与太阳周反相关，并与 Climax 中子监测器（海拔 3.4 km，截止刚度 2.96 GV）吻合良好。
- 对本课题：UoSAT-3/CREDO 是 **"同一平台上环境监测器与 SEU 计数器并置、连续六年半"** 的罕见配置，是本课题"ECC 计数 + 环境协变量融合"（F41 路线）的最早工程先例。`solspa1.pdf` 是本节数字最方便引用的**公开**来源。
- 数据：`solspa1.pdf` **公开可下载**（ESA SSA 空间天气门户）；IEEE 原文订阅。

**I23. 经典源头：耀斑对 SEU 率的影响** — J. H. Adams, A. Gelman, **IEEE Trans. Nucl. Sci., 1984**（"The effects of solar flares on single event upset rates"；**卷期有分歧，部分文献标为 39(6):1212–1216，与 1984 年不符，须核实**）
- 机制：最早预测"大型太阳粒子事件会显著提高 SEU 率"的工作之一。
- 对本课题：作为"λ 时变"命题的历史源头引文；但**卷期必须核对后再用**。
- 数据：IEEE 订阅。

---

## I.3 到达过程模型谱系：谁在用、用在哪、拟合出什么参数

> 结论先行：**本课题应当用"两态 MMPP（= 带 Poisson 发射的 HMM）"作为主模型，用 NHPP 作为仿真生成器，用 Gamma-Poisson/负二项处理器件级过度分散，并明确排除 Hawkes。** 下面逐条给依据。

**I24. 齐次 Poisson 过程（HPP）：现行 SEE 工程的默认假设及其适用边界**
- 机制：标准 SEE 速率预测（CREME96、OMERE、SPENVIS，见 F44/F45/F46）输出的是**一个速率标量**（upsets/bit-day），隐含"在给定环境条件下 SEU 到达是齐次 Poisson"。注意这里有个常被忽略的分层：**工程模型假设的是"条件齐次"——在固定环境下齐次，而不是在整条轨道上齐次**。I17 的 ESP 模型同样把"SPE 事件的发生"显式假设为 Poisson 过程。
- 对本课题：本课题的 Poisson 假设**不是错的，而是被用错了层级**。正确表述是："给定环境状态 s ∈ {SAA 内, SAA 外, SPE}，SEU 到达是速率 λ_s 的齐次 Poisson；环境状态本身随时间切换。"这句话一写出来，模型就自动变成 MMPP，而"λ 会变"也从缺陷变成了模型的一部分。**建议把这句话作为论文建模小节的第一句。**
- 参考：Petersen 等的 SEU 速率预测方法学；CREME96（I16）。

**I25. 非齐次 Poisson 过程（NHPP）：λ(t) 的标准形式与采样算法** — 源头 A. L. Goel, K. Okumoto, **IEEE Trans. Reliability 28:206–211, 1979**（时变错误检出率模型）；J. D. Musa, K. Okumoto, 1984（对数泊松执行时间模型）；软件可靠性领域的 NHPP 族综述见 Pham 等专著
- 机制：NHPP 以均值函数 `m(t) = ∫₀ᵗ λ(u)du` 取代常数 λ，`N(t) ~ Poisson(m(t))`。Goel-Okumoto：`λ(t) = a·b·exp(−b t)`；Musa-Okumoto：`m(t) = (1/θ)·ln(λ₀θt+1)`。**采样算法**（R 包 `NHPoisson` 的 `simNHP.fun` 文档给出标准两步法）：先用独立指数间隔生成强度为 1 的齐次 PP，再把齐次到达时刻经时间变换（`t → m⁻¹`）映射为强度 λ(t) 的 NHPP 到达时刻；等价的替代法是 **thinning（接受—拒绝）**：以 λ_max 生成齐次 PP，每点以概率 λ(t)/λ_max 接受。
- 对本课题：**这就是 §I.6 剖面的仿真生成器**。注意 NHPP 的 λ(t) 是**确定性函数**，适合"已知轨道根数 ⇒ 已知何时进 SAA"的仿真注入（即实验者知道真值，用于测量检测延迟）；而机上控制器看不到 λ(t)，面对的是 MMPP（I26）。**把"仿真用 NHPP、控制器假设 MMPP"这两件事分开写，是一个干净且容易被审稿人接受的实验设计表述。**
- 数据/代码：**R 包 `NHPoisson` 公开（CRAN）**，含 `simNHP.fun`、`GenEnv.fun` 等；Python 侧可用 `tick`、`pointprocess` 或自写 thinning（十余行）。
- 注：NHPP 在软件可靠性增长（SRGM）语境下的 λ(t) 是**单调衰减**（bug 被修掉），与本课题的**周期性 λ(t)** 形状完全不同。引用时必须只借其数学框架与采样算法，不要借其均值函数形式，否则会被质疑。

**I26. 马尔可夫调制 Poisson 过程（MMPP）：与"SAA 内/外两态"数学同构，且带现成的在线推断算法** — 机器学习侧经典用例 A. Ihler, J. Hutchins, P. Smyth, **ACM Transactions on Knowledge Discovery from Data (TKDD) 1(3), Article 13, Dec 2007**（"Learning to Detect Events with Markov-Modulated Poisson Processes"）
- 机制：MMPP 是 doubly stochastic Poisson（Cox）过程的子类：瞬时强度取 `λ(X(t)) ∈ {λ₁,…,λ_k}`，其中 X(t) 是**不可观测的连续时间马尔可夫链**。性质：
  - 在高速率态内到达成簇、低速率态内长间隔 ⇒ **天然再现 burstiness，这是单速率 Poisson 无论怎么调参都做不到的**；
  - 计数的方差超过均值（过度分散）：除态内 Poisson 方差外，还叠加态间切换带来的方差 `Var(λ(X(t)))×(态持续性因子)`，使总方差可达均值的数倍；
  - **仍然可用解析方法处理**（矩阵指数 + 前向算法）。两态 MMPP 的似然为 `L(θ)=π(∏ᵢ exp((Q−Λ)Δtᵢ)Λ)1ᵀ`，即**带 Poisson 发射的 HMM，前向算法直接可用**。
- 对本课题：**这是 F.10 / F56 的正确升级终点，强烈建议作为主模型**。理由：
  1. 结构与物理一一对应：隐状态 = {SAA 外, SAA 内}（或三态加 SPE），`Q` 的驻留时间就是 §I.6 的 90 min / 8 min；
  2. 前向算法输出的**滤波后验 `P(X(t)=HIGH | 事件历史)` 正是 F.10 映射三所需的那个 p**，然后用期望损失 `p·C_miss > (1−p)·C_overhead` 决策，**滞回自然产生、无需手工阈值**；
  3. 比 CUSUM/SPRT 更强的一点：MMPP 同时建模了**切换回去**（SAA 出境），而 CUSUM 只对单向变化最优；
  4. 计算量：两态前向更新是 2×2 矩阵指数，可预计算表格，星载完全可行。
- 数据/代码：**R 包 `LaMa`（CRAN）提供 MMPP/MMMPP 的 `forward()`、`tpm_ct()`、`generator()`，文档含两态示例（`lambda=c(2,15)`、`Q=[[-0.5,0.5],[2.0,-2.0]]`）**；Ihler 的 MMPP 事件检测代码历史上公开于作者主页（**链接有效性待核实**）。

**I27. MMPP 在可靠性/失效建模中的已发表用例与参数估计方法** — Y. G. Yera, R. E. Lillo, B. F. Nielsen, P. Ramírez-Cobo, F. Ruggeri, **Reliability Engineering and System Safety 208:107318, 2021**（"A bivariate two-state Markov modulated Poisson process for failure modeling"，DOI 10.1016/j.ress.2020.107318）；前序 Y. G. Yera, R. E. Lillo, P. Ramírez-Cobo, **European Journal of Operational Research 279(1):79–92, 2019**（两态 BMMPP 的拟合流程）
- 机制：明确陈述"**一维 MMPP 已被提出用于建模相依且非指数的失效间隔时间**（排队、风险、可靠性等语境）"，并把它扩展到二维（两序列失效间隔相依，基于 Marshall–Olkin 指数分布），推断采用**矩匹配 + ABC（近似贝叶斯计算）**组合方法。
- 对本课题：**这是"MMPP 用于失效到达建模"在可靠性顶刊上的正式背书引文**，可以直接写进论文的建模依据，回答"为什么不是普通 Poisson"。其两态拟合流程（EJOR 2019）可用于从真实 ECC 日志反估 `(λ₀, λ₁, Q)`——这正好替代本课题当前"手工设阈值"的做法。
- 数据：Elsevier 订阅；两态 BMMPP 拟合流程在 EJOR 论文与 UC3M 工作论文（公开）中给出。

**I28. 更新过程 / Weibull / 对数正态间隔：HPC 侧的实测拟合结果（本课题应作为"非 Poisson"的量化基线）**
- 机制与拟合参数（详见 I.4 各条）：
  - **Schroeder & Gibson, DSN 2006**：LANL 轨迹的失效间隔**用 Weibull 分布拟合良好，呈递减风险率（decreasing hazard rate）**；同组对磁盘更换的研究（FAST 2007）给出 **Weibull 形状参数 0.7–0.8**，并**拒绝了"间隔服从指数分布"的假设**；
  - **Failure Trace Archive（I32）**：对多类分布式系统统一拟合 **Weibull / Pareto / 对数正态 / Gamma** 四族，逐 trace 给出最佳拟合与参数；
  - **Tiwari DSN 2014（I35）**：直接用 Weibull 形状参数 β<1 推导"懒惰检查点"区间 `Δt_lazy(t) = Δt·(t/Δt)^{1−β}`。
- 对本课题：**递减风险率的物理含义与本课题相反**，这点必须讲清楚：HPC 的递减风险率来自"刚修好的节点更脆弱 / 退化器件反复报错"（器件级），而本课题 SAA 过境期内的**递增风险率**来自环境（进入 SAA 后越来越强直到 L≈1.24 峰值再衰减）。**两类非 Poisson 性的方向相反、机理不同，混用文献会出错。** 建议在论文中显式区分"器件退化致非 Poisson"与"环境调制致非 Poisson"，前者用去重/分桶处理（F.10 已给），后者用 MMPP 建模。这个区分本身就是一个可写的方法学贡献点。

**I29. Gamma-Poisson / 负二项：处理过度分散的最经济补丁（扩展 F56）**
- 机制：令 Poisson 速率 λ ~ Gamma(α, β)，则计数的边际分布为**负二项分布**，`Var(Y) > E(Y)`，允许过度分散；GLM 框架下对应 `negative binomial` 族，诊断量是 **Pearson χ² / 自由度（= 1 为 Poisson，> 1 为过度分散）** 或 deviance/df。替代做法是 quasi-Poisson（估计一个标度参数 φ，只修正标准误不改点估计）。
- 实测的过度分散幅度（可直接引用的数字，来自 I38）：GPU ECC 单比特错误的 **MTBE 为 24.84 ± 67.85 h（Delta）、7.08 ± 27.31 h（Perlmutter）**，即**标准差/均值 ≈ 2.7–3.9**；纯指数分布该比值恒为 1。**这是"真实 ECC 到达严重过度分散"最直接的量化证据。**
- 对本课题：F56 的 Gamma 共轭先验已经天然给出负二项边际，本条只需把"过度分散有多严重"的数字补上（σ/μ ≈ 3），并在论文中给出**检验流程**：对真实/合成 ECC 日志算 Pearson χ²/df，若 >1 则报告该值并改用负二项似然重算 LLR。这是一步成本很低、但能明显提高方法学严谨性的补充实验。
- 注：**未检索到专门针对 SEU/软错误计数做负二项拟合并给出 dispersion 参数的已发表论文**。这是一个真实的文献空白，本课题若做了这个拟合，可以作为一个小的方法学贡献来陈述（但不宜夸大）。

**I30. 自激（Hawkes）过程：数学定义清楚，但在本问题上应当被明确排除** — A. G. Hawkes, **Biometrika 58(1):83–90, 1971**（"Spectra of some self-exciting and mutually exciting point processes"）；无向工程读物见 P. J. Laub 等的 Hawkes 过程教程（公开）
- 机制：条件强度 `λ*(t) = μ(t) + Σ_{tᵢ<t} φ(t−tᵢ)`，每个事件抬高后续强度，产生时间簇集；常用指数核 `φ(u)=αe^{−βu}`，稳定性要求分支比 `n = α/β < 1`。
- 对本课题：**必须明确排除，并把排除理由写进论文——这本身是一个加分的建模判别。** 理由：
  - SAA 过境导致的簇集是**外因调制**（exogenous）：一次 SEU 并不会提高下一次 SEU 的概率，是环境同时提高了两者的概率。这在数学上是 **Cox / MMPP（doubly stochastic）**，而不是 Hawkes（endogenous self-excitation）。用 Hawkes 拟合会把环境效应误归因为自激，参数无物理意义。
  - **唯一可能真自激的成分是器件退化致的重复命中**（F37/F38 的"出过错的 DIMM 更易再出错"，以及 I33 的硬错误占 65–82%）。若本课题的 ECC 计数不做按位置去重，残留的就是这一部分，此时 Hawkes 才有物理解释。
  - 文献现状：**检索未发现把 Hawkes 过程用于 SEU / 软错误 / HPC 节点失效并给出拟合参数的已发表工作**（Hawkes 在金融、地震、社交网络、网络攻击预测中成熟，在本领域是空白）。因此不宜把"用 Hawkes"写成本课题的方法；但"论证为何 MMPP 而非 Hawkes"是可以写的，且显示建模素养。

---

## I.4 HPC/数据中心故障轨迹：确立非 Poisson 行为与时间相关性的实证基础

> 本节的作用是：(a) 给出"ECC/失效事件非 Poisson"的**拟合分布与过度分散数字**；(b) 指出**公开可下载的数据集**，使本课题可以在真实轨迹上验证检测器；(c) 把"环境致"与"器件致"两类非 Poisson 性分离（这是 I28 的论点的实证支撑）。

**I31. LANL 九年 22 套系统的失效轨迹：Weibull + 递减风险率（本领域的奠基引文）** — B. Schroeder, G. A. Gibson, **Proc. IEEE/IFIP International Conference on Dependable Systems and Networks (DSN) 2006**
- 机制：数据来自 LANL 于 1996-06 建立的 remedy 工单库，覆盖 **1996-12 至 2005-11、22 套 HPC 系统、4 750 个节点、24 101 个处理器、约 23 000 次失效**。每条记录含起止时间、系统与节点、负载类型（compute / graphics / fe）与归类后的根因。关键定量结论：
  - **失效间隔时间可用 Weibull 分布良好拟合，且呈递减风险率**；
  - 系统级失效率**大致与处理器数成正比**；
  - 硬件与软件是两大根因来源；
  - 同组 FAST 2007 的磁盘研究给出 **Weibull 形状参数 0.7–0.8**，并明确**拒绝指数间隔假设**。
- 对本课题：**"失效到达不是 Poisson"最常被引用的一篇**，必须引。但要按 I28 的区分使用：它的递减风险率是器件/运维致，与本课题的环境致周期性调制不是同一类现象。
- 数据：**公开可下载**。原始数据在 LANL 页面与 USENIX CFDR（见 I32）；论文 PDF 在 CMU PDL 公开（`pdl.cmu.edu/PDL-FTP/associated/dsn06.pdf`）。

**I32. 两个公开故障轨迹仓库：CFDR 与 Failure Trace Archive**
- (a) **Computer Failure Data Repository (CFDR)** — B. Schroeder, G. A. Gibson, CMU，**USENIX 赞助**；描述见 OSDI 2006 poster 与 MSR workshop 投稿（公开 PDF）。入口 `https://www.usenix.org/cfdr-data`。内容：LANL 22 套系统（1996-12–2005-11，节点中断记录 + 负载日志 + 错误日志）、HPC1（765 节点集群 + 64 文件系统节点，2001-08–2006-05，硬件更换日志）等。
- (b) **The Failure Trace Archive (FTA)** — D. Kondo, B. Javadi, A. Iosup, D. H. J. Epema, **Proc. 10th IEEE/ACM CCGrid 2010, pp. 398–407, DOI 10.1109/CCGRID.2010.71**；期刊扩展版 B. Javadi, D. Kondo, A. Iosup, D. H. J. Epema, **Journal of Parallel and Distributed Computing 73(8):1208–1223, 2013, DOI 10.1016/j.jpdc.2013.04.002**
  - 机制：把异构分布式系统（网格、P2P、桌面网格、HPC 集群、DNS 服务器等）的故障轨迹**统一格式化**，并给统一的统计分析与模型拟合流程；对可用/不可用间隔分别拟合 **Weibull、Pareto、对数正态、Gamma** 四族并逐 trace 报告最佳拟合与参数。
- 对本课题：**这两个仓库是"在真实轨迹上评估检测器"的现实路径**。做法：取 CFDR/FTA 的某条 trace，用 ruptures（F58）离线标定真变点，再跑本课题的四种检测器（F.10 实验设计第 3 步），得到 ARL₀ / 检测延迟 / 抖动次数的**真实数据版**结果——这比纯合成数据有说服力得多，且不需要额外采数据。注意：这些 trace 是节点中断而非 ECC 事件，需在论文中说明代理关系的局限。
- 数据：**均公开可下载**（`usenix.org/cfdr-data`；FTA 原站 `fta.scem.uws.edu.au` 曾下线，可从期刊版补充材料与镜像获取，**可用性待核实**）。

**I33. "宇宙线不会击中两次"：把软错误与硬错误定量分离（F37/F38 的量化版，本课题必引）** — A. A. Hwang, I. A. Stefanovici, B. Schroeder, **Proc. 17th ACM International Conference on Architectural Support for Programming Languages and Operating Systems (ASPLOS XVII) 2012, pp. 111–122**
- 机制：四个大型装置的 DRAM 错误数据——**IBM Blue Gene/L（LLNL）、Blue Gene/P（ANL）、加拿大 SciNet 的 HPC 集群、约 20 000 台 Google 服务器**。核心方法假设："**同一地址的重复错误极可能是硬错误**，因为同一位置在观测期内被宇宙线或其他外部噪声击中两次的统计概率极低。"关键定量结论：
  - **绝大多数内存错误（65%–82%）是硬错误**；
  - 由此倒推出的**软错误率上界**为：**每 GB 每天 3.72 个翻转位（其中一台超算）与 11.98 个翻转位（另一台）**；
  - 作者据此论证页面退役（page retirement）与后台巡检（scrubbing）策略的设计含义；
  - 文中也指出实践中硬错误表现为**间歇性**（不是每次访问都错），因此分类并不干净。
- 对本课题：**这是全册对本课题威胁最大、也最必须正面处理的一条**。它意味着：
  - 若直接把 ECC 可纠正事件计数当作"辐射强度代理"，**65%–82% 的信号其实是器件退化，不是环境**；在地面/数据中心如此，在星上比例会不同，但方向性风险成立；
  - 因此 **F.10 提出的"按物理位置分桶 + 重复命中去重"不是可选优化，而是必需的预处理步骤**，否则指标 A/B 测的主要是器件老化；
  - 反过来，它也给出了**软错误率的可引用上界（3.72 与 11.98 upset bits/GB/day）**，可用于核对本课题注入速率是否落在真实量级内（注意这是地面海平面环境，LEO 应显著更高）；
  - 建议论文的"威胁效度"小节专门用一段处理这篇，并把去重后的残余速率作为指标 A/B 的真实输入。
- 数据：论文公开 PDF 易得；原始数据未全部公开（Google 部分不公开）。

**I34. 五套生产系统八年失效数据：temporal recurrence parameter（时间复发的量化指标）** — S. Gupta, T. Patel, C. Engelmann, D. Tiwari, **Proc. SC17（ACM/IEEE International Conference for High Performance Computing, Networking, Storage and Analysis）2017, pp. 44:1–44:12, DOI 10.1145/3126908.3126937**（接受率 18.7%，61/327）
- 机制：分析 ORNL 五套生产系统（**Jaguar XT4、Jaguar XT5、Jaguar XK6、Eos、Titan XK7**，Titan 约 30 万 CPU 核）合计约 8 系统年的失效日志（时间跨度图覆盖 2008–2015）。关键定量结论：
  - 定义 **temporal recurrence parameter**（**数值越小表示时间复发性越强**），逐失效类型、逐系统给出，**取值范围约 0–1.25**；按季度平均还给出其随时间的演化曲线；
  - 定义 **neighborhood re-occurrence effect（邻域复发效应）**：在 0–96 小时的时间窗内，**机柜（cabinet）级邻域复发比例最高达 30%–35%**，机笼（cage）级最高约 25%–30%，刀片/模块级约 20%–25%，节点级约 6%–10%；
  - **关键结论：MTBF 与 temporal recurrence 是两个正交的量，不能互为代理**——高 MTBF 的系统仍可能有很强的时间复发性。
- 对本课题：
  - **"MTBF 不足以刻画可靠性"这一结论是本课题"为什么不能只用平均 λ"的最佳引文**，可直接支撑 §I.5 的指标选择；
  - temporal recurrence parameter 提供了一个**现成的、可引用的时间聚簇度量**，本课题可以在自己的合成/真实 ECC 日志上算同一个量，使"聚簇程度"有可比的数字，而不是定性描述；
  - 0–96 h 的邻域复发时间窗与 §I.5 的 Weibull 懒惰检查点时间尺度一致，说明 HPC 侧的相关时间尺度是**小时到天**，而本课题 SAA 侧是**分钟**——又一处时间尺度差异需在论文中交代。
- 数据：论文公开 PDF（`christian-engelmann.de/publications/gupta17failures.pdf`）；原始日志未公开。

**I35. 懒惰检查点：直接把 Weibull 形状参数变成"随时间放大的检查点间隔"** — D. Tiwari, S. Gupta, S. S. Vazhkudai, **Proc. IEEE/IFIP DSN 2014, pp. 25–36**（Best Paper Finalist）
- 机制：用 Titan 的失效数据论证**失效具有时间局部性**：失效间隔服从 Weibull 且**自上次失效后风险率随时间递减**（形状参数 β < 1）。据此提出"懒惰检查点"：**检查点间隔随距上次失效的时间递增**，`Δt_lazy(t) = Δt_opt·(t/Δt_opt)^{1−β}`，其斜率与失效率曲线斜率一致；目的是跳过一部分检查点以降低 I/O 开销。实测显著降低检查点 I/O 开销。
- 对本课题：**这是"把时变失效率直接写进控制律"的最简洁、最可照搬的模板**。本课题可以做同构的事：把保护档位（而不是检查点间隔）写成"距上次 ECC 事件时间"的函数——**即"刚发生过 ECC 事件 ⇒ 保持高档；长时间无事件 ⇒ 逐步降档"，且降档速度由拟合出的 β 决定，而不是手工设定的冷却时间。** 这是一个工作量很小但能明确替掉一个手工参数的改进，强烈建议纳入。
- 数据：论文与 artifact 由 Goodwill Computing Lab 公开列出（`goodwillcomputinglab.github.io/publications`，标注 Paper/Artifact/Presentation）。

**I36. 故障预测：信号分析 + 数据挖掘，并评估其对检查点策略的影响** — A. Gainaru, F. Cappello, M. Snir, W. Kramer, **Proc. SC 2012, paper 77**；配套 A. Gainaru, F. Cappello, W. Kramer, **Proc. IEEE IPDPS 2012, pp. 1168–1179**（"Taming of the Shrew: Modeling the Normal and Faulty Behaviour of Large-scale HPC Systems"）；另见 M.-S. Bouguerra, A. Gainaru 等, **IPDPS 2013, pp. 501–512**（主动 + 预防性检查点的组合）
- 机制：SC'12 自称是**首个把信号分析与数据挖掘结合**做 HPC 故障预测的工作；在预测步骤中显式利用**易故障组件的位置信息**；并进一步**研究预测对检查点策略的影响**（而不是只报预测精度）。IPDPS'12 给出"正常行为"与"故障行为"的建模框架。
- 对本课题：
  - "预测 ⇒ 改变容错策略"的闭环评估口径，与 F39/F40（Boixaderas）一脉相承，是本课题"成本—收益而非精度"评估口径的另一条支撑引文；
  - IPDPS'12 的"正常行为建模"方法（为每类事件建立基线信号模型再检测偏离）与本课题的"λ₀ 基线 + 偏离检测"同构，可作为方法学参照。
- 数据：论文公开 PDF（`snir.cs.illinois.edu/listed/C83.pdf`）；日志未公开。

**I37. GPU 侧软错误（SBE/DBE）的大规模现场研究系列** — B. Nie, D. Tiwari, S. Gupta, E. Smirni, J. H. Rogers, **Proc. IEEE HPCA 2016, pp. 519–530**（"A Large-Scale Study of Soft-Errors on GPUs in the Field"）；B. Nie, J. Xue, S. Gupta, C. Engelmann, E. Smirni, D. Tiwari, **Proc. IEEE MASCOTS 2017, pp. 22–31**（温度/功耗/软错误联合刻画）；B. Nie, J. Xue, S. Gupta, T. Patel, C. Engelmann, E. Smirni, D. Tiwari, **Proc. IEEE/IFIP DSN 2018, pp. 95–106**（ML 预测 GPU 错误）
- 机制：Titan 超算 GPU 节点（约 1.8 万块 K20X），数据覆盖 2013-06 至 2015-02。关注**单比特错误（SBE）**，因其最频繁、统计量足；双比特错误（DBE）太稀疏不适合预测。揭示 SBE 的**空间与时间局部性**及其与资源利用率的关系；MASCOTS'17 进一步分析**温度/功耗的空间非均匀性**（机柜左上/右下更热）与软错误的关系，并指出 ECC 本身带来存储与带宽开销，若 SBE 可预测则可考虑**选择性关闭 ECC** 以换性能。
- 对本课题：
  - **"若能预测软错误，就可以把 ECC 这类常开保护改为按需开启"——这正是本课题的核心论点在 GPU 语境下的独立表述**，是最贴近的跨领域同行工作，必须作为相关工作引用并讲清差异（本课题是代码级软件冗余档位，且环境是辐射而非温度/利用率）；
  - MASCOTS'17 的"温度/功耗作为协变量"与 F41（Middleware'17）一致，星上对应协变量是轨道位置 / 单粒子监测器 / 地磁指数。
- 数据：论文公开 PDF（OSTI / 作者主页）；原始日志未公开。

**I38. 最新的 GPU 内存错误到达间隔分布与过度分散实测（本课题"非 Poisson"的最强数字）**
- (a) **Summit（27 648 块 V100）** — V. Oles, A. Schmedding, G. Ostrouchov, W. Shin, E. Smirni, C. Engelmann, **Proc. 38th ACM International Conference on Supercomputing (ICS) 2024, pp. 188–200, DOI 10.1145/3650200.3656615**（接受率 36.0%）。数据含**全部 27 756 块 GPU 的 1 Hz 功耗与热遥测 + 作业调度记录 + NVIDIA XID 记录**；Summit 累计 **295 次 DBE、1 430 次 PRE、35 791 次 PRF**。一条关键发现：**近一半的（独立）作业内 DBE 与 GPU 利用率无关**。
- (b) **Delta / Polaris / Perlmutter 三机群（10 693 块 Ampere GPU，67.77 M GPU 器件小时）** — Z. Zhu 等, **arXiv:2508.03513, 2025（预印本，正式发表venue 待核实）**。关键定量结论（本册最有用的一组数字）：
  - **MTBE(SBE)：Delta 24.84 ± 67.85 h；Perlmutter 7.08 ± 27.31 h**；**MTBE(DBE)：Polaris 228.23 ± 298.27 h；Perlmutter 109.97 ± 199.47 h** —— 标准差远大于均值（σ/μ ≈ 1.3–3.9），**过度分散显著**；
  - Delta 的 SBE 到达间隔 CDF：**94.31% 的间隔 < 1 小时**，同时有长尾——**0.81% 的间隔 > 120 小时，最长达 851 小时（35 天）**；
  - DBE 到达间隔：Polaris 有 27.27% < 1 天、Perlmutter 51.56% < 1 天；最长间隔 Polaris 42 天、Perlmutter 23 天；
  - 按天统计：**Polaris 有 66.02% 的天完全无 DBE、Perlmutter 84.97%；而 Polaris 有 4.55%、Perlmutter 有 9.32% 的天出现"爆发"（单日 > 1 000 次错误）**；
  - 明确结论：**"错误在时间上不是均匀分布的；三个机群都观察到 burstiness"**、**"单一的 MTBE 数字会引入偏差，应当考察到达间隔的整个分布"**、以及"未观察到明显周期性"、"温度/功耗/利用率与错误无强相关"。
- 对本课题：
  - **"94.31% 的间隔 < 1 h 但 0.81% 的间隔 > 120 h"是"到达严重成簇"最直观的一组数字**，可直接用于论证单阈值/单窗长方案不可行；
  - **"不应用单一 MTBE，应看整个间隔分布"可以直接当作本课题评测指标的设计原则引用**（与 I34 的"MTBF 与复发性正交"同一结论的两个独立来源）；
  - σ/μ ≈ 3 给 I29 的负二项/过度分散讨论提供了具体的 dispersion 量级参照。
- 数据：(a) **公开数据集，DOI 10.13139/OLCF/1970187**（"OLCF Summit Supercomputer GPU Snapshots During Double-Bit Errors and Normal Operations"，W. Shin, V. Oles 等, 2023-04-20）——**这是目前最适合本课题直接下载使用的真实 ECC 事件数据集**；(b) arXiv 预印本公开。

**I39. 其他可引的现场研究（简条）**
- C. Di Martino 等, **Proc. IEEE/IFIP DSN 2014** — "Lessons Learned from the Analysis of System Failures at Petascale: The Case of Blue Waters"：使用 LogDiver 工具做日志分析，揭示 petascale 系统失效的时空特性。
- T. Siddiqua, V. Sridharan, S. E. Raasch, N. DeBardeleben, K. B. Ferreira, S. Levy, E. Baseman, Q. Guan, **IEEE DFT 2017, pp. 1–6** — "Lifetime memory reliability data from the field"：指出可纠正错误主要由内存控制器、总线与通道故障引起（而非单元翻转），进一步削弱"CE = 辐射"的朴素映射。
- M. V. Beigi, Y. Cao, S. Gurumurthi, C. Recchia, A. C. Walton, V. Sridharan, **Proc. IEEE HPCA 2023, pp. 991–1002** — "A Systematic Study of DDR4 DRAM Faults in the Field"。
- G. Ostrouchov 等, **Proc. SC 2020** — "GPU Lifetimes on Titan Supercomputer: Survival Analysis and Reliability"（**venue 待核实**）：生存分析方法学，对本课题"任务寿命内的累积风险"口径有参考价值。
- 对本课题：这一组的共同作用是**压制"ECC 可纠正事件 ⇒ 辐射"的朴素推断**，与 F35（Sridharan ASPLOS'15 的 CE→UE 不平凡）、I33 共同构成本课题最重要的威胁效度集合。

---

## I.5 Checkpointing 最优间隔理论：作为"时间感知评测"的指标模板

> 本节的价值不在检查点本身，而在**这个共同体的指标定义**：他们评价的是"长任务期内的期望有效功 / 浪费时间比例"，而不是单次运行的 SDC 率。这正是本课题需要的评测口径。下面把**公式原文**抄清。

**I40. Young 公式：一阶最优检查点间隔** — J. W. Young, **Communications of the ACM 17(9):530–531, 1974**（"A first order approximation to the optimum checkpoint interval"）
- 机制：`T_opt = sqrt(2·C·MTBF)`，其中 C 为单次检查点耗时、MTBF 为平均失效间隔。推导基于指数（无记忆）失效间隔与"失效时平均损失半个间隔"的近似。
- 对本课题：公式本身不直接用，但它确立了**"保护开销与失效损失之间存在平方根型最优"这一基本结构**。本课题的档位切换阈值在本质上是同一类权衡（升档开销 vs 漏保护损失），因此可以把本课题的决策规则表述为"Young/Daly 式权衡在离散档位空间上的在线版本"——这是一个站得住且好听的定位。

**I41. Daly 的高阶公式** — J. T. Daly, **Future Generation Computer Systems 22(3):303–312, 2006**（"A higher order estimate of the optimum checkpoint interval for restart dumps"）
- 机制：在 Young 基础上引入重启（restart）开销与高阶项，给出分段的高阶估计；并讨论次优检查点周期带来的效率损失。
- 对本课题：引用价值在于它**量化了"选错周期的代价"**——这对应本课题"阈值选错的代价"，是把"阈值敏感性分析"做成一张正经图的方法学依据。

**I42. Young/Daly 公式族的权威综述与 waste 的精确定义（本课题指标定义直接照抄这一条）** — A. Benoit, Y. Du, T. Herault, L. Marchal, G. Pallez, L. Perotin, Y. Robert, H. Sun, F. Vivien, **Proc. 14th International Conference on Contemporary Computing (IC3) 2022, pp. 701–710, ACM, DOI 10.1145/3549206.3549328**
- 机制：**waste 的定义（原文）**："应用未在执行有用计算的时间占比；检查点、恢复、停机与重执行都不计为有用计算。" 两个来源：
  - 无故障开销 `S₁ = C/(W+C)`（每做 W 单位有用功就花 C 秒检查点）；
  - 故障开销 `S₂`：每平均 μ 秒发生一次失效，损失 `D + R`（停机 + 恢复）加上自上次检查点以来的重执行功。
  - 带**可叠加检查点**（fraction α 可与计算重叠）的推广：**`W_YD = sqrt(2·μ·(1−α)·C)`**；α=0 退化为原 Young/Daly，α=1 给出 W=0（检查点免费则应一直做）。
  - 组合口径（见 I45 的 Springer 教材，同一组作者）：
    `(1 − Waste_FF)·Time_FF = Time_base`；`(1 − Waste_fail)·Time_final = Time_FF`；**`Waste = 1 − (1 − Waste_FF)(1 − Waste_fail)`**；其中 `Waste_FF = (T − Work)/T = (1−α)C/T`。
  - 分层（G 组）场景的 waste：`Waste_hier = 1 − [1 − (T−Work)/T]·[1 − (1/μ)(D(q)+R(q)+Re-Exec)]`。
  - 适用边界：公式为一阶近似，要求 **C ≪ μ**；文中还给出"检查点代价翻倍则周期应乘 √2，否则策略不再最优"的敏感性说明，以及对松耦合应用不适用的讨论。
- 对本课题：**这是本册对评测方法学最有用的一条，建议直接照搬其指标体系**：
  - 把本课题的指标从"per-run SDC 率"改成 **`Waste = 1 − (1 − Waste_protect)(1 − Waste_fail)`** 的类比形式：`Waste_protect` = 升档带来的指令/能耗开销占比（可由 F31 实测的 1.3×/1.7× 档位代价换算），`Waste_fail` = 未被保护的 SDC 导致的重算/任务失败损失占比；
  - 在一条完整的 λ(t) 轨道时间线上积分这个 waste，得到**"任务期期望有效功"**作为单一比较指标——这就把"固定双阈值 / Page-Hinkley / CUSUM-SPRT / MMPP 后验决策"四种策略放在同一把尺子上比；
  - 这个口径同时自动回答了 F39（SC'20）的要求——**用成本—收益而非 precision/recall 评估**。
- 数据：**开放获取 PDF**（`inria.hal.science/hal-03830322`、`ittc.ku.edu/~sun/publications/ic322.pdf`）。

**I43. 非指数（Weibull）失效下"周期性检查点不再最优"的硬证据** — M. Bougeret, H. Casanova, M. Rabie, Y. Robert, F. Vivien, **Proc. SC 2011, pp. 33:1–33:11**（"Checkpointing strategies for parallel jobs"）
- 机制：关键结论：
  - 对**指数分布**失效间隔，给出（作者自称首个严格的）**周期性检查点最优性证明**；
  - 对**非指数（Weibull）**失效，提出**动态规划**算法最大化"下次失效前完成的功"，作为最小化期望执行时间的启发式；
  - **定量证据：在 45 208 处理器的一次完整执行中，其 DPNextFailure 策略把检查点间隔从 2 984 秒一路改到 6 108 秒**（即最优解明显**非周期**，这解释了此前启发式表现不佳的原因）；
  - 指出 Bouguerra 等的 Weibull 方法依赖"每次失效后所有处理器都被更新（rejuvenate）"这一未声明假设，在实践中不合理；专门为 Weibull 设计的 Liu 方法在大平台上甚至算不出有意义的检查点时刻。
- 对本课题：**"失效率时变 ⇒ 固定周期/固定阈值策略在理论上就不是最优"的最直接引文**，而且带一个具体的数字（2 984 s → 6 108 s，约 2 倍变化范围）。本课题可以写："与 Bougeret 等在检查点间隔上的发现一致，在时变 λ(t) 下保护档位的最优切换时刻也不是周期性的，因此必须由在线统计量驱动。" 这是一句有文献支撑的强论断。
- 数据：**作者主页公开 PDF**（`lirmm.fr/~bougeret/publis/sc2011.pdf`）。

**I44. 周期性在"rejuvenation"假设下的最优性（被 I43 质疑的那一条，需并引）** — M.-S. Bouguerra, T. Gautier, D. Trystram, J.-M. Vincent, **Proc. PPAM 2010, LNCS 6067, pp. 206–215**（"A flexible checkpoint/restart model in distributed systems"）
- 机制：证明在检查点与恢复开销为常数时，对**指数或 Weibull** 失效，最优检查点策略是周期性的——但该结论隐含"所有处理器在每次失效与每次检查点后被更新"。
- 对本课题：引用它 + I43 构成一组**正反对照**，用来精确界定"什么条件下固定周期/固定阈值是可接受的"。本课题的 SAA 场景显然不满足 rejuvenation（环境状态在事件之间是连续演化的），因此应落在 I43 一侧。

**I45. 把故障预测接入检查点理论（最接近本课题"用预测驱动调档"的理论工作）** — G. Aupy, Y. Robert, F. Vivien, D. Zaidouni, **Journal of Parallel and Distributed Computing 74(2):2048–…, 2014, DOI 10.1016/j.jpdc.2013.10.010**（"Checkpointing algorithms and fault prediction"）；配套 G. Aupy, Y. Robert, F. Vivien, D. Zaidouni, **Proc. IEEE PRDC 2013, pp. 1–10**（"Checkpointing strategies with prediction windows"）；G. Aupy, A. Benoit, T. Herault, Y. Robert, F. Vivien, D. Zaidouni, **PRDC 2013**（静默错误检测与检查点的组合）；综合教材 T. Herault, Y. Robert（编），**Fault-Tolerance Techniques for High-Performance Computing, Springer, 2015**（第 1 章给出完整 waste 公式体系，见 I42 引用的公式）
- 机制：**把 Young/Daly 的一阶分析扩展到"有故障预测器"的情形**：预测器以召回率 r 与精度 p 给出预警（PRDC'13 还考虑预警是一个**时间窗**而非时刻），推导出预警驱动的额外检查点应在何时插入、以及最优周期如何随 (r, p) 变化。
- 对本课题：**这是"预测质量 → 最优策略参数"解析关系的唯一现成模板**。本课题可照此写：以检测器的 (ARL₀, 平均检测延迟) 替代 (p, r)，解出最优的档位切换边界——这把 F.10 的"阈值由 α、β 反解"从检测论层面推进到**决策论层面**，并且有正规引文。强烈建议作为论文"方法"章的一个小节或"未来工作"的首条。
- 数据：Elsevier 订阅；Springer 教材章节有公开 PDF（`icl.utk.edu/~herault/papers/100 - Fault Tolerance Techniques for HPC (2015).pdf`）。

**I46. 多级检查点与"不确定执行规模"下的优化（Di / Cappello 线）** — S. Di, M.-S. Bouguerra, L. Bautista-Gomez, F. Cappello, **Proc. IEEE IPDPS 2014, pp. 1181–1190**（"Optimization of multi-level checkpoint model for large scale HPC applications"）；S. Di, L. Bautista-Gomez, F. Cappello, **Proc. SC 2014**（"Optimization of a multilevel checkpoint model with uncertain execution scales"）；S. Di, Y. Robert, F. Vivien, F. Cappello, **IEEE Trans. Parallel and Distributed Systems**（"Toward an Optimal Online Checkpoint Solution under a Two-Level HPC Checkpoint Model"，OSTI 公开稿；**卷期年份待核实**）；另见 A. Benoit, A. Cavelan, V. Le Fèvre, Y. Robert, H. Sun, **IEEE Trans. Computers 66(7):1212–1226, 2017**（"Towards optimal multi-level checkpointing"）
- 机制：多级检查点在不同层级（本地 / 伙伴 / PFS）有不同开销与不同恢复能力，需为每级独立优化间隔；SC'14 版进一步处理**执行规模不确定**的情形；TPDS 版给出**在线**（而非离线预先给定）的两级最优解。
- 对本课题：**结构同构性极强**：多级检查点的"每级不同开销 + 不同覆盖能力"就是本课题"多档保护（无保护 / 近似冗余 / 全 TMR）的不同开销 + 不同 SDC 覆盖率"；"在线、执行规模不确定"对应本课题的"λ 未知、需在线估计"。建议把本课题形式化为**"多级保护的在线最优选择问题"**，并引这一组作为问题形式化的先例——这比"两档状态机 + 两个阈值"的表述学术层级高得多，且几乎不改实现。
- 数据：论文公开 PDF（OSTI / 作者主页）。

**I47. 分区段（region-wise）的 waste 模型：时变 λ 下分段评估的直接模板** — L. Bautista-Gomez, A. Gainaru, S. Perarnau, D. Tiwari, S. Gupta, C. Engelmann, F. Cappello, M. Snir, **Proc. IEEE IPDPS 2016**（"Reducing Waste in Large Scale Systems through Introspective Analysis"；另有同名 "Reducing Waste in Extreme Scale Systems through Introspective Analysis" 版本，**两个标题/会议须核对取一**）
- 机制：把执行划分为 R 个区段，**总浪费时间为各区段的检查点、重启开销与重执行之和**：
  `T_waste^total = Σ_{i=1..R} (Ck_i + Rt_i + Rx_i)`，其中 `Ck_i = (Ex · px_i / α_i) · β`（α_i 为第 i 区段的检查点间隔，β 为单次检查点耗时，`Ex·px_i` 为该区段的执行时间）；区段内失效次数由"一个计算区间 + 一次检查点"构成的 pair 数 `P_i = Ex·px_i/α_i` 推出。
- 对本课题：**这是"把 waste 指标按时间分段累加"的现成公式**，正好匹配 λ(t) 分段常数的结构（SAA 内 / SAA 外 / SPE）。本课题的评测可以直接写成 `Waste_total = Σ_s (Waste_protect(s) + Waste_fail(s))·f_s`，其中 s 遍历环境态、f_s 为该态的时间占比，并引此条为公式形式的来源。
- 数据：论文公开 PDF（`snir.cs.illinois.edu/listed/C103.pdf`）。

**I48. 非无记忆失效下的检查点策略（最新一支）** — A. Benoit, L. Perotin, Y. Robert, F. Vivien, **INRIA Research Report RR-9465, 2022**（"Checkpointing strategies to protect parallel jobs from non-memoryless fail-stop errors"）；A. Benoit, L. Perotin, Y. Robert, H. Sun, **INRIA RR-9413, 2021**（"Checkpointing Workflows à la Young/Daly Is Not Good Enough"）
- 机制：针对非无记忆（non-memoryless）失效分布设计策略，并论证 Young/Daly 对工作流应用不够好。
- 对本课题：**与本课题"λ 非平稳"最贴近的理论支线**，可作为论文"相关工作（时变失效率下的最优策略）"的收尾引文。**注意：这两篇在抓取时只确认了 INRIA 技术报告版，期刊/会议正式版（疑为 ACM TOPC 或 IEEE TPDS）未核实，引用前须查。**
- 数据：INRIA HAL 公开 PDF。

**I49. 其他可引的工程化变体（简条）**
- A. Frank, M. Baumgartner, R. Salkhordeh, A. Brinkmann, **Proc. IEEE IPDPS（"Improving checkpointing intervals by considering individual job failure probabilities"；年份待核实）**：按单作业失效概率而非系统 MTBF 选间隔；在四套 HPC 的作业轨迹（Mogon II、LANL Mustang、LANL Atlas、Curie）上，**Weibull 失效下累计检查点节省 7.1%–25.7%，Poisson 失效下 7.3%–27.5%**；Mogon II 平均节省最高 18.1%，Mustang 13.0%，Atlas 9.0%，Curie 10.5%；仅有不准确 MTBF 时节省 6.0%–26.6%。
- K. Tang, D. Tiwari, S. Gupta, P. Huang, Q. Lu, C. Engelmann, X. He, **Proc. IEEE/IFIP DSN 2016**（"Power-aware Checkpointing: Toward the Optimal Checkpointing Interval under Power Capping"）：功率受限下的最优间隔——与本课题"星载功耗预算下选保护档位"结构同构。
- 对本课题：I49 的数字（7%–27% 的节省幅度）给出了**"把固定策略换成自适应策略能带来多少收益"的量级参照**。本课题若在仿真中得到同一量级（10%–30%）的 waste 改善，就是一个可信的结果；若声称数倍改善则需要特别小心地解释。

---

## I.6 推荐的 LEO λ(t) 剖面：可直接驱动仿真注入实验的一组数字

> 本节给出一个**自洽、每个数字都有引文、且可做敏感性扫描**的 λ(t) 剖面。目标轨道取 **700 km 太阳同步（倾角 98°）**——UoSAT-2（I1）、Alsat-1（I9）、F50 的立方星场景都在这个轨道上，引文最密集。

### I.6.1 模型形式（三项可加，依据 I4 与 I14）

```
λ(t) = λ_GCR  +  λ_SAA · 1{s(t) = SAA}  +  λ_SPE(t)
```
- 第一项常数，第二项由隐状态 s(t) 门控，第三项为慢变包络。可加分解的依据：Harboe-Sørensen TNS 1993（I4，在轨 SEU 被显式分解为 GCR / SAA / 太阳耀斑三成分）与 MSL/RAD（I14，周期成分 + 脉冲成分 + 慢趋势的实测范例）。
- 为与控制器视角一致，把前两项合并写成两态形式：`λ_out = λ_GCR`（SAA 外），`λ_in = λ_GCR + λ_SAA`（SAA 内）。

### I.6.2 时间结构参数

| 参数 | 推荐值 | 依据 |
| :-- | :-- | :-- |
| 轨道周期 `T_orb` | **98.5 min** | Alsat-1 实际轨道周期（I9，700 km SSO / 98°） |
| 单次 SAA 过境时长 `τ_in` | **8 min（区间 5–10 min）** | Underwood ESA SP-439 2000（I2）明确给出"每轨一次、每次 5–10 分钟的簇"；Anderson JASTP 2018（I15）独立表述为"每次数分钟" |
| SAA 外停留时长 `τ_out` | **90 min** | = 98.5 − 8（取整） |
| SAA 时间占比 `f` | **0.082（敏感性区间 0.05–0.10）** | 由 MMPP 稳态解给出：`f = (1/τ_out)/((1/τ_out)+(1/τ_in)) = (1/90)/(1/90+1/8) = 0.0816` |
| MMPP 生成矩阵 `Q`（单位 min⁻¹） | `Q = [[−1/90, 1/90], [1/8, −1/8]] = [[−0.0111, 0.0111], [0.125, −0.125]]` | 由 τ_out / τ_in 直接构造；格式与 R 包 `LaMa` 的 `generator()` 一致（I26） |
| 事件记录/分箱分辨率 | **5 min bin（或事件级时间戳）** | MPTB/CREDO 的公开图件即以"每 5 分钟 SEU 数"呈现（I6）；UoSAT-2 为 8.25 min（I1）；CREME96 的峰值通量档也是 5 min（I16） |

> 工程提醒：上表把"每轨都过 SAA"作为简化。真实 SSO 上一天 14–15 轨中只有约 6 轨穿越 SAA 核心区，若要更保真，应把 `τ_out` 取为"相邻两次 SAA 过境的平均间隔"（约 3.5–4 h）并相应减小 f 至 0.03–0.04，R 随之上升（见下表）。**建议主实验用上表的简化值，并在敏感性分析中报告保真版本。**

### I.6.3 速率比 λ_in / λ_out（关键参数，必须做扫描）

由"SEU 集中度 p"（落在 SAA 内的事件占比）与"时间占比 f"可解析求出：

```
R = λ_in / λ_out = [ p / (1 − p) ] · [ (1 − f) / f ]
```

| p（SAA 内事件占比） | 出处 | f = 0.05 | f = 0.082 | f = 0.10 |
| :-- | :-- | :-- | :-- | :-- |
| 0.975 | **F49 实测（97.5%，峰值 L≈1.24–1.25）** | 741 | **439** | 351 |
| 0.95 | F49 与 I8 之间的折中 | 361 | **214** | 171 |
| 0.90 | **I8（CRUX：>90% 为质子致/SAA 致）** | 171 | **101** | 81 |
| 0.70 | 近似 I11（GCR 主导器件，双模弱） | 44 | **26** | 21 |

**推荐口径**：
- **主实验取 R = 200**（对应 p≈0.95、f≈0.082），这是 F49 与 I8 两个独立来源之间的保守中值；
- **敏感性扫描取 R ∈ {25, 100, 200, 450}**，四个点分别对应"GCR 主导器件 / CRUX 口径 / 主实验 / F49 口径"，每个点都有引文（I11 / I8 / 折中 / F49）；
- **必须在论文中说明 R 是器件相关的**：I11（Flying Laptop，GCR 主导、SEU 与太阳活动反相关）与 F49（SAA 质子主导、97.5% 集中）是同一类轨道上结论相反的两组实测，根因是器件阈值 LET 与屏蔽差异。把 R 做成扫描参数而非单点，正是回应这一矛盾的正确方式。

### I.6.4 绝对速率的标定（两个在轨锚点）

设受保护存储阵列规模为 `B` 比特，任务平均每比特速率为 `σ`（upsets/bit-day），则
```
λ̄ = B · σ / 86400   [events/s]
λ_out = λ̄ / ( f·R + (1 − f) )
λ_in  = R · λ_out
每次 SAA 过境的期望事件数 = λ_in · τ_in
```

两个可引用的在轨锚点：

| 锚点 | σ（upsets/bit-day） | 出处 |
| :-- | :-- | :-- |
| **锚点 A（保守）** | **4.04×10⁻⁷** | Alsat-1 实测，700 km SSO / 98°，COTS 存储器（**I9**） |
| **锚点 B（主用）** | **4.21×10⁻⁶** | SAMPEX 的 Hitachi 256k SRAM 在轨实测（**I8**；同篇给出 TOMS/Meteor-3 为 3.12×10⁻⁶，预测与实测在 25% 内） |

以 **B = 4 MiB = 3.355×10⁷ bit**、**f = 0.082、R = 439** 为例：

| 量 | 锚点 A | 锚点 B |
| :-- | :-- | :-- |
| λ̄ | 13.6 events/day = 1.57×10⁻⁴ /s | 141 events/day = 1.64×10⁻³ /s |
| λ_out | 4.3×10⁻⁶ /s（≈ 1 事件 / 65 h） | **4.5×10⁻⁵ /s（≈ 1 事件 / 6.2 h）** |
| λ_in | 1.9×10⁻³ /s（≈ 1 事件 / 8.9 min） | **2.0×10⁻² /s（≈ 1 事件 / 51 s）** |
| 每次 8 min 过境的期望事件数 | ≈ 0.9 | **≈ 9.4** |

**选型建议**：检测器要在 8 分钟内把 LLR 累积到判决边界，**每次过境至少需要 5–50 个事件**。因此：
- 用**锚点 B + B ∈ [4, 16] MiB**（对应每次过境 9–38 个事件），这是主实验的推荐配置；
- 锚点 A 对应"每次过境约 1 个事件"，此时**任何基于事件计数的检测器都不可能在单次过境内完成判决**——这本身是一个值得报告的负结果，说明在低速率器件上必须改为跨多轨累积（或改用轨道位置先验而非纯 ECC 信号），应作为方法适用边界写入论文。

### I.6.5 可选的 SPE 情节（叠加在上述周期结构之上）

三层嵌套时间窗**直接取 CREME96 的定义**（I16），这是最权威、最省争议的做法：

| 层 | 时长 | 建议的速率倍增（作用于 λ_out 分量，即高纬/GCR 通道） | 依据 |
| :-- | :-- | :-- | :-- |
| 包络（worst week） | **180 h（7.5 天）** | ×1.5 – ×3 | CREME96 worst week = 1989-10-19…10-27 共 180 h 平均（I16）；TDRS-1 高速率期约 6 天（I5） |
| 核心（worst day） | **18 h** | ×2 – ×10 | CREME96 worst day = 1989-10-20…10-21 的 18 h 区间（I16） |
| 峰值（peak） | **5 min** | ×10 – ×50（极端算例可至 ×300） | CREME96 peak 5-minute flux（I16）；Concorde/CREAM 实测瞬时 ×10、飞行平均 ×6，1956-02 事件反算 ×300（I21） |

**三条必须写明的建模约束**：
1. **LEO 低纬受地磁屏蔽，SPE 增强远小于 GEO/行星际。** I5 的 TDRS-1（GEO，6 天 243 次上翻）与 I14 的 MSL（巡航 1.2–19.5 mSv/event vs 火星表面 0.025 mSv，约 500× 落差）共同说明屏蔽可把增强压掉二到三个数量级。因此 LEO 的倍增取上表区间的**下端**，不要照搬 GEO 数字。
2. **SPE 主要抬高 λ_out（高纬/极区通道），而不是 λ_in。** F49 实测：三次小型太阳质子事件期间**未探测到 SEU**；UoSAT-2（I1）的 GCR/耀斑成分聚集在 **>60° 高纬**。因此倍增应作用于 SAA 外分量，这会**降低 R**，是对检测器更不利（更难区分两态）的情形——正好是一个好的鲁棒性测试。
3. **必须加入"反向"对抗情节。** F49 的反直觉实测结论是：**地磁暴期间 SAA 内质子通量被耗空，SER 显著下降**。建议在实验中加一个 `λ_in → 0.5·λ_in` 持续 12–24 h 的情节，检验检测器是否会错误降档并导致后续过境漏保护。**这个对抗用例在现有文献中没有人做过，是本课题实验设计上一个廉价但有辨识度的增量。**

### I.6.6 生成器实现（两条路径，建议都做）

- **路径 1（仿真真值已知，用于测延迟）**：按 §I.6.2 的 `Q` 采样隐状态轨迹 → 得到分段常数 λ(t) → 用 **thinning** 或 **NHPoisson 的时间变换法**（I25）采样事件时刻。实验者知道真变点位置，可直接算检测延迟与 ARL₀，无需 ruptures 标定。
- **路径 2（真实环境驱动，用于说服力）**：取 **PROBA-1/SREM 公开的 >10 MeV 质子计数率时间序列**（I10，`srem.psi.ch`）× 器件质子截面 → 得到真实 λ(t) → 同样按 NHPP 采样。此路径的 λ(t) 自带真实的 SAA 过境形状、SPE 事件与地磁暴响应，不需要人工编排情节。
- **路径 3（真实事件流，用于非 Poisson 鲁棒性）**：取 **OLCF Summit 的公开 DBE 数据集（DOI 10.13139/OLCF/1970187，I38a）** 作为"器件致聚簇噪声"的真实样本，叠加到路径 1/2 的环境致事件流上，检验 F.10 的去重/分桶预处理是否有效。这一步直接回应 I33（硬错误占 65%–82%）的威胁。

### I.6.7 一张可以直接写进论文的参数表

```
# LEO 700 km SSO (i=98°) — 推荐基线剖面
T_orb        = 98.5  min                 # Alsat-1 实测 (I9)
tau_in       = 8     min   (5–10)        # SAA 过境 (I2, I15)
tau_out      = 90    min                 # = T_orb - tau_in
f_SAA        = 0.082       (0.05–0.10)   # MMPP 稳态
Q            = [[-1/90, 1/90], [1/8, -1/8]]   min^-1
R = l_in/l_out = 200       (25 / 100 / 200 / 450 四点扫描)   # p,f 解析式；F49 / I8 / I11
sigma        = 4.21e-6 upsets/bit-day    # SAMPEX 在轨实测 (I8)；保守锚点 4.04e-7 (I9)
B            = 4–16 MiB                  # 使每次过境期望事件数落在 9–38
lambda_out   = 4.5e-5  /s                # ≈ 1 事件 / 6.2 h
lambda_in    = 2.0e-2  /s                # ≈ 1 事件 / 51 s
# 可选 SPE 情节（倍增作用于 lambda_out）
SPE_envelope = 180 h, x1.5–x3            # CREME96 worst week (I16)
SPE_core     = 18  h, x2–x10             # CREME96 worst day  (I16)
SPE_peak     = 5   min, x10–x50          # CREME96 peak 5-min (I16); Concorde x6–x10 (I21)
# 对抗情节（地磁暴致 SAA 质子耗空）
STORM        = 12–24 h, lambda_in -> 0.5 * lambda_in   # F49 实测反直觉结论
```

---

## I.7 核验状态与风险提示

**已核实（venue + 年份 + 页码/卷期至少两处来源一致）**：I1（部分）、I2、I3、I4、I5（期刊论文部分）、I6(a)(b)(c)(d)、I7、I8、I9、I11、I15、I16、I17、I18、I20、I21、I25、I26、I27、I30、I31、I32、I33、I34、I35、I36、I37、I38(a)、I40、I41、I42、I43、I44、I45、I46。

**待核实（引用前必须查，已在条目内标注）**：
1. **I5** 的 NOAA 文档 `TDRS1_SEU.pdf` 作者署名未确认；该站点 TLS 握手在本机失败，需换客户端下载。期刊论文（Croley et al., TNS 42(5):1489–1496, 1995）本身已核实。
2. **I10** 的 ICRC 2013 paper 1224 作者署名未确认。
3. **I12(a)** RadFxSat-2 的 SmallSat 会议年份未确认（约 2021–2022）。
4. **I12(b)** CELESTA（Coronetti et al., 2024）的期刊/会议名未确认，疑为 IEEE TNS。
5. **I13(a)** RHEME on ISS（IEEE Xplore 文档 8584285, 2018）的期刊卷期未确认，疑为 IEEE TNS。
6. **I22** 的 Barth/Dyer/Stassinopoulos "Space, Atmospheric, and Terrestrial Radiation Environments" 卷期（标为 IEEE TNS 50(3):466–482, Jun 2003）未二次确认。
7. **I23** Adams & Gelman 1984 的卷期在不同文献中标注矛盾（有标为 TNS 39(6):1212–1216，与 1984 年不符），**必须查原刊后再引**。
8. **I38(b)** Zhu Zhu 等 "Understanding the Landscape of Ampere GPU Memory Errors" 目前只确认 arXiv:2508.03513（2025），正式发表 venue 未确认。
9. **I39** 中 Ostrouchov 等 "GPU Lifetimes on Titan Supercomputer" 的 venue（标为 SC 2020）未二次确认。
10. **I46** 中 Di/Robert/Vivien/Cappello 的 TPDS 论文卷期年份未确认（仅见 OSTI 公开稿）。
11. **I47** 存在两个相近标题（"Reducing Waste in Large Scale Systems…" / "…in Extreme Scale Systems…"），须核对确定实际会议与标题。
12. **I48** 只确认 INRIA 技术报告版（RR-9465 / RR-9413），期刊或会议正式版未确认。
13. **I49** 中 Frank 等的 IPDPS 年份未确认。
14. **I32(b)** FTA 原站点可用性未确认（原域名曾下线）。

**三条方法学风险（建议写入论文"威胁效度"一节）**：
- **R1：ECC 信号的信噪比问题。** I33 给出地面系统中 65%–82% 的内存错误是硬错误；I39 中 Siddiqua 等进一步指出可纠正错误主要源于内存控制器/总线/通道而非单元翻转。本课题若把 ECC 计数直接当辐射强度代理，信号中可能只有少部分来自环境。**必须做按位置分桶与重复命中去重，并报告去重前后的速率差。**
- **R2：双模强弱是器件相关的。** I11（GCR 主导，SEU 与太阳活动反相关）与 F49（SAA 质子主导，97.5% 集中）在同类轨道上得到相反结论。**R 必须作为扫描参数，并明确声明本方法在"GCR 主导器件（R≈25）"上的收益会显著下降。**
- **R3：两类非 Poisson 性方向相反。** HPC 文献的非 Poisson 性（Weibull 递减风险率，I31/I35）来自器件退化；本课题 SAA 过境内的非 Poisson 性（速率随 L 值上升至 L≈1.24 峰值后衰减）是环境致、方向相反。**混引会出错；建议在论文中显式区分，这个区分本身是一个可陈述的方法学贡献。**

---

### 附：本册引文的论文章节建议归位

- **绪论 / 研究背景（λ 时变的物理必然性与时间常数）**：I1、I2、I4、I5、I15、I16
- **相关工作（在轨实测与环境模型）**：I6–I14、I17–I23
- **方法章（过程模型选择与在线推断）**：I24（Poisson 的正确分层表述）、I25（NHPP 采样）、**I26 + I27（MMPP 主模型）**、I29（过度分散）、I30（为何排除 Hawkes）
- **方法章（决策与控制律）**：**I35（把时变失效率写进控制律的模板）**、I45（预测质量 → 最优策略参数）、I46（多级保护的在线最优选择，建议作为问题形式化的先例）
- **实验章（指标定义）**：**I42（waste 的精确定义，建议直接照搬）**、I47（分区段 waste 累加）、I34 + I38(b)（"单一 MTBF/MTBE 不足"的双独立来源）、I43（时变失效率下非周期最优的硬证据）
- **实验章（λ(t) 剖面与数据源）**：§I.6 全节；数据源 I10（SREM）、I32（CFDR/FTA）、I38(a)（OLCF Summit 公开数据集）
- **威胁效度**：I33（硬错误占比）、I11（器件相关性）、I28/I31/I35（两类非 Poisson 性的区分）、I39（CE 的真实来源）
