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
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/ValueMapper.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <vector>

using namespace llvm;

namespace {

static cl::opt<std::string> AtftMode(
    "atft-mode",
    cl::desc("Protection policy: analyze | native | eddi | swift | lltfi-sid | exact-tmr | approx-scalar | approx-simd | adaptive | pure-approx"),
    cl::init("adaptive"));

static cl::opt<std::string> AtftRankMethod(
    "atft-rank",
    cl::desc("Selective protection ranking: semantic | sdctune | shoestring | hotpath | random"),
    cl::init("semantic"));

static cl::opt<double> AtftBudget(
    "atft-budget",
    cl::desc("Selective protection budget ratio in [0.0, 1.0]"),
    cl::init(1.0));

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
static cl::opt<std::string> AtftDumpSums(
    "atft-dump-sums",
    cl::desc("Extract reduction loop nests into a symbolic sum description (CSV path)"),
    cl::init(""));

static cl::opt<std::string> AtftReportFile(
    "atft-report",
    cl::desc("Output CSV file path for criticality analysis report"),
    cl::init(""));

// --- 中期优化开关：三项都默认关闭，不改动已冻结基线的任何行为 ---

// D2 控制平面三模：复制驱动 br/select/switch 的比较并多数决
static cl::opt<bool> AtftCmpTMR(
    "atft-cmp-tmr",
    cl::desc("Triplicate control-flow-driving compares and majority-vote them"),
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

// 诊断开关：关掉 CSE 屏障，用来**测量**屏障本身的代价（指令数与 IPC 损失）。
// 关掉后 -O2 的 CSE/GVN 会把等价副本折叠，冗余不再成立，所以它只能用于测量，
// 不能用于部署。测出的差值是"屏障税"的上界。
static cl::opt<bool> AtftNoBarrier(
    "atft-no-cse-barrier",
    cl::desc("Measurement only: drop the CSE barriers (destroys redundancy, use for cost accounting)"),
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

      // 计算直接扇出 (Fan-out) 与跨基本块活跃范围
      unsigned fanOut = I.getNumUses();
      unsigned crossBBUses = 0;
      bool feedsStoreDirectly = false;
      for (const User *U : I.users()) {
        if (const auto *userInst = dyn_cast<Instruction>(U)) {
          if (userInst->getParent() != &BB) crossBBUses++;
          if (isa<StoreInst>(userInst)) feedsStoreDirectly = true;
        }
      }

      // 算子类型固有误差放大因子 (除法/超越函数/乘法对位翻转的放大高于加减法)
      double opAmp = 1.0;
      if (I.getOpcode() == Instruction::FDiv || isa<CallInst>(&I)) opAmp = 3.0;
      else if (I.getOpcode() == Instruction::FMul) opAmp = 1.8;

      double s = 0.0;
      if (rankMethod == "hotpath" || rankMethod == "loop-only") {
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

std::set<Instruction *> selectInstructionsByBudget(
    const std::vector<Instruction *> &candidates,
    const std::map<const Instruction *, double> &scores,
    double budget) {
  std::set<Instruction *> selected;
  if (budget <= 0.0 || candidates.empty()) return selected;
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
  for (size_t i = 0; i < k && i < ranked.size(); ++i) {
    selected.insert(ranked[i].second);
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
};

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
  if (P->hasName() && !P->getName().startswith(".")) return P->getName().str();
  if (auto *Arg = dyn_cast<Argument>(P)) return "arg" + std::to_string(Arg->getArgNo());
  return P->hasName() ? P->getName().str() : "anon";
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
                                std::vector<AccessInfo> &terms, std::string &opMix) {
  std::vector<Value *> wl{root};
  std::set<Value *> seen;
  while (!wl.empty()) {
    Value *v = wl.back();
    wl.pop_back();
    if (!seen.insert(v).second) continue;
    if (auto *LD = dyn_cast<LoadInst>(v)) {
      AccessInfo ai;
      if (describeAccessSCEV(LD->getPointerOperand(), SE, depthOf, ai)) terms.push_back(ai);
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
    // 循环不变的标量因子（常系数、循环外算出的值）也要记下来：
    // 它是因子外提规则的作用对象 —— stencil 的 0.2 * (u1+...+u5) 就靠它
    // 把乘法从 N^2 次降到 1 次。记成"自由下标为空"的项。
    if (isa<ConstantFP>(v)) {
      terms.push_back({"const", {}});
      continue;
    }
    if (SE.isSCEVable(v->getType())) {
      AddRecLoopCollector C;
      SCEVTraversal<AddRecLoopCollector> T(C);
      T.visitAll(SE.getSCEV(v));
      bool invariant = true;
      for (const Loop *L : C.loops)
        if (depthOf.count(L)) { invariant = false; break; }
      if (invariant && !isa<LoadInst>(v)) terms.push_back({"scalar", {}});
    }
  }
}

void dumpReductionSums(Function &F, LoopInfo &LI, ScalarEvolution &SE,
                       std::ostream &OS) {
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
    auto emitTrips = [&]() {
      for (unsigned t : tripCounts) OS << t << ";";
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
              collectTermAccesses(term, SE, depthOf, terms, opMix);
              if (!terms.empty()) {
                OS << "REDUCTION," << F.getName().str() << "," << nest.size() << ",";
                emitTrips();
                OS << "," << accAcc.base << ":";
                for (unsigned d : accAcc.idxDeps) OS << "i" << d << ";";
                OS << "," << (opMix.empty() ? "+" : opMix) << ",";
                for (const AccessInfo &t : terms) {
                  OS << t.base << ":";
                  for (unsigned d : t.idxDeps) OS << "i" << d << ";";
                  OS << "|";
                }
                OS << "\n";
                continue;
              }
            }
          }
        }

        // --- 形态二：线性映射。store 的值是 fadd/fmul 构成的树，且不含对同地址的 load
        //     （jacobi2d / deriche / sobel 这类 stencil 就是这一类）
        std::vector<AccessInfo> terms;
        std::string opMix;
        collectTermAccesses(val, SE, depthOf, terms, opMix);
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
        OS << "LINEAR_MAP," << F.getName().str() << "," << nest.size() << ",";
        emitTrips();
        OS << "," << accAcc.base << ":";
        for (unsigned d : accAcc.idxDeps) OS << "i" << d << ";";
        OS << "," << (opMix.empty() ? "+" : opMix) << ",";
        for (const AccessInfo &t : terms) {
          OS << t.base << ":";
          for (unsigned d : t.idxDeps) OS << "i" << d << ";";
          OS << "|";
        }
        OS << "\n";
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
void transformFunctionCmpTMR(Function &F, unsigned *outProtected) {
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

  for (CmpInst *CI : targets) {
    Instruction *insertPt = CI->getNextNode();
    if (!insertPt) continue;
    IRBuilder<> IRB(insertPt);

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
    Instruction *rep2 = inst->clone();
    std::set<Value *> itmrInternalUsers;
    bool isolated = false;
    for (unsigned k = 0; k < inst->getNumOperands(); ++k) {
      Value *op = inst->getOperand(k);
      if (isa<BasicBlock>(op)) continue;
      Value *r1 = replica1Map.count(op) ? replica1Map[op] : op;
      Value *r2 = replica2Map.count(op) ? replica2Map[op] : op;
      // 屏障只需一个操作数：有了它，两条副本与原件就已经互不相同，
      // 后续 CSE/GVN 无法折叠。每个操作数都加等于白付调度代价。
      if (r1 == op && r2 == op && !isa<Constant>(op) && !isolated) {
        r1 = insertZeroCostCSEBarrier(IRB, op);
        r2 = insertZeroCostCSEBarrier(IRB, op);
        isolated = true;
      }
      rep1->setOperand(k, r1);
      rep2->setOperand(k, r2);
    }
    IRB.Insert(rep1, inst->getName() + ".itmr1");
    IRB.Insert(rep2, inst->getName() + ".itmr2");
    // 两条副本**不**打 atft.voter.internal：打了就不再是注错点，整数通路的
    // 故障空间会停在 352 个点不变（与 native 相同），注入必然命中原件，
    // 看起来"保护有效"其实是裁判看不见冗余硬件。与 fp64 精确三模
    // （188 -> 564 个点）保持同一口径。
    if (!AtftIntTMRWire && !AtftIntTMRBoundary) { markInternal(rep1); markInternal(rep2); }
    markReplica(rep1);
    markReplica(rep2);
    itmrInternalUsers.insert(rep1);
    itmrInternalUsers.insert(rep2);
    replica1Map[inst] = rep1;
    replica2Map[inst] = rep2;

    // 边界判定：所有使用者都会被复制时，这个节点不需要表决，
    // 故障沿单条链继续传播，到第一个离开子图的点再被多数决掩盖
    bool needVote = true;
    if (AtftIntTMRBoundary) {
      needVote = false;
      for (User *U : inst->users()) {
        auto *uI = dyn_cast<Instruction>(U);
        if (!uI) { needVote = true; break; }
        if (itmrInternalUsers.count(uI)) continue;
        bool replicated = selectedSet.count(uI) || replica1Map.count(uI);
        if (!replicated) { needVote = true; break; }
      }
    }

    // 按位一致性内联表决（i1 结果不参与，避免无意义开销）
    if (needVote && isSupportedIntTy(inst->getType())) {
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
  Value *a1_f64 = markVoterInternal(IRB.CreateFPExt(a1_f32, F64Ty, "a1.f64"));
  Value *a2_f64 = markVoterInternal(IRB.CreateFPExt(a2_f32, F64Ty, "a2.f64"));

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
  if (AtftReplicaExact) {
    a12Agree = markVoterInternal(IRB.CreateFCmpOEQ(a1_f32, a2_f32, "a12.ok"));
  } else {
    Value *diffA12 = markVoterInternal(IRB.CreateCall(FabsF32, {markVoterInternal(IRB.CreateFSub(a1_f32, a2_f32))}));
    a12Agree = markVoterInternal(IRB.CreateFCmpOLE(diffA12, ConstantFP::get(F32Ty, 1e-5f), "a12.ok"));
  }

  Value *useApprox = markVoterInternal(IRB.CreateAnd(IRB.CreateNot(rIsOk), a12Agree));
  return markVoterInternal(IRB.CreateSelect(useApprox, a1_f64, rExact, "inline.approx.voted"));
}

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

  FunctionCallee VoterApproxFn = M->getOrInsertFunction(
      "atft_voter_approx_f64", F64Ty, F64Ty, F64Ty, F64Ty, F64Ty, I32Ty);
  FunctionCallee VoterApproxReexecFn = M->getOrInsertFunction(
      "atft_voter_approx_reexec_f64", F64Ty, F64Ty, F64Ty, F64Ty, F64Ty, F64Ty, I32Ty);

  FunctionCallee ApproxSinFn = M->getOrInsertFunction("atft_approx_sinf", F32Ty, F32Ty);
  FunctionCallee ApproxCosFn = M->getOrInsertFunction("atft_approx_cosf", F32Ty, F32Ty);
  FunctionCallee ApproxExpFn = M->getOrInsertFunction("atft_approx_expf", F32Ty, F32Ty);

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
      float fval = (float)CFP->getValueAPF().convertToDouble();
      Constant *cF32 = ConstantFP::get(F32Ty, fval);
      return ConstantVector::get({cF32, cF32});
    }
    Value *truncI = markInternal(IRB.CreateFPTrunc(v, F32Ty, "d2f"));
    Value *undefVec = UndefValue::get(V2F32Ty);
    Value *vec0 = markInternal(IRB.CreateInsertElement(undefVec, truncI, ConstantInt::get(I32Ty, 0)));
    Value *vec1 = markInternal(IRB.CreateInsertElement(vec0, truncI, ConstantInt::get(I32Ty, 1), "approx.v2f32"));
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
      Value *ptr = LD->getPointerOperand();
      InlineAsm *ptrBar = InlineAsm::get(
          FunctionType::get(ptr->getType(), {ptr->getType()}, false), "", "=r,0", true);
      Value *p1 = IRB.CreateCall(ptrBar, {ptr});
      Value *p2 = IRB.CreateCall(ptrBar, {ptr});
      LoadInst *ld1 = IRB.CreateLoad(F64Ty, p1, LD->getName() + ".ld.a1");
      LoadInst *ld2 = IRB.CreateLoad(F64Ty, p2, LD->getName() + ".ld.a2");
      markReplica(ld1);
      markReplica(ld2);
      Value *f1 = markInternal(IRB.CreateFPTrunc(ld1, F32Ty, "ld1.f32"));
      Value *f2 = markInternal(IRB.CreateFPTrunc(ld2, F32Ty, "ld2.f32"));
      Value *uVec = UndefValue::get(V2F32Ty);
      Value *v0 = markInternal(IRB.CreateInsertElement(uVec, f1, ConstantInt::get(I32Ty, 0)));
      Value *v1 = markInternal(IRB.CreateInsertElement(v0, f2, ConstantInt::get(I32Ty, 1), "ld.v2f32"));
      simdApproxMap[LD] = v1;
    }
  }

  for (PHINode *PN : phiNodes) {
    IRBuilder<> IRB(PN);
    PHINode *vPhi = IRB.CreatePHI(V2F32Ty, PN->getNumIncomingValues(), PN->getName() + ".v2f32_phi");
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
      Value *argF32_1 = markInternal(IRB.CreateExtractElement(vArg, ConstantInt::get(I32Ty, 0), "math.a1.arg"));
      Value *argF32_2 = markInternal(IRB.CreateExtractElement(vArg, ConstantInt::get(I32Ty, 1), "math.a2.arg"));
      StringRef cname = CI->getCalledFunction()->getName();

      Value *res1 = nullptr;
      Value *res2 = nullptr;
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

      Value *undefVec = UndefValue::get(V2F32Ty);
      Value *vec0 = markInternal(IRB.CreateInsertElement(undefVec, res1, ConstantInt::get(I32Ty, 0)));
      Value *vec1 = markInternal(IRB.CreateInsertElement(vec0, res2, ConstantInt::get(I32Ty, 1)));
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
        Value *uVec = UndefValue::get(V2F32Ty);
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
      Value *a1_f32 = IRB.CreateExtractElement(vApprox, ConstantInt::get(I32Ty, 0), "a1.f32");
      Value *a1_f64 = IRB.CreateFPExt(a1_f32, F64Ty, "a1.f64");

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
        Value *a2_f32 = markVoterInternal(IRB.CreateExtractElement(vApprox, ConstantInt::get(I32Ty, 1), "a2.f32"));
        Value *a2_f64 = markVoterInternal(IRB.CreateFPExt(a2_f32, F64Ty, "a2.f64"));
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
        if (AtftReplicaExact) {
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
        Value *a2_f32 = IRB.CreateExtractElement(vApprox, ConstantInt::get(I32Ty, 1), "a2.f32");
        Value *a2_f64 = IRB.CreateFPExt(a2_f32, F64Ty, "a2.f64");

        Value *voted = IRB.CreateCall(
            VoterApproxFn,
            {val, a1_f64, a2_f64, ConstantFP::get(F64Ty, delta),
             ConstantInt::get(I32Ty, (int)critLevel)},
            "approx_tmr.sync");
        SI->setOperand(0, voted);
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

    std::vector<FuncCriticalityInfo> critReports;

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

    if (!AtftDumpSums.empty()) {
      std::ofstream ofs(AtftDumpSums);
      if (ofs.is_open()) {
        ofs << "kind,function,nest_depth,trip_counts,accumulator,op_mix,terms\n";
        for (Function *F : targetFuncs) {
          // 先把 llvm.fmuladd 展开成 fmul + fadd：PolyBench 的
          // C[i][j] += alpha*A*B 在 IR 里是一条 fmuladd 内建，
          // 不展开就匹配不到"fadd 的一个操作数是对同一地址的 load"这个归约形态
          expandFMulAddIntrinsics(*F);
          auto &LI = FAM.getResult<LoopAnalysis>(*F);
          auto &SE = FAM.getResult<ScalarEvolutionAnalysis>(*F);
          dumpReductionSums(*F, LI, SE, ofs);
        }
      }
    }

    if (AtftMode == "analyze" || AtftMode == "native") {
      for (Function *F : targetFuncs) {
        expandFMulAddIntrinsics(*F);
      }
      if (AtftCmpTMR) {
        unsigned nProtected = 0;
        for (Function *F : targetFuncs) transformFunctionCmpTMR(*F, &nProtected);
        errs() << "ATFT_CMP_TMR_SITES," << nProtected << "\n";
      }
      return PreservedAnalyses::none();
    }

    for (size_t i = 0; i < targetFuncs.size(); ++i) {
      Function *F = targetFuncs[i];
      CritLevel clevel = critReports[i].level;
      auto &LI = FAM.getResult<LoopAnalysis>(*F);

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
          transformFunctionCmpTMR(F, &nProtected);
        }
      }
      errs() << "ATFT_CMP_TMR_SITES," << nProtected << "\n";
    }

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
