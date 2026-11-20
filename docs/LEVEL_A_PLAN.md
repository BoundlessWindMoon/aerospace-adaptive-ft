# 层次 A（空天宏观）实验方案

调研了同题顶刊/顶会怎么做，然后定方案。层次 B 的清单在 `docs/EXPERIMENTS_TO_ADD.md`。

---

## 1. 现在虚在哪

你的链条是：文献 λ（upsets/bit-day）× 假设的 4 MiB 故障空间 × FI 百分比 → "每 90 天期望失效数"。

顶刊走的是**两步法**，中间有两环你没有：

$$\sigma_{app}=\sum_{r}\mathrm{bits}_r\cdot\sigma_{bit,r}\cdot \mathrm{AVF}_r,\qquad
\lambda_{app}(t)=\sigma_{app}\cdot \Phi(t)$$

- `bits_r`、`AVF_r`：按**物理资源**（寄存器文件 / 数据段+栈 / 堆 / 指令流 / cache）分桶的位数与易损概率。你现在是按**注入类别**分桶，没有位数权重，所以折不出 cm²。
- `σ_bit,r`：器件的每位截面（cm²/bit），来自公开辐照报告。你完全没有这一环，直接用了 upsets/bit-day 的整体数。
- `Φ(t)`：沿轨道的粒子通量，标准做法是 SPENVIS/CREME 输出。你用的是两篇文献的 σ 实测值加自建的 `|B|`-深度代理。

缺这三环的直接后果：你报不出 **cross-section** 和 **FIT**，而这两个是空天方向的通用货币；"每 90 天失效数"虽然结论不错，但在审阅者眼里是自造指标。

---

## 2. 顶刊的三条模板

### 模板 1：BYU/LANL 的 COAST 系列 —— 和你同题的 TNS 模板

- Bohman et al., **IEEE TNS 66(1), 2019**，*Microcontroller Compiler-Assisted Software Fault Tolerance*：
  指标五元组 = Code Size / RAM Size / Runtime / MWTF / Fault Coverage，加中子束。结果口径是
  "MWTF 提升最多 7×、cross-section 降最多 28×"。
- James et al., **TNS 67(1), 2020**，*Applying Compiler-Automated SWFT to Multiple Processor
  Platforms*：同一个工具跨多个处理器平台，证明方法的平台普适性。
- James et al., **TNS 68(5), 2021**，*Investigating How Software Characteristics Impact the
  Effectiveness of Automated SWFT*：**这篇是你主张一的模板**。8 个负载 × 3 块 PYNQ
  （ZYNQ XC7Z020，28 nm 双核 A9，无 ECC cache），在 LANSCE 中子束打 5 天，板距中子探测器
  99/101/106 cm（折算各板 fluence）。指标：$\text{cross section}=\text{SDC 数}/\text{fluence}$，
  再用 MWTF $=(\text{raw error rate}\cdot\mathrm{AVF}\cdot t_{exec})^{-1}$ 把运行时惩罚算进去。
  结果：cross-section 降 1.0–54×，MWTF 提升 1.1–43×，**其中 nanojpeg 和 qsortLib 的 MWTF 变差**
  （运行时变长吃掉了 cross-section 收益）。它找到的有效软件特征只有两个：峰值堆用量（负相关，
  根因是 malloc 调用频率）、静态内存 `.data+.bss` 大小（正相关）；并自述"数据集小、可能是伪相关"，
  因此另做一组 FI 实验专门隔离验证这两个特征。
- James & Goeders, **DATE'21**：把同一套东西用到 RTOS，MWTF 提升 1.3–257×。

对你的含义：**顶刊的诚实度门槛是"逐负载给，并保留变差的格子"**，不是给一个几何平均。
而它找到的特征只有两个且自认不稳 —— 你手里 per-kernel 属性比它多得多，这是你的增量空间（见 §4）。

### 模板 2：RADECS/TNS 的 FIT 折算 —— 没有束流也能报 FIT

- Bodmann, Oliveira, Rech, **RADECS'21 → IEEE TNS 2022**，*Accurate FIT Rate Estimation Through
  High-Level Software Fault Injection*：用器件的 **technology sensitivity factor**（原始中子敏感度）
  把高层软件注入的结果折成器件 FIT 率。核心就是"软件侧给 AVF、器件侧给 σ_bit、两者相乘"。
- CERN/ARIS 的两步法（经典表述）：① 地面辐照给静态 SEU 截面 $\sigma_{SEU}=\#upsets/\#particles$
  (cm²/bit)；② 注入给 $\tau_{inj}=\#errors/\#upsets$（多少个 upset 才引发一次应用错误）；
  相乘得应用错误率。还有一条对你很有用的事实：**动态截面远低于静态截面**——寄存器约 10×，
  数据 cache 约 10⁴×。这就是你"masked 占 60%"的物理对应，可以直接引来解释。
