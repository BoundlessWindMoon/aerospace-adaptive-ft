# 要补的实验：对标文献标准实验集（2026-11-20 调研）

按 `docs/HANDOFF_RESEARCH.md` 的两个层次组织。每条给"文献怎么做 → 你现在有什么 → 补什么 → 产出文件"。
排序见最后一节（按 投入/分量 比）。

---

## 层次 B：容错机制微观指标

### B1 指标集补齐：代码体积 + 数据内存开销

**文献口径**：COAST 的 TNS 论文（Bohman et al., *Microcontroller Compiler-Assisted Software Fault
Tolerance*, IEEE TNS）把五个量并列报：① Code Size ② RAM Size ③ Runtime ④ MWTF ⑤ Fault Coverage
=(detected+masked)/total。SWIFT 原文（CGO'05）也报静态体积（EDDI+ECC+CFE 2.83× vs SWIFT 2.40×，
SWIFT 小 15%）。

**你有**：runtime、指令数、SDC/DUE/det_rec、MWTF、静态 IR 指令增量。

**缺**：**二进制体积与数据段/栈增量**。星载是硬约束（码空间、SRAM 预算），评审一定会问，
而你现在一个数都没有。

**补**：对主矩阵每个配置量 `.text`/`.rodata`/`.data`/`.bss` 增量 + 峰值栈深度（`-fstack-usage` 或
valgrind massif），一行一个 (config, benchmark)。产出 `results/footprint.csv`。
这是纯脚本工作量，当天能出。

### B2 编译优化级别 × 保护强度 的交叉实验

**文献口径**：这是当前的热点。ASP-DAC'26 有一篇专门做 *Quantifying Compiler-induced Reliability
Loss in Software-Implemented Hardware Fault Tolerance*（Baroffio et al., PoliMi+TUM）；
RADECS'22（Inria/UFRGS，GPU）实测 `-O3` 的 FIT 率最低、MEBF 比 `-O0` 高 6.1×，结论是
"优化级别本身就是可靠性手段"。

**你有**：向量化宽度 vs 开销的相关性（B1 实验，Spearman 0.82/0.39），以及 SSE2 vs AVX-512 的位宽坑。

**缺**：`-O0/-O1/-O2/-O3` × {native, 你的机制} 的 SDC/开销矩阵。没有这个，审稿人会质疑你的
开销和覆盖数字是优化级别的产物而不是机制的功劳——而你恰好已经踩过"两段式 opt→-O3 只到 xmm"
和"clang -O1 把 ISA 钉在 SSE2"两个坑，正好可以把它写成一节实验而不是一个坑。

**补**：固定 benchmark 子集，4 个 `-O` 级别 × 4 个配置（native / dmr-generic / tmr-generic / exact-tmr），
同 ISA 对齐。产出 `results/opt_level_matrix.csv`。

### B3 检测延迟（detection latency）

**文献口径**：只检错机制的标准第三维（除覆盖、开销之外）。决定回滚距离和能否在截止前恢复，
ABFT 文献（FT-BLAS, SC/TPDS；BLIS ABFT FLAWN76）直接用它来选"在哪一层循环放校验"
——BLIS 那篇算出在 macro-kernel 层放检查的相对开销 ≤5/mc≈2.6%，就是延迟-开销权衡的结果。

**你有**：没有任何延迟数据。

**补**：注入点到检出点的**动态指令距离**（或时间）分布，分机制给直方图/P50-P99。
`harness_rt.c` 已经有 `g_fi_dyn_step` 计数器，检出时记下当前步差即可，改动极小。
产出 `results/detect_latency.csv`。这条同时支撑 P1-5 的截止分析。

### B4 ABFT 的误报率与逐位检出曲线

**文献口径**：V-ABFT（arXiv 2602.08043）的评估模板——① FPR 在多个数值分布 × 多个精度下
各 100k trials 报零误报 ② **逐指数位的检出率表**（bit 7→0.01%、bit 9→73%、bit 11+→100%）
③ 开销（11.98% 均值）。FT-BLAS 另给一条重要归因：ABFT 开销的大头是参考校验和的构造，
fuse 进 kernel 后 15%→2.9%。

**你有**：κ 扫描的检出率（68%→99.5%→100%）、N=256 时 1.04×、κ=0.05 下"误报 2 次"的零散记录。

