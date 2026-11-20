//===-- AdaptiveFTPass.cpp - Decoupled Policy Layer & SOTA Baselines ------===//
//
// 包含本课题提出的自适应混合近似容错方法，以及完整复现的学术界经典/SOTA 对比基线：
//   [A] 容错冗余机制对比基线 (-atft-mode):
//     1. native        : 无冗余保护基线
//     2. eddi          : EDDI (Oh et al., IEEE TR 2002) — 全指令与访存双模复制 + Store/Branch 前比对 (检错触发 DUE)
//     3. swift         : SWIFT (Reis et al., CGO 2005)  — 寄存器计算链双模复制 (Load 移出复制圈) + 同步点比对
//     4. coast-tmr     : COAST-TMR (Bohman et al., BYU, IEEE TNS 2019) [按原文语义复现] — 全精度软件三模冗余 + 多数表决纠错
//     5. approx-scalar : 混合近似 TMR 标量版 (1x FP64 精确 + 2x FP32 标量近似 + 动态阈值投票)
//     6. approx-simd   : 混合近似 TMR 向量版 (1x FP64 精确 + <2 x float> SIMD 向量近似 + 内联快速阈值投票)
//     7. adaptive      : 编译期语义关键性分级 + 运行时多版本 (LOW/HIGH) 动态自适应切换
//     8. pure-approx   : 纯近似计算分支直接输出 (用于离线表征近似固有误差 MRE / PSNR)
//
//   [B] 选择性保护排序算法对比基线 (-atft-rank):
//     1. random        : 随机指令选择基线
//     2. hotpath       : HotPath (Lu et al.) — 基于执行频率/热循环深度优先保护
//     3. shoestring    : Shoestring (Feng et al., ASPLOS 2010) — 基于静态数据流扇出树与高脆弱性汇点距离
//     4. sdctune       : SDCTune (Lu et al., CASES 2014) — 基于静态 SDC 传播特征 (浮点/乘除误差放大/地址依赖)
//     5. semantic      : 本课题方法 — 融合输出紧密度、循环深度、跨块活跃度与循环递归状态反馈 PHI 环路
//
//===----------------------------------------------------------------------===//

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/Transforms/Utils/ScalarEvolutionExpander.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Mem2Reg.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/Host.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <vector>

using namespace llvm;

