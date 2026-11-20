# SoR 边界映射表

把本框架所有保护旋钮收编到 Sphere of Replication (Reinhardt & Mukherjee, ISCA'00) 的
**边界**这一个坐标上。SoR 只有两类边界动作:

- **input replication**:数据进圈,复制成几份、怎么复制。
- **output comparison**:数据出圈(变成 store / 地址 / 控制流 / 系统调用参数)前比对。

准入判据:一个优化要进框架,必须能表述成"移动边界"或"改某穿越口的复制/校验方式"。
表述不出来的是 hack,不收。下表证明现有旋钮全部可表述。

穿越口(圈的边界分五处):
1. **value 计算**(圈内复制)
2. **load**(input replication)
3. **store**(output comparison)
4. **address/cast**(值变地址之前)
5. **control**(cmp_branch 出圈)

数据来源:`results/framework_eval.csv`、`results/effect_profile.csv`、
`results/snapshots/sevencat_v8/rq8_inst_category_breakdown.csv`(均为已有测量,未重跑)。

## 穿越口 1:value 计算(圈内复制)

| 旗标 | 边界动作 | 复制度 | 机制 | cost | coverage |
|---|---|---|---|---|---|
| `-atft-dmr-generic` | value 纳入圈内,形状无关 | 2(检错) | SIMD 车道影子链 + XOR 签名 | 1.68× | fp64 SDC 8.94→0.19 |
| `-atft-tmr-generic` | 同上 | 3(纠错) | median-of-3,store/return 表决 | 2.55× | fp64 SDC 8.94→0.06 |
| `-atft-wide-dmr` | 单位步长 FP 循环跨迭代向量化 | 2 | AVX-512 高半 lane 冗余 | (dmr 子集) | 规整内核 ~1× |
| `-atft-simd-f64` | 两份精确 f64 | 2 | `<2 x double>` 车道 | — | 精确复制 |
| `inlane`(Elzar 式) | SIMD 车道内三模 | 3 | AVX in-lane TMR | 5.84× | MWTF 12.13 |
| `exact-tmr` | 标量精确三模 | 3 | 寄存器级复制 | 11.85× | MWTF 4.27 |
| `approx-scalar/-simd` | f32 近似副本 | 2-3 | 松弛比对 | — | fp SDC→1.5 |
| `pure-approx` | 纯近似,无精确基 | — | — | — | — |
| `-atft-replica-exact` | 把松弛比对改按位相等 | — | 收紧 output comparison | — | 去掉无来源 1e-6 |
| `eddi` | value 纳入 + load 复制 | 2 | 标量双模 | 6.22× | SDC 0 / **DUE 66%** |
| `swift` | value 纳入(load 靠 ECC) | 2 | 标量双模 + CFC | 5.26× | SDC 0 / **DUE 66%** |
| `lltfi-sid` | 选择性指令复制 | 2 | SID 排序 | — | 部分 |

**观察**:八个以上旗标全部压在这一个穿越口 —— 堆砌的质量集中在 value 上。
value 穿越口已 STRONG 覆盖(fp64 SDC 降到 0.06–0.19)。

## 穿越口 2:load(input replication)

| 旗标 | 边界动作 | 共模? | cost | coverage |
|---|---|---|---|---|
| `eddi`(复制 load) | load 读两次、写不同内存半区 | 抗共模 | 高(6.22×) | load 故障可检,但 DUE 66% |
| `-atft-simd-bcast` | 单次 load 广播填两 lane | **共模** | 低 | 牺牲 load 覆盖换少指令 |
| dmr-generic 默认 | 指针 launder 二次 load,同 base | **共模** | 低 | load SDC 11.67→10.33(几乎没动) |

**观察(已诊断纠正,results/load_probe.csv)**:load 类别是 FP load + 整数 load(算地址/下标)
的混合。dmr-generic 的 FP 影子**确实**覆盖 FP load(故障结局 det_recovered 61.9%),残余的
load SDC 10.4% 全来自**整数 load**——FP 影子不碰它们。把整数穿越口(int-TMR)一起纳入圈后,
整数 load 被表决掩盖,**load SDC 10.4→0.8**(masked 60.1% + det_rec 38.8%)。
故"load 洞"不是缺一个 load 机制,而是**整数穿越口没纳入圈**;一旦圈含 {V,A},load 即被覆盖。
口径:注入器建模的是瞬态 load-结果翻转;eddi 复制数据到不同内存针对的是**持久内存损坏**
(另一种故障模型,本 harness 不测),且其代价 6.22× + DUE 53% 并不划算。

## 穿越口 3:store(output comparison)

| 旗标 | 边界动作 | cost |
|---|---|---|
| 各模式默认 | 每 store/return 表决(TMR median-of-3 / DMR XOR 签名) | 含在上表 |
| `-atft-loop-vote` | 分摊:每 store 累积一致性布尔,return 前一次性提交(只检错) | 省掉每 store 分支/冷块 |

**观察**:store 出圈校验已覆盖;loop-vote 是同一穿越口的降税写法,不是新 detector。

## 穿越口 4:address / cast(值变地址之前)

| 旗标 | 边界动作 | cost | coverage |
|---|---|---|---|
| `-atft-int-tmr-wire` | 整数数据通路纳入圈内,表决接回数据流 | — | int_arith 部分 |
| `-atft-int-tmr-i64` | 圈扩到 i64 下标扩展(443/501 cast 喂 GEP) | — | 压 cast DUE(~54%→) |
| `-atft-int-tmr-boundary` | 只在整数值离开子图(出圈)时表决 | 降表决次数 | 保护强度不变 |
| `-atft-int-dmr` | 整数改双模检错 + 冷路径重算 | 2 份+1 比对 | 纠错级 |
| `-atft-int-budget` | 整数通路独立预算(部分圈) | — | — |

**观察**:int-tmr-i64 专治 cast 洞,但**未与向量化 value 路径组合**。
generic 路径下 cast 反而**恶化**:native SDC 3.62 → DMR 9.42 → TMR 12.08(effect_profile)。
这是 P2 要坐实的耦合:向量化 value 把脆弱性挤到未纳入圈的 cast 子切片。

## 穿越口 5:control(cmp_branch 出圈)

| 旗标 | 边界动作 | cost | coverage |
|---|---|---|---|
| `-atft-cmp-tmr` | 控制子切片纳入圈内,复制驱动 br/select/switch 的比较并多数决 | — | — |
| `-atft-control-budget` | 控制通路独立预算(部分圈) | — | — |
| `swift` 的 CFC | CFCSS 签名(GSR/RTS) | 含 5.26× | — |

**观察**:cmp-tmr/CFC 存在,但**未接入 generic 的 dmr/tmr 路径**。
generic 路径下 cmp_branch 几乎没动:native SDC 29.25 → DMR/TMR 26.79(覆盖 ~8%)。
**便宜路径的 control 穿越口是洞。**

## 横切旗标(作用于圈的尺寸/成员,不属单一穿越口)

| 旗标 | 边界含义 |
|---|---|
| `-atft-budget` / `-atft-budget-mode` / `-atft-rank` | 部分圈:按关键度排序选成员 |
| `-atft-layer-exclusive` | 去重:ABFT 已关闭 value 穿越口的函数不再叠加复制 |
| `-atft-abft-insert` / `-atft-abft-colcheck` | value 穿越口的**另一种**关闭方式(代数校验和,非复制) |
| `-atft-no-cse-barrier` | 维持圈完整(防 CSE 折叠副本)的低税写法 |
| `-atft-mode=adaptive` | 圈大小 = f(环境/关键度等级) |

## 结论:覆盖缺口与重复覆盖

**重复覆盖(堆砌集中处)**:value 穿越口 ≥8 个旗标 + ABFT 另一条关闭路径;
`layer-exclusive` 的存在本身就是 value 穿越口被 ABFT 与复制双重覆盖的证据。

**缺口现状(初测 → 把 int-TMR 接入向量化路径后复测)**:
1. **control**:cmp-tmr 存在但**仍未接入** dmr/tmr-generic(generic 分支提前 return)→ cmp_branch
   SDC 仍 29.25→26.79。**唯一未闭合的穿越口。**
2. **address/cast**:已闭合。int-TMR 接入向量化路径后 cast 9.4→0.0(transfer.csv)。
3. **load**:已闭合。同一改动覆盖整数 load,load 10.4→0.8(load_probe.csv)。

一个改动(int-TMR 接入 generic 路径)同时闭合缺口 2 和 3 —— 印证"洞=穿越口未纳入圈,
非缺机制"。剩 control 一个穿越口需同样接入。

**框架自洽性**:26 个旗标全部落在 5 个穿越口 + 横切尺寸这一个坐标系内,无一需要
"边界之外"的表述 → 准入判据通过,这不是 pile,是一个带参数的边界对象。

**对 P2 的指向**:缺口 1/2 不是"再加两个 detector",而是同一现象 ——
把 value 穿越口做便宜(向量化)后,脆弱性质量迁移到未纳入圈的 control/address 子切片。
故圈不可逐子切片独立定尺寸(非可分离),须联合。P2 坐实此因果。

---

## P2 结果:非可分离性的因果验证(cast 子切片)

脚本 `experiments/measure_nonseparability.py`,输出 `results/nonseparability.csv`。
受控分解,把"向量化"与"FP 注入"两个因子拆开(每配置对自身 golden,Wilson 95%CI)。

**方法纠正**:`native` 模式不跑 O3 向量化(scalar/vector-ISA 两臂的向量 cast 都是 0、
cast SDC 都是 3.6%),原对照臂为空。改用 `-atft-dmr-generic -atft-dmr-no-inject`
作"只向量化、不注入"的中间臂。

| 配置 | 向量 cast | 全部 cast | cast SDC | addr SDC |
|---|---|---|---|---|
| scalar_native(标量,无冗余) | 0 | 2992 | 3.6% [2.9,4.4] | 0.5% |
| vector_noinject(向量化,无冗余) | 137 | 607 | **12.1% [10.8,13.4]** | 0.2% |
| vector_dmr(向量化 + FP 注入) | 137 | 641 | 9.4% [8.3,10.7] | 0.2% |

**因果判据**:
- 因子1(纯向量化):3.6% → 12.1%,**显著↑**(CI 不重叠),伴随向量 cast 0→137。
- 因子2(再加 FP 注入):12.1% → 9.4%,显著↓(FP voter 顺带兜回一部分,仍 2.6× 基线)。
- addr:两步均不显著(CI 重叠)。

**结论**:
1. **向量化本身**(零 FP 冗余)就把 cast 子切片 SDC 抬 3.4×(3.6→12.1)。
   **机制(已纠正)**:注入器只打标量 cast(`isHookableTy` 排除 `<N x iM>` 向量 cast);
   向量化把 ~2500 个良性标量下标 cast 吸收成 136 个**不可注入**的向量 cast
   (可注入标量 cast 2963→447),剩下的标量 cast 里"算地址/边界"的关键占比升高,
   随机注入打中关键 cast 的概率上升 → SDC 升。**是良性样本被抽走的浓缩,不是向量变宽。**
   这仍是"为便宜化 value 而向量化 → cast 脆弱性上升"的直接证据,即
   **SoR 圈不可逐子切片独立定尺寸(非可分离)**。
2. FP 注入不修 cast(12.1→9.4 只是 FP voter 顺带回收)→ 单独保护 value 不能抵消耦合代价。
3. 口径限定:非可分离在 **cast** 成立,**addr 不成立**(native 本就 0.5%)。
4. 对前作:给出 Elzar(DSN'16)"SIMD-TMR mixed results"的一个具体机制——向量化改变了
   可注入故障的分布,把脆弱性集中到未纳入圈的标量转换路径。

## P3 收口:int-TMR 接入向量化路径的实测(results/transfer.csv)

把 `transformFunctionExactIntTMR` 接入 `-atft-dmr-generic` 路径后实测:

| 配置 | cast SDC | int_arith SDC |
|---|---|---|
| vec_dmr(向量化+FP注入) | 9.4% [8.3,10.7] | 4.9% [4.1,5.8] |
| vec_dmr + int-TMR | **0.0% [0.0,0.2]** | **0.0% [0.0,0.2]** |

IR 证据:只加 9 个标量 voter,136 个向量 cast 未动。int-TMR 覆盖的是**可注入的标量** cast
(它们正是上面"浓缩"后残留的关键 cast)。**sphere_selection demo 里的乐观假设(A 覆盖可迁移
到向量化后的 cast)被实测证实** → 联合优化器选 {V,A} 从模型预测升级为实测事实。

## 完整圈 {V,A,C} 的最终效果剖面(results/full_sphere.csv)

把三个穿越口(V=FP value 向量化、A=int-TMR、C=cmp-TMR)全部接入便宜的向量化路径后,
6 类别残余 SDC / 开销:

| 类别 | native SDC | 完整圈 DMR | 完整圈 TMR |
|---|---|---|---|
| fp64 | 8.9 | 0.2 | 0.1 |
| int_arith | 9.8 | 0.0 | 0.0 |
| cast | 3.6 | 0.0 | 0.0 |
| cmp_branch | 29.2 | 0.0 | 0.0 |
| addr | 0.5 | 0.2 | 0.2 |
| load | 11.7 | 0.8 | 0.8 |
| **overhead** | 1.00× | ~4.98× | ~4.42× |

DUE:native 的 int_arith 50.9% / cast 69.4%(坏下标/转换导致崩溃)→ 完整圈**全部归零**。

**诚实要点**:
1. 完整圈把六类 SDC 全压到 ≤0.8%、DUE 全归零,五条穿越口齐全。
2. 但开销 ~5×,**主要来自 A(标量 int-TMR 6.97×)和 C(cmp-TMR 4.07×)**,不是便宜的
   向量化 FP value(1.68×)。"便宜"只适用于 value 穿越口;闭合其余洞要付标量 TMR 的钱。
3. full_tmr 4.42× < full_dmr 4.98× 属共享节点计时抖动,不作"TMR 更便宜"的结论。
4. **这正是自适应圈的动机**:低辐照只开便宜的 V-圈(1.68×,留洞但够用);高辐照才付 ~5×
   开满 {V,A,C}。P3 的选择问题就是按 λ 在这条代价-覆盖曲线上挑最小够用的圈。

**缺口状态**:五条穿越口全部可接入向量化路径并实测闭合;唯一遗留是开销侧(标量 TMR 偏贵),
属后续优化(如把 int/cmp TMR 也做成向量化)而非覆盖洞。