**缺**：成表的 FPR（分 N、分数据分布）+ 逐位检出曲线 + 开销归因（校验和构造 vs 校验本身）。
你的 68% 检测地板现在只有一个独立数值模型解释（68.45%），配上逐位曲线就变成实测机理。

**补**：产出 `results/abft_fpr.csv`（分布×N×κ 的误报率）、`results/abft_bitwise.csv`（位域→检出率）。

### B5 注入口径的跨层验证

**文献口径**：① Lu & Pattabiraman（TDSC 2020，*Improving the Accuracy of IR-level Fault
Injection*）：IR 级注入对 **SDC 概率** 与汇编级一致，对 **crash 概率不一致**。
② DATE'25 的 HachiFI 给了可行的对标范式：FI 与辐照实验 R²=0.984。
③ 可用工具：gem5 上的 CHAOS（arXiv 2602.02119）、GemFI（DSN'14）、InjectV（RISC-V，GitHub）。

**你有**：IR 级 + 全指令盲注 + burst/MBU 三个口径，全在 x86 上。

**缺**：跨层验证。而你的 DUE 收益（19.1%→0.02%、cast 60.7%→0）正好落在 TDSC'20 说"IR 级不准"
的那一侧，这是会被直接攻击的点。

**补**：取 3 个核 × 2 个配置，在 gem5 RISC-V（CHAOS 或 InjectV）上做寄存器文件注入，
与你的 IR 级结果做相关性（报 R² 或排序一致性）。产出 `results/crosslevel_fi.csv`。
做不了就必须在效度威胁里引 TDSC'20 明确承认。

### B6 基线补齐

能本地跑的：

- **COAST**（BYU/LANL，已 clone 在 `_refs/coast`）：唯一开源同类 LLVM 框架，且它自己的论文
  同时有 FI 和中子束结果可引。同口径跑一组是性价比最高的基线补充。
- **Young/Daly 最优检查点**：见 A4，层次 A 的必需对照。

只能引数字、不能复现的（在相关工作里对位，注明口径不同）：

- **Elzar**（DSN'16，AVX 寄存器内 TMR）：和你的 dmr-generic/wide-DMR 同路，必须正面对位。
- **nZDC / SecSwift**（SecSwift 是 CGO'26，STMicroelectronics，目标机 RV32E，评测里拿 nZDC
  当基准）：这是最新的编译器级对照，必须引。
- **CRAFT / PROFiT**（Reis et al., TACO'05 *Software-Controlled Fault Tolerance*）：PROFiT 就是
  "按 profile 调整保护强度"，是你主张一（选择性）的最直接先验工作，MWTF 定义也出自这条线。
- **FT-BLAS / BLIS-ABFT**：ABFT 开销的 SOTA 口径（2.9%），你的 1.04× 要和它对位。
- **Trident / ePVF**：SDC 脆弱性预测基线。你已经有 `trident_rank.csv`，补一张预测精度对比表很便宜。

### B7 统计口径统一

**文献口径**：Leveugle et al.（DATE'09，*Statistical Fault Injection: Quantified Error and
Confidence*，被引 600+）给样本量-置信区间公式，是本领域报 FI 结果的标准引用。

**你有**：`benchmark_harness.py:42` 已按 Wilson 实现并引了这篇。

**缺**：各脚本 trials 在 24–500 之间不统一，区间不可横比。主矩阵定一个数（400 或 500）并全表报 CI。

---

## 层次 A：空天负载宏观指标

> 这一层已经单独定了方案，见 `docs/LEVEL_A_PLAN.md`（含顶刊模板调研 + A-1~A-8 的执行顺序）。
> 下面保留要点索引。

### A1 λ(t) 的来源升级为标准工具输出

**文献口径**：轨道→屏蔽→LET 谱→器件截面→upset rate 这条链在本领域走 **SPENVIS**（ESA 官方
在线工具，`spenvis.oma.be`，能按 mission segment 或沿轨道输出 upsets/bit/day）或 OMERE/CREME96。
BYU 的 Morgan *Predicting On-Orbit SEU Rates* 是这条链的教科书式示例。

**你有**：SAMPEX（Adolphsen 1995）4.21e-6、Alsat-1（Bentoutou 2011）4.04e-7、CREME96 worst-day，
以及 `|B|`-深度代理的连续 λ 剖面。