namespace {

static cl::opt<std::string> AtftMode(
    "atft-mode",
    cl::desc("Protection policy: analyze | native | eddi | swift | lltfi-sid | exact-tmr | "
             "adaptive | [DEPRECATED: approx-scalar | approx-simd | pure-approx]. "
             "The approx-* modes (f32 replicas + delta tolerance) are legacy "
             "approximate-computing FT, retired as defaults; for exact SIMD-packed "
             "redundancy use exact-tmr or -atft-simd-f64 instead."),
    cl::init("adaptive"));

static cl::opt<std::string> AtftRankMethod(
    "atft-rank",
    cl::desc("Selective protection ranking: semantic | sdctune | shoestring | hotpath | random"),
    cl::init("semantic"));

static cl::opt<double> AtftBudget(
    "atft-budget",
    cl::desc("Selective protection budget ratio in [0.0, 1.0]"),
    cl::init(1.0));

static cl::opt<std::string> AtftVulnCache(
    "atft-vuln-cache",
    cl::desc("CSV (site_key,sdc) of FI-measured vulnerability; enables -atft-rank=fi-cache"),
    cl::init(""));

static cl::opt<std::string> AtftBudgetMode(
    "atft-budget-mode",
    cl::desc("Budget selector: count | dynamic-cost | risk-cost"),
    cl::init("count"));

static cl::opt<std::string> AtftProfileFile(
    "atft-profile",
    cl::desc("CSV site cost profile used by dynamic-cost/risk-cost selection"),
    cl::init(""));

static cl::opt<std::string> AtftSelectionFile(
    "atft-selection",
    cl::desc("CSV output for selected protection candidates"),
    cl::init(""));

static cl::opt<std::string> AtftFrontierSelectionFile(
    "atft-frontier-selection",
    cl::desc("CSV input containing selected path frontier member site IDs"),
    cl::init(""));

static cl::opt<double> AtftControlBudget(
    "atft-control-budget",
    cl::desc("Independent control-flow protection budget; negative follows total budget"),
    cl::init(-1.0));

static cl::opt<double> AtftDelta(
    "atft-delta",
    cl::desc("Error tolerance threshold delta for approximate TMR voter"),
    cl::init(5e-5));

static cl::opt<double> AtftW1("atft-w1", cl::desc("Weight w1 (Output Closeness)"), cl::init(2.5));
static cl::opt<double> AtftW2("atft-w2", cl::desc("Weight w2 (Loop Depth)"), cl::init(1.8));
static cl::opt<double> AtftW3("atft-w3", cl::desc("Weight w3 (Variable Liveness & State Recurrence)"), cl::init(0.8));

// 等价饱和主线的前端：把被测内核里的"完美嵌套仿射循环 + 单条累加语句"抽成
// 符号求和式，交给离线的 e-graph 搜便宜的等价校验式。
// 识别的是**结构**（归约形态与下标依赖），不是函数名，所以能泛化。
// 代数校验层的编译期自动插桩：对识别出的矩阵乘形态归约，在循环巢前后插
// 行和快照与校验调用。判层（是否降阶）在离线搜索里做，这里只负责"能插就插"，
// 插不了（维度/步长/基址取不到）就整条跳过，绝不猜。
static cl::opt<bool> AtftABFTInsert(
    "atft-abft-insert",
    cl::desc("Insert algebraic-check calls around recognized matmul-shaped reductions"),
    cl::init(false));

static cl::opt<bool> AtftABFTColCheck(
    "atft-abft-colcheck",
    cl::desc("Add a column-direction algebraic check so single errors are locatable "
             "(drops the O(n^2) snapshot; see docs/HANDOFF.md section 9)"),
    cl::init(false));

static cl::opt<bool> AtftLayerExclusive(
    "atft-layer-exclusive",
    cl::desc("Make the layers mutually exclusive: functions that got an algebraic "
             "check do NOT also get instruction redundancy (this is what the "
             "layering criterion actually claims)"),
    cl::init(false));

// 记录哪些函数真的插上了代数校验。分层判据说"搜到就进代数校验层，搜不到就回落
// 指令冗余层"，但实现里两层一直是**叠加**的 —— 这个集合是把它改成互斥所需的。
static std::set<const Function *> g_abftInserted;

static cl::opt<std::string> AtftDumpSums(
    "atft-dump-sums",
    cl::desc("Extract reduction loop nests into a symbolic sum description (CSV path)"),
    cl::init(""));

static cl::opt<std::string> AtftDumpNests(
    "atft-dump-nests",
    cl::desc("Dump a structural description of every innermost-loop store "
             "(CSV path) so reducibility can be decided for all workloads"),
    cl::init(""));

static cl::opt<std::string> AtftReportFile(
    "atft-report",
    cl::desc("Output CSV file path for criticality analysis report"),
    cl::init(""));

static cl::opt<std::string> AtftSiteManifestFile(
    "atft-site-manifest",
    cl::desc("Output CSV file path for auditable protection candidate site manifest"),
    cl::init(""));

static cl::opt<std::string> AtftFrontierManifestFile(
    "atft-frontier-manifest",
    cl::desc("Output CSV file path for path frontier manifest"),
    cl::init(""));

// --- 中期优化开关：三项都默认关闭，不改动已冻结基线的任何行为 ---

// D2 控制平面三模：复制驱动 br/select/switch 的比较并多数决
static cl::opt<bool> AtftCmpTMR(
    "atft-cmp-tmr",
    cl::desc("Triplicate control-flow-driving compares and majority-vote them"),
    cl::init(false));

// 控制平面双模检错：比较只复制一份、不一致置检测标志(交系统级回滚),不就地多数决。
// 对"分支判决翻转"故障模型,检错+回滚即够(空天口径),比三模省一份比较+整个 select 网络。
// CFCSS 式签名法对本故障模型无效(它抓非法 CFG 边,抓不住走错的合法分支),故走复制检错。
static cl::opt<bool> AtftCmpDMR(
    "atft-cmp-dmr",
    cl::desc("Control plane: duplicate control-flow compares and detect (commit flag for "
             "rollback) instead of triplicate+majority. Cheaper, detect-only."),
    cl::init(false));

// 地址平面结构型保护(PRESAGE/FailAmp 思路):不复制整条整数通路,只复制**喂给 GEP 的
// 下标的后向切片**,在 GEP 处比对、检错交回滚。利用"地址来自仿射/结构化下标"这一结构,
// 比全整数三模便宜(只碰地址切片、双份而非三份、无多数决网络)。检错口径,配合回滚。
static cl::opt<bool> AtftAddrDMR(
    "atft-addr-dmr",
    cl::desc("Address plane: duplicate only the GEP-index backward-slice and detect at the "
             "GEP (structure-aware, cheaper than full integer TMR)"),
    cl::init(false));

// correct-before-use:对喂给 GEP 下标的整数做 TMR 取中值,在访存发生前用中值替换下标,
// 把"坏下标→越界→崩溃(DUE)"在源头掐掉(而非 detect-only 事后记录)。
static cl::opt<bool> AtftAddrCorrect(
    "atft-addr-correct",
    cl::desc("Address plane: TMR-median the GEP index and substitute before use "
             "(prevents out-of-bounds crashes / DUE, not just detect)"),
    cl::init(false));

// 整数数据平面:复制流向整数 store/return 的值的后向切片,store 处检错。补 addr-DMR 漏的整数数据。
static cl::opt<bool> AtftIntDataDMR(
    "atft-int-data-dmr",
    cl::desc("Integer data plane: duplicate backward-slice of values feeding integer "
             "stores/returns and detect at the store"),
    cl::init(false));

// 整数数据平面的选择性预算(Shoestring 思路):只保护 SDC 关键度最高的那部分整数 store,
// 其余靠症状(越界崩溃=DUE→回滚)自现。<1 则按关键度排序取 top 比例。实测整数故障本就
// ~半数直接崩溃(DUE 50.9%),故静默 SDC 只集中在少数不经症状直达输出的值上。
static cl::opt<double> AtftIntDataBudget(
    "atft-int-data-budget",
    cl::desc("Fraction of integer stores to protect, ranked by SDC criticality (<1 = selective)"),
    cl::init(1.0));

// 层③·整数编码:沿环运算(+,−,×)携带残差码 r=v mod M,store 处比 (v mod M)==r。
// 检测的是环运算链上的计算故障(非复制,是编码检查);非环运算(位运算/比较/load)处
// 残差链断开、重编码,那些点不被码保护。AN/残差码是整数软错误的领域首选(TRUMP 用其替代多数决)。
static cl::opt<bool> AtftIntResidue(
    "atft-int-residue",
    cl::desc("Integer data: carry a residue (mod M) arithmetic code through ring ops "
             "(+,-,*) and check at stores (layer-3 encoding, not duplication)"),
    cl::init(false));

// 层②·load(内存层级冗余):对整数数据 load 做独立 launder 二次读 + 比对(nZDC 式)。
// 利用"值在内存/cache 有冗余副本"这一结构抓 load 结果/LSU 瞬态;内存阵列翻转假设由 ECC 兜底。
static cl::opt<bool> AtftLoadReload(
    "atft-load-reload",
    cl::desc("Load (memory-hierarchy redundancy): independent laundered reload + compare for "
             "integer data loads (nZDC-style; memory-array flips assumed ECC-covered)"),
    cl::init(false));

// 复用 TRIDENT(Li et al., DSN'18)的方法:输出锚定的误差传播,免 FI 估每条指令的 SDC
// 到达概率(带屏蔽模型:比较/截断/逻辑会屏蔽故障)。纯分析,输出 CSV,不改代码。
static cl::opt<std::string> AtftTridentRank(
    "atft-trident-rank",
    cl::desc("TRIDENT-style analytical SDC reachability ranking (output-anchored, masking-aware); "
             "writes per-instruction weight CSV to this path. Analysis only."),
    cl::init(""));


// 层①·分支(循环控制):仿射循环结束后归纳变量必等于上界(与路径无关的结构不变量)。
// 判决翻转→早退/多跑→i≠N→检出。保守只处理 slt/ult + 步长+1 的规范循环,避免假阳。
// 依据:Goloubeva CFA(2003)、ASU 低成本检测器(10% 开销换 90% SDC)。
static cl::opt<bool> AtftLoopAssert(
    "atft-loop-assert",
    cl::desc("Branch (loop control): assert the affine loop's induction variable equals its "
             "bound at exit (path-independent structural invariant)"),
    cl::init(false));







// D3 表决阈值改为运行期逐负载值：表决器读全局 g_atft_delta_abs（由剖析结果填），
// 而不是把 -atft-delta 的编译期常数烧进去
static cl::opt<bool> AtftDeltaRuntime(
    "atft-delta-runtime",
    cl::desc("Read the voting threshold from the runtime global instead of a compile-time constant"),
    cl::init(false));

// D3 剖析构建：在表决点插桩记录无故障执行下的最大相对偏差 |R-A|/(1+|A|)
static cl::opt<bool> AtftDeltaProfile(
    "atft-delta-profile",
    cl::desc("Instrument voting sites to profile the fault-free |R-A| envelope"),
    cl::init(false));

// D9 整数通路只在**边界**表决：值离开被复制子图（流向 store / 地址 / 调用 /
// 未被复制的指令）时才插表决，子图内部三条链并行传播不校验。
// 依据 SWIFT (CGO'05) 的同步点校验与 nZDC (DAC'16) 的"变成地址或控制流之前校验"，
// 单比特故障沿一条链传播，到第一个边界被多数决掩盖，保护强度不变而表决次数大减。
static cl::opt<bool> AtftIntTMRBoundary(
    "atft-int-tmr-boundary",
    cl::desc("Vote only where a replicated integer value leaves the replicated subgraph (implies -atft-int-tmr-wire)"),
    cl::init(false));

// D4 把整数三模的覆盖面扩到 i64：实测 501 个 cast 注入点里 443 个（88%）是
// i32 -> i64 的 sext/zext —— 正是喂给 GEP 的下标扩展。isSupportedIntTy 原先不含
// i64，这条链从来没被复制过，所以 cast 类的 DUE 一直停在 ~54%（坏下标直接段错误）。
// 扩面后表决给出的是**精确多数值**而不是钳位值，不会把 DUE 换成 SDC，
// 也不需要从剖析学范围（对照 nZDC/SWIFT 的"使用前校验"，这里是"使用前纠正"）。
static cl::opt<bool> AtftIntTMRI64(
    "atft-int-tmr-i64",
    cl::desc("Extend integer TMR coverage to i64 values (index extensions feeding GEPs)"),
    cl::init(false));

// 关掉 CSE 屏障。原先假设关掉后 CSE/GVN 会折叠等价副本、冗余失效，故只敢用于测量。
// 2026-11-18 在 approx-simd + int-tmr-wire 全栈、LARGE 规模、12 个负载上实测否掉了这个假设：
//   关屏障后 SDC 覆盖不变（fp64/cast/int_arith 三目标均保持 0.00%，native 最高 27.5%），
//   可注错静态点数不变（on=off=8666，冗余副本完整），而指令数降约一半（屏障税 ~2×）。
// 原因：近似副本是 f32（与 f64 原值类型不同，CSE 折不动），整数副本被表决 select 吊住也不折，
// 屏障挡的是个不存在的折叠，只白白钉住活跃区间、挡掉 LICM/向量化。
// 因此本开关对该部署配置是有效的降税旋钮；但 exact-tmr/eddi/swift 等纯精确复制路径未经此验证，
// 那些路径的副本逐位相同、确有被折叠风险，仍需保留屏障。默认不变，由部署配置显式开启。
static cl::opt<bool> AtftNoBarrier(
    "atft-no-cse-barrier",
    cl::desc("Drop the CSE barriers. Validated safe (coverage+sites preserved, ~2x less tax) "
             "for approx-simd + int-tmr-wire; keep barriers for pure exact-replication paths"),
    cl::init(false));

// 循环级检错(SWIFT 同步点思路)：实测 FP 近似三模 6.3× 税里 74% 是"每 store 一次
// 分支+冷块+表决"的机制开销，纯指令数(IPC 1.08、无 spill)。本开关把每 store 的
// 分支/冷块去掉，只留一条便宜的 this_ok 布尔，并经一个入口 alloca 累积(交给 mem2reg
// 升成 loop-carried PHI)，在函数返回前一次性提交：不一致则置运行时检测标志(检错，
// 不就地纠错，交系统级回滚——契合空天"检错+回滚"口径)。store 仍写精确值。
static cl::opt<bool> AtftLoopVote(
    "atft-loop-vote",
    cl::desc("Amortized loop-level detection: accumulate per-store agreement into one flag, "
             "commit once before return (detect-only, no per-store branch/cold-path)"),
    cl::init(false));

// 整数通路改用双模检错 + 冷路径重执行：只留一份副本，汇点比对，
// 检到不一致由运行时置位、上层整段重算。常驻成本从"3 份计算 + 4 条表决"
// 降到"2 份计算 + 1 次比对"，而可靠性仍是纠错级（重算给出第三个值）。
static cl::opt<bool> AtftIntDMR(
    "atft-int-dmr",
    cl::desc("Integer datapath: duplicate-and-check with cold-path re-execution instead of TMR"),
    cl::init(false));

// D10 按通路分预算：整数通路与浮点近似通路的"单位开销收益"差一个量级，
// 用同一个 -atft-budget 同时压两条通路必然在其中一条上做错决策。
// < 0 表示沿用全局预算，保持原行为。
static cl::opt<double> AtftIntBudget(
    "atft-int-budget",
    cl::desc("Separate protection budget for the integer datapath (<0 = follow -atft-budget)"),
    cl::init(-1.0));

// D8 整数通路：把内联表决结果接回数据流，并让两条副本保持可注错
static cl::opt<bool> AtftIntTMRWire(
    "atft-int-tmr-wire",
    cl::desc("Wire the integer TMR inline vote back into the dataflow and keep replicas injectable"),
    cl::init(false));

// D3 副本一致性判据改为按位相等：两条近似副本是同一条 f32 链的复制，
// 无故障时按 IEEE-754 必然逐位相同，原先的 1e-6 / 1e-5 松弛量没有来源
static cl::opt<bool> AtftReplicaExact(
    "atft-replica-exact",
    cl::desc("Require bitwise-equal approximate replicas instead of a slack threshold"),
    cl::init(false));

// C4：在 SIMD 保护路径中保留 FP64 精度，把两份精确副本放入 <2 x double>。
// 默认关闭，保持 approx-simd 的冻结行为；只在显式消融时启用。
static cl::opt<bool> AtftSimdF64(
    "atft-simd-f64",
    cl::desc("Use two exact FP64 replicas in a <2 x double> SIMD channel"),
    cl::init(false));

// 计量旋钮：SIMD 通道里复制 Load 时，用单次 Load + broadcast 填满两条车道，
// 取代"两次独立 Load + 两个零开销指针屏障"。省掉热循环里的屏障与第二次 Load，
// 代价是 Load/访存故障会同时打中两条车道（共模，检不出）——因此这是开销换 Load 覆盖面的
// 可测旋钮，不是无损优化。默认关闭，冻结行为不变。
static cl::opt<bool> AtftSimdBcast(
    "atft-simd-bcast",
    cl::desc("SIMD channel: broadcast one load into both lanes instead of two "
             "independent loads (trades load-fault coverage for fewer hot-loop ops)"),
    cl::init(false));

// wide-DMR：对规整的单位步长数据并行 FP 循环(map/stencil)做跨迭代向量化，把冗余副本放进
// AVX-512 的高半 lane。zmm<8 x double> 低 4 lane 跑 4 个真实迭代、高 4 lane 跑同一数据的冗余
// 副本(shuffle 广播造出独立 lane，优化器不会折叠)；每步比低半/高半、异或进 <4 x i64> 签名，
// 循环出口比一次。向量化加速红利付掉冗余开销——对可向量化内核把 ~4.4× 的税压到 ~1×。
// 只转换严格匹配的循环(规范 IV、单位步长 gep、纯 FP DAG、读写数组不别名、无 call)，其余原样留给既有路径。
static cl::opt<bool> AtftWideDMR(
    "atft-wide-dmr",
    cl::desc("Cross-iteration vectorized DMR for unit-stride data-parallel FP "
             "loops: redundant copy rides AVX-512 upper lanes, half-vs-half check"),
    cl::init(false));

// ISA 对齐旋钮：SSA 基准被钉死在 SSE2(baseline target-features)，连 native 都不碰 AVX-512。
// 要公平衡量 wide-DMR 的收益，必须让 native 基线与受保护版跑在同一 ISA。开此旋钮会把所有目标
// 函数的 target-features/target-cpu 提升到带 AVX-512，对 native 与受保护模式一视同仁。
static cl::opt<bool> AtftIsaAvx512(
    "atft-isa-avx512",
    cl::desc("Raise target-features of all target functions to AVX-512 (uniform "
             "ISA baseline for fair wide-DMR comparison)"),
    cl::init(false));

// 通用 DMR：不认循环形状。先把 ISA 提到 AVX-512 并在模块内跑一遍 default<O3>(复用 LLVM
// 成熟通用的 LoopVectorizer 把能向量化的内核自动宽向量化)，再对已向量化/标量的 FP 数据流
// 通用地注入冗余影子链(叶子 load 走指针 launder 防折叠)+XOR 签名，返回点提交。map/stencil/
// 归约/串行一视同仁，换没见过的内核也自动生效(非 pattern-match)。
static cl::opt<bool> AtftDMRGeneric(
    "atft-dmr-generic",
    cl::desc("Shape-agnostic DMR: run LLVM vectorizer, then inject redundant FP "
             "shadow dataflow + signature on whatever got vectorized (or scalar)"),
    cl::init(false));

// 公平基线：走与 -atft-dmr-generic 完全相同的 ISA+内部 O3 管线，但不注入冗余。
// 用它当分母，隔离出纯冗余开销(排除双重 O3 对基准码的影响)。
static cl::opt<bool> AtftDMRNoInject(
    "atft-dmr-no-inject",
    cl::desc("With -atft-dmr-generic: run ISA+O3 pipeline but skip redundancy "
             "injection (fair baseline denominator)"),
    cl::init(false));

// 通用 TMR：与 -atft-dmr-generic 同架构，但注入 2 条影子并在每 FP 存储/返回写 median-of-3
// (纠错，而非 DMR 的只检错)。用于验证"骑 SIMD 空闲车道"能否把三模纠错干到 <3×。
static cl::opt<bool> AtftTMRGeneric(
    "atft-tmr-generic",
    cl::desc("With -atft-dmr-generic: 3-way TMR (2 shadows + median-of-3 vote "
             "writeback = correction) instead of 2-way detect-only DMR"),
    cl::init(false));


enum CritLevel {
  CRIT_NON_CRITICAL = 0,
  CRIT_SUB_CRITICAL = 1,
  CRIT_CRITICAL     = 2
};

struct FuncCriticalityInfo {
  std::string funcName;
  unsigned numInsts = 0;
  unsigned maxLoopDepth = 0;
  double avgDistToOutput = 0.0;
  double avgLiveness = 0.0;
  double stateFeedbackFactor = 0.0;
  double score = 0.0;
  CritLevel level = CRIT_NON_CRITICAL;
};

void expandFMulAddIntrinsics(Function &F) {
  std::vector<CallInst *> fmaCalls;
  for (BasicBlock &BB : F) {
    for (Instruction &I : BB) {
      if (auto *CI = dyn_cast<CallInst>(&I)) {
        if (Function *callee = CI->getCalledFunction()) {
          if (callee->getName() == "llvm.fmuladd.f64") {
            fmaCalls.push_back(CI);
          }
        }
      }
    }
  }
  for (CallInst *CI : fmaCalls) {
    IRBuilder<> IRB(CI);
    Value *a = CI->getArgOperand(0);
    Value *b = CI->getArgOperand(1);
    Value *c = CI->getArgOperand(2);
    Value *mulVal = IRB.CreateFMul(a, b, "fma.mul");
    Value *addVal = IRB.CreateFAdd(mulVal, c, "fma.add");
    CI->replaceAllUsesWith(addVal);
    CI->eraseFromParent();
  }
}

bool isTargetFP64Op(Instruction *I) {
  if (!I || !I->getType()->isDoubleTy()) return false;
  unsigned op = I->getOpcode();
  return (op == Instruction::FAdd || op == Instruction::FSub ||
          op == Instruction::FMul || op == Instruction::FDiv ||
          op == Instruction::FNeg);
}

bool isTargetMathCall(Instruction *I) {
  if (auto *CI = dyn_cast<CallInst>(I)) {
    if (Function *callee = CI->getCalledFunction()) {
      StringRef name = callee->getName();
      return ((name == "sin" || name == "cos" || name == "exp" || name == "sqrt" || name == "log" ||
               name.startswith("llvm.sin") || name.startswith("llvm.cos") ||
               name.startswith("llvm.exp") || name.startswith("llvm.sqrt") ||
               name.startswith("llvm.log")) &&
              CI->getType()->isDoubleTy());
    }
  }
  return false;
}

// ============================================================================
// 模块 1: 关键性排序算法实现 (含 HotPath, Shoestring [ASPLOS'10], SDCTune [CASES'14], Semantic)
// ============================================================================

// fi-cache 排名:site_key -> FI 实测 SDC(%)。由 -atft-vuln-cache 指向的合并 CSV 填充,
// 第一次运行由 vuln_cache.py 定点注入测量得到,之后直接复用,无需再注错。
static std::map<std::string, double> g_atftVulnCache;
static bool g_atftVulnCacheLoaded = false;

static void loadAtftVulnCache(const std::string &path) {
  g_atftVulnCache.clear();
  g_atftVulnCacheLoaded = true;
  std::ifstream in(path);
  if (!in.is_open()) {
    errs() << "ATFT_VULN_CACHE_ERROR," << path << "\n";
    return;
  }
  std::string line;
  bool header = true;
  size_t n = 0;
  while (std::getline(in, line)) {
    if (header) { header = false; continue; }  // 跳过 site_key,sdc 表头
    size_t comma = line.rfind(',');
    if (comma == std::string::npos) continue;
    std::string key = line.substr(0, comma);
    if (key.empty()) continue;                 // 空 key(O3 后合成的控制/地址整数op)不在保护域
    double sdc = atof(line.substr(comma + 1).c_str());
    // 同一 key 取最大 SDC(保守:按最坏脆弱性排)
    auto it = g_atftVulnCache.find(key);
    if (it == g_atftVulnCache.end() || sdc > it->second) g_atftVulnCache[key] = sdc;
    ++n;
  }
  errs() << "ATFT_VULN_CACHE," << path << ",keys=" << g_atftVulnCache.size()
         << ",rows=" << n << "\n";
}

static double atftVulnCacheScore(const Instruction *I) {
  if (MDNode *N = I->getMetadata("atft.site.key")) {
    if (N->getNumOperands() > 0) {
      if (auto *S = dyn_cast<MDString>(N->getOperand(0))) {
        auto it = g_atftVulnCache.find(S->getString().str());
        if (it != g_atftVulnCache.end()) return it->second;
      }
    }
  }
  return -1.0;  // 未命中:cache 里没这条(含空 key 的 O3 合成 op)
}

std::map<const Instruction *, double>
computeInstructionCriticalityMap(Function &F, LoopInfo &LI, const std::string &rankMethod) {
  std::queue<const Instruction *> bfsQueue;
  std::map<const Instruction *, unsigned> distMap;

  for (const BasicBlock &BB : F) {
    for (const Instruction &I : BB) {
      if (isa<StoreInst>(&I) || isa<ReturnInst>(&I)) {
        distMap[&I] = 0;
        bfsQueue.push(&I);
      }
    }
  }

  while (!bfsQueue.empty()) {
    const Instruction *cur = bfsQueue.front();
    bfsQueue.pop();
    unsigned curDist = distMap[cur];
    for (const Use &U : cur->operands()) {
      if (const auto *opInst = dyn_cast<Instruction>(U.get())) {
        if (distMap.find(opInst) == distMap.end()) {
          distMap[opInst] = curDist + 1;
          bfsQueue.push(opInst);
        }
      }
    }
  }

  // 递归追踪整条跨迭代循环状态反馈链 (Recurrent State Dependency Slice)
  std::set<const Instruction *> inRecurrentCycle;
  std::queue<const Instruction *> recQueue;
  for (const BasicBlock &BB : F) {
    if (LI.getLoopDepth(&BB) >= 1) {
      for (const Instruction &I : BB) {
        if (const auto *PN = dyn_cast<PHINode>(&I)) {
          if (PN->getType()->isDoubleTy()) {
            inRecurrentCycle.insert(PN);
            recQueue.push(PN);
          }
        }
      }
    }
  }
  while (!recQueue.empty()) {
    const Instruction *cur = recQueue.front();
    recQueue.pop();
    for (const Use &U : cur->operands()) {
      if (const auto *opInst = dyn_cast<Instruction>(U.get())) {
        if (!inRecurrentCycle.count(opInst)) {
          inRecurrentCycle.insert(opInst);
          recQueue.push(opInst);
        }
      }
    }
  }

  std::map<const Instruction *, double> instScores;
  unsigned idx = 1;
  for (const BasicBlock &BB : F) {
    unsigned depth = LI.getLoopDepth(&BB);
    for (const Instruction &I : BB) {
      idx++;
      unsigned d = distMap.count(&I) ? distMap[&I] : 10;
      double closeness = 1.0 / (1.0 + (double)d);

      // 计算直接扇出 (Fan-out)、跨基本块活跃范围和语义边界特征。
      unsigned fanOut = I.getNumUses();
      unsigned crossBBUses = 0;
      bool feedsStoreDirectly = false;
      bool feedsBranch = false;
      bool feedsStoreAddr = false;
      bool feedsCall = false;
      bool feedsGEP = false;
      bool recurrentFeedback = false;
      for (const User *U : I.users()) {
        if (const auto *userInst = dyn_cast<Instruction>(U)) {
          if (userInst->getParent() != &BB) crossBBUses++;
          if (isa<BranchInst>(userInst) || isa<SelectInst>(userInst) ||
              isa<SwitchInst>(userInst))
            feedsBranch = true;
          if (const auto *SI = dyn_cast<StoreInst>(userInst)) {
            if (SI->getValueOperand() == &I) feedsStoreDirectly = true;
            if (SI->getPointerOperand() == &I) feedsStoreAddr = true;
          }
          if (isa<CallBase>(userInst)) feedsCall = true;
          if (isa<GetElementPtrInst>(userInst)) feedsGEP = true;
          if (const auto *PN = dyn_cast<PHINode>(userInst)) {
            Loop *userLoop = LI.getLoopFor(PN->getParent());
            Loop *instLoop = LI.getLoopFor(&BB);
            if (userLoop && instLoop && instLoop->contains(PN->getParent()))
              recurrentFeedback = true;
          }
        }
      }

      // 算子类型固有误差放大因子 (除法/超越函数/乘法对位翻转的放大高于加减法)
      double opAmp = 1.0;
      if (I.getOpcode() == Instruction::FDiv || isa<CallInst>(&I)) opAmp = 3.0;
      else if (I.getOpcode() == Instruction::FMul) opAmp = 1.8;

      double s = 0.0;
      if (rankMethod == "fi-cache") {
        // 本课题 B 机制:直接用 FI 实测脆弱性缓存排名。命中的站点按实测 SDC 排,
        // 未命中(含 O3 合成的控制/地址整数op)压到负分区,用 closeness 稳定次序。
        double v = atftVulnCacheScore(&I);
        s = (v >= 0.0) ? (1000.0 + v) : (0.001 * closeness);
      } else if (rankMethod == "hotpath" || rankMethod == "loop-only") {
        // HotPath (Lu et al.): 纯按动态执行频率 (10^depth) 与程序早期出现顺序排序
        s = std::pow(10.0, (double)depth) - 0.05 * (double)idx;
      } else if (rankMethod == "shoestring") {
        // Shoestring (Feng et al., ASPLOS'10): 仅保护高扇出 (Fan-out >= 2) 的消费者树，忽略单扇出归约与超越函数
        s = (fanOut >= 2) ? (10.0 * (double)fanOut) : (0.5 * closeness);
      } else if (rankMethod == "sdctune") {
        // SDCTune (Lu et al., CASES'14): 仅根据算子类型放大因子 (FDiv/FMul > FAdd) 排序，忽略控制流深度与扇出树
        s = 4.0 * opAmp + 0.2 * closeness;
      } else if (rankMethod == "random") {
        uint64_t h = (uint64_t)idx * 11400714819323198485ULL;
        s = (double)((h >> 32) & 0xFFFF) / 65536.0;
      } else if (rankMethod == "boundary-branch" ||
                 rankMethod == "boundary-address" ||
                 rankMethod == "boundary-store" ||
                 rankMethod == "boundary-feedback") {
        // 人工语义子集：一次只提升一种边界，作为机制上限消融。
        // 低权重项只用于同组候选的稳定排序，不引入 benchmark/site 白名单。
        bool focus = false;
        if (rankMethod == "boundary-branch") focus = feedsBranch;
        else if (rankMethod == "boundary-address") focus = feedsStoreAddr || feedsGEP;
        else if (rankMethod == "boundary-store") focus = feedsStoreDirectly;
        else focus = recurrentFeedback;
        s = (focus ? 100.0 : 0.0) +
            0.25 * (double)fanOut +
            0.10 * (double)depth +
            0.01 * closeness;
      } else if (rankMethod == "semantic-boundary") {
        // 人工语义上界：优先保护直接进入控制流、地址和外部可见边界的值。
        // 该模式只使用 IR 数据流特征，不依赖 benchmark 名称或固定 site ID。
        s = (feedsStoreAddr ? 5.0 : 0.0) +
            (feedsGEP ? 5.0 : 0.0) +
            (feedsBranch ? 4.0 : 0.0) +
            (feedsStoreDirectly ? 4.0 : 0.0) +
            (feedsCall ? 3.0 : 0.0) +
            (recurrentFeedback ? 2.0 : 0.0) +
            0.5 * (double)crossBBUses +
            0.25 * (double)fanOut +
            0.10 * (double)depth +
            0.01 * closeness;
      } else {
        // 本课题方法 (semantic): 综合高扇出传播树 (Fan-out)、输出汇点紧密度 (Closeness)、
        // 循环嵌套深度 (LDepth)、跨迭代递归状态链 (inRecurrentCycle) 与算子放大因子 (opAmp)
        double fanTreeWeight = (fanOut >= 2) ? (6.0 * (double)fanOut) : (double)fanOut;
        double liveSpan = fanTreeWeight + 2.0 * (double)crossBBUses +
                          (inRecurrentCycle.count(&I) ? 4.0 : 0.0) +
                          (feedsStoreDirectly ? 6.0 : 0.0);
        s = (AtftW1 * 3.0) * closeness * opAmp + AtftW2 * (double)depth + AtftW3 * liveSpan;
      }
      instScores[&I] = s;
    }
  }
  return instScores;
}

FuncCriticalityInfo analyzeFunctionCriticality(Function &F, FunctionAnalysisManager &FAM) {
  FuncCriticalityInfo info;
  info.funcName = F.getName().str();
  auto &LI = FAM.getResult<LoopAnalysis>(F);

  std::queue<const Instruction *> bfsQueue;
  std::map<const Instruction *, unsigned> distMap;

  for (const BasicBlock &BB : F) {
    for (const Instruction &I : BB) {
      info.numInsts++;
      if (isa<StoreInst>(&I) || isa<ReturnInst>(&I)) {
        distMap[&I] = 0;
        bfsQueue.push(&I);
      }
    }
  }

  while (!bfsQueue.empty()) {
    const Instruction *cur = bfsQueue.front();
    bfsQueue.pop();
    unsigned curDist = distMap[cur];
    for (const Use &U : cur->operands()) {
      if (const auto *opInst = dyn_cast<Instruction>(U.get())) {
        if (distMap.find(opInst) == distMap.end()) {
          distMap[opInst] = curDist + 1;
          bfsQueue.push(opInst);
        }
      }
    }
  }

  double sumCloseness = 0.0;
  double sumLiveness = 0.0;
  unsigned arithCount = 0;
  unsigned feedbackPhiCount = 0;

  auto isSmallConstantStencilLoop = [](const Loop *L) -> bool {
    if (!L) return false;
    BasicBlock *H = L->getHeader();
    if (!H) return false;
    if (const auto *BI = dyn_cast<BranchInst>(H->getTerminator())) {
      if (BI->isConditional()) {
        if (const auto *cmp = dyn_cast<ICmpInst>(BI->getCondition())) {
          if (const auto *ci = dyn_cast<ConstantInt>(cmp->getOperand(1))) {
            if (ci->getSExtValue() <= 3) return true;
          }
          // 识别 (base + 2) 这类固定半径 r<=2 的局部模板卷积内层循环
          if (const auto *bo = dyn_cast<BinaryOperator>(cmp->getOperand(1))) {
            if (bo->getOpcode() == Instruction::Add) {
              if (const auto *ci = dyn_cast<ConstantInt>(bo->getOperand(1))) {
                if (ci->getSExtValue() <= 3) return true;
              }
            }
          }
        }
      }
    }
    return false;
  };

  unsigned maxEffectiveDepth = 0;
  for (const BasicBlock &BB : F) {
    unsigned depth = LI.getLoopDepth(&BB);
    if (depth > info.maxLoopDepth) {
      info.maxLoopDepth = depth;
    }

    // 区分有递归累积的大规模循环与 3x3 常数小模板卷积窗 (TripCount <= 3)
    unsigned effDepth = 0;
    for (const Loop *L = LI.getLoopFor(&BB); L != nullptr; L = L->getParentLoop()) {
      if (!isSmallConstantStencilLoop(L)) {
        effDepth++;
      }
    }
    if (effDepth > maxEffectiveDepth) {
      maxEffectiveDepth = effDepth;
    }

    const Loop *curLoop = LI.getLoopFor(&BB);
    bool inSmallStencil = isSmallConstantStencilLoop(curLoop);

    for (const Instruction &I : BB) {
      if (const auto *PN = dyn_cast<PHINode>(&I)) {
        if (PN->getType()->isFloatingPointTy() && depth >= 1 && !inSmallStencil) {
          feedbackPhiCount++;
        }
      }

      if (I.isBinaryOp() || I.isUnaryOp() || isa<CallInst>(&I)) {
        arithCount++;
        unsigned d = distMap.count(&I) ? distMap[&I] : 10;
        sumCloseness += 1.0 / (1.0 + (double)d);

        unsigned liveSpan = 0;
        for (const User *U : I.users()) {
          liveSpan += 1;
          if (const auto *userInst = dyn_cast<Instruction>(U)) {
            if (userInst->getParent() != &BB) {
              liveSpan += 2;
            }
          }
        }
        sumLiveness += (double)liveSpan;
      }
    }
  }

  if (arithCount > 0) {
    info.avgDistToOutput = sumCloseness / (double)arithCount;
    info.avgLiveness = sumLiveness / (double)arithCount;
  }
  info.stateFeedbackFactor = (double)feedbackPhiCount;

  double effectiveLiveness = info.avgLiveness + 0.85 * info.stateFeedbackFactor;
  info.score = AtftW1 * info.avgDistToOutput +
               AtftW2 * (double)maxEffectiveDepth +
               AtftW3 * effectiveLiveness;

  if (info.score >= 6.5) {
    info.level = CRIT_CRITICAL;
  } else if (info.score >= 5.5) {
    info.level = CRIT_SUB_CRITICAL;
  } else {
    info.level = CRIT_NON_CRITICAL;
  }

  return info;
}

struct AtftProfileEntry {
  double dynamicCost = 1.0;
  double boundarySensitivity = 0.0;
  double riskPrior = 1.0;
  std::string className;
};

static std::map<const Instruction *, uint64_t> g_atftSiteIds;
static std::map<const Instruction *, std::string> g_atftSiteKeys;
static std::map<uint64_t, AtftProfileEntry> g_atftProfile;
static std::set<uint64_t> g_atftFrontierSelectedSites;
static std::vector<std::string> g_atftSelectionRows;
static bool g_atftProfileLoaded = false;
static size_t g_atftProfileMissing = 0;

std::set<Instruction *> selectInstructionsByBudget(
    const std::vector<Instruction *> &candidates,
    const std::map<const Instruction *, double> &scores,
    double budget) {
  std::set<Instruction *> selected;
  if (budget <= 0.0 || candidates.empty()) return selected;

  if (AtftBudgetMode == "frontier") {
    for (Instruction *inst : candidates) {
      uint64_t siteId = g_atftSiteIds.count(inst) ? g_atftSiteIds.at(inst) : 0;
      if (g_atftFrontierSelectedSites.count(siteId)) selected.insert(inst);
    }
    return selected;
  }

  if (AtftBudgetMode == "count") {
    if (budget >= 0.999) {
      selected.insert(candidates.begin(), candidates.end());
      return selected;
    }

    std::vector<std::pair<double, Instruction *>> ranked;
    ranked.reserve(candidates.size());
    for (Instruction *inst : candidates) {
      double sc = scores.count(inst) ? scores.at(inst) : 0.0;
      ranked.push_back({sc, inst});
    }
    std::stable_sort(ranked.begin(), ranked.end(),
                     [](const auto &a, const auto &b) { return a.first > b.first; });

    size_t k = std::max<size_t>(1, (size_t)std::round(budget * (double)ranked.size()));
    for (size_t i = 0; i < k && i < ranked.size(); ++i)
      selected.insert(ranked[i].second);
    return selected;
  }

  struct Ranked {
    Instruction *inst;
    uint64_t siteId;
    double cost;
    double boundary;
    double risk;
    double benefit;
    std::string className;
  };

  std::vector<Ranked> ranked;
  ranked.reserve(candidates.size());
  double totalCost = 0.0;
  for (Instruction *inst : candidates) {
    uint64_t siteId = g_atftSiteIds.count(inst) ? g_atftSiteIds.at(inst) : 0;
    auto profileIt = g_atftProfile.find(siteId);
    double cost = 1.0;
    double boundary = 0.0;
    double riskPrior = 1.0;
    std::string className = "unknown";
    if (profileIt != g_atftProfile.end()) {
      cost = profileIt->second.dynamicCost;
      boundary = profileIt->second.boundarySensitivity;
      riskPrior = profileIt->second.riskPrior;
      className = profileIt->second.className;
    } else {
      ++g_atftProfileMissing;
    }
    if (!(cost > 0.0) || !std::isfinite(cost)) cost = 1.0;

    double staticScore = scores.count(inst) ? scores.at(inst) : 0.0;
    double risk = riskPrior * (1.0 + std::max(0.0, staticScore));
    if (AtftBudgetMode == "risk-cost")
      risk *= 1.0 + 0.10 * std::max(0.0, boundary);
    double benefit = risk / cost;
    ranked.push_back({inst, siteId, cost, boundary, risk, benefit, className});
    totalCost += cost;
  }

  std::stable_sort(ranked.begin(), ranked.end(), [](const Ranked &a, const Ranked &b) {
    if (a.benefit != b.benefit) return a.benefit > b.benefit;
    return a.siteId < b.siteId;
  });

  double capacity = budget * totalCost;
  double selectedCost = 0.0;
  for (const Ranked &item : ranked) {
    if (selected.empty() || selectedCost + item.cost <= capacity + 1e-12) {
      selected.insert(item.inst);
      selectedCost += item.cost;
    }
    if (!g_atftSelectionRows.empty() || !AtftSelectionFile.empty()) {
      bool isSelected = selected.count(item.inst) != 0;
      g_atftSelectionRows.push_back(
          std::to_string(item.siteId) + "," +
          (g_atftSiteKeys.count(item.inst) ? g_atftSiteKeys.at(item.inst) : "") + "," +
          item.className + "," + std::to_string(item.cost) + "," +
          std::to_string(item.boundary) + "," + std::to_string(item.risk) + "," +
          std::to_string(item.benefit) + "," + (isSelected ? "1" : "0") + "," +
          AtftBudgetMode);
    }
  }
  return selected;
}

// 给冗余副本打 atft.replica 标记。它**不**影响默认的注错分类（副本仍可注错），
// 只给裁判层的 -fi-origin-only 模式用：配对比较时只往原程序本来就有的故障点注入，
// 这样各策略的故障空间逐点相同，策略间差异不再混入"冗余硬件带来的新故障点"。
void markReplica(Value *V) {
  if (auto *I = dyn_cast<Instruction>(V)) {
    I->setMetadata("atft.replica", MDNode::get(I->getContext(), {}));
  }
}

Value *insertZeroCostCSEBarrier(IRBuilder<> &IRB, Value *val) {
  if (AtftNoBarrier) return val;
  Type *ty = val->getType();
  InlineAsm *noop = InlineAsm::get(
      FunctionType::get(ty, {ty}, false), "", "=r,0", false);
  return IRB.CreateCall(noop, {val});
}

// ============================================================================
// 模块 0: 归约循环巢的符号求和抽取（等价饱和主线的前端）
//
// 目标类：完美嵌套循环 + 单条累加语句，形如
//     for i0 ... for i_{n-1}:  ACC[f(i)] += g(ARR1[h1(i)], ARR2[h2(i)], ...)
// 抽出来的是每个数组访问**依赖哪些循环下标**，这正是 e-graph 的因子外提规则
// 所需要的 free(·) 信息。识别依据是结构（归约形态 + 下标依赖），不看函数名。
// ============================================================================
namespace {

struct AccessInfo {
  std::string base;             // 数组基址名
  std::set<unsigned> idxDeps;   // 依赖的循环层号（0 = 最外层）
  std::string offset;           // 相对输出访问的地址偏移（SCEV 差值，符号式）
};

// 相对偏移：两个访问地址的 SCEV 之差。stencil 的校验式必须知道它 ——
// 移位窗口的校验和与原窗口差一圈边界项，只知道"依赖哪些下标"是不够的。
// 运行期维度（PolyBench 的 n）下行偏移不是常量，但 SCEV 能给出精确的符号式。
static void collectGEPIndices(Value *P, std::vector<Value *> &idxs, Value *&base) {
  P = P->stripPointerCasts();
  while (auto *GEP = dyn_cast<GetElementPtrInst>(P)) {
    std::vector<Value *> here;
    for (Use &U : GEP->indices()) here.push_back(U.get());
    idxs.insert(idxs.begin(), here.begin(), here.end());
    P = GEP->getPointerOperand()->stripPointerCasts();
  }
  base = P;
}

// 逐维下标差。指针整体的 SCEV 在 opaque pointer + 运行期步长下常常算不出
// （getMinusSCEV 返回 CouldNotCompute），但**整数下标**一定可 SCEV 化，
// 逐维相减就能拿到 stencil 需要的相对偏移（0 / ±1 / 运行期维度则报 ?）。
static std::string scevOffset(Value *Ptr, Value *RefPtr, ScalarEvolution &SE) {
  std::vector<Value *> ia, ib;
  Value *ba = nullptr, *bb = nullptr;
  collectGEPIndices(Ptr, ia, ba);
  collectGEPIndices(RefPtr, ib, bb);
  if (ia.empty() || ia.size() != ib.size()) return "?";
  std::string out;
  for (unsigned k = 0; k < ia.size(); ++k) {
    if (k) out += "/";
    if (!SE.isSCEVable(ia[k]->getType()) || !SE.isSCEVable(ib[k]->getType())) {
      out += "?";
      continue;
    }
    const SCEV *D = SE.getMinusSCEV(SE.getSCEV(ia[k]), SE.getSCEV(ib[k]));
    if (auto *C = dyn_cast<SCEVConstant>(D)) {
      out += std::to_string(C->getAPInt().getSExtValue());
    } else if (isa<SCEVCouldNotCompute>(D)) {
      out += "?";
    } else {
      // 非常量偏移（典型是 ±n 这种运行期维度）只记一个标记：CSV 里塞完整
      // SCEV 文本会和分隔符打架，而下游真正需要知道的就是"这一维是符号偏移、
      // 生成校验器时要补边界项"这一个事实
      out += "sym";
    }
  }
  if (ba != bb) out += "(other)";
  return out;
}

// 下标依赖哪些循环：交给 ScalarEvolution。一个访问的 SCEV 里出现的每个
// SCEVAddRecExpr 都对应一层循环，这比手写 use 链遍历鲁棒得多 ——
// sext/zext、步长乘法、多维线性化都被 SCEV 规范掉了。
struct AddRecLoopCollector {
  std::set<const Loop *> loops;
  bool follow(const SCEV *S) {
    if (auto *AR = dyn_cast<SCEVAddRecExpr>(S)) loops.insert(AR->getLoop());
    return true;
  }
  bool isDone() const { return false; }
};

static std::string baseName(Value *Ptr) {
  Value *P = Ptr->stripInBoundsConstantOffsets();
  while (auto *GEP = dyn_cast<GetElementPtrInst>(P)) P = GEP->getPointerOperand();
  while (auto *LD = dyn_cast<LoadInst>(P)) P = LD->getPointerOperand();
  P = P->stripPointerCasts();
  if (auto *Arg = dyn_cast<Argument>(P)) return "arg" + std::to_string(Arg->getArgNo());
  if (P->hasName() && !P->getName().startswith(".")) return P->getName().str();
  if (isa<AllocaInst>(P)) return "local";
  return "anon";
}

// 结构相等：没跑 GVN 时同一个下标表达式会被算多遍，按 Value 身份比会判不相等
static bool sameValueStructural(Value *A, Value *B, unsigned depth = 5) {
  if (A == B) return true;
  if (depth == 0) return false;
  if (auto *CA = dyn_cast<ConstantInt>(A))
    if (auto *CB = dyn_cast<ConstantInt>(B)) return CA->getValue() == CB->getValue();
  auto *IA = dyn_cast<Instruction>(A);
  auto *IB = dyn_cast<Instruction>(B);
  if (!IA || !IB || IA->getOpcode() != IB->getOpcode()) return false;
  if (IA->getNumOperands() != IB->getNumOperands()) return false;
  if (isa<PHINode>(IA)) return false;
  for (unsigned k = 0; k < IA->getNumOperands(); ++k)
    if (!sameValueStructural(IA->getOperand(k), IB->getOperand(k), depth - 1))
      return false;
  return true;
}

static bool samePointer(Value *A, Value *B) {
  A = A->stripPointerCasts();
  B = B->stripPointerCasts();
  if (A == B) return true;
  auto *GA = dyn_cast<GetElementPtrInst>(A);
  auto *GB = dyn_cast<GetElementPtrInst>(B);
  if (!GA || !GB || GA->getNumOperands() != GB->getNumOperands()) return false;
  if (!samePointer(GA->getPointerOperand(), GB->getPointerOperand())) return false;
  for (unsigned k = 1; k < GA->getNumOperands(); ++k)
    if (!sameValueStructural(GA->getOperand(k), GB->getOperand(k))) return false;
  return true;
}

static bool describeAccessSCEV(Value *Ptr, ScalarEvolution &SE,
                               const std::map<const Loop *, unsigned> &depthOf,
                               AccessInfo &out) {
  out.base = baseName(Ptr);
  if (!SE.isSCEVable(Ptr->getType())) {
    // 指针类型不可 SCEV 化时退回逐下标分析
    if (auto *GEP = dyn_cast<GetElementPtrInst>(Ptr)) {
      for (Use &U : GEP->indices()) {
        if (!SE.isSCEVable(U.get()->getType())) continue;
        AddRecLoopCollector C;
        SCEVTraversal<AddRecLoopCollector> T(C);
        T.visitAll(SE.getSCEV(U.get()));
        for (const Loop *L : C.loops) {
          auto it = depthOf.find(L);
          if (it != depthOf.end()) out.idxDeps.insert(it->second);
        }
      }
      AccessInfo inner;
      if (describeAccessSCEV(GEP->getPointerOperand(), SE, depthOf, inner))
        out.idxDeps.insert(inner.idxDeps.begin(), inner.idxDeps.end());
      return true;
    }
    return false;
  }
  AddRecLoopCollector C;
  SCEVTraversal<AddRecLoopCollector> T(C);
  T.visitAll(SE.getSCEV(Ptr));
  for (const Loop *L : C.loops) {
    auto it = depthOf.find(L);
    if (it != depthOf.end()) out.idxDeps.insert(it->second);
  }
  return true;
}

// 收集累加项（或映射体）里的所有数组访问与算子构成
static void collectTermAccesses(Value *root, ScalarEvolution &SE,
                                const std::map<const Loop *, unsigned> &depthOf,
                                Loop *OuterLoop, Value *RefPtr,
                                std::vector<AccessInfo> &terms, std::string &opMix) {
  std::vector<Value *> wl{root};
  std::set<Value *> seen;
  while (!wl.empty()) {
    Value *v = wl.back();
    wl.pop_back();
    if (!seen.insert(v).second) continue;
    if (auto *LD = dyn_cast<LoadInst>(v)) {
      AccessInfo ai;
      if (describeAccessSCEV(LD->getPointerOperand(), SE, depthOf, ai)) {
        if (RefPtr) ai.offset = scevOffset(LD->getPointerOperand(), RefPtr, SE);
        terms.push_back(ai);
      }
      continue;
    }
    if (auto *BO = dyn_cast<BinaryOperator>(v)) {
      unsigned op = BO->getOpcode();
      if (op == Instruction::FMul) opMix += "*";
      else if (op == Instruction::FAdd || op == Instruction::FSub) opMix += "+";
      else continue;   // 非线性算子（fdiv 等）不展开，映射判定会因此失败
      wl.push_back(BO->getOperand(0));
      wl.push_back(BO->getOperand(1));
      continue;
    }
    if (auto *FN = dyn_cast<UnaryOperator>(v)) {
      if (FN->getOpcode() == Instruction::FNeg) {
        opMix += "+";
        wl.push_back(FN->getOperand(0));
      }
      continue;
    }
    // 循环不变的标量因子（常系数、形参、循环外算出的值）必须记下来：
    // 它是因子外提规则的作用对象（stencil 的 0.2 就靠它把乘法从 N^2 降到 1），
    // 而且漏了它抽出来的表达式就是不完整的 —— gemm 的 alpha 之前正是这么被丢掉的。
    // 浮点类型不可 SCEV 化，所以循环不变性要用 LoopInfo 判，不能用 SCEV 判。
    if (isa<ConstantFP>(v)) {
      terms.push_back({"const", {}});
      continue;
    }
    if (OuterLoop && OuterLoop->isLoopInvariant(v)) {
      terms.push_back({"scalar", {}});
      continue;
    }
  }
}

// ---------------------------------------------------------------------------
// 编译期插桩所需的三件东西，全部按语法模式从 IR 里取，取不到就放弃：
//   基址   : GEP 链最底下的指针
//   行步长 : 二维访问里 `mul(sext(iv), stride)` 的另一个操作数
//   迭代数 : 循环 latch 上 `icmp slt iv, bound` 的 bound
// 这样不需要 SCEVExpander 在 preheader 物化表达式，风险低且都是循环不变量。
// ---------------------------------------------------------------------------
static Value *gepBasePtr(Value *P) {
  P = P->stripPointerCasts();
  while (auto *GEP = dyn_cast<GetElementPtrInst>(P))
    P = GEP->getPointerOperand()->stripPointerCasts();
  return P;
}

// 步长：取访问地址 SCEV 里关于循环 L 的 AddRec 步长，再除以元素字节数。
// 这是 SCEV 的定义量，不是从 `mul` 的操作数里猜 —— 后者会被 sext/步长本身也是
// sext 这类写法打败（第一版就栽在这里）。
static Value *rowStrideSCEV(Value *Ptr, Loop *L, ScalarEvolution &SE,
                            SCEVExpander &Exp, Instruction *InsertPt,
                            unsigned elemBytes) {
  if (!SE.isSCEVable(Ptr->getType())) return nullptr;
  const SCEV *S = SE.getSCEV(Ptr);
  const SCEVAddRecExpr *AR = nullptr;
  // 找以 L 为循环的那一层 AddRec（地址 SCEV 可能是多层嵌套）
  while (auto *cur = dyn_cast<SCEVAddRecExpr>(S)) {
    if (cur->getLoop() == L) { AR = cur; break; }
    S = cur->getStart();
  }
  if (!AR || !AR->isAffine()) return nullptr;
  const SCEV *Step = AR->getStepRecurrence(SE);
  if (!SE.isLoopInvariant(Step, L)) return nullptr;
  // 字节步长 -> 元素步长
  const SCEV *Elems = SE.getUDivExactExpr(
      Step, SE.getConstant(Step->getType(), elemBytes));
  if (isa<SCEVCouldNotCompute>(Elems)) return nullptr;
  // 点无关的 isSafeToExpand 只保证"能展开"，不保证展开出来的值在 InsertPt 支配。
  // 对 3mm/syrk 这类巢，trip/stride 的 SCEV 会引用某个循环出口的 lcssa 值，在 preheader
  // 物化会得到不支配的指令（"Instruction does not dominate all uses"）。用点相关的
  // isSafeToExpandAt 兜住：不安全就返回 nullptr，让上层干净地 ABFT_SKIP 而不是产出坏模块。
  if (!Exp.isSafeToExpand(Elems) || !Exp.isSafeToExpandAt(Elems, InsertPt)) return nullptr;
  return Exp.expandCodeFor(Elems, Type::getInt64Ty(Ptr->getContext()), InsertPt);
}

// 迭代次数：backedge-taken count + 1，由 SCEVExpander 在插入点物化。
static Value *tripCountSCEV(Loop *L, ScalarEvolution &SE, SCEVExpander &Exp,
                            Instruction *InsertPt) {
  const SCEV *BTC = SE.getBackedgeTakenCount(L);
  if (isa<SCEVCouldNotCompute>(BTC)) return nullptr;
  // 体执行次数与 backedge 次数的关系取决于判定在哪：
  //   判定在 latch（底测）-> 体跑 BTC + 1 次
  //   判定在 header（旋转后的常见形态）-> 体跑 BTC 次
  // 按结构判，不按经验加一 —— 加错一位会让列和缓冲少一格、校验直接失效。
  bool bottomTested = L->getExitingBlock() &&
                      L->getExitingBlock() == L->getLoopLatch();
  const SCEV *TC = bottomTested
                       ? SE.getAddExpr(BTC, SE.getOne(BTC->getType()))
                       : BTC;
  if (!Exp.isSafeToExpand(TC) || !Exp.isSafeToExpandAt(TC, InsertPt)) return nullptr;
  Value *v = Exp.expandCodeFor(TC, BTC->getType(), InsertPt);
  IRBuilder<> B(InsertPt);
  Type *I32 = Type::getInt32Ty(L->getHeader()->getContext());
  if (v->getType() != I32) v = B.CreateTruncOrBitCast(v, I32);
  return v;
}

// ---------------------------------------------------------------------------
// 插桩：粒度由结构推导，准入由代价不等式判定，不含"形状"硬编码。
//
// 归约区域 L*：巢里最外层的那个循环，满足"累加器的内存在它内部除了归约语句
// 之外没有别的写入"。不变量 post - pre == sum_K term 在 L* 上按归约的定义成立，
// 不管 L* 外面发生过什么（gemm 的 C *= beta、2mm 的 tmp = 0 都在外面）。
// 旧版本把"巢内有别的写入"当成拒绝条件，那是症状补丁；这里改成**往内找 L***，
// 粒度随之确定。
//
// 准入判据（与离线搜索同一套口径）：校验要在 L* 外面每一组下标上各做一次，
// 每次至少要读一遍归约维，所以
//     deg(校验) >= (L* 外的循环层数) + 1
// 取等号作为下界，当且仅当它严格小于内核的 deg（= 巢深度）时才插。
//     gemm : L* = k 层，外面只剩 i，1 + 1 = 2 < 3  -> 插
//     2mm  : L* = k 层，外面是 i 与 j，2 + 1 = 3 >= 3 -> 不插（判据自己拒掉）
// ---------------------------------------------------------------------------
// 某个访问相对循环 L 的 AddRec 步长（字节）。取不到返回 nullptr。
static const SCEV *addrStepFor(Value *Ptr, Loop *L, ScalarEvolution &SE) {
  if (!SE.isSCEVable(Ptr->getType())) return nullptr;
  const SCEV *S = SE.getSCEV(Ptr);
  while (auto *cur = dyn_cast<SCEVAddRecExpr>(S)) {
    if (cur->getLoop() == L) return cur->getStepRecurrence(SE);
    S = cur->getStart();
  }
  return nullptr;
}

// 行维与列维：行主序二维数组里，**列维的步长等于元素大小**，行维不是。
// 这是由数据布局决定的，不依赖项的收集顺序 —— 原来按 loads[0]/loads[1] 认 A/B，
// 顺序一反，I/J 与 A/B 全部转置，步长与基址就全错了。
static bool classifyRowCol(Value *AccPtr, const std::vector<Loop *> &nest,
                           const std::set<unsigned> &accDeps, ScalarEvolution &SE,
                           unsigned elemBytes, unsigned &rowIdx, unsigned &colIdx) {
  bool haveRow = false, haveCol = false;
  for (unsigned d : accDeps) {
    const SCEV *St = addrStepFor(AccPtr, nest[d], SE);
    if (!St) return false;
    auto *C = dyn_cast<SCEVConstant>(St);
    bool isElem = C && C->getAPInt().getSExtValue() == (int64_t)elemBytes;
    if (isElem) {
      if (haveCol) return false;
      colIdx = d;
      haveCol = true;
    } else {
      if (haveRow) return false;
      rowIdx = d;
      haveRow = true;
    }
  }
  return haveRow && haveCol;
}

// 归约区域：最外层那个"累加器在归约开始前的值是**已知的**"的循环。
// 已知有两种来源：
//   (a) 区域内除归约语句外没有别的写入 -> 用 preheader 快照拿到
//   (b) 别的写入是把一个常量写进累加器 -> 初值解析可得，连快照都不用
// (b) 比做循环分布更一般：分布只是让快照点存在，而这里直接把初值算出来。
// 返回区域循环；initConst 非空时表示走 (b)，值为那个常量。
static Loop *reductionRegion(const std::vector<Loop *> &nest, StoreInst *SI,
                             ConstantFP *&initConst, Value *&initScale) {
  Value *accBase = gepBasePtr(SI->getPointerOperand());
  for (Loop *L : nest) {           // 最外到最内
    bool clean = true;
    ConstantFP *konst = nullptr;
    Value *scaleVal = nullptr;
    for (BasicBlock *BB : L->blocks()) {
      for (Instruction &I : *BB) {
        auto *OtherSI = dyn_cast<StoreInst>(&I);
        if (!OtherSI || OtherSI == SI) continue;
        if (gepBasePtr(OtherSI->getPointerOperand()) != accBase) continue;
        auto *CFP = dyn_cast<ConstantFP>(OtherSI->getValueOperand());
        if (CFP && !scaleVal && (!konst || konst->isExactlyValue(CFP->getValueAPF()))) {
          konst = CFP;        // 常量初始化：初值已知
          continue;
        }
        // `ACC[..] = ACC[..] * inv`（gemm 的 C *= beta、2mm 的 D *= beta）：
        // 初值同样是已知的 —— 快照乘上那个循环不变标量即可。
        // 所以"已知初值"有三种来源：无其它写入 / 常量 / 已知仿射缩放。
        if (!konst) {
          if (auto *FM = dyn_cast<BinaryOperator>(OtherSI->getValueOperand())) {
            if (FM->getOpcode() == Instruction::FMul) {
              for (unsigned q = 0; q < 2; ++q) {
                auto *LD = dyn_cast<LoadInst>(FM->getOperand(q));
                Value *inv = FM->getOperand(1 - q);
                if (LD && samePointer(LD->getPointerOperand(),
                                      OtherSI->getPointerOperand()) &&
                    L->isLoopInvariant(inv) && inv->getType()->isDoubleTy() &&
                    (!scaleVal || scaleVal == inv)) {
                  scaleVal = inv;
                  break;
                }
              }
              if (scaleVal) continue;
            }
          }
        }
        clean = false;        // 其它形式的写入：初值不可知
        break;
      }
      if (!clean) break;
    }
    if (clean) {
      initConst = konst;
      initScale = scaleVal;
      return L;
    }
  }
  initConst = nullptr;
  initScale = nullptr;
  return nullptr;
}

static bool insertABFTForNest(Function &F, const std::vector<Loop *> &nest,
                              StoreInst *SI, Value *termA, Value *termB,
                              Value *alphaVal, Loop *LoopI, Loop *LoopJ,
                              Loop *LoopK, ScalarEvolution &SE,
                              const DataLayout &DL) {
  Module *M = F.getParent();
  LLVMContext &Ctx = M->getContext();
  Type *F64 = Type::getDoubleTy(Ctx);
  Type *I32 = Type::getInt32Ty(Ctx);
  Type *I64 = Type::getInt64Ty(Ctx);
  Type *PtrTy = PointerType::getUnqual(Ctx);

  ConstantFP *initConst = nullptr;
  Value *initScale = nullptr;
  Loop *Star = reductionRegion(nest, SI, initConst, initScale);
  if (!Star) {
    errs() << "ATFT_ABFT_SKIP," << F.getName() << ",no-reduction-region\n";
    return false;
  }
  // L* 外面的循环层数
  unsigned outerLevels = 0;
  for (Loop *L : nest) {
    if (L == Star) break;
    outerLevels++;
  }
  if (outerLevels + 1 >= nest.size()) {
    errs() << "ATFT_ABFT_SKIP," << F.getName() << ",no-order-reduction(deg "
           << (outerLevels + 1) << ">=" << nest.size() << ")\n";
    return false;
  }
  // 两条插桩路径，按 L* 的位置选：
  //   (a) L* 把行循环也包在里面，且累加器是常量初始化 -> 整矩阵一次校验，不用快照
  //   (b) L* 在行循环内部（行级）           -> 每行快照 + 每行校验
  bool matrixLevel = (initConst || initScale) &&
                     (Star == LoopI || Star->contains(LoopI));
  if (!matrixLevel && Star != LoopK && Star != LoopJ) {
    errs() << "ATFT_ABFT_SKIP," << F.getName() << ",region-not-row-level\n";
    return false;
  }

  BasicBlock *Pre = Star->getLoopPreheader();
  BasicBlock *Exit = Star->getExitBlock();
  if (!Pre || !Exit) {
    errs() << "ATFT_ABFT_SKIP," << F.getName() << ",no "
           << (Pre ? "exit-block" : "preheader") << "\n";
    return false;
  }

  SCEVExpander Exp(SE, DL, "atft.abft");
  Instruction *IP = Pre->getTerminator();
  unsigned elemBytes = DL.getTypeAllocSize(F64);
  Instruction *OuterIP = nest.front()->getLoopPreheader()
                             ? nest.front()->getLoopPreheader()->getTerminator()
                             : nullptr;
  if (!OuterIP) {
    errs() << "ATFT_ABFT_SKIP," << F.getName() << ",no-outer-preheader\n";
    return false;
  }

  // 行基址用 SCEV 构造，不靠"GEP 链上找循环不变的指针"：
  // 未跑 LICM 时行 GEP 就在内层循环里算，按循环不变性去找会一路退到数组基址，
  // 校验就永远在验第 0 行。正确做法是把沿本行变化的那一维的 AddRec 剥掉，
  // 取其 start —— 那就是本行首元素的地址 —— 再在 L* 的 preheader 物化。
  auto rowBaseSCEV = [&](Value *Ptr, Loop *VaryingLoop) -> Value * {
    if (!SE.isSCEVable(Ptr->getType())) return nullptr;
    const SCEV *S = SE.getSCEV(Ptr);
    const SCEV *start = nullptr;
    while (auto *cur = dyn_cast<SCEVAddRecExpr>(S)) {
      if (cur->getLoop() == VaryingLoop) { start = cur->getStart(); break; }
      S = cur->getStart();
    }
    if (!start || !Exp.isSafeToExpand(start) || !Exp.isSafeToExpandAt(start, IP))
      return nullptr;
    return Exp.expandCodeFor(start, Ptr->getType(), IP);
  };

  if (matrixLevel) {
    // 整矩阵路径：维度与步长在 L* 的 preheader 物化，校验调用插在 L* 出口
    Value *baseC = gepBasePtr(SI->getPointerOperand());
    Value *baseA = gepBasePtr(termA);
    Value *bB = gepBasePtr(termB);
    Value *ldc = rowStrideSCEV(SI->getPointerOperand(), LoopI, SE, Exp, IP, elemBytes);
    Value *lda = rowStrideSCEV(termA, LoopI, SE, Exp, IP, elemBytes);
    Value *ldbv = rowStrideSCEV(termB, LoopK, SE, Exp, IP, elemBytes);
    Value *niv = tripCountSCEV(LoopI, SE, Exp, IP);
    Value *njv = tripCountSCEV(LoopJ, SE, Exp, IP);
    Value *nkv = tripCountSCEV(LoopK, SE, Exp, IP);
    if (!baseC || !baseA || !bB || !ldc || !lda || !ldbv || !niv || !njv || !nkv) {
      errs() << "ATFT_ABFT_SKIP," << F.getName() << ",matrix-bind-missing\n";
      return false;
    }
    IRBuilder<> E2(&*Exit->getFirstInsertionPt());
    Value *alpha2 = (alphaVal && alphaVal->getType() == F64)
                        ? alphaVal : ConstantFP::get(F64, 1.0);
    Instruction *call = nullptr;
    if (initConst) {
      FunctionCallee MatFn = M->getOrInsertFunction(
          "atft_abft_check_matrix_const", Type::getVoidTy(Ctx), PtrTy, PtrTy, PtrTy,
          I32, I32, I32, I64, I64, I64, F64, F64);
      call = E2.CreateCall(
          MatFn, {baseC, baseA, bB, niv, njv, nkv, ldc, lda, ldbv, alpha2,
                  ConstantFP::get(F64, initConst->getValueAPF().convertToDouble())});
    } else {
      // 已知仿射缩放：初值 = scale x 快照。快照在 L* 的 preheader 拍，
      // 那时缩放还没发生（缩放语句在 L* 内部）。
      FunctionCallee ScratchFn =
          M->getOrInsertFunction("atft_abft_scratch", PtrTy, I32, I64);
      FunctionCallee PreSumFn = M->getOrInsertFunction(
          "atft_abft_pre_rowsum", Type::getVoidTy(Ctx), PtrTy, I32, I32, I64, PtrTy);
      FunctionCallee PreCopyFn = M->getOrInsertFunction(
          "atft_abft_pre_copy", Type::getVoidTy(Ctx), PtrTy, I32, I32, I64, PtrTy);
      FunctionCallee PostFn = M->getOrInsertFunction(
          "atft_abft_post_gemm", Type::getVoidTy(Ctx), PtrTy, PtrTy, PtrTy,
          I32, I32, I32, I64, I64, I64, F64, F64, PtrTy, PtrTy);
      IRBuilder<> P(IP);
      auto mk = [&](Value *v) {
        if (auto *I = dyn_cast<Instruction>(v))
          I->setMetadata("atft.voter.internal", MDNode::get(Ctx, {}));
        return v;
      };
      Value *niL = mk(P.CreateSExt(niv, I64));
      Value *njL = mk(P.CreateSExt(njv, I64));
      Value *rs = mk(P.CreateCall(ScratchFn, {ConstantInt::get(I32, 0), niL}));
      mk(P.CreateCall(PreSumFn, {baseC, niv, njv, ldc, rs}));
      if (AtftABFTColCheck) {
        // 列方向预和取代原数据副本：O(n) 取代 O(n^2)，换来单错可定位。
        FunctionCallee PreColFn = M->getOrInsertFunction(
            "atft_abft_pre_colsum", Type::getVoidTy(Ctx), PtrTy, I32, I32, I64, PtrTy);
        FunctionCallee PostRcFn = M->getOrInsertFunction(
            "atft_abft_post_gemm_rc", Type::getVoidTy(Ctx), PtrTy, PtrTy, PtrTy,
            I32, I32, I32, I64, I64, I64, F64, F64, PtrTy, PtrTy);
        Value *cs = mk(P.CreateCall(ScratchFn, {ConstantInt::get(I32, 3), njL}));
        mk(P.CreateCall(PreColFn, {baseC, niv, njv, ldc, cs}));
        call = E2.CreateCall(PostRcFn, {baseC, baseA, bB, niv, njv, nkv, ldc, lda,
                                        ldbv, alpha2, initScale, rs, cs});
      } else {
        Value *cp = mk(P.CreateCall(ScratchFn, {ConstantInt::get(I32, 1),
                                                mk(P.CreateMul(niL, njL))}));
        mk(P.CreateCall(PreCopyFn, {baseC, niv, njv, ldc, cp}));
        call = E2.CreateCall(PostFn, {baseC, baseA, bB, niv, njv, nkv, ldc, lda,
                                      ldbv, alpha2, initScale, rs, cp});
      }
    }
    call->setMetadata("atft.voter.internal", MDNode::get(Ctx, {}));
    errs() << "ATFT_ABFT_INSERTED," << F.getName() << ",matrix-level,region-depth="
           << (outerLevels + 1) << ",nest-depth=" << nest.size() << "\n";
    g_abftInserted.insert(&F);
    return true;
  }

  Value *cRow = rowBaseSCEV(SI->getPointerOperand(), LoopJ);
  Value *aRow = rowBaseSCEV(termA, LoopK);
  Value *baseB = gepBasePtr(termB);
  Value *ldb = rowStrideSCEV(termB, LoopK, SE, Exp, OuterIP, elemBytes);
  Value *nj = tripCountSCEV(LoopJ, SE, Exp, OuterIP);
  Value *nk = tripCountSCEV(LoopK, SE, Exp, OuterIP);
  if (!cRow || !aRow || !baseB || !ldb || !nj || !nk) {
    errs() << "ATFT_ABFT_SKIP," << F.getName() << ",bind-missing:"
           << (cRow ? "" : "cRow ") << (aRow ? "" : "aRow ")
           << (ldb ? "" : "ldb ") << (nj ? "" : "nj ") << (nk ? "" : "nk ") << "\n";
    return false;
  }

  FunctionCallee ScratchFn = M->getOrInsertFunction("atft_abft_scratch", PtrTy, I32, I64);
  FunctionCallee ColSumFn = M->getOrInsertFunction(
      "atft_abft_colsum_into", Type::getVoidTy(Ctx), PtrTy, I32, I32, I64, PtrTy);
  FunctionCallee PreSumFn = M->getOrInsertFunction(
      "atft_abft_row_presum", F64, PtrTy, I32, PtrTy);
  FunctionCallee CheckRowFn = M->getOrInsertFunction(
      "atft_abft_check_row", Type::getVoidTy(Ctx), PtrTy, PtrTy, PtrTy, PtrTy,
      PtrTy, I32, I32, I64, F64, F64);

  auto mark = [&](Value *v) {
    if (auto *I = dyn_cast<Instruction>(v))
      I->setMetadata("atft.voter.internal", MDNode::get(Ctx, {}));
    return v;
  };

  // 最外层：vb[k] = sum_j B[k][j]，只算一次（这就是 e-graph 提出来的那个内层求和）
  IRBuilder<> O(OuterIP);
  Value *nkL = mark(O.CreateSExt(nk, I64));
  Value *vb = mark(O.CreateCall(ScratchFn, {ConstantInt::get(I32, 2), nkL}));
  mark(O.CreateCall(ColSumFn, {baseB, nk, nj, ldb, vb}));

  // L* 的 preheader：记录本行的行和与原值副本
  IRBuilder<> B(IP);
  Value *njL = mark(B.CreateSExt(nj, I64));
  Value *rowCopy = mark(B.CreateCall(ScratchFn, {ConstantInt::get(I32, 1), njL}));
  Value *vbHere = mark(B.CreateCall(ScratchFn, {ConstantInt::get(I32, 2), nkL}));
  Value *preSum = mark(B.CreateCall(PreSumFn, {cRow, nj, rowCopy}));

  // L* 的出口：校验本行（必要时重算）
  IRBuilder<> E(&*Exit->getFirstInsertionPt());
  Value *alpha = (alphaVal && alphaVal->getType() == F64) ? alphaVal
                                                          : ConstantFP::get(F64, 1.0);
  mark(E.CreateCall(CheckRowFn, {cRow, aRow, baseB, vbHere, rowCopy,
                                 nj, nk, ldb, alpha, preSum}));
  errs() << "ATFT_ABFT_INSERTED," << F.getName() << ",region-depth="
         << (outerLevels + 1) << ",nest-depth=" << nest.size() << "\n";
  g_abftInserted.insert(&F);
  return true;
}

// ----------------------------------------------------------------------------
// 全量巢描述：给**每一个**最内层循环里的标量 store 落一行，不管它是否匹配
// 已识别的归约/线性映射形态。
//
// 为什么需要它：判据只能对"被识别出来的"内核给出进/不进的理由，而答辩会问
// "剩下那一半工作量为什么没覆盖"。要回答这个问题就得对全部负载给出归类，
// 而归类必须由程序结构导出，不能按内核名手写。
//
// 每行给出：巢深度、输出下标维数、被读数组的下标维数并集、算子集合、
// 以及运算是否落在环上（有加法逆元）—— min/max 型的半环没有逆元，
// 校验和在半环上无定义，这是 floyd-warshall / nussinov 不可约的根本原因。
// ----------------------------------------------------------------------------
void dumpAllNests(Function &F, LoopInfo &LI, ScalarEvolution &SE,
                  std::ostream &OS) {
  std::set<std::string> emitted;
  std::vector<Loop *> innermost;
  for (Loop *L : LI.getLoopsInPreorder())
    if (L->getSubLoops().empty()) innermost.push_back(L);

  for (Loop *Inner : innermost) {
    std::vector<Loop *> nest;
    for (Loop *L = Inner; L; L = L->getParentLoop()) nest.push_back(L);
    std::reverse(nest.begin(), nest.end());
    std::map<const Loop *, unsigned> depthOf;
    for (unsigned d = 0; d < nest.size(); ++d) depthOf[nest[d]] = d;

    for (BasicBlock *BB : Inner->blocks()) {
      for (Instruction &I : *BB) {
        auto *SI = dyn_cast<StoreInst>(&I);
        if (!SI) continue;
        Type *ty = SI->getValueOperand()->getType();
        if (!ty->isFloatingPointTy() && !ty->isIntegerTy()) continue;

        AccessInfo accAcc;
        if (!describeAccessSCEV(SI->getPointerOperand(), SE, depthOf, accAcc))
          continue;

        // 沿 store 的值向上走，收集算子与被读数组的下标维数
        std::set<unsigned> readDims;
        std::map<std::string, size_t> arrayDims;   // 每个被读数组自己的维数
        std::set<std::string> ops;
        bool hasCall = false;
        unsigned phiOutsideHeader = 0;
        bool selfPhi = false;
        std::set<Value *> seen;
        std::vector<Value *> work{SI->getValueOperand()};
        while (!work.empty()) {
          Value *v = work.back();
          work.pop_back();
          if (!v || !seen.insert(v).second) continue;
          if (auto *LD = dyn_cast<LoadInst>(v)) {
            AccessInfo t;
            if (describeAccessSCEV(LD->getPointerOperand(), SE, depthOf, t)) {
              readDims.insert(t.idxDeps.begin(), t.idxDeps.end());
              size_t &d = arrayDims[t.base];
              d = std::max(d, t.idxDeps.size());
            }
            continue;
          }
          auto *inst = dyn_cast<Instruction>(v);
          if (!inst) continue;
          ops.insert(inst->getOpcodeName());
          if (auto *cal = dyn_cast<CallInst>(inst)) hasCall = true;
          // 由控制流选出来的值（min/max/条件更新）在代数上是 select，没有加法
          // 逆元。LLVM 把它编译成"非循环头上的 phi"，所以这里数的是这种 phi：
          // floyd-warshall 与 nussinov 的 min-plus 就是这么现身的。
          if (auto *ph = dyn_cast<PHINode>(inst)) {
            bool isHeader = false;
            for (Loop *L : nest)
              if (L->getHeader() == ph->getParent()) isHeader = true;
            if (!isHeader) phiOutsideHeader++;
            // min/max 型条件更新的判别式：phi 的某个入边直接就是"对被写地址
            // 的 load"，也就是"要么保留旧值、要么换成新值"。这正是 min-plus /
            // max-plus 半环的形态，而半环没有加法逆元，校验和在其上无定义。
            for (Value *iv : ph->incoming_values())
              if (auto *LD = dyn_cast<LoadInst>(iv))
                if (samePointer(LD->getPointerOperand(), SI->getPointerOperand()))
                  selfPhi = true;
          }
          for (Use &U : inst->operands()) work.push_back(U.get());
        }
        // store 本身是否被条件保护。floyd-warshall / nussinov 写成
        //     if (d[i][j] > d[i][k] + d[k][j]) d[i][j] = ...;
        // 也就是**条件写**而不是 phi 选择，所以要看 store 所在块的前驱是不是
        // 由比较驱动的条件分支。条件写等价于 min/max，落在半环上，没有加法逆元。
        bool guardedStore = false;
        for (BasicBlock *pred : predecessors(SI->getParent())) {
          auto *br = dyn_cast<BranchInst>(pred->getTerminator());
          if (br && br->isConditional() && isa<CmpInst>(br->getCondition()) &&
              Inner->contains(pred))
            guardedStore = true;
        }
        // 最内层循环体里是否有喂给条件分支的比较（min/max 的另一半证据）
        bool cmpDrivesBranch = false;
        for (BasicBlock *B2 : Inner->blocks()) {
          auto *br = dyn_cast<BranchInst>(B2->getTerminator());
          if (!br || !br->isConditional()) continue;
          if (B2 == Inner->getLoopLatch()) continue;
          if (isa<CmpInst>(br->getCondition())) cmpDrivesBranch = true;
        }
        size_t maxArrayDims = 0;
        for (auto &kv : arrayDims) maxArrayDims = std::max(maxArrayDims, kv.second);

        std::string line = "NEST," + F.getName().str() + "," +
                           std::to_string(nest.size()) + "," +
                           std::to_string(accAcc.idxDeps.size()) + ",";
        for (unsigned d : accAcc.idxDeps) line += "i" + std::to_string(d) + ";";
        line += "," + std::to_string(readDims.size()) + ",";
        for (unsigned d : readDims) line += "i" + std::to_string(d) + ";";
        line += "," + std::to_string(maxArrayDims);
        line += "," + std::string(ty->isFloatingPointTy() ? "fp" : "int") + ",";
        for (const std::string &o : ops) line += o + ";";
        line += "," + std::to_string(phiOutsideHeader) + "," +
                std::string((selfPhi || guardedStore) ? "cond_update"
                                                      : "unconditional") +
                "," + std::string(cmpDrivesBranch ? "cmp_branch" : "no_cmp_branch") +
                "," + std::string(hasCall ? "has_call" : "no_call");
        if (emitted.insert(line).second) OS << line << "\n";
      }
    }
  }
}

void dumpReductionSums(Function &F, LoopInfo &LI, ScalarEvolution &SE,
                       std::ostream &OS) {
  std::set<std::string> emitted;   // 结构相同的描述去重
  std::vector<Loop *> innermost;
  for (Loop *L : LI.getLoopsInPreorder())
    if (L->getSubLoops().empty()) innermost.push_back(L);

  for (Loop *Inner : innermost) {
    std::vector<Loop *> nest;
    for (Loop *L = Inner; L; L = L->getParentLoop()) nest.push_back(L);
    std::reverse(nest.begin(), nest.end());

    std::map<const Loop *, unsigned> depthOf;
    std::vector<unsigned> tripCounts;
    for (unsigned d = 0; d < nest.size(); ++d) {
      depthOf[nest[d]] = d;
      tripCounts.push_back(SE.getSmallConstantTripCount(nest[d]));
    }
    auto emitLine = [&](const char *kind, const AccessInfo &acc,
                        const std::string &opMix,
                        const std::vector<AccessInfo> &terms) {
      std::string line = std::string(kind) + "," + F.getName().str() + "," +
                         std::to_string(nest.size()) + ",";
      for (unsigned t : tripCounts) line += std::to_string(t) + ";";
      line += "," + acc.base + ":";
      for (unsigned d : acc.idxDeps) line += "i" + std::to_string(d) + ";";
      line += "," + (opMix.empty() ? std::string("+") : opMix) + ",";
      for (const AccessInfo &t : terms) {
        line += t.base + ":";
        for (unsigned d : t.idxDeps) line += "i" + std::to_string(d) + ";";
        line += "@" + (t.offset.empty() ? std::string("-") : t.offset) + "|";
      }
      return line + "\n";
    };

    for (BasicBlock *BB : Inner->blocks()) {
      for (Instruction &I : *BB) {
        auto *SI = dyn_cast<StoreInst>(&I);
        if (!SI) continue;
        Value *val = SI->getValueOperand();
        if (!val->getType()->isDoubleTy() && !val->getType()->isFloatTy()) continue;

        AccessInfo accAcc;
        if (!describeAccessSCEV(SI->getPointerOperand(), SE, depthOf, accAcc)) continue;

        // --- 形态一：归约。store 的值是 fadd，且一个操作数是对同一地址的 load
        if (auto *Acc = dyn_cast<BinaryOperator>(val)) {
          if (Acc->getOpcode() == Instruction::FAdd) {
            Value *term = nullptr;
            for (unsigned k = 0; k < 2; ++k)
              if (auto *LD = dyn_cast<LoadInst>(Acc->getOperand(k)))
                if (samePointer(LD->getPointerOperand(), SI->getPointerOperand()))
                  term = Acc->getOperand(1 - k);
            if (term) {
              std::vector<AccessInfo> terms;
              std::string opMix;
              collectTermAccesses(term, SE, depthOf, nest.front(), SI->getPointerOperand(), terms, opMix);
              if (!terms.empty()) {
                std::string line = emitLine("REDUCTION", accAcc, opMix, terms);
                if (emitted.insert(line).second) OS << line;

                // 编译期插桩：只认"矩阵乘形态"——三层巢、累加器二维、
                // 两个二维项各与累加器共享一维。形态不符就不插，绝不猜。
                // 不再限定"三层巢的矩阵乘形状"：只要求累加器是二维、
                // 两个数组项、角色唯一，深度与是否值得插由代价判据决定
                if (AtftABFTInsert && nest.size() >= 2 && accAcc.idxDeps.size() == 2) {
                  std::vector<std::pair<LoadInst *, AccessInfo>> loads;
                  Value *scalarVal = nullptr;
                  std::vector<Value *> wl2{term};
                  std::set<Value *> seen2;
                  while (!wl2.empty()) {
                    Value *v = wl2.back();
                    wl2.pop_back();
                    if (!seen2.insert(v).second) continue;
                    if (auto *LD = dyn_cast<LoadInst>(v)) {
                      AccessInfo ai;
                      if (describeAccessSCEV(LD->getPointerOperand(), SE, depthOf, ai))
                        loads.push_back({LD, ai});
                      continue;
                    }
                    if (auto *BO = dyn_cast<BinaryOperator>(v)) {
                      if (BO->getOpcode() == Instruction::FMul ||
                          BO->getOpcode() == Instruction::FAdd) {
                        wl2.push_back(BO->getOperand(0));
                        wl2.push_back(BO->getOperand(1));
                      }
                      continue;
                    }
                    if (v->getType()->isDoubleTy() && nest.front()->isLoopInvariant(v) &&
                        !scalarVal)
                      scalarVal = v;
                  }
                  if (loads.size() == 2) {
                    // 归约下标 K：不在累加器下标里的那一层
                    int K = -1;
                    for (unsigned d = 0; d < nest.size(); ++d)
                      if (!accAcc.idxDeps.count(d)) { K = (int)d; break; }
                    unsigned rowIdx = 0, colIdx = 0;
                    const DataLayout &DL = F.getParent()->getDataLayout();
                    unsigned eb = DL.getTypeAllocSize(Type::getDoubleTy(F.getContext()));
                    bool ok = K >= 0 &&
                              classifyRowCol(SI->getPointerOperand(), nest,
                                             accAcc.idxDeps, SE, eb, rowIdx, colIdx);
                    // A 是含 row 维的那一项，B 是含 col 维的那一项，与收集顺序无关
                    int ai = -1, bi = -1;
                    if (ok)
                      for (int t = 0; t < 2; ++t) {
                        bool hasRow = loads[t].second.idxDeps.count(rowIdx);
                        bool hasCol = loads[t].second.idxDeps.count(colIdx);
                        bool hasK = loads[t].second.idxDeps.count((unsigned)K);
                        if (hasRow && hasK && !hasCol) ai = t;
                        else if (hasCol && hasK && !hasRow) bi = t;
                      }
                    if (ok && ai >= 0 && bi >= 0 && ai != bi) {
                      insertABFTForNest(F, nest, SI,
                                        loads[ai].first->getPointerOperand(),
                                        loads[bi].first->getPointerOperand(),
                                        scalarVal, nest[rowIdx], nest[colIdx],
                                        nest[(unsigned)K], SE, DL);
                    } else {
                      errs() << "ATFT_ABFT_SKIP," << F.getName() << ",role-ambiguous\n";
                    }
                  }
                }
                continue;
              }
            }
          }
        }

        // --- 形态二：线性映射。store 的值是 fadd/fmul 构成的树，且不含对同地址的 load
        //     （jacobi2d / deriche / sobel 这类 stencil 就是这一类）
        std::vector<AccessInfo> terms;
        std::string opMix;
        collectTermAccesses(val, SE, depthOf, nest.front(), SI->getPointerOperand(), terms, opMix);
        if (terms.size() < 2) continue;      // 单个 load 的拷贝不算映射
        bool selfRef = false;
        std::vector<Value *> wl{val};
        std::set<Value *> seen;
        while (!wl.empty() && !selfRef) {
          Value *v = wl.back();
          wl.pop_back();
          if (!seen.insert(v).second) continue;
          if (auto *LD = dyn_cast<LoadInst>(v)) {
            if (samePointer(LD->getPointerOperand(), SI->getPointerOperand())) selfRef = true;
            continue;
          }
          if (auto *I2 = dyn_cast<Instruction>(v))
            for (Use &U : I2->operands()) wl.push_back(U.get());
        }
        if (selfRef) continue;
        std::string line = emitLine("LINEAR_MAP", accAcc, opMix, terms);
        if (emitted.insert(line).second) OS << line;
      }
    }
  }
}

} // namespace

// ============================================================================
// 模块 1B: 控制平面三模冗余 (Control-Plane TMR)
//
// 问题：数据平面的表决点在 store 与算术链末端，而喂给 br/select/switch 的比较
// 结果被分支**直接消费**，不进复制链。分支走错后三条副本在错误路径上一致地算
// 错，表决器看到全票通过。根因是副本共享控制流，不是覆盖不到。
//
// 做法：对驱动控制流的 ICmp/FCmp 做三份独立求值（操作数经 CSE 屏障隔离），
// 用按位多数决 (c1&c2)|(c1&c3)|(c2&c3) 取代原比较结果。代价是 2 条额外比较加
// 4 条布尔运算，相对三份算术链低一个量级。
//
// 文献定位：
//   - EDDI (Oh et al., IEEE TR 51(1), 2002) 在分支前比对复制的操作数，
//     本课题的数据已证明这条路径可达：cmp_branch 类 SDC 57.23% -> 39.35%；
//   - nZDC (Didehban & Shrivastava, DAC 2016) 的"校验点挪到使用之前"；
//   - CFCSS (Oh et al., IEEE TR 51(1), 2002) 校验跳转合法性，抓跑飞，
//     抓不到"合法但判断错"的分支，与本变换互补。
// 与 EDDI 的差别：EDDI 全程复制所有指令（开销 2.55x），本变换只复制
// **控制流条件本身**，数据平面仍由近似三模负责，两者正交可叠加。
//
// 注意：副本比较**不**打 atft.voter.internal 标记。打了标记就不再是注错点，
// 保护越多注入点越少，可靠性会被系统性虚高（见 FaultInjectorPass 的分类注释）。
// 三副本都可注错时，随机单比特落到任一副本都被多数决掩盖，这才是真实的 TMR。
// ============================================================================
void transformFunctionCmpTMR(Function &F, unsigned *outProtected,
                              double controlBudget,
                              const std::string &rankMethod) {
  Module *M = F.getParent();
  LLVMContext &Ctx = M->getContext();

  auto drivesControlFlow = [](Instruction *I) {
    for (User *U : I->users()) {
      if (isa<BranchInst>(U) || isa<SelectInst>(U) || isa<SwitchInst>(U)) return true;
    }
    return false;
  };

  auto replicable = [](Value *V) {
    Type *T = V->getType();
    return T->isIntegerTy() || T->isFloatingPointTy() || T->isPointerTy();
  };

  std::vector<CmpInst *> targets;
  for (BasicBlock &BB : F) {
    for (Instruction &I : BB) {
      auto *CI = dyn_cast<CmpInst>(&I);
      if (!CI) continue;
      // 表决器自身的内部比较不再复制：它已经是恢复逻辑的一部分
      if (CI->getMetadata("atft.voter.internal") != nullptr) continue;
      if (!drivesControlFlow(CI)) continue;
      if (!replicable(CI->getOperand(0)) || !replicable(CI->getOperand(1))) continue;
      if (isa<Constant>(CI->getOperand(0)) && isa<Constant>(CI->getOperand(1))) continue;
      targets.push_back(CI);
    }
  }

  std::vector<Instruction *> controlCandidates;
  std::map<const Instruction *, double> controlScores;
  for (CmpInst *CI : targets) {
    controlCandidates.push_back(CI);
    if (rankMethod != "semantic-boundary") {
      controlScores[CI] = 1.0;
      continue;
    }
    double score = 4.0; // every target already drives branch/select/switch
    score += 0.25 * (double)CI->getNumUses();
    for (User *U : CI->users()) {
      auto *UI = dyn_cast<Instruction>(U);
      if (!UI) continue;
      if (const auto *SI = dyn_cast<StoreInst>(UI)) {
        if (SI->getPointerOperand() == CI) score += 5.0;
        if (SI->getValueOperand() == CI) score += 4.0;
      }
      if (isa<GetElementPtrInst>(UI)) score += 5.0;
      if (isa<CallBase>(UI)) score += 3.0;
      if (UI->getParent() != CI->getParent()) score += 0.5;
    }
    controlScores[CI] = score;
  }
  std::set<Instruction *> selectedControls =
      selectInstructionsByBudget(controlCandidates, controlScores, controlBudget);

  // DMR 检错模式:入口建签名 alloca,每条被选比较把"与影子不一致"累进去,返回前提交。
  Type *I64Ty = Type::getInt64Ty(Ctx);
  Type *I32Ty = Type::getInt32Ty(Ctx);
  AllocaInst *cmpSig = nullptr;
  FunctionCallee CmpCommitFn;
  if (AtftCmpDMR) {
    IRBuilder<> EB(&*F.getEntryBlock().getFirstInsertionPt());
    cmpSig = EB.CreateAlloca(I64Ty, nullptr, "atft.cmp.sig");
    EB.CreateStore(ConstantInt::get(I64Ty, 0), cmpSig);
    CmpCommitFn = M->getOrInsertFunction("atft_loop_vote_commit",
                                         Type::getVoidTy(Ctx), I32Ty);
  }

  for (CmpInst *CI : targets) {
    if (!selectedControls.count(CI)) continue;
    Instruction *insertPt = CI->getNextNode();
    if (!insertPt) continue;
    IRBuilder<> IRB(insertPt);

    if (AtftCmpDMR) {
      // 单份影子比较 + 不一致累进签名(检错,不改分支用的原比较 → 走回滚)
      int isoIdx = isa<Constant>(CI->getOperand(0)) ? 1 : 0;
      Value *a = (isoIdx == 0) ? insertZeroCostCSEBarrier(IRB, CI->getOperand(0))
                               : CI->getOperand(0);
      Value *b = (isoIdx == 1) ? insertZeroCostCSEBarrier(IRB, CI->getOperand(1))
                               : CI->getOperand(1);
      Value *rep = IRB.CreateCmp(CI->getPredicate(), a, b, CI->getName() + ".dmr");
      markReplica(rep);
      Value *diff = IRB.CreateZExt(IRB.CreateICmpNE(CI, rep), I64Ty, "cmp.diff");
      IRB.CreateStore(IRB.CreateOr(IRB.CreateLoad(I64Ty, cmpSig), diff), cmpSig);
      if (outProtected) (*outProtected)++;
      continue;
    }

    std::set<Value *> created;
    auto isolate = [&](Value *v) -> Value * {
      if (isa<Constant>(v)) return v;
      Value *b = insertZeroCostCSEBarrier(IRB, v);
      created.insert(b);
      return b;
    };

    // 屏障只用来让三份副本在 CSE/GVN 眼里互不相同，**一个操作数上加就够了**。
    // 原先每个操作数都加，屏障数翻倍 —— 而 asm 是调度屏障，会钉住活跃区间、
    // 阻止 LICM 与展开，实测这部分占到周期开销的一大块。
    int isoIdx = isa<Constant>(CI->getOperand(0)) ? 1 : 0;
    Value *rep[2];
    for (int k = 0; k < 2; ++k) {
      Value *a = (isoIdx == 0) ? isolate(CI->getOperand(0)) : CI->getOperand(0);
      Value *b = (isoIdx == 1) ? isolate(CI->getOperand(1)) : CI->getOperand(1);
      rep[k] = IRB.CreateCmp(CI->getPredicate(), a, b,
                             CI->getName() + ".tmr" + std::to_string(k + 1));
      markReplica(rep[k]);
      created.insert(rep[k]);
    }

    // 位级多数决：(c1&c2) | (c1&c3) | (c2&c3)
    // 多数决写成嵌套 select 而不是与或网络：
    //   maj = c1 ? (c2 | c3) : (c2 & c3)
    //       = select(c1, select(c2, true, c3), select(c2, c3, false))
    // 三条比较副本因此都直接被 SelectInst 消费，裁判层
    // (FaultInjectorPass::cmpDrivesControlFlow) 判定它们仍然驱动控制流，
    // 三份副本全部保持可注错。若写成 and/or 网络，三条比较的使用者都变成位运算，
    // 该类别的注入点会归零 —— 那是把注入点做没了，不是把故障防住了。
    Value *orArm = IRB.CreateSelect(rep[0], ConstantInt::getTrue(Ctx), rep[1], "cmp.or.arm");
    Value *andArm = IRB.CreateSelect(rep[0], rep[1], ConstantInt::getFalse(Ctx), "cmp.and.arm");
    Value *maj = IRB.CreateSelect(CI, orArm, andArm, "cmp.majority");
    created.insert(orArm);
    created.insert(andArm);
    created.insert(maj);
    // 多数决网络本身属于表决逻辑，不作为注错点；三条比较副本保持可注错
    for (Value *v : {orArm, andArm, maj}) {
      if (auto *I = dyn_cast<Instruction>(v))
        I->setMetadata("atft.voter.internal", MDNode::get(Ctx, {}));
    }

    CI->replaceUsesWithIf(maj, [&](Use &U) {
      return created.count(U.getUser()) == 0;
    });
    if (outProtected) (*outProtected)++;
  }

  // DMR 模式:返回前提交签名(ok=1 一致 / 0 检出不一致 → 系统级回滚)
  if (AtftCmpDMR && cmpSig) {
    for (BasicBlock &BB : F) {
      auto *RI = dyn_cast<ReturnInst>(BB.getTerminator());
      if (!RI) continue;
      IRBuilder<> B(RI);
      Value *ok = B.CreateICmpEQ(B.CreateLoad(I64Ty, cmpSig), ConstantInt::get(I64Ty, 0));
      B.CreateCall(CmpCommitFn, {B.CreateZExt(ok, I32Ty)});
    }
  }
}

// 整数后向切片影子构造器:沿整数 load/phi/binop/cast/select 往回复制一份独立影子,
// 叶子(常量/参数/非整数/call)共用。load 走指针 launder 防 CSE 折叠。addr/data 两种 DMR 共用。
static bool atftIsIntTy(Type *T) {
  return T->isIntegerTy(8) || T->isIntegerTy(16) ||
         T->isIntegerTy(32) || T->isIntegerTy(64);
}
static Value *atftBuildIntShadow(Value *v, DenseMap<Value *, Value *> &shadow) {
  auto it = shadow.find(v);
  if (it != shadow.end()) return it->second;
  auto *I = dyn_cast<Instruction>(v);
  if (!I || !atftIsIntTy(v->getType())) { shadow[v] = v; return v; }  // 叶子:共用
  if (auto *PN = dyn_cast<PHINode>(I)) {                              // 先插空 phi 断环
    IRBuilder<> B(PN);
    PHINode *sp = B.CreatePHI(PN->getType(), PN->getNumIncomingValues(), "atft.int.phi");
    shadow[v] = sp;
    for (unsigned i = 0; i < PN->getNumIncomingValues(); ++i)
      sp->addIncoming(atftBuildIntShadow(PN->getIncomingValue(i), shadow),
                      PN->getIncomingBlock(i));
    return sp;
  }
  if (auto *LD = dyn_cast<LoadInst>(I)) {                             // 独立 launder 二次 load
    IRBuilder<> B(LD->getNextNode());
    Value *ptr = LD->getPointerOperand();
    InlineAsm *pbar = InlineAsm::get(
        FunctionType::get(ptr->getType(), {ptr->getType()}, false), "", "=r,0", true);
    Value *p = B.CreateCall(pbar, {ptr});
    Value *s = B.CreateAlignedLoad(LD->getType(), p, LD->getAlign(), "atft.int.ld");
    shadow[v] = s;
    return s;
  }
  if (I->isBinaryOp() || isa<CastInst>(I) || isa<SelectInst>(I)) {    // 克隆并重映射整数操作数
    Instruction *cl = I->clone();
    for (unsigned k = 0; k < cl->getNumOperands(); ++k)
      if (atftIsIntTy(I->getOperand(k)->getType()))
        cl->setOperand(k, atftBuildIntShadow(I->getOperand(k), shadow));
    cl->insertAfter(I);
    cl->setName("atft.int.sh");
    shadow[v] = cl;
    return cl;
  }
  shadow[v] = v;  // 其它(call 等)不复制,当叶子
  return v;
}

// 整数三数取中值 median(a,b,c) = max(min(a,b), min(max(a,b), c))。
// 任一单份被翻则被另两份多数盖掉(TMR 纠正)。
static Value *atftIntMedian3(IRBuilder<> &B, Value *a, Value *b, Value *c) {
  Value *abLt = B.CreateICmpSLT(a, b);
  Value *mn = B.CreateSelect(abLt, a, b);
  Value *mx = B.CreateSelect(abLt, b, a);
  Value *mxc = B.CreateSelect(B.CreateICmpSLT(mx, c), mx, c);     // min(max(a,b), c)
  return B.CreateSelect(B.CreateICmpSGT(mn, mxc), mn, mxc);       // max(min(a,b), that)
}

// 地址平面结构型保护:只复制喂给 GEP 的下标后向切片,GEP 处比对检错。
void transformFunctionAddrDMR(Function &F) {
  Module *M = F.getParent();
  LLVMContext &Ctx = M->getContext();
  Type *I64Ty = Type::getInt64Ty(Ctx);
  Type *I32Ty = Type::getInt32Ty(Ctx);
  FunctionCallee CommitFn = M->getOrInsertFunction(
      "atft_loop_vote_commit", Type::getVoidTy(Ctx), I32Ty);

  std::vector<std::pair<GetElementPtrInst *, Value *>> idxSites;
  for (BasicBlock &BB : F)
    for (Instruction &I : BB)
      if (auto *G = dyn_cast<GetElementPtrInst>(&I))
        for (Use &op : G->indices())
          if (!isa<Constant>(op.get()) && atftIsIntTy(op.get()->getType()))
            idxSites.push_back({G, op.get()});
  if (idxSites.empty()) return;

  IRBuilder<> EB(&*F.getEntryBlock().getFirstInsertionPt());
  AllocaInst *sig = EB.CreateAlloca(I64Ty, nullptr, "atft.addr.sig");
  EB.CreateStore(ConstantInt::get(I64Ty, 0), sig);

  DenseMap<Value *, Value *> shadow;
  DenseMap<Value *, Value *> shadow2;   // 第二份独立影子(correct 模式下用于 TMR 取中)
  unsigned nSites = 0, nCorrected = 0;
  for (auto &pr : idxSites) {
    GetElementPtrInst *G = pr.first;
    Value *idx = pr.second;
    Value *s = atftBuildIntShadow(idx, shadow);
    if (s == idx) continue;
    IRBuilder<> B(G);
    Value *diff = B.CreateZExt(B.CreateICmpNE(idx, s), I64Ty, "addr.diff");
    B.CreateStore(B.CreateXor(B.CreateLoad(I64Ty, sig), diff), sig);
    ++nSites;
    // correct-before-use:第二份独立影子 + 三数取中,用中值替换本 GEP 的下标操作数,
    // 让坏下标在越界访问发生前就被多数盖掉 → 防 DUE。
    if (AtftAddrCorrect) {
      Value *s2 = atftBuildIntShadow(idx, shadow2);
      if (s2 != idx) {
        Value *med = atftIntMedian3(B, idx, s, s2);
        for (unsigned k = 1; k < G->getNumOperands(); ++k)   // 跳过指针操作数 0
          if (G->getOperand(k) == idx) G->setOperand(k, med);
        ++nCorrected;
      }
    }
  }
  if (!nSites) return;
  for (BasicBlock &BB : F) {
    auto *RI = dyn_cast<ReturnInst>(BB.getTerminator());
    if (!RI) continue;
    IRBuilder<> B(RI);
    Value *ok = B.CreateICmpEQ(B.CreateLoad(I64Ty, sig), ConstantInt::get(I64Ty, 0));
    B.CreateCall(CommitFn, {B.CreateZExt(ok, I32Ty)});
  }
  errs() << "ATFT_ADDR_DMR," << F.getName().str() << "," << nSites
         << (AtftAddrCorrect ? (",corrected=" + std::to_string(nCorrected)) : "") << "\n";
}

// 整数数据平面:复制流向整数 store/return 的值的后向切片,在 store 处比对检错。
// 跟随的是"值"而非地址(不穿过 GEP/指针),补 addr-DMR 漏掉的整数数据 load/arith。
// budget<1 时按 SDC 关键度选 top 比例的 store,其余靠症状(崩溃)自现(Shoestring 思路)。
void transformFunctionIntDataDMR(Function &F, const std::string &rankMethod, double budget) {
  Module *M = F.getParent();
  LLVMContext &Ctx = M->getContext();
  Type *I64Ty = Type::getInt64Ty(Ctx);
  Type *I32Ty = Type::getInt32Ty(Ctx);
  FunctionCallee CommitFn = M->getOrInsertFunction(
      "atft_loop_vote_commit", Type::getVoidTy(Ctx), I32Ty);

  std::vector<StoreInst *> intStores;
  for (BasicBlock &BB : F)
    for (Instruction &I : BB)
      if (auto *SI = dyn_cast<StoreInst>(&I))
        if (atftIsIntTy(SI->getValueOperand()->getType()) &&
            !isa<Constant>(SI->getValueOperand()))
          intStores.push_back(SI);
  if (intStores.empty()) return;

  // 选择性:按 store 值的关键度排序,取 top budget 比例
  std::set<StoreInst *> selected;
  if (budget >= 0.999) {
    selected.insert(intStores.begin(), intStores.end());
  } else {
    DominatorTree DT(F);
    LoopInfo LI(DT);
    auto scores = computeInstructionCriticalityMap(F, LI, rankMethod);
    std::vector<Instruction *> cand;
    std::map<Instruction *, StoreInst *> back;
    for (StoreInst *SI : intStores)
      if (auto *vi = dyn_cast<Instruction>(SI->getValueOperand())) {
        cand.push_back(vi);
        back[vi] = SI;
      }
    auto sel = selectInstructionsByBudget(cand, scores, budget);
    for (Instruction *vi : sel)
      if (back.count(vi)) selected.insert(back[vi]);
  }
  if (selected.empty()) return;

  IRBuilder<> EB(&*F.getEntryBlock().getFirstInsertionPt());
  AllocaInst *sig = EB.CreateAlloca(I64Ty, nullptr, "atft.intd.sig");
  EB.CreateStore(ConstantInt::get(I64Ty, 0), sig);

  DenseMap<Value *, Value *> shadow;
  unsigned nSites = 0;
  for (StoreInst *SI : intStores) {
    if (!selected.count(SI)) continue;
    Value *val = SI->getValueOperand();
    Value *s = atftBuildIntShadow(val, shadow);
    if (s == val) continue;
    IRBuilder<> B(SI);
    Value *diff = B.CreateZExt(B.CreateICmpNE(val, s), I64Ty, "intd.diff");
    B.CreateStore(B.CreateXor(B.CreateLoad(I64Ty, sig), diff), sig);
    ++nSites;
  }
  if (!nSites) return;
  for (BasicBlock &BB : F) {
    auto *RI = dyn_cast<ReturnInst>(BB.getTerminator());
    if (!RI) continue;
    IRBuilder<> B(RI);
    Value *ok = B.CreateICmpEQ(B.CreateLoad(I64Ty, sig), ConstantInt::get(I64Ty, 0));
    B.CreateCall(CommitFn, {B.CreateZExt(ok, I32Ty)});
  }
  errs() << "ATFT_INTDATA_DMR," << F.getName().str() << "," << nSites << "\n";
}

// 层③·整数残差码:沿 +,−,× 携带 r=v mod M,store 处比 (v mod M)==r。检测环运算链上的计算故障。
void transformFunctionIntResidue(Function &F) {
  Module *M = F.getParent();
  LLVMContext &Ctx = M->getContext();
  Type *I64Ty = Type::getInt64Ty(Ctx);
  Type *I32Ty = Type::getInt32Ty(Ctx);
  FunctionCallee CommitFn = M->getOrInsertFunction(
      "atft_loop_vote_commit", Type::getVoidTy(Ctx), I32Ty);
  const int64_t MOD = 65521;  // 最大的 < 2^16 素数,乘积在 i64 内不溢出
  Constant *MODC = ConstantInt::get(I64Ty, MOD);

  auto afterPt = [](Instruction *I) -> Instruction * {
    return isa<PHINode>(I) ? &*I->getParent()->getFirstInsertionPt() : I->getNextNode();
  };
  auto norm = [&](IRBuilder<> &B, Value *x) {            // 归一到 [0,M):((x%M)+M)%M
    Value *m = B.CreateSRem(x, MODC);
    return B.CreateSRem(B.CreateAdd(m, MODC), MODC);
  };

  DenseMap<Value *, Value *> resid;
  std::function<Value *(Value *)> res = [&](Value *v) -> Value * {
    auto it = resid.find(v);
    if (it != resid.end()) return it->second;
    auto *I = dyn_cast<Instruction>(v);
    auto encode = [&](Value *val) -> Value * {           // 叶子/断链:r = val mod M(重编码)
      Instruction *ip = I ? afterPt(I) : &*F.getEntryBlock().getFirstInsertionPt();
      IRBuilder<> B(ip);
      return norm(B, B.CreateSExtOrTrunc(val, I64Ty));
    };
    if (!I || !atftIsIntTy(v->getType())) { Value *r = encode(v); resid[v] = r; return r; }
    unsigned op = I->getOpcode();
    if (op == Instruction::Add || op == Instruction::Sub || op == Instruction::Mul) {
      Value *ra = res(I->getOperand(0)), *rb = res(I->getOperand(1));
      IRBuilder<> B(afterPt(I));
      Value *raw = (op == Instruction::Mul) ? B.CreateMul(ra, rb)
                 : (op == Instruction::Add) ? B.CreateAdd(ra, rb)
                                            : B.CreateSub(ra, rb);
      Value *r = norm(B, raw);
      resid[v] = r; return r;
    }
    Value *r = encode(v); resid[v] = r; return r;         // 非环运算:链断开,重编码(此点不受码保护)
  };

  std::vector<StoreInst *> intStores;
  for (BasicBlock &BB : F)
    for (Instruction &I : BB)
      if (auto *SI = dyn_cast<StoreInst>(&I))
        if (atftIsIntTy(SI->getValueOperand()->getType()) && !isa<Constant>(SI->getValueOperand()))
          intStores.push_back(SI);
  if (intStores.empty()) return;

  IRBuilder<> EB(&*F.getEntryBlock().getFirstInsertionPt());
  AllocaInst *sig = EB.CreateAlloca(I64Ty, nullptr, "atft.res.sig");
  EB.CreateStore(ConstantInt::get(I64Ty, 0), sig);
  unsigned n = 0;
  for (StoreInst *SI : intStores) {
    Value *val = SI->getValueOperand();
    Value *rv = res(val);
    IRBuilder<> B(SI);
    Value *vm = norm(B, B.CreateSExtOrTrunc(val, I64Ty));
    Value *diff = B.CreateZExt(B.CreateICmpNE(vm, rv), I64Ty, "res.diff");
    B.CreateStore(B.CreateXor(B.CreateLoad(I64Ty, sig), diff), sig);
    ++n;
  }
  if (!n) return;
  for (BasicBlock &BB : F) {
    auto *RI = dyn_cast<ReturnInst>(BB.getTerminator());
    if (!RI) continue;
    IRBuilder<> B(RI);
    Value *ok = B.CreateICmpEQ(B.CreateLoad(I64Ty, sig), ConstantInt::get(I64Ty, 0));
    B.CreateCall(CommitFn, {B.CreateZExt(ok, I32Ty)});
  }
  errs() << "ATFT_INT_RESIDUE," << F.getName().str() << "," << n << "\n";
}

// 层①·分支:仿射循环退出时 IV==bound 的结构不变量断言。
void transformFunctionLoopIndexAssert(Function &F) {
  Module *M = F.getParent();
  LLVMContext &Ctx = M->getContext();
  Type *I64Ty = Type::getInt64Ty(Ctx);
  Type *I32Ty = Type::getInt32Ty(Ctx);
  FunctionCallee CommitFn = M->getOrInsertFunction(
      "atft_loop_vote_commit", Type::getVoidTy(Ctx), I32Ty);
  DominatorTree DT(F);
  LoopInfo LI(DT);

  std::vector<Loop *> loops(LI.begin(), LI.end());
  for (size_t i = 0; i < loops.size(); ++i)
    for (Loop *sub : *loops[i]) loops.push_back(sub);

  // 规范归纳:header 里的 PHI,latch 入边 = add(phi, 1)
  auto inductionStep1 = [](PHINode *PN, Loop *L) -> bool {
    BasicBlock *latch = L->getLoopLatch();
    if (!latch) return false;
    auto *lv = dyn_cast<BinaryOperator>(PN->getIncomingValueForBlock(latch));
    if (!lv || lv->getOpcode() != Instruction::Add) return false;
    Value *other = (lv->getOperand(0) == PN) ? lv->getOperand(1)
                 : (lv->getOperand(1) == PN) ? lv->getOperand(0) : nullptr;
    auto *c = dyn_cast<ConstantInt>(other);
    return c && c->equalsInt(1);
  };

  IRBuilder<> EB(&*F.getEntryBlock().getFirstInsertionPt());
  AllocaInst *sig = EB.CreateAlloca(I64Ty, nullptr, "atft.lpa.sig");
  EB.CreateStore(ConstantInt::get(I64Ty, 0), sig);

  unsigned n = 0;
  for (Loop *L : loops) {
    BasicBlock *exiting = L->getExitingBlock();
    BasicBlock *exit = L->getExitBlock();
    if (!exiting || !exit) continue;                 // 只处理单出口
    auto *br = dyn_cast<BranchInst>(exiting->getTerminator());
    if (!br || !br->isConditional()) continue;
    auto *cmp = dyn_cast<ICmpInst>(br->getCondition());
    if (!cmp) continue;
    auto p = cmp->getPredicate();
    if (p != ICmpInst::ICMP_SLT && p != ICmpInst::ICMP_ULT) continue;  // 保守:i<N
    Value *iv = nullptr, *bound = nullptr;
    for (int k = 0; k < 2; ++k) {
      Value *a = cmp->getOperand(k), *b = cmp->getOperand(1 - k);
      auto *PN = dyn_cast<PHINode>(a);
      if (PN && PN->getParent() == L->getHeader() && inductionStep1(PN, L) &&
          L->isLoopInvariant(b)) { iv = a; bound = b; break; }
    }
    if (!iv || !bound) continue;
    Instruction *ip = &*exit->getFirstInsertionPt();
    if (!DT.dominates(cast<Instruction>(iv), ip)) continue;  // iv 须支配出口
    if (iv->getType() != bound->getType()) continue;
    IRBuilder<> B(ip);
    Value *ne = B.CreateICmpNE(iv, bound, "lpa.ne");
    Value *d = B.CreateZExt(ne, I64Ty);
    B.CreateStore(B.CreateXor(B.CreateLoad(I64Ty, sig), d), sig);
    ++n;
  }
  if (!n) return;
  for (BasicBlock &BB : F) {
    auto *RI = dyn_cast<ReturnInst>(BB.getTerminator());
    if (!RI) continue;
    IRBuilder<> B(RI);
    Value *ok = B.CreateICmpEQ(B.CreateLoad(I64Ty, sig), ConstantInt::get(I64Ty, 0));
    B.CreateCall(CommitFn, {B.CreateZExt(ok, I32Ty)});
  }
  errs() << "ATFT_LOOP_ASSERT," << F.getName().str() << "," << n << "\n";
}

// 层②·load:整数数据 load 的独立 launder 二次读 + 比对(nZDC 式内存层级冗余)。
void transformFunctionLoadReload(Function &F) {
  Module *M = F.getParent();
  LLVMContext &Ctx = M->getContext();
  Type *I64Ty = Type::getInt64Ty(Ctx);
  Type *I32Ty = Type::getInt32Ty(Ctx);
  FunctionCallee CommitFn = M->getOrInsertFunction(
      "atft_loop_vote_commit", Type::getVoidTy(Ctx), I32Ty);

  std::vector<LoadInst *> loads;
  for (BasicBlock &BB : F)
    for (Instruction &I : BB)
      if (auto *LD = dyn_cast<LoadInst>(&I))
        if (atftIsIntTy(LD->getType()) && !LD->use_empty())
          loads.push_back(LD);
  if (loads.empty()) return;

  IRBuilder<> EB(&*F.getEntryBlock().getFirstInsertionPt());
  AllocaInst *sig = EB.CreateAlloca(I64Ty, nullptr, "atft.ldr.sig");
  EB.CreateStore(ConstantInt::get(I64Ty, 0), sig);

  unsigned n = 0;
  for (LoadInst *LD : loads) {
    IRBuilder<> B(LD->getNextNode());
    Value *ptr = LD->getPointerOperand();
    InlineAsm *pbar = InlineAsm::get(
        FunctionType::get(ptr->getType(), {ptr->getType()}, false), "", "=r,0", true);
    Value *p2 = B.CreateCall(pbar, {ptr});
    Value *reload = B.CreateAlignedLoad(LD->getType(), p2, LD->getAlign(), "atft.ldr.ld");
    Value *d = B.CreateZExt(B.CreateICmpNE(LD, reload), I64Ty, "ldr.diff");
    B.CreateStore(B.CreateXor(B.CreateLoad(I64Ty, sig), d), sig);
    ++n;
  }
  for (BasicBlock &BB : F) {
    auto *RI = dyn_cast<ReturnInst>(BB.getTerminator());
    if (!RI) continue;
    IRBuilder<> B(RI);
    Value *ok = B.CreateICmpEQ(B.CreateLoad(I64Ty, sig), ConstantInt::get(I64Ty, 0));
    B.CreateCall(CommitFn, {B.CreateZExt(ok, I32Ty)});
  }
  errs() << "ATFT_LOAD_RELOAD," << F.getName().str() << "," << n << "\n";
}

// ============================================================================
// TRIDENT 方法复现(Li et al., DSN'18, "Modeling Soft-Error Propagation in Programs")
// 核心:以程序输出(写回 arg/global 的 store)为锚,免 FI 地估每条指令"故障传到输出"的
// 概率。关键是**屏蔽模型**:并非所有故障都传播——比较会屏蔽(1 bit 结果,宽操作数翻转
// 多不改布尔)、截断丢高位、逻辑运算部分屏蔽。反向定点传播(max 式,单调收敛),再按
// 循环深度近似动态频次加权。纯分析,产出每指令 SDC 权重,不做任何代码变换。
// ============================================================================
static bool atftIsOutputStore(StoreInst *SI) {
  const Value *obj = getUnderlyingObject(SI->getPointerOperand());
  return isa<Argument>(obj) || isa<GlobalValue>(obj);  // 写回参数数组/全局 = 程序输出
}

void transformFunctionTridentRank(Function &F, LoopInfo &LI,
                                   std::vector<std::string> &out) {
  // 每条用户边的屏蔽因子(故障从 I 传到 user u 的通过概率),参考 TRIDENT 的屏蔽处理
  auto factor = [](Instruction *I, Instruction *u) -> double {
    if (auto *SI = dyn_cast<StoreInst>(u)) {
      if (SI->getValueOperand() == I) return atftIsOutputStore(SI) ? 1.0 : 0.7; // 经内存 ~0.7
      if (SI->getPointerOperand() == I) return 1.0;                             // 地址损坏→SDC
      return 0.0;
    }
    if (isa<ICmpInst>(u) || isa<FCmpInst>(u)) return 0.10;      // 比较屏蔽(TRIDENT)
    if (isa<TruncInst>(u)) return 0.5;                          // 截断丢高位
    if (isa<ZExtInst>(u) || isa<SExtInst>(u)) return 1.0;
    if (u->isBitwiseLogicOp()) return 0.5;                      // and/or/xor 部分屏蔽
    if (isa<BranchInst>(u) || isa<SelectInst>(u) || isa<SwitchInst>(u)) return 1.0;
    return 1.0;                                                 // 算术等:全传播
  };

  std::vector<Instruction *> insts;
  DenseMap<Instruction *, double> reach;
  for (BasicBlock &BB : F)
    for (Instruction &I : BB) { insts.push_back(&I); reach[&I] = 0.0; }

  auto drivesBranch = [](Instruction *I) {
    for (User *u : I->users())
      if (isa<BranchInst>(u) || isa<SelectInst>(u) || isa<SwitchInst>(u)) return true;
    return false;
  };

  // 反向定点:reach[I] = 故障传到输出的概率(max 式,单调增,收敛)
  for (int iter = 0; iter < 8; ++iter) {
    bool changed = false;
    for (auto it = insts.rbegin(); it != insts.rend(); ++it) {
      Instruction *I = *it;
      double r = reach[I];
      if (auto *SI = dyn_cast<StoreInst>(I)) { if (atftIsOutputStore(SI)) r = std::max(r, 1.0); }
      if ((isa<ICmpInst>(I) || isa<FCmpInst>(I)) && drivesBranch(I)) r = std::max(r, 1.0);
      for (User *u : I->users())
        if (auto *ui = dyn_cast<Instruction>(u))
          r = std::max(r, factor(I, ui) * reach[ui]);
      if (r > reach[I] + 1e-9) { reach[I] = r; changed = true; }
    }
    if (!changed) break;
  }

  // 分类 + 循环深度加权(动态频次近似)
  auto cat = [](Instruction *I) -> const char * {
    if (isa<GetElementPtrInst>(I)) return "addr";
    if (isa<LoadInst>(I)) return "load";
    if (isa<TruncInst>(I) || isa<ZExtInst>(I) || isa<SExtInst>(I) ||
        isa<SIToFPInst>(I) || isa<FPToSIInst>(I)) return "cast";
    if ((isa<ICmpInst>(I) || isa<FCmpInst>(I))) return "cmp_branch";
    if (I->getType()->isDoubleTy() || I->getType()->isFloatTy()) return "fp";
    if (I->getType()->isIntegerTy() && (I->isBinaryOp())) return "int_arith";
    return "other";
  };
  for (BasicBlock &BB : F)
    for (Instruction &I : BB) {
      double w = reach[&I] * std::pow(2.0, std::min<unsigned>(LI.getLoopDepth(&BB), 12));
      if (w <= 0) continue;
      out.push_back(std::string(F.getName()) + "," + cat(&I) + "," +
                    std::to_string(reach[&I]) + "," + std::to_string(w));
    }
}



// ============================================================================
// 模块 2A: 复现 EDDI (Oh et al., IEEE TR'02) 与 SWIFT (Reis et al., CGO'05)
//   - duplicateLoads = true  -> EDDI (包含内存 Load 与地址链双模复制 + 同步比对)
//   - duplicateLoads = false -> SWIFT (假设 ECC 保护内存，仅复制寄存器计算链 + 同步比对)
// ============================================================================
void transformFunctionEDDIOrSWIFT(Function &F, LoopInfo &LI, bool duplicateLoads,
                                  const std::string &rankMethod, double budget) {
  expandFMulAddIntrinsics(F);
  auto instScores = computeInstructionCriticalityMap(F, LI, rankMethod);

  Module *M = F.getParent();
  LLVMContext &Ctx = M->getContext();
  Type *F64Ty = Type::getDoubleTy(Ctx);

  FunctionCallee DMRCheckFn = M->getOrInsertFunction(
      "atft_eddi_swift_check_f64", F64Ty, F64Ty, F64Ty);

  std::vector<PHINode *> phiNodes;
  std::vector<LoadInst *> loadInsts;
  std::vector<Instruction *> computeOrdered;
  std::vector<StoreInst *> storeInsts;

  for (BasicBlock &BB : F) {
    for (Instruction &I : BB) {
      if (auto *PN = dyn_cast<PHINode>(&I)) {
        if (PN->getType()->isDoubleTy()) phiNodes.push_back(PN);
      } else if (auto *LD = dyn_cast<LoadInst>(&I)) {
        if (LD->getType()->isDoubleTy()) loadInsts.push_back(LD);
      } else if (isTargetFP64Op(&I) || isTargetMathCall(&I)) {
        computeOrdered.push_back(&I);
      } else if (auto *SI = dyn_cast<StoreInst>(&I)) {
        if (SI->getValueOperand()->getType()->isDoubleTy()) storeInsts.push_back(SI);
      }
    }
  }

  std::set<Instruction *> selectedSet = selectInstructionsByBudget(computeOrdered, instScores, budget);
  if (selectedSet.empty()) return;

  std::map<Value *, Value *> shadowMap;

  if (duplicateLoads) {
    // EDDI: 复制所有 Load 指令及地址指针
    for (LoadInst *LD : loadInsts) {
      IRBuilder<> IRB(LD->getNextNode());
      Value *ptr = LD->getPointerOperand();
      InlineAsm *ptrBar = InlineAsm::get(
          FunctionType::get(ptr->getType(), {ptr->getType()}, false), "", "=r,0", true);
      Value *pShadow = IRB.CreateCall(ptrBar, {ptr});
      LoadInst *ldShadow = IRB.CreateLoad(F64Ty, pShadow, LD->getName() + ".eddi.ld");
      shadowMap[LD] = ldShadow;
    }
  }

  for (PHINode *PN : phiNodes) {
    IRBuilder<> IRB(PN);
    PHINode *phiShadow = IRB.CreatePHI(F64Ty, PN->getNumIncomingValues(), PN->getName() + ".dmr");
    shadowMap[PN] = phiShadow;
  }

  for (Instruction *inst : computeOrdered) {
    if (!selectedSet.count(inst)) continue;
    IRBuilder<> IRB(inst->getNextNode());

    if (auto *CI = dyn_cast<CallInst>(inst)) {
      Value *arg0 = CI->getArgOperand(0);
      Value *s_arg = shadowMap.count(arg0) ? shadowMap[arg0] : arg0;
      s_arg = insertZeroCostCSEBarrier(IRB, s_arg);

      CallInst *cShadow = dyn_cast<CallInst>(CI->clone());
      cShadow->setArgOperand(0, s_arg);
      IRB.Insert(cShadow, "call.dmr");
      shadowMap[CI] = cShadow;
      continue;
    }

    Value *rep = nullptr;
    if (inst->getOpcode() == Instruction::FNeg) {
      Value *op0 = inst->getOperand(0);
      Value *s_op0 = shadowMap.count(op0) ? shadowMap[op0] : insertZeroCostCSEBarrier(IRB, op0);
      rep = IRB.CreateFNeg(s_op0, inst->getName() + ".dmr");
    } else {
      Value *op0 = inst->getOperand(0);
      Value *op1 = inst->getOperand(1);
      Value *s_op0 = shadowMap.count(op0) ? shadowMap[op0] : insertZeroCostCSEBarrier(IRB, op0);
      Value *s_op1 = shadowMap.count(op1) ? shadowMap[op1] : insertZeroCostCSEBarrier(IRB, op1);
      rep = IRB.CreateBinOp(cast<BinaryOperator>(inst)->getOpcode(),
                            s_op0, s_op1, inst->getName() + ".dmr");
    }

    // EDDI 在算术步与分支前插入双模寄存器一致性检查指令
    if (duplicateLoads) {
      Value *cmpEq = IRB.CreateFCmpOEQ(inst, rep, "eddi.cmp");
      Value *sel = IRB.CreateSelect(cmpEq, inst, rep, "eddi.chk");
      InlineAsm *keepChk = InlineAsm::get(
          FunctionType::get(F64Ty, {F64Ty}, false), "", "=r,0", true);
      IRB.CreateCall(keepChk, {sel});
    }

    shadowMap[inst] = rep;
  }

  for (PHINode *PN : phiNodes) {
    PHINode *phiShadow = cast<PHINode>(shadowMap[PN]);
    for (unsigned i = 0; i < PN->getNumIncomingValues(); ++i) {
      Value *inVal = PN->getIncomingValue(i);
      BasicBlock *inBB = PN->getIncomingBlock(i);
      Value *sv = shadowMap.count(inVal) ? shadowMap[inVal] : inVal;
      phiShadow->addIncoming(sv, inBB);
    }
  }

  for (StoreInst *SI : storeInsts) {
    Value *val = SI->getValueOperand();
    if (shadowMap.count(val)) {
      IRBuilder<> IRB(SI);
      Value *checked = IRB.CreateCall(DMRCheckFn, {val, shadowMap[val]}, "dmr.sync");
      SI->setOperand(0, checked);
    }
  }
}

// ============================================================================
// 模块 2C: 移植 third_party/LLTFI 官方开源指令链复制与位与修复器
//   源自: third_party/LLTFI/llvm_passes/instruction_duplication/InstructionDuplication.cpp
//         & third_party/LLTFI/llvm_passes/instruction_duplication/shared_lib/SIDHelperFunctions.cpp
//   机制: 用 inst->clone() + llvm::RemapInstruction 复制连续算术指令链，
//         在链末端调用 LLTFI 的 compareFloatValues (按 IEEE-754 符号位/指数位/尾数位做 bitwise AND 抑制高位尖峰)
// ============================================================================
void transformFunctionLLTFI_SID(Function &F, LoopInfo &LI,
                                const std::string &rankMethod, double budget) {
  expandFMulAddIntrinsics(F);
  auto instScores = computeInstructionCriticalityMap(F, LI, rankMethod);

  Module *M = F.getParent();
  LLVMContext &Ctx = M->getContext();
  Type *F64Ty = Type::getDoubleTy(Ctx);

  FunctionCallee CompareFn = M->getOrInsertFunction(
      "atft_lltfi_sid_compare_f64", F64Ty, F64Ty, F64Ty);

  std::vector<Instruction *> candidates;
  for (BasicBlock &BB : F) {
    for (Instruction &I : BB) {
      if (isTargetFP64Op(&I)) candidates.push_back(&I);
    }
  }
  std::set<Instruction *> selectedSet = selectInstructionsByBudget(candidates, instScores, budget);
  if (selectedSet.empty()) return;

  // 按照 LLTFI doArithmeticChainDuplication (InstructionDuplication.cpp:392) 识别基本块内的连续算术链
  std::vector<std::vector<Instruction *>> chains;
  for (BasicBlock &BB : F) {
    std::vector<Instruction *> curChain;
    for (Instruction &I : BB) {
      if (selectedSet.count(&I)) {
        curChain.push_back(&I);
      } else if (!curChain.empty()) {
        chains.push_back(curChain);
        curChain.clear();
      }
    }
    if (!curChain.empty()) chains.push_back(curChain);
  }

  // 按照 LLTFI duplicateInstructionChain (InstructionDuplication.cpp:238) 执行克隆与 RemapInstruction
  for (const auto &insVector : chains) {
    ValueToValueMapTy vmap;
    std::vector<Instruction *> new_instructions;
    Instruction *lastInst = nullptr, *lastInstDupl = nullptr;

    for (Instruction *inst : insVector) {
      Instruction *duplicatedInst = inst->clone();
      lastInst = inst;
      lastInstDupl = duplicatedInst;
      for (unsigned int i = 0; i < duplicatedInst->getNumOperands(); i++) {
        duplicatedInst->setOperand(i, inst->getOperand(i));
      }
      duplicatedInst->insertAfter(inst);
      new_instructions.push_back(duplicatedInst);
      vmap[inst] = duplicatedInst;
    }

    for (Instruction *i : new_instructions) {
      llvm::RemapInstruction(i, vmap, RF_NoModuleLevelChanges | RF_IgnoreMissingLocals);
    }

    if (lastInst && lastInstDupl) {
      IRBuilder<> IRB(lastInst->getParent());
      IRB.SetInsertPoint(lastInstDupl->getNextNode());
      Value *duplBar = insertZeroCostCSEBarrier(IRB, lastInstDupl);
      Value *funret = IRB.CreateCall(CompareFn, {lastInst, duplBar}, "lltfi.sid.cmp");
      lastInst->replaceUsesWithIf(funret, [&](Use &U) {
        return U.getUser() != funret && U.getUser() != duplBar;
      });
    }
  }
}

// ============================================================================
// 模块 2B: 传统软件精确 TMR 变换 (COAST-TMR Baseline, IEEE TNS'19)
// ============================================================================
void transformFunctionExactTMR(Function &F, LoopInfo &LI,
                               const std::string &rankMethod, double budget) {
  expandFMulAddIntrinsics(F);
  auto instScores = computeInstructionCriticalityMap(F, LI, rankMethod);

  Module *M = F.getParent();
  LLVMContext &Ctx = M->getContext();
  Type *F64Ty = Type::getDoubleTy(Ctx);

  FunctionCallee VoterExactFn = M->getOrInsertFunction(
      "atft_voter_exact_f64", F64Ty, F64Ty, F64Ty, F64Ty);

  std::vector<PHINode *> phiNodes;
  std::vector<LoadInst *> loadInsts;
  std::vector<Instruction *> computeOrdered;
  std::vector<StoreInst *> storeInsts;

  for (BasicBlock &BB : F) {
    for (Instruction &I : BB) {
      if (auto *PN = dyn_cast<PHINode>(&I)) {
        if (PN->getType()->isDoubleTy()) phiNodes.push_back(PN);
      } else if (auto *LI_inst = dyn_cast<LoadInst>(&I)) {
        if (LI_inst->getType()->isDoubleTy()) loadInsts.push_back(LI_inst);
      } else if (isTargetFP64Op(&I) || isTargetMathCall(&I)) {
        computeOrdered.push_back(&I);
      } else if (auto *SI = dyn_cast<StoreInst>(&I)) {
        if (SI->getValueOperand()->getType()->isDoubleTy()) storeInsts.push_back(SI);
      }
    }
  }

  std::set<Instruction *> selectedSet = selectInstructionsByBudget(computeOrdered, instScores, budget);
  if (selectedSet.empty()) return;

  std::map<Value *, Value *> replica1Map;
  std::map<Value *, Value *> replica2Map;

  for (LoadInst *LD : loadInsts) {
    IRBuilder<> IRB(LD->getNextNode());
    Value *ptr = LD->getPointerOperand();
    InlineAsm *ptrBar = InlineAsm::get(
        FunctionType::get(ptr->getType(), {ptr->getType()}, false), "", "=r,0", true);
    Value *p1 = IRB.CreateCall(ptrBar, {ptr});
    Value *p2 = IRB.CreateCall(ptrBar, {ptr});
    LoadInst *ld1 = IRB.CreateLoad(F64Ty, p1, LD->getName() + ".ld.tmr1");
    LoadInst *ld2 = IRB.CreateLoad(F64Ty, p2, LD->getName() + ".ld.tmr2");
    replica1Map[LD] = ld1;
    replica2Map[LD] = ld2;
  }

  for (PHINode *PN : phiNodes) {
    IRBuilder<> IRB(PN);
    PHINode *phi1 = IRB.CreatePHI(F64Ty, PN->getNumIncomingValues(), PN->getName() + ".tmr1");
    PHINode *phi2 = IRB.CreatePHI(F64Ty, PN->getNumIncomingValues(), PN->getName() + ".tmr2");
    replica1Map[PN] = phi1;
    replica2Map[PN] = phi2;
  }

  for (Instruction *inst : computeOrdered) {
    if (!selectedSet.count(inst)) continue;
    IRBuilder<> IRB(inst->getNextNode());

    if (auto *CI = dyn_cast<CallInst>(inst)) {
      Value *arg0 = CI->getArgOperand(0);
      Value *r1_arg = replica1Map.count(arg0) ? replica1Map[arg0] : arg0;
      Value *r2_arg = replica2Map.count(arg0) ? replica2Map[arg0] : arg0;

      r1_arg = insertZeroCostCSEBarrier(IRB, r1_arg);
      r2_arg = insertZeroCostCSEBarrier(IRB, r2_arg);

      CallInst *c1 = dyn_cast<CallInst>(CI->clone());
      c1->setArgOperand(0, r1_arg);
      IRB.Insert(c1, "call.tmr1");

      CallInst *c2 = dyn_cast<CallInst>(CI->clone());
      c2->setArgOperand(0, r2_arg);
      IRB.Insert(c2, "call.tmr2");

      replica1Map[CI] = c1;
      replica2Map[CI] = c2;
      continue;
    }

    Value *rep1 = nullptr;
    Value *rep2 = nullptr;

    if (inst->getOpcode() == Instruction::FNeg) {
      Value *op0 = inst->getOperand(0);
      Value *r1_op0 = replica1Map.count(op0) ? replica1Map[op0] : insertZeroCostCSEBarrier(IRB, op0);
      Value *r2_op0 = replica2Map.count(op0) ? replica2Map[op0] : insertZeroCostCSEBarrier(IRB, op0);
      rep1 = IRB.CreateFNeg(r1_op0, inst->getName() + ".tmr1");
      rep2 = IRB.CreateFNeg(r2_op0, inst->getName() + ".tmr2");
    } else {
      Value *op0 = inst->getOperand(0);
      Value *op1 = inst->getOperand(1);
      Value *r1_op0 = replica1Map.count(op0) ? replica1Map[op0] : insertZeroCostCSEBarrier(IRB, op0);
      Value *r1_op1 = replica1Map.count(op1) ? replica1Map[op1] : insertZeroCostCSEBarrier(IRB, op1);
      Value *r2_op0 = replica2Map.count(op0) ? replica2Map[op0] : insertZeroCostCSEBarrier(IRB, op0);
      Value *r2_op1 = replica2Map.count(op1) ? replica2Map[op1] : insertZeroCostCSEBarrier(IRB, op1);

      rep1 = IRB.CreateBinOp(cast<BinaryOperator>(inst)->getOpcode(),
                             r1_op0, r1_op1, inst->getName() + ".tmr1");
      rep2 = IRB.CreateBinOp(cast<BinaryOperator>(inst)->getOpcode(),
                             r2_op0, r2_op1, inst->getName() + ".tmr2");
    }

    Value *cmp12 = IRB.CreateFCmpOEQ(inst, rep1, "tmr.c12");
    Value *cmp23 = IRB.CreateFCmpOEQ(rep1, rep2, "tmr.c23");
    Value *sel23 = IRB.CreateSelect(cmp23, rep1, inst, "tmr.s23");
    Value *inlineVoted = IRB.CreateSelect(cmp12, inst, sel23, "tmr.inline.vote");
    for (Value *vv : {cmp12, cmp23, sel23, inlineVoted}) {
      if (auto *vi = dyn_cast<Instruction>(vv)) {
        vi->setMetadata("atft.voter.internal", MDNode::get(Ctx, {}));
      }
    }
    InlineAsm *keepVote = InlineAsm::get(
        FunctionType::get(F64Ty, {F64Ty}, false), "", "=r,0", true);
    inlineVoted = IRB.CreateCall(keepVote, {inlineVoted});

    replica1Map[inst] = rep1;
    replica2Map[inst] = rep2;
  }

  for (PHINode *PN : phiNodes) {
    PHINode *phi1 = cast<PHINode>(replica1Map[PN]);
    PHINode *phi2 = cast<PHINode>(replica2Map[PN]);
    for (unsigned i = 0; i < PN->getNumIncomingValues(); ++i) {
      Value *inVal = PN->getIncomingValue(i);
      BasicBlock *inBB = PN->getIncomingBlock(i);
      Value *v1 = replica1Map.count(inVal) ? replica1Map[inVal] : inVal;
      Value *v2 = replica2Map.count(inVal) ? replica2Map[inVal] : inVal;
      phi1->addIncoming(v1, inBB);
      phi2->addIncoming(v2, inBB);
    }
  }

  for (StoreInst *SI : storeInsts) {
    Value *val = SI->getValueOperand();
    if (replica1Map.count(val) && replica2Map.count(val)) {
      IRBuilder<> IRB(SI);
      Value *voted = IRB.CreateCall(
          VoterExactFn, {val, replica1Map[val], replica2Map[val]}, "tmr.sync");
      SI->setOperand(0, voted);
    }
  }
}

// ============================================================================
// 模块 2C: 整数数据通路精确 TMR (对应开题报告 3.4.4 中"近似不适用"的代码段)
//
//   近似变换（FP64->FP32、多项式近似、i32->i16 压缩）对**流入比较/分支/地址计算**
//   的整数运算链在语义上不可用：一旦截断位宽，比较结果与控制流就会改变。
//   因此对这类整数数据通路只能采用精确三模复制 + 按位一致性表决。
//   与 FP64 通路保持一致的设计：复制链不改写原数据流（只在 Store 汇点表决），
//   内联表决结果由零开销 InlineAsm 保活，保证变换是语义保持的。
// ============================================================================
static bool isSupportedIntTy(Type *T) {
  if (AtftIntTMRI64 && T->isIntegerTy(64)) return true;
  return T->isIntegerTy(8) || T->isIntegerTy(16) || T->isIntegerTy(32);
}

// 判断一条指令能否被整数通路三模复制。覆盖 load / phi / 二元运算 / 位宽转换 /
// select / icmp —— select 与 icmp 必须覆盖，否则 min/max 这类用条件选择表达的
// 内核（floyd-warshall、nussinov、quicksort、OBPMark 像元流水线）副本链会在
// select 处断掉，Store 汇点拿不到三路值，整数负载实际上得不到任何保护。
static bool isReplicableIntInst(Instruction *I) {
  if (I->getMetadata("atft.voter.internal") != nullptr) return false;
  if (I->getMetadata("atft.int16.approx") != nullptr) return false;
  if (isa<LoadInst>(I) || isa<PHINode>(I))
    return isSupportedIntTy(I->getType());
  if (I->isBinaryOp())
    return isSupportedIntTy(I->getType());
  if (isa<TruncInst>(I) || isa<ZExtInst>(I) || isa<SExtInst>(I))
    return isSupportedIntTy(I->getType()) &&
           isSupportedIntTy(I->getOperand(0)->getType());
  if (isa<SelectInst>(I))
    return isSupportedIntTy(I->getType());
  if (auto *IC = dyn_cast<ICmpInst>(I))
    return isSupportedIntTy(IC->getOperand(0)->getType());
  return false;
}

void transformFunctionExactIntTMR(Function &F, LoopInfo &LI,
                                  const std::string &rankMethod, double budget) {
  if (AtftIntBudget >= 0.0) budget = AtftIntBudget;
  auto instScores = computeInstructionCriticalityMap(F, LI, rankMethod);

  Module *M = F.getParent();
  LLVMContext &Ctx = M->getContext();
  Type *I32Ty = Type::getInt32Ty(Ctx);

  FunctionCallee VoterExactI32Fn = M->getOrInsertFunction(
      "atft_voter_exact_i32", I32Ty, I32Ty, I32Ty, I32Ty);

  auto markInternal = [&](Value *V) -> Value * {
    if (auto *I = dyn_cast<Instruction>(V)) {
      I->setMetadata("atft.voter.internal", MDNode::get(Ctx, {}));
    }
    return V;
  };

  std::vector<PHINode *> phiNodes;
  std::vector<LoadInst *> loadInsts;
  std::vector<Instruction *> computeOrdered;
  std::vector<StoreInst *> storeInsts;

  for (BasicBlock &BB : F) {
    for (Instruction &I : BB) {
      if (auto *SI = dyn_cast<StoreInst>(&I)) {
        if (isSupportedIntTy(SI->getValueOperand()->getType())) storeInsts.push_back(SI);
        continue;
      }
      if (!isReplicableIntInst(&I)) continue;
      if (auto *PN = dyn_cast<PHINode>(&I))      phiNodes.push_back(PN);
      else if (auto *LD = dyn_cast<LoadInst>(&I)) loadInsts.push_back(LD);
      else                                        computeOrdered.push_back(&I);
    }
  }

  std::set<Instruction *> selectedSet =
      selectInstructionsByBudget(computeOrdered, instScores, budget);
  if (selectedSet.empty()) return;

  std::map<Value *, Value *> replica1Map;
  std::map<Value *, Value *> replica2Map;

  for (LoadInst *LD : loadInsts) {
    IRBuilder<> IRB(LD->getNextNode());
    Value *ptr = LD->getPointerOperand();
    InlineAsm *ptrBar = InlineAsm::get(
        FunctionType::get(ptr->getType(), {ptr->getType()}, false), "", "=r,0", true);
    Value *p1 = markInternal(IRB.CreateCall(ptrBar, {ptr}));
    Value *p2 = markInternal(IRB.CreateCall(ptrBar, {ptr}));
    Value *l1 = IRB.CreateLoad(LD->getType(), p1, LD->getName() + ".ld.itmr1");
    Value *l2 = IRB.CreateLoad(LD->getType(), p2, LD->getName() + ".ld.itmr2");
    // 冗余 load 在 D8 下保持可注错：三份 load 都是真实存在的硬件读，
    // 裁判层看不见它们就等于把故障空间固定在原件上
    if (!AtftIntTMRWire && !AtftIntTMRBoundary) { markInternal(l1); markInternal(l2); }
    markReplica(l1);
    markReplica(l2);
    replica1Map[LD] = l1;
    replica2Map[LD] = l2;
  }

  for (PHINode *PN : phiNodes) {
    IRBuilder<> IRB(PN);
    PHINode *phi1 = IRB.CreatePHI(PN->getType(), PN->getNumIncomingValues(), PN->getName() + ".itmr1");
    PHINode *phi2 = IRB.CreatePHI(PN->getType(), PN->getNumIncomingValues(), PN->getName() + ".itmr2");
    markInternal(phi1);
    markInternal(phi2);
    replica1Map[PN] = phi1;
    replica2Map[PN] = phi2;
  }

  // 统一按"克隆 + 换操作数"的方式复制，二元运算 / 位宽转换 / select / icmp 同一套路径
  for (Instruction *inst : computeOrdered) {
    if (!selectedSet.count(inst)) continue;
    Instruction *nextNode = inst->getNextNode();
    if (!nextNode) continue;
    IRBuilder<> IRB(nextNode);

    Instruction *rep1 = inst->clone();
    Instruction *rep2 = AtftIntDMR ? nullptr : inst->clone();
    std::set<Value *> itmrInternalUsers;
    bool isolated = false;
    for (unsigned k = 0; k < inst->getNumOperands(); ++k) {
      Value *op = inst->getOperand(k);
      if (isa<BasicBlock>(op)) continue;
      Value *r1 = replica1Map.count(op) ? replica1Map[op] : op;
      Value *r2 = replica2Map.count(op) ? replica2Map[op] : op;
      // 屏障只需一个操作数：有了它，两条副本与原件就已经互不相同，
      // 后续 CSE/GVN 无法折叠。每个操作数都加等于白付调度代价。
      if (r1 == op && (rep2 ? r2 == op : true) && !isa<Constant>(op) && !isolated) {
        r1 = insertZeroCostCSEBarrier(IRB, op);
        if (rep2) r2 = insertZeroCostCSEBarrier(IRB, op);
        isolated = true;
      }
      rep1->setOperand(k, r1);
      if (rep2) rep2->setOperand(k, r2);
    }
    IRB.Insert(rep1, inst->getName() + ".itmr1");
    if (rep2) IRB.Insert(rep2, inst->getName() + ".itmr2");
    // 两条副本**不**打 atft.voter.internal：打了就不再是注错点，整数通路的
    // 故障空间会停在 352 个点不变（与 native 相同），注入必然命中原件，
    // 看起来"保护有效"其实是裁判看不见冗余硬件。与 fp64 精确三模
    // （188 -> 564 个点）保持同一口径。
    if (!AtftIntTMRWire && !AtftIntTMRBoundary && !AtftIntDMR) {
      markInternal(rep1);
      if (rep2) markInternal(rep2);
    }
    markReplica(rep1);
    if (rep2) markReplica(rep2);
    itmrInternalUsers.insert(rep1);
    if (rep2) itmrInternalUsers.insert(rep2);
    replica1Map[inst] = rep1;
    if (rep2) replica2Map[inst] = rep2;

    // 边界判定：所有使用者都会被复制时，这个节点不需要表决/比对，
    // 故障沿单条链继续传播，到第一个离开子图的点再被处理
    bool atBoundary = false;
    for (User *U : inst->users()) {
      auto *uI = dyn_cast<Instruction>(U);
      if (!uI) { atBoundary = true; break; }
      if (itmrInternalUsers.count(uI)) continue;
      if (!(selectedSet.count(uI) || replica1Map.count(uI))) { atBoundary = true; break; }
    }
    // DMR：在**每个边界**比一次，而不是只在整数 store 汇点比。
    // 只在 store 比的那一版实测丢了 6.4pp SDC（整数故障多半是喂给 GEP 的下标，
    // 根本到不了整数 store），所以检测点必须覆盖"值离开被复制子图"的所有位置。
    if (AtftIntDMR && atBoundary && isSupportedIntTy(inst->getType())) {
      GlobalVariable *GFlag = cast<GlobalVariable>(
          M->getOrInsertGlobal("g_atft_dmr_hit", I32Ty));
      Value *ne = markInternal(IRB.CreateICmpNE(inst, rep1, "dmr.ne"));
      Value *neI = markInternal(IRB.CreateZExt(ne, I32Ty));
      Value *cur = markInternal(IRB.CreateLoad(I32Ty, GFlag, "dmr.flag"));
      Value *upd = markInternal(IRB.CreateOr(cur, neI));
      markInternal(IRB.CreateStore(upd, GFlag));
    }

    bool needVote = true;
    if (AtftIntTMRBoundary) {
      needVote = atBoundary;
    }

    // 按位一致性内联表决（i1 结果不参与，避免无意义开销）
    // DMR 模式不做逐节点表决，只在汇点比对一次
    if (!AtftIntDMR && needVote && isSupportedIntTy(inst->getType())) {
      Value *cmp12 = markInternal(IRB.CreateICmpEQ(inst, rep1, "itmr.c12"));
      Value *cmp23 = markInternal(IRB.CreateICmpEQ(rep1, rep2, "itmr.c23"));
      Value *sel23 = markInternal(IRB.CreateSelect(cmp23, rep1, inst, "itmr.s23"));
      Value *voted = markInternal(IRB.CreateSelect(cmp12, inst, sel23, "itmr.inline.vote"));
      itmrInternalUsers.insert(cmp12);
      itmrInternalUsers.insert(cmp23);
      itmrInternalUsers.insert(sel23);
      itmrInternalUsers.insert(voted);
      // 表决结果必须**接回数据流**。原实现只用一条 InlineAsm 把 voted 保活、
      // 不替换使用者，于是原件被打坏后照样往下游传，整条内联表决等于白跑：
      // 整数类 SDC 36.17% 在三模后只降 1.8pp 就是这么来的。
      if (AtftIntTMRWire || AtftIntTMRBoundary) {
        inst->replaceUsesWithIf(voted, [&](Use &U) {
          if (itmrInternalUsers.count(U.getUser())) return false;
          if (!AtftIntTMRBoundary) return true;
          // 边界模式下只替换"离开子图"的那些使用者，子图内部仍走原链
          auto *uI = dyn_cast<Instruction>(U.getUser());
          if (!uI) return true;
          return !(selectedSet.count(uI) || replica1Map.count(uI));
        });
      } else {
        // 冻结基线的原行为：只把表决结果保活，不接回数据流
        Type *vt = inst->getType();
        InlineAsm *keepVote = InlineAsm::get(
            FunctionType::get(vt, {vt}, false), "", "=r,0", true);
        markInternal(IRB.CreateCall(keepVote, {voted}));
      }
    }
  }

  for (PHINode *PN : phiNodes) {
    PHINode *phi1 = cast<PHINode>(replica1Map[PN]);
    PHINode *phi2 = cast<PHINode>(replica2Map[PN]);
    for (unsigned i = 0; i < PN->getNumIncomingValues(); ++i) {
      Value *inVal = PN->getIncomingValue(i);
      BasicBlock *inBB = PN->getIncomingBlock(i);
      phi1->addIncoming(replica1Map.count(inVal) ? replica1Map[inVal] : inVal, inBB);
      phi2->addIncoming(replica2Map.count(inVal) ? replica2Map[inVal] : inVal, inBB);
    }
  }

  // Store 汇点三路表决：窄于 32 位的值先零扩展再表决，表决后截回原宽度，
  // 以复用运行时唯一的 i32 表决器（它同时负责统计检出次数）
  for (StoreInst *SI : storeInsts) {
    Value *val = SI->getValueOperand();
    if (AtftIntDMR) {
      // 双模：汇点比一次，不一致就把全局标志按位或上去，由上层整段重算。
      // 用"或"而不是分支或函数调用：无分支、无调用，3 条指令，而且标志是粘性的，
      // 一次执行里只要有一处不一致就会被上层看到。
      // 这里不纠错 —— 两份副本无法定位谁错，纠错交给重执行给出第三个值。
      if (!replica1Map.count(val) || !isSupportedIntTy(val->getType())) continue;
      IRBuilder<> IRB(SI);
      GlobalVariable *GFlag = cast<GlobalVariable>(
          M->getOrInsertGlobal("g_atft_dmr_hit", I32Ty));
      Value *ne = markInternal(IRB.CreateICmpNE(val, replica1Map[val], "dmr.ne"));
      Value *neI = markInternal(IRB.CreateZExt(ne, I32Ty));
      Value *cur = markInternal(IRB.CreateLoad(I32Ty, GFlag, "dmr.flag"));
      Value *upd = markInternal(IRB.CreateOr(cur, neI));
      markInternal(IRB.CreateStore(upd, GFlag));
      continue;
    }
    if (!replica1Map.count(val) || !replica2Map.count(val)) continue;
    IRBuilder<> IRB(SI);
    Type *vt = val->getType();
    Value *a = val, *b = replica1Map[val], *c = replica2Map[val];
    if (vt->isIntegerTy(64)) {
      // i64 比 i32 表决器宽，零扩展再截回会丢高 32 位，这里直接内联多数决
      Value *c12 = markInternal(IRB.CreateICmpEQ(a, b, "i64.c12"));
      Value *c23 = markInternal(IRB.CreateICmpEQ(b, c, "i64.c23"));
      Value *s23 = markInternal(IRB.CreateSelect(c23, b, a, "i64.s23"));
      Value *maj = markInternal(IRB.CreateSelect(c12, a, s23, "i64.vote"));
      SI->setOperand(0, maj);
      continue;
    }
    if (vt != I32Ty) {
      a = markInternal(IRB.CreateZExt(a, I32Ty));
      b = markInternal(IRB.CreateZExt(b, I32Ty));
      c = markInternal(IRB.CreateZExt(c, I32Ty));
    }
    Value *voted = markInternal(IRB.CreateCall(VoterExactI32Fn, {a, b, c}, "itmr.sync"));
    if (vt != I32Ty) voted = markInternal(IRB.CreateTrunc(voted, vt));
    SI->setOperand(0, voted);
  }
}

// ============================================================================
// 模块 3A: 中间计算结果位宽压缩变换 (对应开题报告 3.4.2.1: 32-bit -> 16-bit 整数运算链压缩)
//   - 识别非循环控制/非 GEP 下标的 32-bit 整数数据运算链 (Add/Sub/Mul/Shl/AShr/And/Or/Xor)
//   - 将其截断为 <2 x i16> 窄位宽双路近似副本并行计算，并在恢复点符号扩展 (SExt) 回 i32 做一致性表决
// ============================================================================
void transformFunctionInt32ToInt16Compression(Function &F, CritLevel critLevel, bool useSIMD) {
  Module *M = F.getParent();
  LLVMContext &Ctx = M->getContext();
  Type *I32Ty = Type::getInt32Ty(Ctx);
  Type *I16Ty = Type::getInt16Ty(Ctx);
  VectorType *V2I16Ty = FixedVectorType::get(I16Ty, 2);

  FunctionCallee VoterApproxI32Fn = M->getOrInsertFunction(
      "atft_voter_approx_i32", I32Ty, I32Ty, I32Ty, I32Ty, I32Ty, I32Ty);

  auto markInternal = [&](Value *V) -> Value * {
    if (auto *I = dyn_cast<Instruction>(V)) {
      I->setMetadata("atft.voter.internal", MDNode::get(Ctx, {}));
    }
    return V;
  };

  std::vector<Instruction *> intDataOps;
  for (BasicBlock &BB : F) {
    for (Instruction &I : BB) {
      if (isa<PHINode>(&I) || I.getMetadata("atft.voter.internal") != nullptr) continue;
      if (I.isBinaryOp() && I.getType()->isIntegerTy(32)) {
        // 位宽压缩的**可靠性前提**：参与运算的值必须可证明落在 int16 范围内，
        // 否则 trunc 会静默改变结果（实测在 OBPMark 1.1 的 uint16 像元累加上
        // 把相对误差放大到 1.2e5）。开题报告 3.4.2.1 要求的"识别数值范围
        // 受限的变量"正是这一步：用 ComputeNumSignBits 判定有效位宽
        // （>=17 个符号位 <=> 16 位有符号可表示），不满足就不压缩。
        const DataLayout &DL = F.getParent()->getDataLayout();
        if (ComputeNumSignBits(&I, DL) < 17 ||
            ComputeNumSignBits(I.getOperand(0), DL) < 17 ||
            ComputeNumSignBits(I.getOperand(1), DL) < 17) {
          continue;
        }
        bool isLoopOrAddr = false;
        for (User *U : I.users()) {
          if (isa<PHINode>(U) || isa<ICmpInst>(U) || isa<GetElementPtrInst>(U) ||
              isa<SExtInst>(U) || isa<ZExtInst>(U)) {
            isLoopOrAddr = true;
            break;
          }
        }
        if (!isLoopOrAddr) {
          intDataOps.push_back(&I);
        }
      }
    }
  }

  if (intDataOps.empty()) return;

  std::map<Value *, Value *> i16VecMap;
  auto getOrCreateI16Vec = [&](Value *v, IRBuilder<> &IRB) -> Value * {
    auto it = i16VecMap.find(v);
    if (it != i16VecMap.end()) return it->second;
    if (auto *CI = dyn_cast<ConstantInt>(v)) {
      int16_t sval = (int16_t)CI->getSExtValue();
      Constant *c16 = ConstantInt::getSigned(I16Ty, sval);
      return ConstantVector::get({c16, c16});
    }
    Value *tr16 = markInternal(IRB.CreateTrunc(v, I16Ty, "i32.to.i16"));
    Value *uVec = UndefValue::get(V2I16Ty);
    Value *v0 = markInternal(IRB.CreateInsertElement(uVec, tr16, ConstantInt::get(I32Ty, 0)));
    Value *v1 = markInternal(IRB.CreateInsertElement(v0, tr16, ConstantInt::get(I32Ty, 1), "v2i16.pack"));
    return v1;
  };

  for (Instruction *inst : intDataOps) {
    Instruction *nextNode = inst->getNextNode();
    if (!nextNode) continue;
    IRBuilder<> IRB(nextNode);

    // 标记该 i32 运算已由位宽压缩近似覆盖，后续整数精确 TMR 不再重复保护
    // 注意：这里的 inst 是**原件**（被位宽压缩覆盖），不是副本，不能打 atft.replica，
    // 否则配对注入模式会把它从原程序故障空间里误删
    inst->setMetadata("atft.int16.approx", MDNode::get(Ctx, {}));

    Value *op0 = inst->getOperand(0);
    Value *op1 = inst->getOperand(1);
    Value *v0 = getOrCreateI16Vec(op0, IRB);
    Value *v1 = getOrCreateI16Vec(op1, IRB);
    auto opc = cast<BinaryOperator>(inst)->getOpcode();

    // 针对移位指令确保移位量在 [0, 15] 范围内
    if (opc == Instruction::Shl || opc == Instruction::AShr || opc == Instruction::LShr) {
      Constant *mask15 = ConstantInt::get(I16Ty, 15);
      v1 = markInternal(IRB.CreateAnd(v1, ConstantVector::get({mask15, mask15})));
    }

    Value *vRes = nullptr;
    if (useSIMD) {
      vRes = markInternal(IRB.CreateBinOp(opc, v0, v1, inst->getName() + ".v2i16"));
      markReplica(vRes);
    } else {
      Value *a1_0 = markInternal(IRB.CreateExtractElement(v0, ConstantInt::get(I32Ty, 0)));
      Value *a2_0 = markInternal(IRB.CreateExtractElement(v0, ConstantInt::get(I32Ty, 1)));
      Value *a1_1 = markInternal(IRB.CreateExtractElement(v1, ConstantInt::get(I32Ty, 0)));
      Value *a2_1 = markInternal(IRB.CreateExtractElement(v1, ConstantInt::get(I32Ty, 1)));
      Value *r1 = markInternal(IRB.CreateBinOp(opc, a1_0, a1_1, "i16.a1"));
      Value *r2 = markInternal(IRB.CreateBinOp(opc, a2_0, a2_1, "i16.a2"));
      Value *uVec = UndefValue::get(V2I16Ty);
      Value *t0 = markInternal(IRB.CreateInsertElement(uVec, r1, ConstantInt::get(I32Ty, 0)));
      vRes = markInternal(IRB.CreateInsertElement(t0, r2, ConstantInt::get(I32Ty, 1)));
    }
    i16VecMap[inst] = vRes;

    // 若该 i32 指令流出到 Store、SExt、GEP 或 FP 转换等恢复汇点，插入 SExt i16->i32 与三路一致性恢复
    bool isRecoverySink = false;
    for (User *U : inst->users()) {
      Instruction *uI = dyn_cast<Instruction>(U);
      if (!uI || std::find(intDataOps.begin(), intDataOps.end(), uI) == intDataOps.end()) {
        isRecoverySink = true;
        break;
      }
    }
    if (isRecoverySink) {
      Value *lane0 = markInternal(IRB.CreateExtractElement(vRes, ConstantInt::get(I32Ty, 0)));
      Value *lane1 = markInternal(IRB.CreateExtractElement(vRes, ConstantInt::get(I32Ty, 1)));
      Value *sext0 = markInternal(IRB.CreateSExt(lane0, I32Ty, "i16.sext0"));
      Value *sext1 = markInternal(IRB.CreateSExt(lane1, I32Ty, "i16.sext1"));
      Value *votedI32 = markInternal(IRB.CreateCall(
          VoterApproxI32Fn,
          {inst, sext0, sext1, ConstantInt::get(I32Ty, 2), ConstantInt::get(I32Ty, (int)critLevel)},
          "i16.voted"));
      inst->replaceUsesWithIf(votedI32, [&](Use &U) {
        Instruction *uI = dyn_cast<Instruction>(U.getUser());
        return uI == nullptr || uI->getMetadata("atft.voter.internal") == nullptr;
      });
      i16VecMap[votedI32] = vRes;
    }
  }
}
//   - 在 SIMD 模式下，Store 同步点直接采用 LLVM IR 内联指令实现三路阈值表决：
//     无需调用外部 C 函数，仅用 5 条 SSE/RVV 标量指令完成 |R - A1| <= eff_delta 判定与恢复！
// ============================================================================
Value *createInlineFastApproxVote(IRBuilder<> &IRB, Module *M,
                                  Value *rExact, Value *vApprox2xF32, double delta) {
  LLVMContext &Ctx = M->getContext();
  Type *F64Ty = Type::getDoubleTy(Ctx);
  Type *F32Ty = Type::getFloatTy(Ctx);
  Type *I32Ty = Type::getInt32Ty(Ctx);

  auto markVoterInternal = [&](Value *V) -> Value * {
    if (auto *I = dyn_cast<Instruction>(V)) {
      I->setMetadata("atft.voter.internal", MDNode::get(Ctx, {}));
    }
    return V;
  };

  Value *a1_f32 = markVoterInternal(IRB.CreateExtractElement(vApprox2xF32, ConstantInt::get(I32Ty, 0), "a1.f32"));
  Value *a2_f32 = markVoterInternal(IRB.CreateExtractElement(vApprox2xF32, ConstantInt::get(I32Ty, 1), "a2.f32"));
  // 通道本来就是 fp64 时（-atft-simd-f64：两份**精确**副本打进 <2 x double>）
  // 不需要回扩，fpext double->double 也不合法
  Value *a1_f64 = a1_f32->getType()->isDoubleTy()
                      ? a1_f32
                      : markVoterInternal(IRB.CreateFPExt(a1_f32, F64Ty, "a1.f64"));
  Value *a2_f64 = a2_f32->getType()->isDoubleTy()
                      ? a2_f32
                      : markVoterInternal(IRB.CreateFPExt(a2_f32, F64Ty, "a2.f64"));

  Function *FabsF64 = Intrinsic::getDeclaration(M, Intrinsic::fabs, {F64Ty});
  Function *FabsF32 = Intrinsic::getDeclaration(M, Intrinsic::fabs, {F32Ty});

  Value *absA1 = markVoterInternal(IRB.CreateCall(FabsF64, {a1_f64}));
  Value *scale = markVoterInternal(IRB.CreateFAdd(ConstantFP::get(F64Ty, 1.0), absA1));

  // D3 剖析：记录无故障执行下的 |R - A| / (1 + |A|) 包络，供离线定出逐负载阈值
  if (AtftDeltaProfile) {
    FunctionCallee ObserveFn = M->getOrInsertFunction(
        "atft_delta_profile_observe", Type::getVoidTy(Ctx), F64Ty, F64Ty);
    markVoterInternal(IRB.CreateCall(ObserveFn, {rExact, a1_f64}));
  }

  // D3 阈值来源：编译期常数 或 运行期逐负载值（全局 <= 0 时回落到常数）
  Value *deltaBase = ConstantFP::get(F64Ty, delta);
  if (AtftDeltaRuntime) {
    GlobalVariable *GDelta = cast<GlobalVariable>(
        M->getOrInsertGlobal("g_atft_delta_abs", F64Ty));
    Value *loaded = markVoterInternal(IRB.CreateLoad(F64Ty, GDelta, "delta.rt"));
    Value *valid = markVoterInternal(
        IRB.CreateFCmpOGT(loaded, ConstantFP::get(F64Ty, 0.0), "delta.rt.ok"));
    deltaBase = markVoterInternal(IRB.CreateSelect(valid, loaded, deltaBase, "delta.eff"));
  }
  Value *effDelta = markVoterInternal(IRB.CreateFMul(deltaBase, scale));

  Value *diffRA1 = markVoterInternal(IRB.CreateCall(FabsF64, {markVoterInternal(IRB.CreateFSub(rExact, a1_f64))}));
  Value *rIsOk = markVoterInternal(IRB.CreateFCmpOLE(diffRA1, effDelta, "r.ok"));

  // 两条近似副本是同一条 f32 链的复制，无故障时按位相同；AtftReplicaExact 下
  // 直接要求相等，不再给 1e-5 的无来源松弛量
  Value *a12Agree = nullptr;
  if (AtftReplicaExact || a1_f32->getType()->isDoubleTy()) {
    a12Agree = markVoterInternal(IRB.CreateFCmpOEQ(a1_f32, a2_f32, "a12.ok"));
  } else {
    Value *diffA12 = markVoterInternal(IRB.CreateCall(FabsF32, {markVoterInternal(IRB.CreateFSub(a1_f32, a2_f32))}));
    a12Agree = markVoterInternal(IRB.CreateFCmpOLE(diffA12, ConstantFP::get(F32Ty, 1e-5f), "a12.ok"));
  }

  Value *useApprox = markVoterInternal(IRB.CreateAnd(IRB.CreateNot(rIsOk), a12Agree));
  return markVoterInternal(IRB.CreateSelect(useApprox, a1_f64, rExact, "inline.approx.voted"));
}

// ============================================================================
// 模块 2E: wide-DMR —— 跨迭代向量化 + 高半冗余 + 半区比对
//   对规范单块单位步长数据并行 FP 循环(map/stencil)做 VF=4 向量化：
//   zmm<8 x double> 低 4 lane = 4 个真实迭代，高 4 lane = 同数据冗余副本(shuffle 广播造出
//   独立 lane，优化器不折叠)；每步低半^高半异或进 <4 x i64> 签名，出口比一次；标量余数跑尾巴。
// ============================================================================

// 剥掉 sext/zext/trunc 取底层值
static Value *atftPeelIntCast(Value *v) {
  while (auto *C = dyn_cast<CastInst>(v)) {
    if (isa<SExtInst>(C) || isa<ZExtInst>(C) || isa<TruncInst>(C))
      v = C->getOperand(0);
    else break;
  }
  return v;
}

// idx(可带 sext/add const) 是否 = iv + 常量偏移；是则填 off 返回 true
static bool atftMatchAffineIV(Value *idx, PHINode *iv, int64_t &off) {
  idx = atftPeelIntCast(idx);
  if (idx == iv) { off = 0; return true; }
  if (auto *BO = dyn_cast<BinaryOperator>(idx)) {
    Value *a = atftPeelIntCast(BO->getOperand(0));
    auto *c = dyn_cast<ConstantInt>(BO->getOperand(1));
    if (a == iv && c) {
      if (BO->getOpcode() == Instruction::Add) { off = c->getSExtValue(); return true; }
      if (BO->getOpcode() == Instruction::Sub) { off = -c->getSExtValue(); return true; }
    }
  }
  return false;
}

// 单位步长 double gep：返回 base 指针与相对 iv 的常量偏移
static bool atftMatchUnitStrideDoubleGEP(Value *ptr, PHINode *iv, Type *F64Ty,
                                         Value *&base, int64_t &off) {
  auto *gep = dyn_cast<GetElementPtrInst>(ptr);
  if (!gep || gep->getNumIndices() != 1) return false;
  if (gep->getSourceElementType() != F64Ty) return false;
  base = gep->getPointerOperand();
  return atftMatchAffineIV(gep->getOperand(1), iv, off);
}

// 沿 gep 指针链回溯到根指针(alloca/arg/global)，用于按底层数组做保守别名判断
static Value *atftRootPtr(Value *p) {
  while (auto *g = dyn_cast<GetElementPtrInst>(p)) p = g->getPointerOperand();
  return p;
}

// v 是否可外提到 preheader：在循环外=是；在循环内但为纯算术/gep/cast 且操作数全可外提=是。
// 禁 load/store/call/phi（有副作用或循环携带），保证克隆到 PH 后语义不变。
static bool atftIsHoistable(Value *v, Loop *L) {
  auto *in = dyn_cast<Instruction>(v);
  if (!in || !L->contains(in->getParent())) return true;   // 常量/参数/循环外定义
  if (isa<GetElementPtrInst>(in) || isa<CastInst>(in) || isa<BinaryOperator>(in)) {
    for (Use &u : in->operands())
      if (!atftIsHoistable(u.get(), L)) return false;
    return true;
  }
  return false;
}

// 把可外提的不变量表达式(递归)克隆进 PH，返回克隆后的值；带 cache 去重
static Value *atftMaterializeInPH(Value *v, Loop *L, IRBuilder<> &PB,
                                  std::map<Value *, Value *> &cache) {
  auto *in = dyn_cast<Instruction>(v);
  if (!in || !L->contains(in->getParent())) return v;      // 无需克隆
  auto it = cache.find(v);
  if (it != cache.end()) return it->second;
  Instruction *cl = in->clone();
  for (unsigned i = 0; i < cl->getNumOperands(); ++i)
    cl->setOperand(i, atftMaterializeInPH(in->getOperand(i), L, PB, cache));
  PB.Insert(cl);
  cache[v] = cl;
  return cl;
}

// 返回成功向量化的循环数。只转换严格匹配者，其余原样保留。
static void atftApplyHostISA(Function &F);       // 前置声明(定义在 atftRunO3 附近)
static int transformFunctionWideDMR(Function &F, LoopInfo &LI) {
  expandFMulAddIntrinsics(F);                    // fmuladd → fmul+fadd，便于匹配纯 FP DAG
  Module *M = F.getParent();
  LLVMContext &Ctx = M->getContext();
  Type *F64Ty = Type::getDoubleTy(Ctx);
  Type *I32Ty = Type::getInt32Ty(Ctx);
  Type *I64Ty = Type::getInt64Ty(Ctx);
  const unsigned VF = 4;                       // 低 4 真实 + 高 4 冗余 = zmm<8>
  VectorType *V4F64 = FixedVectorType::get(F64Ty, VF);
  VectorType *V8F64 = FixedVectorType::get(F64Ty, 2 * VF);
  VectorType *V4I64 = FixedVectorType::get(I64Ty, VF);
  FunctionCallee CommitFn = M->getOrInsertFunction(
      "atft_loop_vote_commit", Type::getVoidTy(Ctx), I32Ty);
  (void)V4F64; (void)V8F64; (void)V4I64; (void)CommitFn; (void)I64Ty;

  std::vector<Loop *> innermost;
  for (Loop *top : LI)
    for (Loop *L : depth_first(top))
      if (L->isInnermost()) innermost.push_back(L);

  int converted = 0;
  for (Loop *L : innermost) {
    if (L->getNumBlocks() != 2) continue;          // header + 独立 body/latch
    BasicBlock *H = L->getHeader();
    BasicBlock *PH = L->getLoopPreheader();
    BasicBlock *B = L->getLoopLatch();
    BasicBlock *Exit = L->getExitBlock();
    if (!PH || !Exit || !B || B == H) continue;
    auto *latchBr = dyn_cast<BranchInst>(B->getTerminator());
    if (!latchBr || !latchBr->isUnconditional() || latchBr->getSuccessor(0) != H) continue;

    // header 条件分支：cmp ? body(B) : Exit（顶测）
    auto *hbr = dyn_cast<BranchInst>(H->getTerminator());
    if (!hbr || !hbr->isConditional()) continue;
    auto *cmp = dyn_cast<ICmpInst>(hbr->getCondition());
    if (!cmp) continue;
    if (hbr->getSuccessor(0) != B || hbr->getSuccessor(1) != Exit) continue;

    // 规范 IV：header phi [start, PH] + [iv+1, B]
    PHINode *iv = nullptr;
    for (PHINode &PN : H->phis()) {
      if (!PN.getType()->isIntegerTy() || PN.getNumIncomingValues() != 2) continue;
      Value *fromPH = PN.getIncomingValueForBlock(PH);
      Value *fromLatch = PN.getIncomingValueForBlock(B);
      if (!fromPH || !fromLatch) continue;
      auto *add = dyn_cast<BinaryOperator>(fromLatch);
      if (!add || add->getOpcode() != Instruction::Add) continue;
      auto *one = dyn_cast<ConstantInt>(add->getOperand(1));
      if (add->getOperand(0) != &PN || !one || !one->isOne()) continue;
      if (!L->isLoopInvariant(fromPH)) continue;
      iv = &PN; break;
    }
    if (!iv) continue;
    Value *start = iv->getIncomingValueForBlock(PH);
    Value *ivNext = iv->getIncomingValueForBlock(B);

    // 退出比较在 iv（顶测），上界 bound 为不变量或可克隆到 PH 的不变量表达式
    if (cmp->getOperand(0) != iv) continue;
    Value *bound = cmp->getOperand(1);
    ICmpInst::Predicate P = cmp->getPredicate();
    bool incl;
    if (P == ICmpInst::ICMP_SLT || P == ICmpInst::ICMP_ULT) incl = false;
    else if (P == ICmpInst::ICMP_SLE || P == ICmpInst::ICMP_ULE) incl = true;
    else continue;

    // header 必须纯控制/寻址：只允许 iv phi、cmp、整数/指针不变量、终结符
    bool hdrOk = true;
    for (Instruction &I : *H) {
      if (&I == iv || &I == cmp || I.isTerminator()) continue;
      if (isa<LoadInst>(&I) || isa<StoreInst>(&I) || isa<CallInst>(&I) ||
          !I.getType()->isIntOrPtrTy()) { hdrOk = false; break; }
    }
    if (!hdrOk) continue;

    // === 识别 body(B) ===
    bool ok = true;
    std::vector<LoadInst *> loads;
    std::vector<StoreInst *> stores;
    std::map<Value *, std::pair<Value *, int64_t>> addrOf; // load/store -> (base,off)
    std::vector<Instruction *> fpDag;
    std::set<Value *> loadBases, storeBases;
    for (Instruction &I : *B) {
      if (I.isTerminator()) continue;
      if (auto *PN = dyn_cast<PHINode>(&I)) { if (PN != iv) { ok = false; break; } continue; }
      if (isa<GetElementPtrInst>(&I)) continue;               // 寻址
      if (auto *C = dyn_cast<CastInst>(&I)) {                  // 仅整数/指针寻址型 cast
        if (C->getType()->isIntOrPtrTy() && C->getSrcTy()->isIntOrPtrTy()) continue;
        ok = false; break;                                    // FP cast → bail
      }
      if (I.getType()->isIntegerTy() || I.getType()->isPointerTy()) continue; // 整数寻址运算
      if (auto *LD = dyn_cast<LoadInst>(&I)) {
        Value *base; int64_t off;
        if (!LD->getType()->isDoubleTy() ||
            !atftMatchUnitStrideDoubleGEP(LD->getPointerOperand(), iv, F64Ty, base, off) ||
            !atftIsHoistable(base, L)) { ok = false; break; }
        loads.push_back(LD); addrOf[LD] = {base, off}; loadBases.insert(atftRootPtr(base)); continue;
      }
      if (auto *SI = dyn_cast<StoreInst>(&I)) {
        Value *base; int64_t off;
        if (!SI->getValueOperand()->getType()->isDoubleTy() ||
            !atftMatchUnitStrideDoubleGEP(SI->getPointerOperand(), iv, F64Ty, base, off) ||
            !atftIsHoistable(base, L)) { ok = false; break; }
        stores.push_back(SI); addrOf[SI] = {base, off}; storeBases.insert(atftRootPtr(base)); continue;
      }
      unsigned op = I.getOpcode();
      if (I.getType()->isDoubleTy() &&
          (op == Instruction::FAdd || op == Instruction::FSub ||
           op == Instruction::FMul || op == Instruction::FDiv ||
           op == Instruction::FNeg)) { fpDag.push_back(&I); continue; }
      ok = false; break;                                      // call/其它 → bail
    }
    if (!ok || stores.empty() || fpDag.empty()) continue;
    bool alias = false;
    for (Value *sb : storeBases) if (loadBases.count(sb)) alias = true;
    if (alias) continue;                                      // 读写别名 → bail
    auto inVec = [](std::vector<Instruction *> &v, Instruction *x) {
      return std::find(v.begin(), v.end(), x) != v.end();
    };
    auto okOperand = [&](Value *v) -> bool {
      if (isa<Constant>(v)) return true;
      if (auto *in = dyn_cast<Instruction>(v)) {
        if (L->contains(in->getParent()))
          return inVec(fpDag, in) ||
                 std::find(loads.begin(), loads.end(), in) != loads.end();
        return true;                                          // 循环外 = 不变量
      }
      return true;                                            // 参数/全局 = 不变量
    };
    for (Instruction *d : fpDag) { for (Use &u : d->operands()) if (!okOperand(u.get())) { ok = false; break; } if (!ok) break; }
    if (!ok) continue;
    for (StoreInst *SI : stores) if (!okOperand(SI->getValueOperand())) { ok = false; break; }
    if (!ok) continue;

    // === 发射：向量预导 [start,Vn) + 标量余数循环 [Vn,Nx) ===
    Type *IvTy = iv->getType();
    Function *Fn = H->getParent();
    Instruction *phTerm = PH->getTerminator();
    IRBuilder<> PB(phTerm);
    std::map<Value *, Value *> hoistCache;         // 外提克隆去重(bound 与各 base 共用)
    Value *boundPH = atftMaterializeInPH(bound, L, PB, hoistCache);  // 物化上界到 PH
    std::map<Value *, Value *> baseHoist;          // 原 base → PH 里的外提副本
    for (auto &kv : addrOf)
      baseHoist[kv.second.first] =
          atftMaterializeInPH(kv.second.first, L, PB, hoistCache);
    Value *Nx = incl ? PB.CreateAdd(boundPH, ConstantInt::get(IvTy, 1)) : boundPH;
    Value *tc = PB.CreateSub(Nx, start);
    Value *tcv = PB.CreateAnd(tc, ConstantInt::get(IvTy, ~(int64_t)(VF - 1)));
    Value *Vn = PB.CreateAdd(start, tcv);
    Value *needVec = PB.CreateICmpSGT(tcv, ConstantInt::get(IvTy, 0));
    Value *remStart = PB.CreateSelect(needVec, Vn, start);

    BasicBlock *vph = BasicBlock::Create(Ctx, "wdmr.vph", Fn, H);
    BasicBlock *vbody = BasicBlock::Create(Ctx, "wdmr.vbody", Fn, H);
    BasicBlock *vexit = BasicBlock::Create(Ctx, "wdmr.vexit", Fn, H);
    BasicBlock *premerge = BasicBlock::Create(Ctx, "wdmr.premerge", Fn, H);
    PB.CreateCondBr(needVec, vph, premerge);
    phTerm->eraseFromParent();
    IRBuilder<>(vph).CreateBr(vbody);
    IRBuilder<>(premerge).CreateBr(H);
    int phiIdx = iv->getBasicBlockIndex(PH);        // header iv phi: PH 入口 → premerge(remStart)
    iv->setIncomingBlock(phiIdx, premerge);
    iv->setIncomingValue(phiIdx, remStart);

    // 向量体：低 4 lane 真实 4 迭代、高 4 lane 冗余副本，异或进 <4 x i64> 签名
    IRBuilder<> VB(vbody);
    PHINode *ivV = VB.CreatePHI(IvTy, 2, "wdmr.ivV");
    PHINode *sig = VB.CreatePHI(V4I64, 2, "wdmr.sig");
    ivV->addIncoming(start, vph);
    sig->addIncoming(ConstantAggregateZero::get(V4I64), vph);
    Value *idx64 = VB.CreateIntCast(ivV, I64Ty, true);
    std::map<Value *, Value *> wide;
    auto wideOf = [&](Value *v) -> Value * {
      auto it = wide.find(v);
      if (it != wide.end()) return it->second;
      return VB.CreateVectorSplat(2 * VF, v);       // 不变量/常量 → 广播满 8 lane
    };
    auto gepAt = [&](Value *base, int64_t off) -> Value * {
      Value *ix = off ? VB.CreateAdd(idx64, ConstantInt::get(I64Ty, off)) : idx64;
      return VB.CreateGEP(F64Ty, baseHoist[base], ix);     // 用 PH 外提后的基址
    };
    // 冗余 lane 必须对优化器不透明，否则 InstCombine 发现高半==低半会折叠掉冗余(=假保护)。
    // 低半从原指针 load，高半从"经零开销指针屏障(=r,0，标量寄存器可分配)洗过的同地址指针"
    // 再 load 一次——两条 load 链 SSA 不同，GVN 不会合并，拼成 <8> 后高/低半不可证等价。
    // codegen 仍选 zmm；load 数据故障属共模(与 -atft-simd-bcast 同口径)，检错针对计算通路。
    Type *PtrTy = loads.empty() ? stores[0]->getPointerOperand()->getType()
                                : loads[0]->getPointerOperand()->getType();
    InlineAsm *pbar = InlineAsm::get(
        FunctionType::get(PtrTy, {PtrTy}, false), "", "=r,0", /*hasSideEffects=*/true);
    for (LoadInst *LD : loads) {
      Value *p = gepAt(addrOf[LD].first, addrOf[LD].second);
      Value *lo = VB.CreateAlignedLoad(V4F64, p, Align(8));
      Value *pRed = VB.CreateCall(pbar, {p});
      Value *hi = VB.CreateAlignedLoad(V4F64, pRed, Align(8));
      wide[LD] = VB.CreateShuffleVector(lo, hi,
                    ArrayRef<int>({0, 1, 2, 3, 4, 5, 6, 7}));   // [低4 真实 | 高4 冗余]
    }
    for (Instruction *d : fpDag) {
      Value *w;
      if (d->getOpcode() == Instruction::FNeg)
        w = VB.CreateFNeg(wideOf(d->getOperand(0)));
      else
        w = VB.CreateBinOp((Instruction::BinaryOps)d->getOpcode(),
                           wideOf(d->getOperand(0)), wideOf(d->getOperand(1)));
      wide[d] = w;
    }
    Value *sigCur = sig;
    for (StoreInst *SI : stores) {
      Value *wv = wideOf(SI->getValueOperand());
      Value *lo = VB.CreateShuffleVector(wv, PoisonValue::get(V8F64), ArrayRef<int>({0, 1, 2, 3}));
      Value *hi = VB.CreateShuffleVector(wv, PoisonValue::get(V8F64), ArrayRef<int>({4, 5, 6, 7}));
      VB.CreateAlignedStore(lo, gepAt(addrOf[SI].first, addrOf[SI].second), Align(8));
      sigCur = VB.CreateXor(VB.CreateXor(sigCur, VB.CreateBitCast(lo, V4I64)),
                            VB.CreateBitCast(hi, V4I64));
    }
    Value *ivVn = VB.CreateAdd(ivV, ConstantInt::get(IvTy, VF));
    VB.CreateCondBr(VB.CreateICmpSLT(ivVn, Vn), vbody, vexit);
    ivV->addIncoming(ivVn, vbody);
    sig->addIncoming(sigCur, vbody);

    IRBuilder<> XB(vexit);                           // 出口：<4 x i64> 异或归约 + 提交
    Value *s = XB.CreateExtractElement(sigCur, (uint64_t)0);
    for (unsigned k = 1; k < VF; k++)
      s = XB.CreateXor(s, XB.CreateExtractElement(sigCur, (uint64_t)k));
    XB.CreateCall(CommitFn, {XB.CreateZExt(XB.CreateICmpEQ(s, ConstantInt::get(I64Ty, 0)), I32Ty)});
    XB.CreateBr(premerge);
    converted++;
  }
  if (converted) atftApplyHostISA(F);            // 放开向量宽度到 host，让 codegen 选最宽
  return converted;
}

// FP(标量或向量)判定
static bool atftIsFP(Value *v) {
  Type *t = v->getType();
  if (auto *vt = dyn_cast<VectorType>(t)) t = vt->getElementType();
  return t->isFloatingPointTy();
}
static bool atftIsFPArith(Instruction *I) {
  unsigned op = I->getOpcode();
  return atftIsFP(I) &&
         (op == Instruction::FAdd || op == Instruction::FSub ||
          op == Instruction::FMul || op == Instruction::FDiv ||
          op == Instruction::FNeg);
}
// 纯 FP 数学调用(sqrt/exp/sin/fma/fabs… 的 intrinsic 或任何 readnone 返回 FP 的调用)：
// 无内存副作用→复制安全。覆盖它们才能堵住 cholesky/blackscholes 这类的超越函数共模洞。
static bool atftIsPureFPCall(Instruction *I) {
  auto *CI = dyn_cast<CallInst>(I);
  if (!CI || CI->isInlineAsm() || !atftIsFP(CI)) return false;
  return CI->doesNotAccessMemory();            // intrinsic(llvm.sqrt…) 与 readnone libm 都满足
}

// 从 host 探测 CPU 特征串(通用、可移植；不写死 x86/avx512 具体型号)
static std::string atftHostFeatures() {
  StringMap<bool> f;
  std::string s;
  if (sys::getHostCPUFeatures(f))
    for (auto &e : f)
      if (e.second) { if (!s.empty()) s += ","; s += "+"; s += e.first().str(); }
  return s;
}
// 把目标函数的 ISA 对齐到 host(target-cpu/features)，并在有宽 SIMD 时放开向量宽度。
// 全程从 host 推导——换机器/换架构自动适配，无需改代码。
static void atftApplyHostISA(Function &F) {
  F.addFnAttr("target-cpu", std::string(sys::getHostCPUName()));
  std::string feats = atftHostFeatures();
  if (!feats.empty()) F.addFnAttr("target-features", feats);
  StringMap<bool> f;
  if (sys::getHostCPUFeatures(f)) {
    unsigned w = f.lookup("avx512f") ? 512 : f.lookup("avx") ? 256 : f.lookup("sse2") ? 128 : 0;
    if (w) {
      F.addFnAttr("min-legal-vector-width", std::to_string(w));
      F.addFnAttr("prefer-vector-width", std::to_string(w));
    }
  }
}

// 模块内跑一遍 default<O3>（复用 LLVM 通用向量化器等）。TargetMachine 必须带 host 特征，
// 否则 TTI 退化为 baseline、向量化器拿不到宽 SIMD 的 cost model 就不会宽向量化。
static void atftRunO3(Module &M) {
  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();
  std::string triple = M.getTargetTriple();
  if (triple.empty()) triple = sys::getDefaultTargetTriple();
  std::string err;
  const Target *T = TargetRegistry::lookupTarget(triple, err);
  std::unique_ptr<TargetMachine> TM;
  if (T) {
    TargetOptions opts;
    TM.reset(T->createTargetMachine(
        triple, std::string(sys::getHostCPUName()), atftHostFeatures(),
        opts, Optional<Reloc::Model>(Reloc::PIC_)));
  }
  PassBuilder PB(TM.get());
  LoopAnalysisManager LAM;
  FunctionAnalysisManager FAM;
  CGSCCAnalysisManager CGAM;
  ModuleAnalysisManager MAM;
  PB.registerModuleAnalyses(MAM);
  PB.registerCGSCCAnalyses(CGAM);
  PB.registerFunctionAnalyses(FAM);
  PB.registerLoopAnalyses(LAM);
  PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);
  ModulePassManager MPM = PB.buildPerModuleDefaultPipeline(OptimizationLevel::O3);
  MPM.run(M, MAM);
}

