# 调研：实验还缺什么（2026-11-20）

对照 `docs/HANDOFF_RESEARCH.md` + 当前代码 + `results/` 实际产物做的缺口审计。
每条给"为什么缺、怎么补、产出什么文件"。P0 = 不补会在答辩/评审被问住，P1 = 补了能加分。

---

## P0-1 同一个量有 6 套互相冲突的数字

浮点保护的时间/指令开销，各文档现在是：

- `docs/STRUCTURE_LAYERS.md:20` → **1.68×**（dmr-generic，AVX-512）
- `docs/SOR_BOUNDARY_MAP.md:26,30` → **1.68× / inlane 5.84× / exact-tmr 11.85×**
- `docs/EXPERIMENT_SUMMARY.md:21` → 近似三模 **6.75×**（几何均，SSE2 口径）
- `docs/METHOD.md:340` → 全开+i64 **指令 10.95 / 周期 8.53 / 时间 8.69×**
- `docs/BOUNDS_AND_CONJECTURES.md:42` → `opt_layered` **7.21×**（工作量加权）
- `docs/HANDOFF_RESEARCH.md:128` → 防御面 **+162.7%**（静态指令，又一个口径）

native 的 fp64 SDC 同样有四个值：`results/full_sphere.csv` 8.94%、`results/e2e_struct.csv` 2.75%、
`docs/EXPERIMENT_SUMMARY.md:21` 16.08%、`docs/METHOD.md:38` 28.98%。

差异来源能解释（等权方式、benchmark 子集、trials、SSE2 vs AVX-512、串行 vs 并行），
但论文里不能并存。补法：

1. 定一张**主结果表**的口径并写进 `docs/METHOD.md` 开头：固定 benchmark 子集（BTAG_REPR）、
   trials=400、ISA 对齐 `x86-64-v4`、串行单核、几何平均、Wilson 95% CI。
2. 在这一口径下重跑全机制矩阵（native / swift / eddi / exact-tmr / dmr-generic / tmr-generic /
   struct_full+ABFT / coarse / coarse-fork），落 `results/main_matrix.csv`，schema 带 `benchmark` 列。
3. 其余所有数字在论文里一律标成"探针口径"，只做机理论证，不进摘要。

## P0-2 11-19 这批代码没有对应的可复现产物

`eb64149`(dmr-generic)、`453a463`/`27d3b97`(wide-dmr)、`a064614`(tmr-generic)、
`89018d7`(屏障 hasSideEffects)、`9ddb7d6`(host ISA 探测)、`83cddb4`(超越函数影子) 都在 11-19 落地。
`results/` 下 553 个文件早于 11-17，含全部 `rq1`–`rq9`（11-14）、`capstone_*`（11-16）、
`run_opt_ablation` 系列（11-15/16）。也就是说**论文现在的 headline 全部来自被改动过的代码之前的版本**。

另有 31 个 `measure_*.py` 只打 stdout 不落盘，包括正在当证据用的
`measure_dmr_generic.py`、`measure_widedmr_ab.py`、`measure_blind_sdc.py`、
`measure_defense_surface.py`、`measure_barrier_tax.py`、`measure_energy.py`。

补法：给这 31 个脚本加统一的 `--out results/<name>.csv`（schema 至少
`config,benchmark,category,trials,sdc_pct,due_pct,det_rec_pct,ci_lo,ci_hi,overhead`），
重跑后 `git add results/`——当前 `results/` 里 11-19 的新 CSV 全是 untracked，随时会丢。

## P0-3 逐核数据在最新产物里不可恢复

11-17 之后的 FI 产物 schema 是 `config,category,sdc_pct,due_pct,n[,overhead]`，**没有 benchmark 列**。
论文的"逐负载表"和"哪些核走哪条机制"都拿不出原始数据，也没法算 per-kernel CI。
`docs/SOR_BOUNDARY_MAP.md:19` 自己标了"均为已有测量，未重跑"。

补法：和 P0-1 的重跑合并，一次解决。