- 可引的公开 σ_bit 来源（不用自己打束流）：ESA ESCIES 的 **GR740 Radiation Summary**、
  Microchip 的 **RTG4 Proton Test Report**（给 error/bit/day 与 cm²/bit 两列）、NASA NEPP 的 SEE
  测试汇总、OSTI 2430642（Xavier NX / Ryzen 的质子与重离子数据）。
  **注意**：GR740 那份文档明确写了"本文给出的 per-bit SEU 截面不能用来估算整芯片的错误截面"，
  所以 σ_app 必须报成**区间 + 敏感度曲线**，不能报单点。

### 模板 3：公平比较的口径 —— Schirmeier, DSN'15

*Avoiding Pitfalls in Fault-Injection Based Comparison of Program Susceptibility*：不同配置的运行时间
和内存足迹不同，裸的"SDC 百分比"不可横比；正确做法是按"时间 × 位"的故障空间加权，报**期望绝对
失效数**（并按 FIT/Mbit 的物理速率把 λ 折进去，泊松近似）。

你已经在报"每 90 天期望失效数"——引这篇，它就从自造指标变成方法学。

---

## 3. 确定的实验方案

### A-1 资源加权 AVF 分解（链条的第一环）

把"注入类别"重新映射成"物理资源"，并补上位数权重与驻留时间。

- 资源桶：寄存器文件 / 栈+静态数据（`.data`+`.bss`）/ 堆 / 指令流 / 地址生成。
  你的七类注入天然对应：`fp64`/`int_arith`/`cast` → 寄存器与功能单元，`load` → 数据内存到寄存器的路径，
  `addr` → 地址生成，`cmp_branch` → 控制流。
- 要补的是 `bits_r`（来自 §B1 的 footprint 测量）和驻留时间占比，再把 AVF_r 定义成
  $P(\text{error}\mid\text{upset in } r)$ —— 就是你现有的逐类别 SDC/DUE，只是换了分母语义。
- 产出 `results/avf_resource.csv`：`config,benchmark,resource,bits,residency_frac,avf_sdc,avf_due,ci_lo,ci_hi`

### A-2 应用 cross-section 与 FIT（第二环）

- 按 §2 的公式算 $\sigma_{app}$，σ_bit 取两到三档公开数据（rad-hard 的 GR740/RTG4 一档、
  COTS 的 A9/Xavier 一档），输出区间而不是单点。
- 同时给 FIT 和每天/每 90 天错误数，这样你原来的结论还在，但换成了通用货币。
- 产出 `results/cross_section.csv` + `figures/sigma_sensitivity.pdf`（σ_app 对 σ_bit 的敏感度曲线）。

### A-3 轨道通量 → λ(t)（第三环）

- 用 SPENVIS（ESA 官方在线，`spenvis.oma.be`，可按 mission segment 或沿轨道输出 upsets/bit/day）
  跑你论文里的三条轨道：LEO（含 SAA）、SSO、GEO（含耀斑）。
- 与现有 SAMPEX（Adolphsen 1995）、Alsat-1（Bentoutou 2011）实测值叠图交叉校验，
  用它替换 `|B|`-深度代理（那个代理现在只能标注"非 CREME96"，是个弱点）。
- 产出 `results/spenvis_lambda.csv` + `figures/lambda_crosscheck.pdf`。

### A-4 任务级指标三元组（替换自造指标）

每个 (config, benchmark) 给三个数，逐负载列表、不只给均值：

1. cross-section（或 errors/day）—— 来自 A-2
2. MWTF（Reis ISCA'05 定义）
3. 期望绝对失效数 / 90 天（Schirmeier DSN'15 口径）

**照 TNS'21 的做法保留变差的格子**。你现有数据里"六类等权后没有任何方案的 MWTF 优于不保护"
（`docs/EXPERIMENT_SYSTEM.md:254`）这类结果，是可信度来源，不要藏。

- 产出 `results/mission_metrics.csv`

### A-5 λ(t) 闭环注故障（主张二唯一的实测路径）

- 按 A-3 的 λ(t) 把每个时间窗的注入次数调制出来；三策略（adaptive / always-high / always-low）
  × 三轨迹 × 3 种子，跑同一条轨迹同一组种子。
- 量：端到端失效数、累计开销、GNC 100 ms 截止违约次数。
- 对照 Essex 那篇 *A Self-Adaptive SEU Mitigation Scheme*（仿真 FI 99.97% 纠正率、
  中子实验 100%，两者趋势一致）的做法——你做仿真那一半，趋势一致性那一半割让。
- 产出 `results/adaptive_closed_loop.csv`

### A-6 两条必需基线

- **Young/Daly 最优检查点**：$T^*=\sqrt{2C/\lambda}$，在同一条 λ(t) 轨迹上跑 checkpoint+rollback，
  量同样的三元组。这是答辩必问的"为什么不直接 checkpoint"。
  `measure_adaptive_checkpoint.py` 已存在但不落盘，改造成本低。
- **硬件锁步对照表**（不需要新实验，摆引用数字）：Rogenmoser et al., **ACM TACO 2025**，
  *Exploring Modular Redundancy Approaches in RISC-V Multi-core Processors*（多核 DMR/TMR/锁步实测）、
  NOEL-V 的 DCLS/TCLS、TCLS ARM for Space、GR740（RHBD+TMR+EDAC，厂商口径 <1e-5 events/day）。
  论点：锁步是固定付费，软件冗余能按 λ(t) 关掉——这正是你主张二的立足点。