// 通用冗余注入：对 F 全部 FP 算术/加载/phi 造 (ways-1) 条独立影子链(不认循环形状)。
// ways=2 → DMR(检错，XOR 签名返回点提交)；ways=3 → TMR(纠错，每 FP 存储/返回写 median-of-3)。
// 前提：F 已向量化(调用方先跑 O3)。返回注入的影子算术条数。
static int transformFunctionDMRGeneric(Function &F, int ways) {
  Module *M = F.getParent();
  LLVMContext &Ctx = M->getContext();
  Type *I64Ty = Type::getInt64Ty(Ctx);
  Type *I32Ty = Type::getInt32Ty(Ctx);
  FunctionCallee CommitFn = M->getOrInsertFunction(
      "atft_loop_vote_commit", Type::getVoidTy(Ctx), I32Ty);

  std::vector<LoadInst *> fpLoads;
  std::vector<PHINode *> fpPhis;
  std::vector<Instruction *> fpArith;
  std::vector<StoreInst *> fpStores;
  for (BasicBlock &BB : F)
    for (Instruction &I : BB) {
      if (auto *LD = dyn_cast<LoadInst>(&I)) { if (atftIsFP(LD)) fpLoads.push_back(LD); }
      else if (auto *PN = dyn_cast<PHINode>(&I)) { if (atftIsFP(PN)) fpPhis.push_back(PN); }
      else if (auto *SI = dyn_cast<StoreInst>(&I)) { if (atftIsFP(SI->getValueOperand())) fpStores.push_back(SI); }
      else if (atftIsFPArith(&I)) fpArith.push_back(&I);
      else if (atftIsPureFPCall(&I)) fpArith.push_back(&I);   // sqrt/exp/… 也造影子
    }
  if (fpArith.empty()) return 0;

  Type *PtrTy = fpLoads.empty() ? nullptr : fpLoads[0]->getPointerOperand()->getType();
  // hasSideEffects=true：否则纯 inline-asm 恒等会被 GVN 当可 CSE 合并两条 launder→影子折叠(假冗余)。
  InlineAsm *pbar = PtrTy ? InlineAsm::get(FunctionType::get(PtrTy, {PtrTy}, false),
                                           "", "=r,0", /*hasSideEffects=*/true)
                          : nullptr;
  int nShadow = ways - 1;
  std::vector<DenseMap<Value *, Value *>> sh(nShadow);
  for (int c = 0; c < nShadow; ++c) {
    DenseMap<Value *, Value *> &S = sh[c];
    for (LoadInst *LD : fpLoads) {                 // 每条影子各自独立 launder 二次 load
      IRBuilder<> B(LD->getNextNode());
      Value *ps = B.CreateCall(pbar, {LD->getPointerOperand()});
      S[LD] = B.CreateAlignedLoad(LD->getType(), ps, LD->getAlign(), "atft.dmr.ld");
    }
    for (PHINode *PN : fpPhis) {
      IRBuilder<> B(&*PN->getParent()->getFirstInsertionPt());
      S[PN] = B.CreatePHI(PN->getType(), PN->getNumIncomingValues(), "atft.dmr.phi");
    }
    auto so = [&](Value *v) -> Value * {
      auto it = S.find(v);
      return it != S.end() ? it->second : v;
    };
    for (Instruction *A : fpArith) {
      IRBuilder<> B(A->getNextNode());
      Value *s;
      if (auto *CI = dyn_cast<CallInst>(A)) {        // 纯 FP 调用：克隆并重映射实参
        auto *cl = cast<CallInst>(CI->clone());
        for (unsigned k = 0; k < cl->arg_size(); ++k)
          cl->setArgOperand(k, so(CI->getArgOperand(k)));
        B.Insert(cl);
        s = cl;
      } else if (A->getOpcode() == Instruction::FNeg) {
        s = B.CreateFNeg(so(A->getOperand(0)));
      } else {
        s = B.CreateBinOp((Instruction::BinaryOps)A->getOpcode(),
                          so(A->getOperand(0)), so(A->getOperand(1)));
      }
      S[A] = s;
    }
    for (PHINode *PN : fpPhis) {
      auto *ps = cast<PHINode>(S[PN]);
      for (unsigned i = 0; i < PN->getNumIncomingValues(); ++i)
        ps->addIncoming(so(PN->getIncomingValue(i)), PN->getIncomingBlock(i));
    }
  }
  int injected = (int)fpArith.size() * nShadow;

  // median-of-3（标量或向量 FP）：无故障三者相等→原值；单故障→取多数(中位=一致那对)
  auto med3 = [&](IRBuilder<> &B, Value *a, Value *b, Value *cc) -> Value * {
    Value *mnab = B.CreateBinaryIntrinsic(Intrinsic::minnum, a, b);
    Value *mxab = B.CreateBinaryIntrinsic(Intrinsic::maxnum, a, b);
    Value *inner = B.CreateBinaryIntrinsic(Intrinsic::minnum, mxab, cc);
    return B.CreateBinaryIntrinsic(Intrinsic::maxnum, mnab, inner);
  };

  if (ways == 3) {                                 // TMR：投票写回(纠错)
    for (StoreInst *SI : fpStores) {
      Value *v = SI->getValueOperand();
      if (sh[0].count(v) && sh[1].count(v)) {
        IRBuilder<> B(SI);
        SI->setOperand(0, med3(B, v, sh[0][v], sh[1][v]));
      }
    }
    for (BasicBlock &BB : F) {
      auto *RI = dyn_cast<ReturnInst>(BB.getTerminator());
      if (!RI || RI->getNumOperands() != 1) continue;
      Value *v = RI->getOperand(0);
      if (atftIsFP(v) && sh[0].count(v) && sh[1].count(v)) {
        IRBuilder<> B(RI);
        RI->setOperand(0, med3(B, v, sh[0][v], sh[1][v]));
      }
    }
    return injected;
  }

  // ways==2：DMR 检错，XOR 签名
  DenseMap<Value *, Value *> &shadow = sh[0];
  IRBuilder<> EB(&*F.getEntryBlock().getFirstInsertionPt());
  AllocaInst *sig = EB.CreateAlloca(I64Ty, nullptr, "atft.dmr.sig");
  EB.CreateStore(ConstantInt::get(I64Ty, 0), sig);
  auto foldDiff = [&](IRBuilder<> &B, Value *a, Value *b) {
    Type *t = a->getType();
    unsigned bits = t->getScalarSizeInBits();
    Value *x;
    if (auto *vt = dyn_cast<FixedVectorType>(t)) {
      unsigned n = vt->getNumElements();
      Type *vit = FixedVectorType::get(IntegerType::get(Ctx, bits), n);
      x = B.CreateXor(B.CreateBitCast(a, vit), B.CreateBitCast(b, vit));
      if (bits != 64) x = B.CreateZExtOrTrunc(x, FixedVectorType::get(I64Ty, n));
      x = B.CreateIntrinsic(Intrinsic::vector_reduce_xor,
                            {FixedVectorType::get(I64Ty, n)}, {x});
    } else {
      x = B.CreateXor(B.CreateBitCast(a, IntegerType::get(Ctx, bits)),
                      B.CreateBitCast(b, IntegerType::get(Ctx, bits)));
      if (bits != 64) x = B.CreateZExtOrTrunc(x, I64Ty);
    }
    B.CreateStore(B.CreateXor(B.CreateLoad(I64Ty, sig), x), sig);
  };
  for (StoreInst *SI : fpStores) {
    Value *v = SI->getValueOperand();
    if (shadow.count(v)) { IRBuilder<> B(SI); foldDiff(B, v, shadow[v]); }
  }
  for (BasicBlock &BB : F) {
    auto *RI = dyn_cast<ReturnInst>(BB.getTerminator());
    if (!RI) continue;
    IRBuilder<> B(RI);
    if (RI->getNumOperands() == 1 && atftIsFP(RI->getOperand(0)) &&
        shadow.count(RI->getOperand(0)))
      foldDiff(B, RI->getOperand(0), shadow[RI->getOperand(0)]);
    Value *ok = B.CreateICmpEQ(B.CreateLoad(I64Ty, sig), ConstantInt::get(I64Ty, 0));
    B.CreateCall(CommitFn, {B.CreateZExt(ok, I32Ty)});
  }
  return injected;
}