**补**：用 SPENVIS 跑你论文里那条 LEO（含 SAA）轨道，导出 upset rate 时间序列，与你现有的
λ(t) 剖面做交叉校验。这是免费的在线计算，一两小时的事，但能把 λ 的来源从"文献数值 + 自建代理"
升级成"标准工具输出 + 文献实测校验"。产出 `results/spenvis_lambda.csv` + 一张叠加图。

### A2 宏观主指标改用 MEBF / MWTF

**文献口径**：MWTF = 完成的工作量 / 遇到的错误数 = (raw error rate × AVF × 执行时间)^-1
（Reis et al. ISCA'05 *Design and Evaluation of Hybrid Fault-Detection Systems* 提出，
TACO'05 细化）。GPU/辐照社区的等价量是 MEBF（Mean Executions Between Failures，RADECS'22）。
它的价值是把"更慢"和"更对"合成一个可比的数，并且**能直接和硬件冗余比**。

**你有**：MWTF 在部分表里出现（4.92/5.08/5940），但宏观层的主指标是"开销 1.03× + 省 48–68%"。

**补**：用已有数据重算一张 MEBF/MWTF 表（纯后处理，不用重跑），覆盖 native / 静态菜单 /
自适应 / 硬件锁步模型四列。这是把宏观结论接进文献口径最便宜的一步。

### A3 软件冗余 vs 硬件锁步的对照

**文献口径**：Rogenmoser et al.（ACM TACO 2025，*Exploring Modular Redundancy Approaches in
RISC-V Multi-core Processors for Safety-critical Applications*，被引 40+）实测了 RISC-V 多核上
DMR/TMR/锁步的多种配置，正是"粗粒度整核冗余"的硬件对照；TCLS ARM for Space（H2020）
和 NOEL-V 的 DCLS/TCLS 是工业对照。

**你有**：`measure_cnn_capstone.py` 的 HPSC 模型估算（DCLS 锁步/split 两模）。

**补**：一张对照表——你的 coarse/fork-TMR（实测开销+覆盖）vs 硬件 DCLS/TCLS（引用面积/功耗/
性能数字）。重点论证你的卖点："锁步是固定付费，软件冗余可以按 λ(t) 关掉"。
这节不需要新实验，需要的是把引用数字摆进同一张表。

### A4 Young/Daly 检查点基线（层次 A 最缺的基线）

**理由**：你的 λ 参数化控制器理论来源就是 Young/Daly 最优检查点间隔，答辩必问
"为什么不直接 checkpoint/restart"。现在宏观层只和"全程 TMR""静态菜单"比，没有和检查点比。