## P0-4 注入器两处采样缺陷会系统性压低 addr / i16 类的脆弱性

- `harness/FaultInjectorPass.cpp:344`：地址类把翻转差值 `& 0x3`，只有第 0/1 位生效，
  均匀采 64 位时约 96% 的 trial 是零扰动却照样计入分母。i64 下标还先 `trunc` 到 i32 再 `sext` 回。
- `harness/harness_rt.c:241`：i16 hook 用 32 位掩码再 `& 0xFFFF`，采到 16–31 位时掩码为 0，同样是计入分母的空注入。

受影响的结论正是"地址天生不脆弱"（addr native SDC `docs/SOR_SELECTION.md:60` 0.46%、
`docs/EXPERIMENT_SYSTEM.md:216` 1.90%）和地址机制的收益（cast→0，1.91×）。

补法：把位域限制在"有效位"内再采样（地址类只采低 log2(N) 位或显式声明 ±3 元素的故障模型），
修完单独重跑 addr/cast/int_arith 三类，落 `results/addr_remeasure.csv`，并在论文注明旧口径与新口径差值。

## P0-5 自适应（主张二）没有任何注故障证据

`docs/EXPERIMENT_SYSTEM.md:178` 原文："该实验不注入任何故障，只量执行时间与 PMU，
所以自适应的可靠性收益至今无证据"。`capstone_*` 的 1.03× / 省 48–68% / Pareto 占优全部是解析模型
（菜单 (o,e) 查表 + λ(t) 积分），不是闭环实测。而主张二是论文的一半。

补法：做一个按 λ(t) 调制注入率的闭环实验——把 `rq9_orbit_timeseries.csv` 的 λ 序列映射成每窗口注入次数，
三条策略（adaptive / always-high / always-low）跑同一条轨迹、同一组种子，量**端到端失效数 + 累计开销 + 截止违约次数**。
产出 `results/adaptive_closed_loop.csv`。这是把"模型估算"升级成"实测"唯一必要的一步，
规模可以很小（LEO 一轨 + GEO 一次耀斑，各 3 种子）。

## P0-6 机制开关会静默无效，消融结果可能是"什么都没插"

- `-atft-addr-dmr`/`-atft-addr-correct`/`-atft-int-data-dmr`/`-atft-int-residue`/
  `-atft-loop-assert`/`-atft-load-reload` 的唯一调用点都在 `passes/AdaptiveFTPass.cpp:4943` 的
  `if (AtftDMRGeneric)` 块内，不给 `-atft-dmr-generic` 时静默无效、不报警。
- `-atft-wide-dmr` 的调用点 `:5133` 在 `native/analyze` 的提前 return（`:5100-5124`）之后，
  所以 `measure_widedmr_ab.py:17` 只能把它挂在已退役的 `approx-simd` 上。
- `-atft-simd-bcast`、`-atft-loop-vote` 要求 `-atft-simd-f64` 同时在，否则无效。
- `-atft-mode` 给错字符串不报错，静默等于 native。

补法：在 Pass 里加参数校验（缺前置 flag 直接 `report_fatal_error`，照 `:4918` 已有的写法），
并在每个度量脚本里加前置断言——指令增量 > 0 且机器码 `zmm`/packed FP 条数相对 native 有变化，
断言不过就 fail 而不是出数（把"改完数机器码确认真冗余"这步变成脚本里的硬门）。

---

## P1-1 全部实测在 x86 上，目标机只有静态指令数

`results/rq8_riscv_rvv_evaluation.csv`（11-14）只给"336 条静态指令中 45.7 条 RVV"，
`docs/EXPERIMENT_SYSTEM.md:182` 自标"无周期数"。而论文平台建模（`experiments/atft_core.py`）
锚的是 HPSC（SiFive X280，RVV 512-bit）。空天方向的评审会直接问"在 LEON/NOEL-V 上多少"。