// 注意：本函数是 approx-simd / exact-SIMD / adaptive 三者共用的实现。
// 默认(AtftSimdF64=false)走 f32+δ 近似副本 —— 这是历史遗留的"近似计算容错"分支，
// 已退役为默认基线（见 config/default.yaml 与 -atft-mode 的 DEPRECATED 说明）。
// 精确冗余请走 AtftSimdF64=true（-atft-simd-f64，2×f64 精确 SIMD 打包）。
// 代码不删：exact-SIMD 与 adaptive 模式都复用这条路径，仅按 flag 切精确/近似。
void transformFunctionApproxTMR(Function &F, LoopInfo &LI, CritLevel critLevel,
                                double delta, bool useSIMD,
                                const std::string &rankMethod, double budget,
                                bool pureApproxOutput = false) {
  expandFMulAddIntrinsics(F);
  auto instScores = computeInstructionCriticalityMap(F, LI, rankMethod);

  Module *M = F.getParent();
  LLVMContext &Ctx = M->getContext();
  Type *F64Ty = Type::getDoubleTy(Ctx);
  Type *F32Ty = Type::getFloatTy(Ctx);
  Type *I32Ty = Type::getInt32Ty(Ctx);
  VectorType *V2F32Ty = FixedVectorType::get(F32Ty, 2);
  VectorType *V2F64Ty = FixedVectorType::get(F64Ty, 2);
  VectorType *ReplicaVecTy = (AtftSimdF64 && useSIMD) ? V2F64Ty : V2F32Ty;

  FunctionCallee VoterApproxFn = M->getOrInsertFunction(
      "atft_voter_approx_f64", F64Ty, F64Ty, F64Ty, F64Ty, F64Ty, I32Ty);
  FunctionCallee VoterApproxReexecFn = M->getOrInsertFunction(
      "atft_voter_approx_reexec_f64", F64Ty, F64Ty, F64Ty, F64Ty, F64Ty, F64Ty, I32Ty);

  FunctionCallee ApproxSinFn = M->getOrInsertFunction("atft_approx_sinf", F32Ty, F32Ty);
  FunctionCallee ApproxCosFn = M->getOrInsertFunction("atft_approx_cosf", F32Ty, F32Ty);
  FunctionCallee ApproxExpFn = M->getOrInsertFunction("atft_approx_expf", F32Ty, F32Ty);

  // 循环级检错(-atft-loop-vote)：精确 simd-f64 路径走 Elzar 式向量内冗余——两条副本就在同一
  // <2 x f64> 向量的两 lane 里，对向量整体按位 XOR 累一个 <2 x i64> 签名(每 store 仅 1 条向量
  // XOR，无 extract)，返回前比两 lane 一次；store 写 lane0，标量 val 链随之死代码消除。非精确路径
  // 回落到 i1 ok 容差累加。alloca 交 -O3 mem2reg 升 loop-carried PHI，省手工 PHI 与每 store 分支。
  Type *I1Ty = Type::getInt1Ty(Ctx);
  Type *I64Ty = Type::getInt64Ty(Ctx);
  VectorType *V2I64Ty = FixedVectorType::get(I64Ty, 2);
  FunctionCallee LoopVoteCommitFn = M->getOrInsertFunction(
      "atft_loop_vote_commit", Type::getVoidTy(Ctx), I32Ty);
  AllocaInst *okFlag = nullptr, *sigVec = nullptr;
  bool lvSig = AtftLoopVote && AtftSimdF64 && useSIMD && !pureApproxOutput;
  bool emittedLoopVote = false;
  if (AtftLoopVote && !pureApproxOutput) {
    IRBuilder<> EB(&*F.getEntryBlock().getFirstInsertionPt());
    if (lvSig) {
      sigVec = EB.CreateAlloca(V2I64Ty, nullptr, "atft.sigVec");
      EB.CreateStore(ConstantAggregateZero::get(V2I64Ty), sigVec);
    } else {
      okFlag = EB.CreateAlloca(I1Ty, nullptr, "atft.ok.flag");
      EB.CreateStore(ConstantInt::getTrue(Ctx), okFlag);
    }
  }



  std::vector<PHINode *> phiNodes;
  std::vector<LoadInst *> loadInsts;
  std::vector<Instruction *> computeOrdered;
  std::vector<StoreInst *> storeInsts;

  for (BasicBlock &BB : F) {
    for (Instruction &I : BB) {
      if (auto *PN = dyn_cast<PHINode>(&I)) {
        if (PN->getType()->isDoubleTy()) phiNodes.push_back(PN);
      } else if (auto *LD = dyn_cast<LoadInst>(&I)) {
        if (LD->getType()->isDoubleTy()) loadInsts.push_back(LD);
      } else if (isTargetFP64Op(&I) || isTargetMathCall(&I)) {
        computeOrdered.push_back(&I);
      } else if (auto *SI = dyn_cast<StoreInst>(&I)) {
        if (SI->getValueOperand()->getType()->isDoubleTy()) storeInsts.push_back(SI);
      }
    }
  }

  std::set<Instruction *> selectedSet = selectInstructionsByBudget(computeOrdered, instScores, budget);
  if (selectedSet.empty()) {
    // 该函数没有可近似的 FP64 计算（典型如纯整数内核 floyd-warshall / nussinov /
    // crc16 / quicksort）：近似变换不适用，直接走整数数据通路的精确 TMR
    if (!pureApproxOutput) {
      transformFunctionInt32ToInt16Compression(F, critLevel, useSIMD);
      transformFunctionExactIntTMR(F, LI, rankMethod, budget);
    }
    return;
  }

  std::map<Value *, Value *> simdApproxMap;

  auto markInternal = [&](Value *V) -> Value * {
    if (auto *I = dyn_cast<Instruction>(V)) {
      I->setMetadata("atft.voter.internal", MDNode::get(Ctx, {}));
    }
    return V;
  };

  auto getOrCreateApproxVec = [&](Value *v, IRBuilder<> &IRB) -> Value * {
    auto it = simdApproxMap.find(v);
    if (it != simdApproxMap.end()) {
      return it->second;
    }
    if (auto *CFP = dyn_cast<ConstantFP>(v)) {
      if (AtftSimdF64 && useSIMD) {
        Constant *cF64 = ConstantFP::get(F64Ty, CFP->getValueAPF().convertToDouble());
        return ConstantVector::get({cF64, cF64});
      }
      float fval = (float)CFP->getValueAPF().convertToDouble();
      Constant *cF32 = ConstantFP::get(F32Ty, fval);
      return ConstantVector::get({cF32, cF32});
    }
    Value *lane = (AtftSimdF64 && useSIMD)
                      ? v
                      : markInternal(IRB.CreateFPTrunc(v, F32Ty, "d2f"));
    Value *undefVec = UndefValue::get(ReplicaVecTy);
    Value *vec0 = markInternal(IRB.CreateInsertElement(undefVec, lane, ConstantInt::get(I32Ty, 0)));
    Value *vec1 = markInternal(IRB.CreateInsertElement(
        vec0, lane, ConstantInt::get(I32Ty, 1),
        (AtftSimdF64 && useSIMD) ? "approx.v2f64" : "approx.v2f32"));
    return vec1;
  };

  // 复制被选中算术链直接依赖的 Load 指令，使得 Load 寄存器故障在部分保护预算下同样被三路隔离
  for (LoadInst *LD : loadInsts) {
    bool usedBySelected = (budget >= 0.999);
    if (!usedBySelected) {
      for (User *U : LD->users()) {
        if (auto *uI = dyn_cast<Instruction>(U)) {
          if (selectedSet.count(uI)) {
            usedBySelected = true;
            break;
          }
        }
      }
    }
    if (usedBySelected) {
      IRBuilder<> IRB(LD->getNextNode());
      // 计量旋钮：单次 Load + broadcast 填满两条 f64 车道，省掉第二次 Load 与两个指针屏障。
      // 两条车道共享同一个 Load 值 => Load/访存故障共模、检不出；寄存器级故障仍逐车道可检。
      if (AtftSimdBcast && AtftSimdF64 && useSIMD) {
        Value *uVec = UndefValue::get(ReplicaVecTy);
        Value *v0 = markInternal(IRB.CreateInsertElement(uVec, LD, ConstantInt::get(I32Ty, 0)));
        Value *v1 = markInternal(IRB.CreateInsertElement(
            v0, LD, ConstantInt::get(I32Ty, 1), "ld.bcast.v2f64"));
        simdApproxMap[LD] = v1;
        continue;
      }
      Value *ptr = LD->getPointerOperand();
      InlineAsm *ptrBar = InlineAsm::get(
          FunctionType::get(ptr->getType(), {ptr->getType()}, false), "", "=r,0", true);
      Value *p1 = IRB.CreateCall(ptrBar, {ptr});
      Value *p2 = IRB.CreateCall(ptrBar, {ptr});
      LoadInst *ld1 = IRB.CreateLoad(F64Ty, p1, LD->getName() + ".ld.a1");
      LoadInst *ld2 = IRB.CreateLoad(F64Ty, p2, LD->getName() + ".ld.a2");
      markReplica(ld1);
      markReplica(ld2);
      Value *f1 = (AtftSimdF64 && useSIMD)
                      ? static_cast<Value *>(ld1)
                      : markInternal(IRB.CreateFPTrunc(ld1, F32Ty, "ld1.f32"));
      Value *f2 = (AtftSimdF64 && useSIMD)
                      ? static_cast<Value *>(ld2)
                      : markInternal(IRB.CreateFPTrunc(ld2, F32Ty, "ld2.f32"));
      Value *uVec = UndefValue::get(ReplicaVecTy);
      Value *v0 = markInternal(IRB.CreateInsertElement(uVec, f1, ConstantInt::get(I32Ty, 0)));
      Value *v1 = markInternal(IRB.CreateInsertElement(
          v0, f2, ConstantInt::get(I32Ty, 1),
          (AtftSimdF64 && useSIMD) ? "ld.v2f64" : "ld.v2f32"));
      simdApproxMap[LD] = v1;
    }
  }

  for (PHINode *PN : phiNodes) {
    IRBuilder<> IRB(PN);
    PHINode *vPhi = IRB.CreatePHI(
        ReplicaVecTy, PN->getNumIncomingValues(),
        PN->getName() + ((AtftSimdF64 && useSIMD) ? ".v2f64_phi" : ".v2f32_phi"));
    simdApproxMap[PN] = vPhi;
  }

  for (Instruction *inst : computeOrdered) {
    if (!selectedSet.count(inst)) continue;
    IRBuilder<> IRB(inst->getNextNode());

    bool isProtectionBoundary = false;
    for (User *U : inst->users()) {
      if (auto *userI = dyn_cast<Instruction>(U)) {
        if ((isTargetFP64Op(userI) || isTargetMathCall(userI)) && !selectedSet.count(userI)) {
          isProtectionBoundary = true;
          break;
        }
      }
    }

    if (auto *CI = dyn_cast<CallInst>(inst)) {
      Value *arg0 = CI->getArgOperand(0);
      Value *vArg = getOrCreateApproxVec(arg0, IRB);
      Value *arg1 = markInternal(IRB.CreateExtractElement(
          vArg, ConstantInt::get(I32Ty, 0), "math.a1.arg"));
      Value *arg2 = markInternal(IRB.CreateExtractElement(
          vArg, ConstantInt::get(I32Ty, 1), "math.a2.arg"));
      Value *res1 = nullptr;
      Value *res2 = nullptr;
      if (AtftSimdF64 && useSIMD) {
        CallInst *c1 = cast<CallInst>(CI->clone());
        c1->setArgOperand(0, arg1);
        IRB.Insert(c1, "exact.simd.call1");
        markReplica(c1);
        CallInst *c2 = cast<CallInst>(CI->clone());
        c2->setArgOperand(0, arg2);
        IRB.Insert(c2, "exact.simd.call2");
        markReplica(c2);
        res1 = c1;
        res2 = c2;
      } else {
        Value *argF32_1 = arg1;
        Value *argF32_2 = arg2;
        StringRef cname = CI->getCalledFunction()->getName();
        if (cname.contains("sin")) {
          res1 = IRB.CreateCall(ApproxSinFn, {argF32_1}, "approx.sin1");
          res2 = IRB.CreateCall(ApproxSinFn, {argF32_2}, "approx.sin2");
        } else if (cname.contains("cos")) {
          res1 = IRB.CreateCall(ApproxCosFn, {argF32_1}, "approx.cos1");
          res2 = IRB.CreateCall(ApproxCosFn, {argF32_2}, "approx.cos2");
        } else if (cname.contains("exp")) {
          res1 = IRB.CreateCall(ApproxExpFn, {argF32_1}, "approx.exp1");
          res2 = IRB.CreateCall(ApproxExpFn, {argF32_2}, "approx.exp2");
        } else if (cname.contains("log")) {
          FunctionCallee LogF32 = M->getOrInsertFunction("logf", F32Ty, F32Ty);
          res1 = IRB.CreateCall(LogF32, {argF32_1}, "approx.log1");
          res2 = IRB.CreateCall(LogF32, {argF32_2}, "approx.log2");
        } else {
          FunctionCallee SqrtF32 = M->getOrInsertFunction("sqrtf", F32Ty, F32Ty);
          res1 = IRB.CreateCall(SqrtF32, {argF32_1}, "approx.sqrt1");
          res2 = IRB.CreateCall(SqrtF32, {argF32_2}, "approx.sqrt2");
        }
      }

      Value *undefVec = UndefValue::get(ReplicaVecTy);
      Value *vec0 = markInternal(IRB.CreateInsertElement(
          undefVec, res1, ConstantInt::get(I32Ty, 0)));
      Value *vec1 = markInternal(IRB.CreateInsertElement(
          vec0, res2, ConstantInt::get(I32Ty, 1)));
      if (isProtectionBoundary && !pureApproxOutput && budget < 0.999) {
        Value *boundaryVoted = createInlineFastApproxVote(IRB, M, CI, vec1, delta * 2.0);
        CI->replaceUsesWithIf(boundaryVoted, [&](Use &U) {
          Instruction *userI = dyn_cast<Instruction>(U.getUser());
          return userI == nullptr || userI->getMetadata("atft.voter.internal") == nullptr;
        });
        simdApproxMap[boundaryVoted] = vec1;
      } else if (!pureApproxOutput && budget >= 0.999) {
        Value *votedMath = createInlineFastApproxVote(IRB, M, CI, vec1, delta * 2.0);
        CI->replaceUsesWithIf(votedMath, [&](Use &U) {
          Instruction *userI = dyn_cast<Instruction>(U.getUser());
          return userI == nullptr || userI->getMetadata("atft.voter.internal") == nullptr;
        });
        simdApproxMap[votedMath] = vec1;
      }
      simdApproxMap[CI] = vec1;
      continue;
    }

    Value *vRes = nullptr;
    if (inst->getOpcode() == Instruction::FNeg) {
      Value *op0 = inst->getOperand(0);
      Value *vOp0 = getOrCreateApproxVec(op0, IRB);
      vRes = IRB.CreateFNeg(vOp0, inst->getName() + ".simd_fneg");
    } else {
      Value *op0 = inst->getOperand(0);
      Value *op1 = inst->getOperand(1);
      Value *vOp0 = getOrCreateApproxVec(op0, IRB);
      Value *vOp1 = getOrCreateApproxVec(op1, IRB);

      auto binOp = cast<BinaryOperator>(inst)->getOpcode();
      if (useSIMD) {
        vRes = IRB.CreateBinOp(binOp, vOp0, vOp1, inst->getName() + ".simd_approx");
        markReplica(vRes);
      } else {
        Value *a1_0 = markInternal(IRB.CreateExtractElement(vOp0, ConstantInt::get(I32Ty, 0)));
        Value *a2_0 = markInternal(IRB.CreateExtractElement(vOp0, ConstantInt::get(I32Ty, 1)));
        Value *a1_1 = markInternal(IRB.CreateExtractElement(vOp1, ConstantInt::get(I32Ty, 0)));
        Value *a2_1 = markInternal(IRB.CreateExtractElement(vOp1, ConstantInt::get(I32Ty, 1)));
        Value *b_a1 = IRB.CreateBinOp(binOp, a1_0, a1_1, "scalar.a1");
        Value *b_a2 = IRB.CreateBinOp(binOp, a2_0, a2_1, "scalar.a2");
        markReplica(b_a1);
        markReplica(b_a2);
        Value *r_a1 = insertZeroCostCSEBarrier(IRB, b_a1);
        Value *r_a2 = insertZeroCostCSEBarrier(IRB, b_a2);
        Value *uVec = UndefValue::get(ReplicaVecTy);
        Value *t0 = markInternal(IRB.CreateInsertElement(uVec, r_a1, ConstantInt::get(I32Ty, 0)));
        Value *t1 = markInternal(IRB.CreateInsertElement(t0, r_a2, ConstantInt::get(I32Ty, 1)));
        vRes = t1;
      }
    }

    simdApproxMap[inst] = vRes;
    if (isProtectionBoundary && !pureApproxOutput && budget < 0.999) {
      Value *boundaryVoted = createInlineFastApproxVote(IRB, M, inst, vRes, delta);
      inst->replaceUsesWithIf(boundaryVoted, [&](Use &U) {
        Instruction *userI = dyn_cast<Instruction>(U.getUser());
        return userI == nullptr || userI->getMetadata("atft.voter.internal") == nullptr;
      });
      simdApproxMap[boundaryVoted] = vRes;
    }
  }

  for (PHINode *PN : phiNodes) {
    PHINode *vPhi = cast<PHINode>(simdApproxMap[PN]);
    for (unsigned i = 0; i < PN->getNumIncomingValues(); ++i) {
      Value *inVal = PN->getIncomingValue(i);
      BasicBlock *inBB = PN->getIncomingBlock(i);
      IRBuilder<> BBEndBuilder(inBB->getTerminator());
      Value *inVec = getOrCreateApproxVec(inVal, BBEndBuilder);
      vPhi->addIncoming(inVec, inBB);

      // 通用静态结构规则 (无任何函数名特判):
      // 当循环携带的跨迭代状态反馈 PHINode 在回边块 (inBB) 内没有内存 Store 同步点、
      // 且函数包含多个递归状态环 (phiNodes.size() >= 4) 时，在回边处插入内联快速同步以阻断跨时间步误差发散
      if (!pureApproxOutput) {
        if (auto *inInst = dyn_cast<Instruction>(inVal)) {
          bool hasStoreInLatch = false;
          for (Instruction &BI : *inBB) {
            if (isa<StoreInst>(&BI)) {
              hasStoreInLatch = true;
              break;
            }
          }
          if (selectedSet.count(inInst) && LI.getLoopDepth(inBB) >= 1 &&
              !hasStoreInLatch && phiNodes.size() >= 4) {
            Value *votedState = createInlineFastApproxVote(BBEndBuilder, M, inVal, inVec, delta);
            PN->setIncomingValue(i, votedState);
          }
        }
      }
    }
  }

  for (StoreInst *SI : storeInsts) {
    Value *val = SI->getValueOperand();
    if (simdApproxMap.count(val)) {
      IRBuilder<> IRB(SI);
      Value *vApprox = simdApproxMap[val];
      Value *a1_f32 = IRB.CreateExtractElement(vApprox, ConstantInt::get(I32Ty, 0),
                                               (AtftSimdF64 && useSIMD) ? "a1.f64" : "a1.f32");
      Value *a1_f64 = (AtftSimdF64 && useSIMD)
                          ? a1_f32
                          : IRB.CreateFPExt(a1_f32, F64Ty, "a1.f64");

      // 循环级检错：不建分支/冷块
      if (AtftLoopVote && (okFlag || sigVec)) {
        auto mk = [&](Value *V) -> Value * {
          if (auto *I = dyn_cast<Instruction>(V))
            I->setMetadata("atft.voter.internal", MDNode::get(Ctx, {}));
          return V;
        };
        if (lvSig && sigVec) {
          // Elzar 式：两条冗余即 <2 x f64> 向量的两 lane，对向量整体 XOR 累 <2 x i64> 签名
          // (每 store 1 条向量 XOR，零 extract)；store 改写 lane0，标量 val 链死代码消除。
          Value *vecBits = mk(IRB.CreateBitCast(vApprox, V2I64Ty, "lv.vec.bits"));
          Value *cur = mk(IRB.CreateLoad(V2I64Ty, sigVec, "lv.sigVec.cur"));
          mk(IRB.CreateStore(mk(IRB.CreateXor(cur, vecBits, "lv.sigVec.next")), sigVec));
          SI->setOperand(0, a1_f64);
        } else {
          // 非精确回落：逐元素容差比累 i1
          Value *a2 = mk(IRB.CreateExtractElement(vApprox, ConstantInt::get(I32Ty, 1), "lv.a2"));
          Value *a2_f64 = (AtftSimdF64 && useSIMD) ? a2 : mk(IRB.CreateFPExt(a2, F64Ty, "lv.a2.f64"));
          Function *FabsLV = Intrinsic::getDeclaration(M, Intrinsic::fabs, {F64Ty});
          Value *absA1 = mk(IRB.CreateCall(FabsLV, {a1_f64}));
          Value *scale = mk(IRB.CreateFAdd(ConstantFP::get(F64Ty, 1.0), absA1));
          Value *effDelta = mk(IRB.CreateFMul(ConstantFP::get(F64Ty, delta), scale));
          Value *diff = mk(IRB.CreateCall(FabsLV, {mk(IRB.CreateFSub(val, a1_f64))}));
          Value *rOk = mk(IRB.CreateFCmpOLE(diff, effDelta, "lv.r.ok"));
          Value *a12Ok = mk(IRB.CreateFCmpOEQ(a1_f64, a2_f64, "lv.a12.ok"));
          Value *thisOk = mk(IRB.CreateAnd(rOk, a12Ok, "lv.this.ok"));
          Value *cur = mk(IRB.CreateLoad(I1Ty, okFlag, "lv.ok.cur"));
          mk(IRB.CreateStore(mk(IRB.CreateAnd(cur, thisOk, "lv.ok.next")), okFlag));
        }
        emittedLoopVote = true;
        continue;
      }

      if (pureApproxOutput) {
        SI->setOperand(0, a1_f64);

      } else if (useSIMD) {
        // SIMD 混合近似 TMR 在 Store 同步点采用内联快速无分支比较，仅在检测到故障时转入 ColdBB:
        // 为防止后置 FaultInjectorPass 在内联比较临时变量上二次注入，为内联比较指令标记 "atft.voter.internal"
        auto markVoterInternal = [&](Value *V) -> Value * {
          if (auto *I = dyn_cast<Instruction>(V)) {
            I->setMetadata("atft.voter.internal", MDNode::get(Ctx, {}));
          }
          return V;
        };
        Value *a2_f32 = markVoterInternal(IRB.CreateExtractElement(
            vApprox, ConstantInt::get(I32Ty, 1),
            (AtftSimdF64 && useSIMD) ? "a2.f64" : "a2.f32"));
        Value *a2_f64 = (AtftSimdF64 && useSIMD)
                            ? a2_f32
                            : markVoterInternal(IRB.CreateFPExt(a2_f32, F64Ty, "a2.f64"));
        markVoterInternal(a1_f32);
        markVoterInternal(a1_f64);
        Function *FabsF64 = Intrinsic::getDeclaration(M, Intrinsic::fabs, {F64Ty});
        Function *FabsF32 = Intrinsic::getDeclaration(M, Intrinsic::fabs, {F32Ty});

        Value *absA1 = markVoterInternal(IRB.CreateCall(FabsF64, {a1_f64}));
        Value *scale = markVoterInternal(IRB.CreateFAdd(ConstantFP::get(F64Ty, 1.0), absA1));

        // D3 剖析：记录无故障执行下 |R - A| / (1 + |A|) 的包络
        if (AtftDeltaProfile) {
          FunctionCallee ObserveFn = M->getOrInsertFunction(
              "atft_delta_profile_observe", Type::getVoidTy(Ctx), F64Ty, F64Ty);
          markVoterInternal(IRB.CreateCall(ObserveFn, {val, a1_f64}));
        }

        // D3 阈值来源：编译期常数 或 运行期逐负载值
        Value *deltaBase = ConstantFP::get(F64Ty, delta);
        if (AtftDeltaRuntime) {
          GlobalVariable *GDelta = cast<GlobalVariable>(
              M->getOrInsertGlobal("g_atft_delta_abs", F64Ty));
          Value *loaded = markVoterInternal(IRB.CreateLoad(F64Ty, GDelta, "delta.rt"));
          Value *valid = markVoterInternal(
              IRB.CreateFCmpOGT(loaded, ConstantFP::get(F64Ty, 0.0), "delta.rt.ok"));
          deltaBase = markVoterInternal(IRB.CreateSelect(valid, loaded, deltaBase, "delta.eff"));
        }
        Value *effDelta = markVoterInternal(IRB.CreateFMul(deltaBase, scale));

        Value *subRA1 = markVoterInternal(IRB.CreateFSub(val, a1_f64));
        Value *diffRA1 = markVoterInternal(IRB.CreateCall(FabsF64, {subRA1}));
        Value *rIsOk = markVoterInternal(IRB.CreateFCmpOLE(diffRA1, effDelta, "r.ok"));
        Value *a12Ok = nullptr;
        if (AtftReplicaExact || (AtftSimdF64 && useSIMD)) {
          a12Ok = markVoterInternal(IRB.CreateFCmpOEQ(a1_f32, a2_f32, "a12.ok"));
        } else {
          Value *subA12 = markVoterInternal(IRB.CreateFSub(a1_f32, a2_f32));
          Value *diffA12 = markVoterInternal(IRB.CreateCall(FabsF32, {subA12}));
          a12Ok = markVoterInternal(IRB.CreateFCmpOLE(diffA12, ConstantFP::get(F32Ty, 1e-5f), "a12.ok"));
        }
        Value *fastPass = markVoterInternal(IRB.CreateAnd(rIsOk, a12Ok, "fast.pass"));

        Instruction *splitBefore = SI;
        BasicBlock *CurBB = splitBefore->getParent();
        BasicBlock *ContBB = CurBB->splitBasicBlock(splitBefore, "vote.cont");
        BasicBlock *ColdBB = BasicBlock::Create(Ctx, "vote.cold", &F, ContBB);

        CurBB->getTerminator()->eraseFromParent();
        IRBuilder<> BranchBuilder(CurBB);
        BranchBuilder.CreateCondBr(fastPass, ContBB, ColdBB);

        IRBuilder<> ColdBuilder(ColdBB);
        // 对应开题报告 3.4.4.3 分级恢复策略:
        // 当 critLevel == CRIT_CRITICAL 时，在 ColdBB 冷路径中直接对当前基本块的精确计算切片执行指令级重执行 (Cold-Path Exact Re-execution)，
        // 实现零正常执行开销、故障时零精度损失的精确重算恢复!
        Value *reexecExactVal = a1_f64;
        if (critLevel == CRIT_CRITICAL) {
          std::map<Value *, Value *> coldReexecMap;
          for (Instruction &BI : *CurBB) {
            if (&BI == fastPass || BI.getMetadata("atft.voter.internal") != nullptr) continue;
            if (isTargetFP64Op(&BI)) {
              if (BI.getOpcode() == Instruction::FNeg) {
                Value *op0 = BI.getOperand(0);
                Value *r0 = coldReexecMap.count(op0) ? coldReexecMap[op0] : op0;
                coldReexecMap[&BI] = markVoterInternal(ColdBuilder.CreateFNeg(r0, "cold.reexec.fneg"));
              } else if (auto *BO = dyn_cast<BinaryOperator>(&BI)) {
                Value *op0 = BO->getOperand(0);
                Value *op1 = BO->getOperand(1);
                Value *r0 = coldReexecMap.count(op0) ? coldReexecMap[op0] : op0;
                Value *r1 = coldReexecMap.count(op1) ? coldReexecMap[op1] : op1;
                coldReexecMap[&BI] = markVoterInternal(
                    ColdBuilder.CreateBinOp(BO->getOpcode(), r0, r1, "cold.reexec.bin"));
              }
            }
          }
          if (coldReexecMap.count(val)) {
            reexecExactVal = coldReexecMap[val];
          }
        }

        Value *coldVoted = markVoterInternal(ColdBuilder.CreateCall(
            VoterApproxReexecFn,
            {val, reexecExactVal, a1_f64, a2_f64, ConstantFP::get(F64Ty, delta),
             ConstantInt::get(I32Ty, (int)critLevel)},
            "approx_tmr.cold"));
        ColdBuilder.CreateBr(ContBB);

        IRBuilder<> PhiBuilder(&*ContBB->begin());
        PHINode *mergedVal = PhiBuilder.CreatePHI(F64Ty, 2, "approx_tmr.merged");
        mergedVal->addIncoming(val, CurBB);
        mergedVal->addIncoming(coldVoted, ColdBB);
        SI->setOperand(0, mergedVal);
      } else {
        Value *a2_f32 = IRB.CreateExtractElement(
            vApprox, ConstantInt::get(I32Ty, 1),
            (AtftSimdF64 && useSIMD) ? "a2.f64" : "a2.f32");
        Value *a2_f64 = (AtftSimdF64 && useSIMD)
                            ? a2_f32
                            : IRB.CreateFPExt(a2_f32, F64Ty, "a2.f64");

        Value *voted = IRB.CreateCall(
            VoterApproxFn,
            {val, a1_f64, a2_f64, ConstantFP::get(F64Ty, delta),
             ConstantInt::get(I32Ty, (int)critLevel)},
            "approx_tmr.sync");
        SI->setOperand(0, voted);
      }
    }
  }

  // 循环级检错：在每个返回点一次性提交(zext：1=一致，0=不一致→置检测标志)
  if (AtftLoopVote && emittedLoopVote && (okFlag || sigVec)) {
    for (BasicBlock &BB : F) {
      if (auto *RI = dyn_cast<ReturnInst>(BB.getTerminator())) {
        IRBuilder<> RB(RI);
        Value *ok;
        if (lvSig && sigVec) {
          Value *lv = RB.CreateLoad(V2I64Ty, sigVec, "lv.sigVec.final");
          Value *l0 = RB.CreateExtractElement(lv, ConstantInt::get(I32Ty, 0), "lv.sig.l0");
          Value *l1 = RB.CreateExtractElement(lv, ConstantInt::get(I32Ty, 1), "lv.sig.l1");
          ok = RB.CreateICmpEQ(l0, l1, "lv.sig.ok");
        } else {
          ok = RB.CreateLoad(I1Ty, okFlag, "lv.ok.final");
        }
        RB.CreateCall(LoopVoteCommitFn, {RB.CreateZExt(ok, I32Ty)});
      }
    }
  }

  // 对包含 32-bit 整数数据运算链的函数同步应用 32-bit -> 16-bit 中间计算结果压缩 (对应开题报告 3.4.2.1)
  if (!pureApproxOutput) {
    transformFunctionInt32ToInt16Compression(F, critLevel, useSIMD);
    // 流入比较/分支/地址计算的整数链无法安全近似（截断位宽会改变控制流），
    // 对这部分回退到整数精确 TMR，保证整数负载同样受保护
    transformFunctionExactIntTMR(F, LI, rankMethod, budget);
  }
}