- 产出 `results/checkpoint_baseline.csv` + 文档里一张软硬对照表

### A-7 真实星载负载（便宜且提升说服力）

OBPMark（ESA + BSC）仓库自带 `data/`（输入 + 验证输出）、`scripts/`（默认配置）和技术说明。
把 1.1 Image Calibration & Correction 与 2.1 CCSDS 121.0 Data Compression 接进主矩阵，
用官方参考输出做 QoR 判据。你注册表里已有 `obpmark_image_prep`/`image_proc`，但 FI 是 0 trials、
未进聚合，相当于白接了。

### A-8 属性 → 机制 的可检验映射（把 A 和 B 粘起来，也是你对 TNS'21 的增量）

TNS'21 只找到两个软件特征（堆用量、静态内存），自述数据集小、可能伪相关。你手里有
`build_property_table.py` / `kernel_properties_*.csv` 的 per-kernel 属性（向量化宽度 meanW、
归约/非归约、ABFT 可识别性、访存比），样本更多，且你预测的是**该用哪个机制**而不是
"单一 TMR 有多有效"。

- 做法：属性向量 → 最省且 SDC=0 的机制，建决策表或小回归，报**留一交叉验证准确率**，
  并和 random / 单一机制两个朴素基线比。
- 这是把"结构匹配"从案例叙事变成可检验结论的唯一方式。注意你自己已经证过一件事：
  基于噪声差值的 metric（集中度那类）不能做路由判据——所以这里只用**确定性属性**做输入。
- 产出 `results/property_to_mechanism.csv` + 留一验证的准确率表

---

## 4. 明确割让（写进效度威胁，不要留白）

- **束流/质子实验**：无条件。改为用公开 σ_bit 折算并报区间，引 RADECS'21 的折算方法学 +
  TNS'21 的束流结果作为"同类工作做过、我们折算"的锚点。
- **门级/RTL 注入**：割让。Safe-NEureka（arXiv 2602.04803，星载 AI 加速器，post-synthesis netlist
  注 SEU/SET）是那条口径的代表，引它说明层级差异。
- **真实飞控软件**：割让。用 OBPMark（真实星载算法 + 官方数据）+ GNC/AOCS 合成核的组合，
  并说明规模限制来自 FI campaign 的代价。

---

## 5. 执行顺序

| 步骤 | 依赖 | 粗估 |
|---|---|---|
| A-1 资源 AVF 分解 | 需先有 footprint（B1） | 1 天 |
| A-2 cross-section / FIT | A-1 | 半天 |
| A-3 SPENVIS λ(t) | 无 | 半天 |
| A-4 指标三元组 | A-2、A-3 | 半天（后处理） |
| A-5 λ(t) 闭环注故障 | A-3 | 1 天 |
| A-6 检查点基线 + 软硬对照 | A-3 | 1 天 |
| A-7 OBPMark 进主矩阵 | 无 | 半天 |
| A-8 属性 → 机制映射 | 主矩阵 | 1 天 |

A-1→A-2→A-3→A-4 这条链是核心：做完，层次 A 的指标就从自造量变成 cross-section/FIT/MWTF
三件通用货币。A-5 和 A-6 补的是论证闭环（自适应有实测、和 checkpoint 比过）。A-8 是加分项。

---

## 6. 引用清单

- COAST 系列：Bohman et al., TNS 66(1) 2019；James et al., TNS 67(1) 2020；
  James et al., TNS 68(5) 2021；James & Goeders, DATE 2021
- 指标定义：Reis et al., ISCA'05（MWTF）、TACO'05（PROFiT、AVF+MWTF 框架）
- FIT 折算：Bodmann, Oliveira, Rech, RADECS'21 / TNS 2022；CERN/ARIS 两步法
- 公平比较口径：Schirmeier et al., DSN'15
- 器件截面数据：GR740 Radiation Summary（ESCIES）、RTG4 Proton Test Report（Microchip）、
  NASA NEPP SEE 数据、OSTI 2430642（Xavier NX / Ryzen）
- 轨道环境：SPENVIS（ESA）、CREME96、Adolphsen 1995（SAMPEX）、Bentoutou 2011（Alsat-1）、
  Morgan, *Predicting On-Orbit SEU Rates*（BYU）
- 硬件冗余对照：Rogenmoser et al., ACM TACO 2025；NOEL-V（Frontgrade Gaisler）；TCLS ARM for Space
- 自适应缓解：*A Self-Adaptive SEU Mitigation Scheme for Embedded Systems in Extreme Radiation
  Environments*（Essex）；CHREC 的环境自适应 TMR
- 空天基准：OBPMark / OBPMark-ML（ESA + BSC）
- 硬件侧注入口径：Safe-NEureka（arXiv 2602.04803）；HachiFI（DATE'25，FI vs 辐照 R²=0.984）