补法（择一，按成本）：qemu-riscv 下量动态指令数（不是周期，但比静态强）→ `results/riscv_dyn.csv`；
或 gem5 SE 模式跑 3 个核出周期数。拿不到就在"效度威胁"里写明，并引 NOEL-V 的 FI+质子对照
（Cano-Páez 2026, Microelectronics Reliability）说明外推限制。

## P1-2 "并行 ~1×"只在 gemm 上验过

`results/opt/core_saturation_sweep.csv` 只有 gemm N=640。而 1.03× headline 依赖
"空闲核 ≥ 副本数"这个前提（`docs/REPRO.md:50`）。
补法：核占用扫描扩到 4 类负载（归约 gemm / stencil jacobi2d / 线代 cholesky / 整数 floyd），
落 `results/core_saturation_multi.csv`。

## P1-3 ABFT 覆盖面卡在 48.7%，Freivalds 原型没进主路径

`insertABFTForNest`（`passes/AdaptiveFTPass.cpp:1719`）只认 `nest>=2 && idxDeps==2`，即二维下标 matmul 形归约。
`experiments/freivalds_check.c`/`stencil_check.c` 已实测 Cholesky 1.08–1.17×、LU 1.03–1.07×，
但是独立 C 程序，没进 Pass、没进 FI 主路径，所以这部分收益在主结果表里拿不到。

补法：不必做自动识别（已被"禁 case-by-case"否过）。退一步，把 Freivalds 校验做成运行时库入口
+ 手动标注（`__attribute__((annotate))` 或 `ATFT_FACTORED` 宏），让 FI 能打到它，
出 `results/freivalds_fi.csv`（开销 + 检出率 + 误报率）。论文把它定位成"结构已知时的下界演示"。

## P1-4 tmr-generic 的 median-of-3 会吞 NaN

`passes/AdaptiveFTPass.cpp:3714-3719` 用 `minnum/maxnum` 取中值，IEEE 语义下这两个函数**丢弃** NaN 操作数。
单路产生 NaN 时会被静默替换成正常值，而 `eval_suite.c:1562` 的判据把 NaN 算 SDC。
净效果：NaN 类故障被记成 masked，tmr-generic 的覆盖率可能虚高。
补法：换 `fcmp ord` 显式比较，或单独跑一组"强制 NaN 注入"（指数全 1 位域）A/B，落 `results/nan_semantics.csv`。

## P1-5 恢复路径的时间代价没有分布

`ATFT_RETRY_ON_DETECT` 的重执行代价现在按整核上界估（`docs/HANDOFF_RESEARCH.md:164`）。
GNC 是 10Hz / 100ms 硬截止，"检出后重跑还赶不赶得上"是个时间分布问题，不是均值问题。
补法：在 gnc_kalman / aocs_pid 上量"检出时刻分布 + 剩余重跑时间"，出截止违约率曲线 →
`results/retry_deadline.csv`。`measure_deadline_recovery.py` 已存在但不落盘。

## P1-6 能量轴管线通但无可信数据

`measure_energy.py`（RAPL，commit `e6cabfb`）标注"待干净节点出可信值"，无产物。
论文第 4 章如果要留"能量"这一列，就必须有数；留不了就从指标里删掉，别写成估算。

## P1-7 共模与输入侧是已知的覆盖洞，但没写成实验

已实测：污染共享输入时 `coarse_rmt` MC 100% SDC 逃逸；输出侧冗余（含 ABFT）对共模输入结构上无效。
这是威胁模型的边界，评审一定会问。
补法：不扩机制，只补一张量化表——共模故障在总故障空间里的占比（按"几份副本被同一次事件覆盖"的概率模型）
+ 一组共模注入的逃逸率，落 `results/common_mode.csv`，论文里明确割让给 ECC/硬件。

---

## 基线与文献对标缺口

现有基线 4 条：SWIFT、EDDI、LLTFI-SID、exact-TMR（都在 Pass 里实现）。缺的：

- **COAST**（BYU/LANL，已 clone 到 `_refs/coast`）：目前只做过代码参照，没跑过实测对比。
  这是唯一能跑的开源同类，补一组同口径 A/B 性价比最高。