// ============================================================================
// 模块 4: 运行时多版本代码生成与动态自适应分发 (对应开题报告 3.3.2 & 4.2.1)
//   - 三级关键性代码段 (CRITICAL / SUB_CRITICAL / NON_CRITICAL) 均生成 LOW 与 HIGH 双版本:
//     * CRITICAL     : LOW 采用混合近似 TMR (budget), HIGH 采用 100% 混合近似 TMR + 冷路径精确重执行
//     * SUB_CRITICAL : LOW 采用原始轻量版 (或低预算保护), HIGH 切换为 100% 混合近似 TMR (近似替代恢复)
//     * NON_CRITICAL : LOW 采用原始无保护版, HIGH 切换为 50% 核心算子轻量近似容错版
// ============================================================================
void transformFunctionAdaptiveMultiVersion(Function &F, FunctionAnalysisManager &FAM,
                                           CritLevel critLevel, double delta,
                                           const std::string &rankMethod, double budget) {
  Module *M = F.getParent();
  LLVMContext &Ctx = M->getContext();
  Type *I32Ty = Type::getInt32Ty(Ctx);
  Type *I64Ty = Type::getInt64Ty(Ctx);

  unsigned staticInstCount = 0;
  for (BasicBlock &BB : F) staticInstCount += BB.size();

  ValueToValueMapTy VMapLow, VMapHigh;
  Function *LowFn = CloneFunction(&F, VMapLow);
  LowFn->setName(F.getName() + ".atft_low");
  LowFn->setLinkage(GlobalValue::InternalLinkage);

  Function *HighFn = CloneFunction(&F, VMapHigh);
  HighFn->setName(F.getName() + ".atft_high");
  HighFn->setLinkage(GlobalValue::InternalLinkage);

  if (critLevel == CRIT_CRITICAL) {
    auto &LILow = FAM.getResult<LoopAnalysis>(*LowFn);
    transformFunctionApproxTMR(*LowFn, LILow, CRIT_CRITICAL, delta, true, rankMethod, budget);
    auto &LIHigh = FAM.getResult<LoopAnalysis>(*HighFn);
    transformFunctionApproxTMR(*HighFn, LIHigh, CRIT_CRITICAL, delta, true, rankMethod, 1.0);
  } else if (critLevel == CRIT_SUB_CRITICAL) {
    auto &LIHigh = FAM.getResult<LoopAnalysis>(*HighFn);
    transformFunctionApproxTMR(*HighFn, LIHigh, CRIT_SUB_CRITICAL, delta, true, rankMethod, 1.0);
  } else {
    // CRIT_NON_CRITICAL: 平静期 (LOW) 零冗余运行，高通量辐射期 (HIGH) 保护前 50% 关键算子并采用安全回退
    auto &LIHigh = FAM.getResult<LoopAnalysis>(*HighFn);
    transformFunctionApproxTMR(*HighFn, LIHigh, CRIT_NON_CRITICAL, delta, true, rankMethod, 0.5);
  }

  F.deleteBody();
  BasicBlock *EntryBB = BasicBlock::Create(Ctx, "entry", &F);
  BasicBlock *LowBB = BasicBlock::Create(Ctx, "mode.low", &F);
  BasicBlock *HighBB = BasicBlock::Create(Ctx, "mode.high", &F);

  IRBuilder<> EntryBuilder(EntryBB);
  FunctionCallee AdvInstFn = M->getOrInsertFunction("atft_advance_insts", Type::getVoidTy(Ctx), I64Ty);
  EntryBuilder.CreateCall(AdvInstFn, {ConstantInt::get(I64Ty, staticInstCount * 64)});
  FunctionCallee GetModeFn = M->getOrInsertFunction("atft_get_mode", I32Ty);
  Value *modeVal = EntryBuilder.CreateCall(GetModeFn, {}, "prot.mode");
  Value *isHigh = EntryBuilder.CreateICmpEQ(
      modeVal, ConstantInt::get(I32Ty, 1), "is.high.mode");
  EntryBuilder.CreateCondBr(isHigh, HighBB, LowBB);

  std::vector<Value *> args;
  for (Argument &Arg : F.args()) {
    args.push_back(&Arg);
  }

  IRBuilder<> HighBuilder(HighBB);
  CallInst *highCall = HighBuilder.CreateCall(HighFn, args);
  if (F.getReturnType()->isVoidTy()) HighBuilder.CreateRetVoid();
  else HighBuilder.CreateRet(highCall);

  IRBuilder<> LowBuilder(LowBB);
  CallInst *lowCall = LowBuilder.CreateCall(LowFn, args);
  if (F.getReturnType()->isVoidTy()) LowBuilder.CreateRetVoid();
  else LowBuilder.CreateRet(lowCall);
}