**补**：在同一条 λ(t) 轨迹上加一条基线——周期按 Young/Daly 最优间隔
($T^*=\sqrt{2\,C/\lambda}$，C=检查点代价）的 checkpoint+rollback，量同样的
MEBF/开销/截止违约。产出 `results/checkpoint_baseline.csv`，并进 Pareto 图。
`measure_adaptive_checkpoint.py` 已存在但不落盘，改造成本低。

### A5 λ(t) 闭环注故障实验

**文献口径**：自适应缓解类工作的标准做法是"仿真注入 + 辐照趋势对照"
（如 Essex 的 *A Self-Adaptive SEU Mitigation Scheme*：仿真 FI 得 99.97% 纠正率，
中子实验得 100%，两者趋势一致）。至少要有仿真注入那一半。

**你有**：`capstone_*` 全部是解析模型（菜单查表 + λ 积分），`docs/EXPERIMENT_SYSTEM.md:178`
自述自适应的 RQ4 实验不注入故障。

**补**：按 `rq9_orbit_timeseries.csv` 的 λ 序列把每个窗口的注入次数调制出来，
三条策略（adaptive / always-high / always-low）跑同一轨迹同一组种子，量端到端失效数 +
累计开销 + 截止违约。规模可以很小（LEO 一轨 + GEO 一次耀斑，各 3 种子）。
产出 `results/adaptive_closed_loop.csv`。层次 A 的论证闭环靠这一条。

### A6 真实星载负载进主矩阵

**文献口径**：OBPMark（ESA + BSC 联合开发，`obpmark.github.io`，公开 Beta）是目前唯一
openly available 的星载处理基准集，含图像校正/雷达/CCSDS 121.0 压缩/AES，带参考输出；
另有 OBPMark-ML。它存在的理由就是"缺少代表性空天基准"——引用它比自造合成核有力得多。

**你有**：`obpmark_image_prep` / `image_proc` 已在注册表里，但 fp64 FI 是 0 trials、未进菜单，
也没进聚合。空天核整体是"数值真实但规模小、合成输入"。

**补**：把这两个核纳入主矩阵（真实输入 + 官方参考输出做 QoR 判据），
再补一个 CCSDS 121.0 压缩核（OBPMark 2.1，C 顺序版可直接接）。
这是"空天相关性"最便宜的硬证据。

### A7 能量轴：要么做实，要么删掉

**文献口径**：RAPL 的已知限制——不能分核归因、需要独占节点、计数器约 60s 溢出、
需与外部功率计校准（ICPE'24 有专门的校准方法学论文；Weaver 的 rapl-read 是标准读法）。

**你有**：`measure_energy.py` 管线通，标注"待干净节点出可信值"，无产物。

**补（二选一）**：① 独占节点跑，报方法学（采样周期、溢出处理、独占性声明），
出 `results/energy.csv`；② 换 Jetson Orin Nano 用板载 INA3221 实测功耗——这更贴空天语境
（你的平台模型里本来就有 Jetson 7W/15W 两档）。做不了就把"能量"从指标集里删掉，
不要留成估算值。

---

## 优先级（投入 / 分量 比）

免费或当天完成，直接加分：

1. **A2** MEBF/MWTF 重算（纯后处理）
2. **B1** 代码体积 + 数据内存开销表
3. **B3** 检测延迟分布（改 10 行 harness）
4. **A1** SPENVIS 跑一条轨道做 λ 交叉校验

一到两天，补的是论证闭环：

5. **A5** λ(t) 闭环注故障（主张二唯一的实测路径）
6. **A4** Young/Daly 检查点基线（必答的那个"为什么不 checkpoint"）
7. **B2** `-O` 级别 × 保护强度 交叉矩阵（当前热点，且你已有踩坑素材）
8. **A6** OBPMark 两核进主矩阵

一周量级，决定相关工作章节能写多硬：

9. **B6** COAST 同口径实测对比
10. **B4** ABFT 的 FPR + 逐位检出曲线
11. **B5** gem5 RISC-V 上的跨层验证（同时解决"目标机只有静态指令数"）
12. **A7 / A3** 能量实测或删除；软硬件冗余对照表

---

## 引用清单（按用途）

- 指标定义：Reis et al., ISCA'05（MWTF 提出）/ TACO'05（PROFiT、AVF+MWTF 框架）；
  Bohman et al., IEEE TNS（COAST 五指标 + 中子束）
- 统计口径：Leveugle et al., DATE'09
- 注入效度：Lu & Pattabiraman, TDSC'20（IR vs 汇编）；HachiFI, DATE'25（FI vs 辐照 R²=0.984）
- 注入工具：CHAOS（gem5, arXiv 2602.02119）、GemFI（DSN'14）、InjectV（RISC-V/gem5）
- 编译优化与可靠性：ASP-DAC'26（compiler-induced reliability loss）；RADECS'22（-O3 与 MEBF）
- 编译器级对照：SWIFT（CGO'05）、EDDI（TR'02）、Elzar（DSN'16）、nZDC、SecSwift（CGO'26）
- ABFT：FT-BLAS、BLIS-ABFT（FLAWN76）、V-ABFT（arXiv 2602.08043）、ApproxABFT
- 空天平台/硬件冗余：Rogenmoser et al., TACO'25；NOEL-V（Frontgrade Gaisler）；TCLS ARM for Space；
  Cano-Páez et al., Microelectron. Reliab. 2026（NOEL-V 的 FI + 质子对照）
- 轨道环境：SPENVIS（ESA）、CREME96、Adolphsen 1995（SAMPEX）、Bentoutou 2011（Alsat-1）、
  Morgan, *Predicting On-Orbit SEU Rates*（BYU）
- 空天基准：OBPMark / OBPMark-ML（ESA + BSC）
- 能量方法学：ICPE'24 RAPL 校准；Weaver rapl-read