- **nZDC / SecSwift**：SecSwift 是 CGO'26 的工作，评测里拿 nZDC 当对照、目标机是 RV32E。
  这两条无法本地复现，引数字即可，但要在相关工作里明确"口径不同不可直接比"。
- **Elzar**（DSN'16，AVX-TMR）：和你的 dmr-generic/wide-DMR 同路，必须在相关工作里正面对位
  ——你的增量是"复用 LLVM 向量化器 + shape-agnostic 注入"，Elzar 是手写 AVX intrinsic 层。
- **CONDA**（SC'24，1.58×）/ **PROTEGO**（SC'25）：无开源，引数字。

方法学上要在论文里显式承认的三条（都有文献支撑）：

1. IR 级注入对 SDC 概率的估计与汇编级一致，但**对 crash/DUE 概率不一致**
   （Lu & Pattabiraman, TDSC 2020）。你的 DUE 收益（19.1%→0.02%）正好落在不准的那一侧，
   要么补一小子集的汇编级交叉验证，要么在效度威胁里写明。
2. 统计口径：Wilson CI 已按 Leveugle (DATE'09) 实现（`benchmark_harness.py:42`），
   但各脚本 trials 在 24–500 之间不统一，论文里的区间不可横比。
3. 差值类指标在 200 trials 下是噪声（已自测需 7000+ trials 才能分辨 1%），
   论文不要报任何 SDC 差值派生量（集中度、脆弱性集中度等）。

本领域的强结论通常配束流/中子辐照（如 BYU Wilson 2025 的 FI+neutron）。你没有这个条件，
所以"仿真 FI 非束流"必须进效度威胁小节，而不是留白。

---

## 建议不补的（已被自己的数据否证，省时间）

- 寄存器压力 / spill 优化：`results/opt/gap_nonintrusive.csv` 实测 IPC 比 1.08、stack% 10.5→11.5 不动，税是纯指令数的。
- 选择性 budget 的启发式排序、"集中度 metric"做路由：噪声主导，换排序救不了，已在 11-19 否决。
- 在线从 ECC 事件流估 λ：`capstone_online.csv` 显示 SAA 过境尺度上事件太稀疏，加裕度封顶在 3.94% 违约。
  正确叙事是"星历驱动"，这条负结果本身可以写进论文。
- approx f32 打包（`<2 x float>`）：x86 上比精确 f64 打包贵 17%，已退役（`a77c705`）。
- 继续压无结构 FP 的指令级开销：屏障/打包/签名/向量化都榨过了，剩下的要 ISA 扩展或算术编码。

---

## 文档本身的两个问题

- `docs/HANDOFF_RESEARCH.md` 完全没提 wide-DMR、dmr-generic/tmr-generic、coarse/coarse-fork、
  Freivalds、`adaptive_capstone`——而这些是 11-16 到 11-19 的主要产出。交接文档会误导读者。
- `results/offline/{d3_oracle_delta,d5_score_vs_sdc,d6_sprt_feasibility}.csv` 是孤儿产物：
  生产脚本 `experiments/archive/offline_probe.py` 已在 `e7b6bc9` 删除，而 `docs/METHOD.md:179,251`
  还在引它们当 D3/D6 的证据。

---

## 最小动作序列

1. 定主结果表口径 → 改 `docs/METHOD.md` 开头；加 Pass 参数校验 + 脚本前置断言（P0-6）。
2. 修注入器 addr/i16 采样（P0-4）。
3. 统一 `--out`，在新口径下重跑全机制矩阵 → `results/main_matrix.csv`（P0-1/2/3 一次解决）。
4. 跑 λ(t) 闭环注故障实验 → `results/adaptive_closed_loop.csv`（P0-5）。
5. 跑 COAST 同口径对比 + qemu-riscv 动态指令数（P1-1，基线对标）。

1–4 做完，两条主张各有一张自洽的主表；5 决定相关工作章节能写多硬。

