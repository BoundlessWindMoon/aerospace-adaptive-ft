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