static uint64_t atftFNV1a(StringRef text, uint64_t hash = 1469598103934665603ULL) {
  for (unsigned char c : text) {
    hash ^= c;
    hash *= 1099511628211ULL;
  }
  return hash;
}

static std::string atftCsvField(const std::string &value) {
  if (value.find_first_of(",\"\n") == std::string::npos) return value;
  std::string escaped = "\"";
  for (char c : value) {
    if (c == '\"') escaped += '\"';
    escaped += c;
  }
  escaped += '\"';
  return escaped;
}

static std::string atftCandidateClass(Instruction *I) {
  if (isTargetFP64Op(I) || isTargetMathCall(I)) return "fp64";
  if (isa<ICmpInst>(I)) return "control";
  if (isReplicableIntInst(I)) return "integer";
  return "";
}

static uint64_t computeAtftIRFingerprint(ArrayRef<Function *> targetFuncs) {
  uint64_t irFingerprint = 1469598103934665603ULL;
  for (Function *F : targetFuncs) {
    unsigned bbIndex = 0;
    for (BasicBlock &BB : *F) {
      unsigned instIndex = 0;
      for (Instruction &I : BB) {
        std::string token = F->getName().str() + "|" + std::to_string(bbIndex) +
                            "|" + std::to_string(instIndex) + "|" +
                            I.getOpcodeName() + "|" +
                            std::to_string((unsigned)I.getType()->getTypeID());
        irFingerprint = atftFNV1a(token, irFingerprint);
        ++instIndex;
      }
      ++bbIndex;
    }
  }
  return irFingerprint;
}

static void assignAtftSiteIds(ArrayRef<Function *> targetFuncs) {
  g_atftSiteIds.clear();
  g_atftSiteKeys.clear();
  for (Function *F : targetFuncs) {
    unsigned bbIndex = 0;
    for (BasicBlock &BB : *F) {
      std::string bbName = BB.hasName() ? BB.getName().str()
                                        : ("bb" + std::to_string(bbIndex));
      unsigned instIndex = 0;
      for (Instruction &I : BB) {
        std::string candidateClass = atftCandidateClass(&I);
        if (candidateClass.empty()) {
          ++instIndex;
          continue;
        }
        std::string siteKey = F->getName().str() + "|" + bbName + "|" +
                              std::to_string(bbIndex) + "|" +
                              std::to_string(instIndex) + "|" +
                              I.getOpcodeName() + "|" + candidateClass;
        g_atftSiteIds[&I] = atftFNV1a(siteKey);
        g_atftSiteKeys[&I] = siteKey;
        I.setMetadata("atft.site.key",
                      MDNode::get(F->getContext(), MDString::get(F->getContext(), siteKey)));
        ++instIndex;
      }
      ++bbIndex;
    }
  }
}

static std::vector<std::string> splitAtftCsv(const std::string &line) {
  std::vector<std::string> fields;
  std::string field;
  bool quoted = false;
  for (size_t i = 0; i < line.size(); ++i) {
    char c = line[i];
    if (c == '\"') {
      if (quoted && i + 1 < line.size() && line[i + 1] == '\"') {
        field += '\"';
        ++i;
      } else {
        quoted = !quoted;
      }
    } else if (c == ',' && !quoted) {
      fields.push_back(field);
      field.clear();
    } else {
      field += c;
    }
  }
  fields.push_back(field);
  return fields;
}

static void loadAtftProfile(ArrayRef<Function *> targetFuncs, StringRef path) {
  std::ifstream in(path.str());
  if (!in.is_open())
    report_fatal_error("cannot open -atft-profile");

  std::string line;
  if (!std::getline(in, line))
    report_fatal_error("empty -atft-profile");
  auto header = splitAtftCsv(line);
  if (header.size() < 10 || header[0] != "site_id" ||
      header[5] != "dynamic_count_proxy" || header[9] != "boundary_sensitivity")
    report_fatal_error("invalid selective protection profile header");

  uint64_t fingerprint = computeAtftIRFingerprint(targetFuncs);
  g_atftProfile.clear();
  while (std::getline(in, line)) {
    if (line.empty()) continue;
    auto fields = splitAtftCsv(line);
    if (fields.size() < 10)
      report_fatal_error("malformed selective protection profile row");
    uint64_t siteId = 0;
    uint64_t rowFingerprint = 0;
    double cost = 0.0;
    double boundary = 0.0;
    double riskPrior = 1.0;
    try {
      siteId = std::stoull(fields[0]);
      rowFingerprint = std::stoull(fields[2]);
      cost = std::stod(fields[5]);
      boundary = std::stod(fields[9]);
      if (fields.size() > 10) riskPrior = std::stod(fields[10]);
    } catch (const std::exception &) {
      report_fatal_error("invalid numeric field in selective protection profile");
    }
    if (rowFingerprint != fingerprint)
      report_fatal_error("selective protection profile IR fingerprint mismatch");
    if (!(cost > 0.0) || !std::isfinite(cost) || boundary < 0.0 ||
        !std::isfinite(boundary) || !(riskPrior > 0.0) ||
        !std::isfinite(riskPrior))
      report_fatal_error("invalid cost, boundary, or risk prior in selective protection profile");
    if (!g_atftProfile.emplace(siteId, AtftProfileEntry{cost, boundary, riskPrior, fields[3]}).second)
      report_fatal_error("duplicate site_id in selective protection profile");
  }
  g_atftProfileLoaded = true;
  errs() << "ATFT_PROFILE," << path << ",sites=" << g_atftProfile.size()
         << ",ir_fingerprint=" << fingerprint << "\n";
}

static void loadAtftFrontierSelection(ArrayRef<Function *> targetFuncs,
                                         StringRef path) {
  std::ifstream in(path.str());
  if (!in.is_open()) report_fatal_error("cannot open -atft-frontier-selection");

  std::string line;
  if (!std::getline(in, line)) report_fatal_error("empty -atft-frontier-selection");
  auto header = splitAtftCsv(line);
  auto column = [&](StringRef name) -> int {
    for (size_t i = 0; i < header.size(); ++i)
      if (header[i] == name) return (int)i;
    return -1;
  };
  int fingerprintCol = column("ir_fingerprint");
  int selectedCol = column("selected");
  int membersCol = column("member_site_ids");
  if (fingerprintCol < 0 || selectedCol < 0 || membersCol < 0)
    report_fatal_error("invalid frontier selection header");

  const uint64_t fingerprint = computeAtftIRFingerprint(targetFuncs);
  g_atftFrontierSelectedSites.clear();
  size_t selectedFrontiers = 0;
  while (std::getline(in, line)) {
    if (line.empty()) continue;
    auto fields = splitAtftCsv(line);
    if ((size_t)std::max({fingerprintCol, selectedCol, membersCol}) >= fields.size())
      report_fatal_error("malformed frontier selection row");
    try {
      if (std::stoull(fields[fingerprintCol]) != fingerprint)
        report_fatal_error("frontier selection IR fingerprint mismatch");
    } catch (const std::exception &) {
      report_fatal_error("invalid frontier selection fingerprint");
    }
    if (fields[selectedCol] != "1") continue;
    ++selectedFrontiers;
    std::stringstream members(fields[membersCol]);
    std::string member;
    while (std::getline(members, member, ';')) {
      if (member.empty()) continue;
      try {
        g_atftFrontierSelectedSites.insert(std::stoull(member));
      } catch (const std::exception &) {
        report_fatal_error("invalid frontier member site ID");
      }
    }
  }
  errs() << "ATFT_FRONTIER_SELECTION," << path
         << ",frontiers=" << selectedFrontiers
         << ",sites=" << g_atftFrontierSelectedSites.size()
         << ",ir_fingerprint=" << fingerprint << "\n";
}

static void writeAtftSelectionFile(StringRef path) {
  if (path.empty()) return;
  std::ofstream out(path.str());
  if (!out.is_open()) {
    errs() << "ATFT_SELECTION_ERROR," << path << "\n";
    return;
  }
  out << "site_id,site_key,class,dynamic_cost,boundary_sensitivity,risk,benefit,selected,budget_mode\n";
  for (const std::string &row : g_atftSelectionRows) out << row << "\n";
  errs() << "ATFT_SELECTION," << path << ",rows=" << g_atftSelectionRows.size()
         << ",missing_profile_sites=" << g_atftProfileMissing << "\n";
}

static void writeAtftFrontierManifest(
    ArrayRef<Function *> targetFuncs,
    FunctionAnalysisManager &FAM,
    StringRef path) {
  std::ofstream out(path.str());
  if (!out.is_open()) {
    errs() << "ATFT_FRONTIER_MANIFEST_ERROR," << path << "\n";
    return;
  }

  const unsigned kMaxPathDepth = 16;
  const uint64_t irFingerprint = computeAtftIRFingerprint(targetFuncs);
  std::set<uint64_t> emitted;
  std::map<uint64_t, std::string> seen;

  out << "frontier_id,ir_fingerprint,function,source_site_id,source_site_key,"
         "source_class,sink_kind,sink_site_id,sink_key,member_count,member_site_ids,"
         "path_depth,static_cost_proxy,closure_complete,match_status\n";

  for (Function *F : targetFuncs) {
    auto &LI = FAM.getResult<LoopAnalysis>(*F);
    auto instructionKey = [&](const Instruction *I, StringRef role) {
      unsigned bbIndex = 0;
      for (const BasicBlock &BB : *F) {
        unsigned instIndex = 0;
        for (const Instruction &Candidate : BB) {
          if (&Candidate == I) {
            std::string className = atftCandidateClass(const_cast<Instruction *>(I));
            if (className.empty()) className = role.str();
            return F->getName().str() + "|bb" + std::to_string(bbIndex) + "|" +
                   std::to_string(instIndex) + "|" + I->getOpcodeName() + "|" +
                   className;
          }
          ++instIndex;
        }
        ++bbIndex;
      }
      return F->getName().str() + "|unknown|" + I->getOpcodeName() + "|" + role.str();
    };

    auto sinkKind = [&](const Instruction *from, const Instruction *user) {
      if (isa<BranchInst>(user) || isa<SelectInst>(user) || isa<SwitchInst>(user))
        return std::string("branch");
      if (const auto *SI = dyn_cast<StoreInst>(user)) {
        if (SI->getPointerOperand() == from) return std::string("address");
        if (SI->getValueOperand() == from) return std::string("store");
      }
      if (isa<GetElementPtrInst>(user)) return std::string("address");
      if (isa<CallBase>(user)) return std::string("call");
      if (isa<ReturnInst>(user)) return std::string("return");
      if (const auto *PN = dyn_cast<PHINode>(user)) {
        Loop *userLoop = LI.getLoopFor(PN->getParent());
        Loop *fromLoop = LI.getLoopFor(from->getParent());
        if (userLoop && fromLoop && fromLoop->contains(PN->getParent()))
          return std::string("feedback");
      }
      return std::string();
    };

    std::vector<Instruction *> sources;
    for (BasicBlock &BB : *F) {
      for (Instruction &I : BB) {
        if (!atftCandidateClass(&I).empty()) sources.push_back(&I);
      }
    }

    for (Instruction *source : sources) {
      std::map<const Instruction *, const Instruction *> parent;
      std::map<const Instruction *, unsigned> depth;
      std::queue<const Instruction *> queue;
      parent[source] = nullptr;
      depth[source] = 0;
      queue.push(source);

      while (!queue.empty()) {
        const Instruction *current = queue.front();
        queue.pop();
        unsigned currentDepth = depth[current];
        if (currentDepth >= kMaxPathDepth) continue;

        for (const User *U : current->users()) {
          const auto *user = dyn_cast<Instruction>(U);
          if (!user || parent.count(user)) continue;
          parent[user] = current;
          depth[user] = currentDepth + 1;

          std::string kind = sinkKind(current, user);
          if (kind.empty()) {
            queue.push(user);
            continue;
          }

          std::vector<const Instruction *> members;
          for (const Instruction *I = user; I; I = parent[I]) members.push_back(I);
          std::reverse(members.begin(), members.end());
          std::set<const Instruction *> uniqueMembers(members.begin(), members.end());

          std::vector<uint64_t> memberSiteIds;
          double staticCost = 0.0;
          for (const Instruction *I : uniqueMembers) {
            auto siteIt = g_atftSiteIds.find(I);
            if (siteIt != g_atftSiteIds.end()) memberSiteIds.push_back(siteIt->second);
            unsigned loopDepth = LI.getLoopDepth(I->getParent());
            staticCost += std::pow(10.0, (double)std::min(loopDepth, 6U));
          }
          std::sort(memberSiteIds.begin(), memberSiteIds.end());

          const std::string sourceKey = g_atftSiteKeys.count(source)
                                            ? g_atftSiteKeys.at(source)
                                            : instructionKey(source, "source");
          const std::string sinkKey = g_atftSiteKeys.count(user)
                                          ? g_atftSiteKeys.at(user)
                                          : instructionKey(user, "sink");
          std::string token = sourceKey + "|" + kind + "|" + sinkKey + "|";
          for (uint64_t id : memberSiteIds) token += std::to_string(id) + ";";
          uint64_t frontierId = atftFNV1a(token);
          std::string matchStatus = "ok";
          auto prior = seen.find(frontierId);
          if (prior != seen.end() && prior->second != token) {
            matchStatus = "hash_collision";
          } else {
            seen.emplace(frontierId, token);
          }
          if (!emitted.insert(frontierId).second) continue;

          std::string ids;
          for (size_t i = 0; i < memberSiteIds.size(); ++i) {
            if (i) ids += ";";
            ids += std::to_string(memberSiteIds[i]);
          }
          out << frontierId << "," << irFingerprint << ","
              << atftCsvField(F->getName().str()) << ","
              << (g_atftSiteIds.count(source) ? g_atftSiteIds.at(source) : 0) << ","
              << atftCsvField(sourceKey) << ","
              << atftCandidateClass(source) << "," << kind << ","
              << (g_atftSiteIds.count(user) ? g_atftSiteIds.at(user) : 0) << ","
              << atftCsvField(sinkKey) << "," << uniqueMembers.size() << ","
              << atftCsvField(ids) << "," << depth[user] << "," << staticCost
              << ",1," << matchStatus << "\n";
        }
      }
    }
  }

  errs() << "ATFT_FRONTIER_MANIFEST," << path << ",frontiers=" << emitted.size()
         << ",ir_fingerprint=" << irFingerprint << "\n";
}

static void writeAtftSiteManifest(Module &M,
                                   ArrayRef<Function *> targetFuncs,
                                   FunctionAnalysisManager &FAM,
                                   StringRef path) {
  std::ofstream out(path.str());
  if (!out.is_open()) {
    errs() << "ATFT_SITE_MANIFEST_ERROR," << path << "\n";
    return;
  }

  assignAtftSiteIds(targetFuncs);
  uint64_t irFingerprint = computeAtftIRFingerprint(targetFuncs);

  out << "site_id,site_key,ir_fingerprint,function,basic_block,bb_index,inst_index,"
         "opcode,class,loop_depth,fanout,cross_bb_uses,feeds_branch,feeds_store,"
         "feeds_store_addr,feeds_call,feeds_gep,recurrent_feedback,boundary_sensitivity,"
         "match_status\n";

  std::map<uint64_t, std::string> seen;
  for (Function *F : targetFuncs) {
    auto &LI = FAM.getResult<LoopAnalysis>(*F);
    unsigned bbIndex = 0;
    for (BasicBlock &BB : *F) {
      std::string bbName = BB.hasName() ? BB.getName().str()
                                        : ("bb" + std::to_string(bbIndex));
      unsigned instIndex = 0;
      for (Instruction &I : BB) {
        std::string candidateClass = atftCandidateClass(&I);
        if (candidateClass.empty()) {
          ++instIndex;
          continue;
        }

        unsigned fanout = I.getNumUses();
        unsigned crossBBUses = 0;
        bool feedsBranch = false;
        bool feedsStore = false;
        bool feedsStoreAddr = false;
        bool feedsCall = false;
        bool feedsGEP = false;
        bool recurrentFeedback = false;

        for (const User *U : I.users()) {
          const auto *UI = dyn_cast<Instruction>(U);
          if (!UI) continue;
          if (UI->getParent() != &BB) ++crossBBUses;
          if (isa<BranchInst>(UI) || isa<SwitchInst>(UI)) feedsBranch = true;
          if (const auto *SI = dyn_cast<StoreInst>(UI)) {
            if (SI->getValueOperand() == &I) feedsStore = true;
            if (SI->getPointerOperand() == &I) feedsStoreAddr = true;
          }
          if (isa<CallBase>(UI)) feedsCall = true;
          if (isa<GetElementPtrInst>(UI)) feedsGEP = true;
          if (const auto *PN = dyn_cast<PHINode>(UI)) {
            Loop *userLoop = LI.getLoopFor(PN->getParent());
            Loop *instLoop = LI.getLoopFor(&BB);
            if (userLoop && instLoop && instLoop->contains(PN->getParent()))
              recurrentFeedback = true;
          }
        }

        std::string siteKey = F->getName().str() + "|" + bbName + "|" +
                              std::to_string(bbIndex) + "|" +
                              std::to_string(instIndex) + "|" +
                              I.getOpcodeName() + "|" + candidateClass;
        uint64_t siteId = atftFNV1a(siteKey);
        std::string matchStatus = "ok";
        auto prior = seen.find(siteId);
        if (prior != seen.end() && prior->second != siteKey)
          matchStatus = "hash_collision";
        else
          seen.emplace(siteId, siteKey);

        double boundarySensitivity =
            (feedsBranch ? 4.0 : 0.0) + (feedsStore ? 4.0 : 0.0) +
            (feedsStoreAddr ? 5.0 : 0.0) + (feedsCall ? 3.0 : 0.0) +
            (feedsGEP ? 5.0 : 0.0) + 0.5 * (double)crossBBUses +
            (recurrentFeedback ? 2.0 : 0.0);

        out << siteId << "," << atftCsvField(siteKey) << "," << irFingerprint
            << "," << atftCsvField(F->getName().str()) << ","
            << atftCsvField(bbName) << "," << bbIndex << "," << instIndex << ","
            << I.getOpcodeName() << "," << candidateClass << ","
            << LI.getLoopDepth(&BB) << "," << fanout << "," << crossBBUses << ","
            << (feedsBranch ? 1 : 0) << "," << (feedsStore ? 1 : 0) << ","
            << (feedsStoreAddr ? 1 : 0) << "," << (feedsCall ? 1 : 0) << ","
            << (feedsGEP ? 1 : 0) << "," << (recurrentFeedback ? 1 : 0) << ","
            << boundarySensitivity << "," << matchStatus << "\n";
        ++instIndex;
      }
      ++bbIndex;
    }
  }

  errs() << "ATFT_SITE_MANIFEST," << path << ",sites=" << seen.size()
         << ",ir_fingerprint=" << irFingerprint << "\n";
}

class AdaptiveFTModulePass : public PassInfoMixin<AdaptiveFTModulePass> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM) {
    auto &FAM = MAM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();

    std::vector<Function *> targetFuncs;

    // ------------------------------------------------------------------
    // 保护范围 = 从被测内核入口 bench_* 出发的**调用图闭包**
    //
    // 早期版本只遍历 bench_* 本身，依赖上游 kernel_* 被 always_inline 内联进
    // 入口函数才能被保护；凡是非 static 的上游函数（如 sobel.c 的
    // sobel_filtering、crc16.c 的 crc16、quicksort.c 的递归 quick_sort）
    // 都会被漏保护，且该前提会被编译器的内联决策随时打破（参见 nZDC, DAC'16
    // 与 ASP-DAC'26 关于"编译器优化削弱 IR 层冗余"的结论）。
    // 改为调用图闭包后，保护范围由程序结构而非内联启发式决定。
    // ------------------------------------------------------------------
    auto isOutOfScope = [](StringRef n) {
      return n.startswith("atft_") || n.startswith("harness_") ||
             n.startswith("llvm.") || n.startswith("polybench_") ||
             n.endswith(".atft_low") || n.endswith(".atft_high") ||
             n.endswith("_unused_main") || n == "main";
    };

    std::set<Function *> inClosure;
    std::vector<Function *> worklist;
    for (Function &F : M) {
      if (F.isDeclaration()) continue;
      if (F.getName().startswith("bench_") && !isOutOfScope(F.getName())) {
        inClosure.insert(&F);
        worklist.push_back(&F);
      }
    }
    while (!worklist.empty()) {
      Function *cur = worklist.back();
      worklist.pop_back();
      for (BasicBlock &BB : *cur) {
        for (Instruction &I : BB) {
          auto *CB = dyn_cast<CallBase>(&I);
          if (!CB) continue;
          Function *callee = CB->getCalledFunction();
          if (!callee || callee->isDeclaration()) continue;
          if (callee->isIntrinsic() || isOutOfScope(callee->getName())) continue;
          if (inClosure.insert(callee).second) worklist.push_back(callee);
        }
      }
    }
    // 按 Module 内顺序输出，保证变换顺序可复现
    for (Function &F : M) {
      if (inClosure.count(&F)) targetFuncs.push_back(&F);
    }

    if (!AtftSiteManifestFile.empty() || !AtftFrontierManifestFile.empty() ||
        AtftBudgetMode != "count") {
      for (Function *F : targetFuncs) expandFMulAddIntrinsics(*F);
    }
    assignAtftSiteIds(targetFuncs);
    // fi-cache 排名:在 O3 改写 IR 之前把实测脆弱性缓存读进来(key=site_key,
    // 跟随 atft.site.key metadata 一起被 O3 保留在存活指令上)。
    if (!AtftVulnCache.empty() && !g_atftVulnCacheLoaded)
      loadAtftVulnCache(AtftVulnCache);
    // TRIDENT 方法复现:免 FI 的输出锚定 SDC 可达性排名(纯分析,写 CSV 后返回)。
    if (!AtftTridentRank.empty()) {
      std::vector<std::string> rows;
      for (Function *F : targetFuncs) {
        auto &LI = FAM.getResult<LoopAnalysis>(*F);
        transformFunctionTridentRank(*F, LI, rows);
      }
      std::ofstream tr(AtftTridentRank);
      tr << "function,category,reach,weight\n";
      for (const std::string &r : rows) tr << r << "\n";
      errs() << "ATFT_TRIDENT_RANK," << AtftTridentRank << ",rows=" << rows.size() << "\n";
      return PreservedAnalyses::all();
    }
    if (!AtftFrontierManifestFile.empty())
      writeAtftFrontierManifest(targetFuncs, FAM, AtftFrontierManifestFile);
    if (AtftBudgetMode != "count" && AtftBudgetMode != "dynamic-cost" &&
        AtftBudgetMode != "risk-cost" && AtftBudgetMode != "frontier")
      report_fatal_error("invalid -atft-budget-mode");
    if (AtftBudgetMode == "frontier") {
      if (AtftFrontierSelectionFile.empty())
        report_fatal_error("-atft-frontier-selection is required for frontier budget mode");
      loadAtftFrontierSelection(targetFuncs, AtftFrontierSelectionFile);
    } else if (AtftBudgetMode != "count") {
      if (AtftProfileFile.empty())
        report_fatal_error("-atft-profile is required for non-count budget mode");
      loadAtftProfile(targetFuncs, AtftProfileFile);
    }
    if (!AtftSiteManifestFile.empty()) {
      writeAtftSiteManifest(M, targetFuncs, FAM, AtftSiteManifestFile);
    }

    std::vector<FuncCriticalityInfo> critReports;

    // ISA 对齐：把目标函数对齐到 host ISA（native 与受保护模式一视同仁，保证公平对比）
    if (AtftIsaAvx512) {
      for (Function *F : targetFuncs) atftApplyHostISA(*F);
    }

    // 通用 DMR/TMR：ISA 对齐 host → 模块内跑 O3(复用 LLVM 向量化器) → 对每个目标函数通用注入
    // 冗余影子+签名/投票。不认循环形状，换没见过的内核也自动生效。处理完直接返回。
    if (AtftDMRGeneric) {
      std::set<std::string> targetNames;
      for (Function *F : targetFuncs) {
        targetNames.insert(F->getName().str());
        atftApplyHostISA(*F);
      }
      atftRunO3(M);                               // 通用向量化(O3 可能改/删函数，故按名重解析)
      for (const std::string &n : targetNames) {
        Function *F = M.getFunction(n);
        if (!F || F->isDeclaration()) continue;
        if (AtftDMRNoInject) continue;             // 公平基线：同管线但不注入
        int inj = transformFunctionDMRGeneric(*F, AtftTMRGeneric ? 3 : 2);
        if (inj) errs() << "ATFT_DMR_GENERIC," << n << "," << inj << "\n";
      }
      // 实验(P3 收口):把整数通路三模接入向量化路径,实测它能否覆盖 O3 向量化
      // 产生的向量 cast。本地构 LoopInfo,避开外层 FAM 在 O3 后的陈旧结果。
      if (AtftIntTMRWire) {
        for (const std::string &n : targetNames) {
          Function *F = M.getFunction(n);
          if (!F || F->isDeclaration()) continue;
          DominatorTree DT(*F);
          LoopInfo LI(DT);
          transformFunctionExactIntTMR(*F, LI, AtftRankMethod, AtftBudget);
        }
      }
      // 地址平面结构型保护:只复制 GEP 下标切片并检错(替代全整数三模的便宜法)。
      if (AtftAddrDMR) {
        for (const std::string &n : targetNames) {
          Function *F = M.getFunction(n);
          if (!F || F->isDeclaration()) continue;
          transformFunctionAddrDMR(*F);
        }
      }
      // 整数数据平面:复制流向整数 store 的值切片并检错(补整数数据 load/arith 的洞)。
      if (AtftIntDataDMR) {
        for (const std::string &n : targetNames) {
          Function *F = M.getFunction(n);
          if (!F || F->isDeclaration()) continue;
          transformFunctionIntDataDMR(*F, AtftRankMethod, AtftIntDataBudget);
        }
      }
      // 层③·整数残差码:环运算链携带 mod-M 残差,store 处检查(编码检查,非复制)。
      if (AtftIntResidue) {
        for (const std::string &n : targetNames) {
          Function *F = M.getFunction(n);
          if (!F || F->isDeclaration()) continue;
          transformFunctionIntResidue(*F);
        }
      }
      // 层①·分支:仿射循环 IV==bound 结构不变量断言。
      if (AtftLoopAssert) {
        for (const std::string &n : targetNames) {
          Function *F = M.getFunction(n);
          if (!F || F->isDeclaration()) continue;
          transformFunctionLoopIndexAssert(*F);
        }
      }
      // 层②·load:整数数据 load 的 nZDC 式 reload-compare。
      if (AtftLoadReload) {
        for (const std::string &n : targetNames) {
          Function *F = M.getFunction(n);
          if (!F || F->isDeclaration()) continue;
          transformFunctionLoadReload(*F);
        }
      }
      // 控制穿越口接入向量化路径:复制驱动标量分支的比较并多数决(TMR)或检错(DMR)。
      if (AtftCmpTMR || AtftCmpDMR) {
        unsigned nProtected = 0;
        double controlBudget = AtftControlBudget >= 0.0 ? AtftControlBudget : AtftBudget;
        for (const std::string &n : targetNames) {
          Function *F = M.getFunction(n);
          if (!F || F->isDeclaration()) continue;
          transformFunctionCmpTMR(*F, &nProtected, controlBudget, AtftRankMethod);
        }
        errs() << "ATFT_CMP_TMR_SITES," << nProtected << "\n";
      }
      // 清理：mem2reg 把签名 alloca 升 SSA(reduction phi)，省掉每迭代访存
      {
        PassBuilder PB;
        LoopAnalysisManager LAM; FunctionAnalysisManager FAM2;
        CGSCCAnalysisManager CGAM; ModuleAnalysisManager MAM2;
        PB.registerModuleAnalyses(MAM2); PB.registerCGSCCAnalyses(CGAM);
        PB.registerFunctionAnalyses(FAM2); PB.registerLoopAnalyses(LAM);
        PB.crossRegisterProxies(LAM, FAM2, CGAM, MAM2);
        FunctionPassManager FPM; FPM.addPass(PromotePass());
        ModulePassManager MPM;
        MPM.addPass(createModuleToFunctionPassAdaptor(std::move(FPM)));
        MPM.run(M, MAM2);
      }
      writeAtftSelectionFile(AtftSelectionFile);
      return PreservedAnalyses::none();
    }

    for (Function *F : targetFuncs) {
      FuncCriticalityInfo info = analyzeFunctionCriticality(*F, FAM);
      critReports.push_back(info);

      LLVMContext &Ctx = M.getContext();
      std::string levelStr = (info.level == CRIT_CRITICAL)      ? "CRITICAL"
                             : (info.level == CRIT_SUB_CRITICAL) ? "SUB_CRITICAL"
                                                                 : "NON_CRITICAL";
      MDNode *MD = MDNode::get(Ctx, {MDString::get(Ctx, levelStr),
                                     MDString::get(Ctx, std::to_string(info.score))});
      F->setMetadata("atft.criticality", MD);
    }

    if (!AtftReportFile.empty()) {
      std::ofstream ofs(AtftReportFile);
      if (ofs.is_open()) {
        ofs << "function,num_insts,max_loop_depth,avg_output_closeness,avg_liveness,state_feedback,score,criticality_level\n";
        for (const auto &r : critReports) {
          std::string lstr = (r.level == CRIT_CRITICAL)      ? "CRITICAL"
                             : (r.level == CRIT_SUB_CRITICAL) ? "SUB_CRITICAL"
                                                              : "NON_CRITICAL";
          ofs << r.funcName << "," << r.numInsts << "," << r.maxLoopDepth << ","
              << r.avgDistToOutput << "," << r.avgLiveness << ","
              << r.stateFeedbackFactor << "," << r.score << "," << lstr << "\n";
        }
      }
    }

    // 抽取 + 插桩共用同一遍分析：只给插桩开关时也要跑，产物丢进空流
    if (!AtftDumpNests.empty()) {
      std::ofstream nf(AtftDumpNests);
      if (nf.is_open()) {
        nf << "kind,function,nest_depth,out_dims,out_idx,read_dims,read_idx,"
              "max_array_dims,elem_type,ops,phi_outside_header,cond_self_update,"
              "cmp_branch,calls\n";
        for (Function *F : targetFuncs) {
          expandFMulAddIntrinsics(*F);
          auto &LI = FAM.getResult<LoopAnalysis>(*F);
          auto &SE = FAM.getResult<ScalarEvolutionAnalysis>(*F);
          dumpAllNests(*F, LI, SE, nf);
        }
      }
    }

    if (!AtftDumpSums.empty() || AtftABFTInsert) {
      std::ofstream ofs;
      std::ostringstream devnull;
      if (!AtftDumpSums.empty()) ofs.open(AtftDumpSums);
      std::ostream &out = ofs.is_open() ? static_cast<std::ostream &>(ofs)
                                        : static_cast<std::ostream &>(devnull);
      {
        out << "kind,function,nest_depth,trip_counts,accumulator,op_mix,terms\n";
        for (Function *F : targetFuncs) {
          // 先把 llvm.fmuladd 展开成 fmul + fadd：PolyBench 的
          // C[i][j] += alpha*A*B 在 IR 里是一条 fmuladd 内建，
          // 不展开就匹配不到"fadd 的一个操作数是对同一地址的 load"这个归约形态
          expandFMulAddIntrinsics(*F);
          auto &LI = FAM.getResult<LoopAnalysis>(*F);
          auto &SE = FAM.getResult<ScalarEvolutionAnalysis>(*F);
          dumpReductionSums(*F, LI, SE, out);
        }
      }
    }

    if (AtftMode == "analyze" || AtftMode == "native") {
      for (Function *F : targetFuncs) {
        expandFMulAddIntrinsics(*F);
      }
      // 整数通路三模可以在 native 模式下单独叠加：代数校验层只清 SDC，
      // DUE（崩溃）要靠对 i32->i64 下标扩展表决来压。两层正交，必须能分开计量，
      // 否则"分层到底省多少"永远只能估。默认关闭，native 基线行为不变。
      if (AtftIntTMRWire) {
        for (Function *F : targetFuncs) {
          auto &LI = FAM.getResult<LoopAnalysis>(*F);
          transformFunctionExactIntTMR(*F, LI, AtftRankMethod, AtftBudget);
        }
      }
      if (AtftCmpTMR) {
        unsigned nProtected = 0;
        double controlBudget = AtftControlBudget >= 0.0
                                   ? AtftControlBudget
                                   : AtftBudget;
        for (Function *F : targetFuncs)
          transformFunctionCmpTMR(*F, &nProtected, controlBudget, AtftRankMethod);
        errs() << "ATFT_CMP_TMR_SITES," << nProtected << "\n";
      }
      writeAtftSelectionFile(AtftSelectionFile);
      return PreservedAnalyses::none();
    }

    for (size_t i = 0; i < targetFuncs.size(); ++i) {
      Function *F = targetFuncs[i];
      CritLevel clevel = critReports[i].level;
      auto &LI = FAM.getResult<LoopAnalysis>(*F);

      // wide-DMR：先对规整单位步长数据并行 FP 循环做跨迭代向量化(高半冗余)，
      // 转换掉的循环写 [start,Vn)；其标量余数与未匹配循环仍交给下面选定模式保护。
      if (AtftWideDMR) {
        int nv = transformFunctionWideDMR(*F, LI);
        if (nv) errs() << "ATFT_WIDE_DMR," << F->getName().str() << "," << nv << "\n";
      }

      // 分层互斥：这个函数已经有代数校验了，就不再叠加浮点近似三模。
      // 判据本来就是这么说的（"搜到就进代数校验层，搜不到就回落"），
      // 只是实现一直把两层叠加在一起算，于是开销被重复计了一遍。
      //
      // 但整数通路三模要保留：六类别实测显示代数校验把 gemm/2mm 六个类别的
      // SDC 全清零，**DUE 却一点没动**（int_arith 54.5%、cast 73.75% 照旧），
      // 因为校验和挡不住崩溃。压 DUE 的是对 i32->i64 下标扩展表决。
      if (AtftLayerExclusive && g_abftInserted.count(F)) {
        errs() << "ATFT_LAYER_SKIP," << F->getName() << ",has-algebraic-check"
               << (AtftIntTMRWire ? ",keep-int-tmr" : "") << "\n";
        if (AtftIntTMRWire)
          transformFunctionExactIntTMR(*F, LI, AtftRankMethod, AtftBudget);
        continue;
      }


      if (AtftMode == "eddi") {
        transformFunctionEDDIOrSWIFT(*F, LI, true, AtftRankMethod, AtftBudget);
      } else if (AtftMode == "swift") {
        transformFunctionEDDIOrSWIFT(*F, LI, false, AtftRankMethod, AtftBudget);
      } else if (AtftMode == "lltfi-sid") {
        transformFunctionLLTFI_SID(*F, LI, AtftRankMethod, AtftBudget);
      } else if (AtftMode == "exact-tmr") {
        transformFunctionExactTMR(*F, LI, AtftRankMethod, AtftBudget);
        transformFunctionExactIntTMR(*F, LI, AtftRankMethod, AtftBudget);
      } else if (AtftMode == "approx-scalar") {
        transformFunctionApproxTMR(*F, LI, clevel, AtftDelta, false, AtftRankMethod, AtftBudget, false);
      } else if (AtftMode == "approx-simd") {
        transformFunctionApproxTMR(*F, LI, clevel, AtftDelta, true, AtftRankMethod, AtftBudget, false);
      } else if (AtftMode == "pure-approx") {
        transformFunctionApproxTMR(*F, LI, clevel, AtftDelta, true, AtftRankMethod, 1.0, true);
      } else if (AtftMode == "adaptive") {
        transformFunctionAdaptiveMultiVersion(*F, FAM, clevel, AtftDelta, AtftRankMethod, AtftBudget);
      }
    }

    // 调用图闭包已覆盖 KissFFT 的内部蝶形算子 (kf_bfly2/3/4/5) 与 kf_work，
    // 不再需要按函数名单独补一遍变换。

    // D2 控制平面三模：在数据平面变换之后再做，正交叠加。
    // adaptive 模式下 LOW/HIGH 两个克隆体已经取代了原函数体，这里对克隆体生效。
    if (AtftCmpTMR) {
      unsigned nProtected = 0;
      for (Function &F : M) {
        if (F.isDeclaration()) continue;
        StringRef n = F.getName();
        if (inClosure.count(&F) || n.endswith(".atft_low") || n.endswith(".atft_high")) {
          // 分层互斥同样适用于控制平面：代数校验已把 cmp_branch 的 SDC 清零
          // （gemm 18.0 -> 0.00、2mm 40.5 -> 0.00，六类别实测），再三模是重复付费。
          if (AtftLayerExclusive && g_abftInserted.count(&F)) continue;
          double controlBudget = AtftControlBudget >= 0.0
                                     ? AtftControlBudget
                                     : AtftBudget;
          transformFunctionCmpTMR(F, &nProtected, controlBudget, AtftRankMethod);
        }
      }
      errs() << "ATFT_CMP_TMR_SITES," << nProtected << "\n";
    }

    writeAtftSelectionFile(AtftSelectionFile);
    return PreservedAnalyses::none();
  }
};

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "AdaptiveFTPass", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "adaptive-ft") {
                    MPM.addPass(AdaptiveFTModulePass());
                    return true;
                  }
                  return false;
                });
          }};
}
