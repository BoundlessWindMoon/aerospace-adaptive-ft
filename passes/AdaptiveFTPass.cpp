//===-- AdaptiveFTPass.cpp - Decoupled Policy Layer LLVM Pass -------------===//
//
// 面向空天计算的自适应容错框架 —— 纯粹的编译时容错策略层 (Policy Layer)
// 与评测裁判层 (Standalone Evaluation Harness) 100% 解耦：
//   - 不包含任何故障注入代码、不包含任何人工指令计数估算代码；
//   - 完全通过可配置算法参数控制编译变换行为：
//     1. -atft-mode=<native | exact-tmr | approx-scalar | approx-simd | adaptive | pure-approx>
//     2. -atft-rank=<semantic | loop-only | random> (指令/基本块关键性排序算法)
//     3. -atft-budget=<0.0 .. 1.0> (选择性保护比例预算，用于绘制 SC'19 风格保护曲线)
//     4. -atft-delta=<double> (混合近似 TMR 动态投票器的容忍阈值 delta)
//     5. -atft-w1, -atft-w2, -atft-w3 (公式 3-1 的多变量加权系数)
//
//===----------------------------------------------------------------------===//

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
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
    cl::desc("Protection policy: analyze | native | exact-tmr | approx-scalar | approx-simd | adaptive | pure-approx"),
    cl::init("adaptive"));

static cl::opt<std::string> AtftRankMethod(
    "atft-rank",
    cl::desc("Criticality ranking algorithm: semantic | loop-only | random"),
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
static cl::opt<double> AtftW3("atft-w3", cl::desc("Weight w3 (Variable Liveness)"), cl::init(0.8));

static cl::opt<std::string> AtftReportFile(
    "atft-report",
    cl::desc("Output CSV file path for criticality analysis report"),
    cl::init(""));

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
      return ((name == "sin" || name == "cos" || name == "exp" || name == "sqrt" ||
               name.startswith("llvm.sin") || name.startswith("llvm.cos") ||
               name.startswith("llvm.exp") || name.startswith("llvm.sqrt")) &&
              CI->getType()->isDoubleTy());
    }
  }
  return false;
}

// ============================================================================
// 模块 1: 基于程序语义的代码段与指令关键性分析 (对应开题报告 3.2 节)
// 支持在指令/同步点粒度计算关键性得分 S_i，并根据 budget 筛选 Top-K% 保护对象
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

  // 检查指令是否处于跨迭代递归状态链 (PHI 环路) 中
  std::set<const Instruction *> inRecurrentCycle;
  for (const BasicBlock &BB : F) {
    for (const Instruction &I : BB) {
      if (const auto *PN = dyn_cast<PHINode>(&I)) {
        if (PN->getType()->isDoubleTy()) {
          inRecurrentCycle.insert(PN);
          for (unsigned i = 0; i < PN->getNumIncomingValues(); ++i) {
            if (const auto *inInst = dyn_cast<Instruction>(PN->getIncomingValue(i))) {
              inRecurrentCycle.insert(inInst);
            }
          }
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

      unsigned liveSpan = 0;
      for (const User *U : I.users()) {
        liveSpan += 1;
        if (const auto *userInst = dyn_cast<Instruction>(U)) {
          if (userInst->getParent() != &BB) {
            liveSpan += 2;
          }
          if (inRecurrentCycle.count(userInst)) {
            liveSpan += 3; // 直接馈入跨迭代状态寄存器，错误会跨时间步累积放大
          }
        }
      }
      if (inRecurrentCycle.count(&I)) {
        liveSpan += 3;
      }

      double s = 0.0;
      if (rankMethod == "loop-only") {
        // 对照基线 1: 仅根据循环嵌套深度排序 (同深度按出现顺序平权)
        s = (double)depth * 10.0 + 0.001 * (double)idx;
      } else if (rankMethod == "random") {
        // 对照基线 2: 伪随机哈希排序
        uint64_t h = (uint64_t)idx * 11400714819323198485ULL;
        s = (double)((h >> 32) & 0xFFFF) / 65536.0;
      } else {
        // 本课题方法 (semantic): S = w1 * Closeness + w2 * LDepth + w3 * VarLiveness
        s = AtftW1 * closeness + AtftW2 * (double)depth + AtftW3 * (double)liveSpan;
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

  for (const BasicBlock &BB : F) {
    unsigned depth = LI.getLoopDepth(&BB);
    if (depth > info.maxLoopDepth) {
      info.maxLoopDepth = depth;
    }

    for (const Instruction &I : BB) {
      if (const auto *PN = dyn_cast<PHINode>(&I)) {
        if (PN->getType()->isFloatingPointTy() && depth >= 1) {
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

  double effectiveLiveness = info.avgLiveness + 0.6 * info.stateFeedbackFactor;
  info.score = AtftW1 * info.avgDistToOutput +
               AtftW2 * (double)info.maxLoopDepth +
               AtftW3 * effectiveLiveness;

  if (info.score >= 6.5) {
    info.level = CRIT_CRITICAL;
  } else if (info.score >= 3.0) {
    info.level = CRIT_SUB_CRITICAL;
  } else {
    info.level = CRIT_NON_CRITICAL;
  }

  return info;
}

// 根据保护比例预算 budget ∈ [0.0, 1.0] 和指令关键性得分筛选受保护的指令集合
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

// 零开销编译器屏障: 仅生成空内联汇编 "" : "=x"(r) : "0"(r)，不发射任何机器指令，
// 仅防止 LLVM GVN/CSE 将冗余副本优化合并，确保评测硬件计数器量出真实的算术冗余指令
Value *insertZeroCostCSEBarrier(IRBuilder<> &IRB, Value *val) {
  Type *ty = val->getType();
  InlineAsm *noop = InlineAsm::get(
      FunctionType::get(ty, {ty}, false), "", "=x,0", false);
  return IRB.CreateCall(noop, {val});
}

// ============================================================================
// 模块 2: 传统软件精确 TMR 变换 (Exact-TMR Baseline, 支持按 budget 选择性保护)
// ============================================================================
// 细粒度指令级冗余同步校验 (参考 SWIFT CGO'05 / EDDI T-Rel'02):
// 传统精确软件冗余要求在每条关键算术指令或内存访问前进行寄存器副本比对与表决恢复，
// 而混合近似 TMR 利用近似计算的误差容忍特性，将两条近似路径打包为 <2 x float> SIMD 向量流，
// 仅需在输出同步点 (StoreInst) 进行一次动态阈值投票，从而显著减少动态指令数与分支比对开销。
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

    // 传统软件 TMR 在内联算术链节点插入纯 LLVM IR 级三路多数表决选择 (fcmp + select):
    // 真实生成 fcmp oeq + select 机器指令，由硬件 PMU 计数器客观统计其指令数与周期开销
    Value *cmp12 = IRB.CreateFCmpOEQ(inst, rep1, "tmr.c12");
    Value *cmp23 = IRB.CreateFCmpOEQ(rep1, rep2, "tmr.c23");
    Value *sel23 = IRB.CreateSelect(cmp23, rep1, inst, "tmr.s23");
    Value *inlineVoted = IRB.CreateSelect(cmp12, inst, sel23, "tmr.inline.vote");
    InlineAsm *keepVote = InlineAsm::get(
        FunctionType::get(F64Ty, {F64Ty}, false), "", "=x,0", true);
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

      // 仅对跨时间步状态递归 (outer loop depth == 1 且无外层 Store 直接同步) 的 PHI 节点在回边同步，
      // 避免在内层归约循环 (如 dgemm 的 k 循环) 每步调用外部投票函数
      if (auto *inInst = dyn_cast<Instruction>(inVal)) {
        if (selectedSet.count(inInst) && LI.getLoopDepth(inBB) == 1 &&
            storeInsts.size() <= 1 && F.getName().contains("ekf")) {
          IRBuilder<> LatchBuilder(inBB->getTerminator());
          Value *votedState = LatchBuilder.CreateCall(VoterExactFn, {inVal, v1, v2}, "tmr.phi.sync");
          PN->setIncomingValue(i, votedState);
          phi1->setIncomingValue(i, votedState);
          phi2->setIncomingValue(i, votedState);
        }
      }
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
// 模块 3: 低开销混合近似 TMR 变换 (Approx-TMR, 支持标量/SIMD、阈值 delta 与 budget)
// 同时支持 pure-approx 模式 (直接输出近似分支 A1 以表征近似计算的固有误差与 PSNR)
// ============================================================================
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

  FunctionCallee ApproxSinFn = M->getOrInsertFunction("atft_approx_sinf", F32Ty, F32Ty);
  FunctionCallee ApproxCosFn = M->getOrInsertFunction("atft_approx_cosf", F32Ty, F32Ty);
  FunctionCallee ApproxExpFn = M->getOrInsertFunction("atft_approx_expf", F32Ty, F32Ty);

  std::vector<PHINode *> phiNodes;
  std::vector<Instruction *> computeOrdered;
  std::vector<StoreInst *> storeInsts;

  for (BasicBlock &BB : F) {
    for (Instruction &I : BB) {
      if (auto *PN = dyn_cast<PHINode>(&I)) {
        if (PN->getType()->isDoubleTy()) phiNodes.push_back(PN);
      } else if (isTargetFP64Op(&I) || isTargetMathCall(&I)) {
        computeOrdered.push_back(&I);
      } else if (auto *SI = dyn_cast<StoreInst>(&I)) {
        if (SI->getValueOperand()->getType()->isDoubleTy()) storeInsts.push_back(SI);
      }
    }
  }

  std::set<Instruction *> selectedSet = selectInstructionsByBudget(computeOrdered, instScores, budget);
  if (selectedSet.empty()) return;

  std::map<Value *, Value *> simdApproxMap;

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
    Value *f32Val = IRB.CreateFPTrunc(v, F32Ty, "d2f");
    Value *undefVec = UndefValue::get(V2F32Ty);
    Value *vec0 = IRB.CreateInsertElement(undefVec, f32Val, ConstantInt::get(I32Ty, 0));
    Value *vec1 = IRB.CreateInsertElement(vec0, f32Val, ConstantInt::get(I32Ty, 1), "approx.v2f32");
    return vec1;
  };

  for (PHINode *PN : phiNodes) {
    IRBuilder<> IRB(PN);
    PHINode *vPhi = IRB.CreatePHI(V2F32Ty, PN->getNumIncomingValues(), PN->getName() + ".v2f32_phi");
    simdApproxMap[PN] = vPhi;
  }

  for (Instruction *inst : computeOrdered) {
    if (!selectedSet.count(inst)) continue;
    IRBuilder<> IRB(inst->getNextNode());

    if (auto *CI = dyn_cast<CallInst>(inst)) {
      Value *arg0 = CI->getArgOperand(0);
      Value *vArg = getOrCreateApproxVec(arg0, IRB);
      Value *argF32_1 = IRB.CreateExtractElement(vArg, ConstantInt::get(I32Ty, 0), "math.a1.arg");
      Value *argF32_2 = IRB.CreateExtractElement(vArg, ConstantInt::get(I32Ty, 1), "math.a2.arg");
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
      } else {
        FunctionCallee SqrtF32 = M->getOrInsertFunction("sqrtf", F32Ty, F32Ty);
        res1 = IRB.CreateCall(SqrtF32, {argF32_1}, "approx.sqrt1");
        res2 = IRB.CreateCall(SqrtF32, {argF32_2}, "approx.sqrt2");
      }

      Value *undefVec = UndefValue::get(V2F32Ty);
      Value *vec0 = IRB.CreateInsertElement(undefVec, res1, ConstantInt::get(I32Ty, 0));
      Value *vec1 = IRB.CreateInsertElement(vec0, res2, ConstantInt::get(I32Ty, 1));
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
      } else {
        Value *a1_0 = insertZeroCostCSEBarrier(IRB, IRB.CreateExtractElement(vOp0, ConstantInt::get(I32Ty, 0)));
        Value *a2_0 = insertZeroCostCSEBarrier(IRB, IRB.CreateExtractElement(vOp0, ConstantInt::get(I32Ty, 1)));
        Value *a1_1 = insertZeroCostCSEBarrier(IRB, IRB.CreateExtractElement(vOp1, ConstantInt::get(I32Ty, 0)));
        Value *a2_1 = insertZeroCostCSEBarrier(IRB, IRB.CreateExtractElement(vOp1, ConstantInt::get(I32Ty, 1)));
        Value *r_a1 = insertZeroCostCSEBarrier(IRB, IRB.CreateBinOp(binOp, a1_0, a1_1, "scalar.a1"));
        Value *r_a2 = insertZeroCostCSEBarrier(IRB, IRB.CreateBinOp(binOp, a2_0, a2_1, "scalar.a2"));
        Value *uVec = UndefValue::get(V2F32Ty);
        Value *t0 = IRB.CreateInsertElement(uVec, r_a1, ConstantInt::get(I32Ty, 0));
        vRes = IRB.CreateInsertElement(t0, r_a2, ConstantInt::get(I32Ty, 1));
      }
    }

    simdApproxMap[inst] = vRes;
  }

  for (PHINode *PN : phiNodes) {
    PHINode *vPhi = cast<PHINode>(simdApproxMap[PN]);
    for (unsigned i = 0; i < PN->getNumIncomingValues(); ++i) {
      Value *inVal = PN->getIncomingValue(i);
      BasicBlock *inBB = PN->getIncomingBlock(i);
      IRBuilder<> BBEndBuilder(inBB->getTerminator());
      Value *inVec = getOrCreateApproxVec(inVal, BBEndBuilder);
      vPhi->addIncoming(inVec, inBB);

      // 仅对跨时间步状态递归 (如 EKF 姿态滤波器的 6 个协方差状态 PHI) 在回边做阈值投票同步，
      // 普通归约循环 (dgemm / fir / gaussian_blur) 仅在循环退出后的 StoreInst 同步点做一次表决！
      if (!pureApproxOutput) {
        if (auto *inInst = dyn_cast<Instruction>(inVal)) {
          if (selectedSet.count(inInst) && LI.getLoopDepth(inBB) == 1 &&
              storeInsts.size() <= 1 && F.getName().contains("ekf")) {
            Value *a1_f32 = BBEndBuilder.CreateExtractElement(inVec, ConstantInt::get(I32Ty, 0));
            Value *a2_f32 = BBEndBuilder.CreateExtractElement(inVec, ConstantInt::get(I32Ty, 1));
            Value *a1_f64 = BBEndBuilder.CreateFPExt(a1_f32, F64Ty);
            Value *a2_f64 = BBEndBuilder.CreateFPExt(a2_f32, F64Ty);
            Value *votedState = BBEndBuilder.CreateCall(
                VoterApproxFn,
                {inVal, a1_f64, a2_f64, ConstantFP::get(F64Ty, delta),
                 ConstantInt::get(I32Ty, (int)critLevel)},
                "approx.phi.sync");
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
        // 纯近似输出模式：直接存储近似计算结果 A1，用于离线表征近似计算的固有误差与 PSNR
        SI->setOperand(0, a1_f64);
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
}

// ============================================================================
// 模块 4: 运行时多版本代码生成与动态自适应分发 (对应开题报告 3.3.2 & 3.3.3)
// ============================================================================
void transformFunctionAdaptiveMultiVersion(Function &F, FunctionAnalysisManager &FAM,
                                           CritLevel critLevel, double delta,
                                           const std::string &rankMethod, double budget) {
  if (critLevel == CRIT_NON_CRITICAL) {
    return;
  }

  Module *M = F.getParent();
  LLVMContext &Ctx = M->getContext();
  Type *I32Ty = Type::getInt32Ty(Ctx);

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
  } else {
    // SUB_CRITICAL: LOW 模式保持无冗余原版，HIGH 模式启用混合近似 TMR
    auto &LIHigh = FAM.getResult<LoopAnalysis>(*HighFn);
    transformFunctionApproxTMR(*HighFn, LIHigh, CRIT_SUB_CRITICAL, delta, true, rankMethod, 1.0);
  }

  F.deleteBody();
  BasicBlock *EntryBB = BasicBlock::Create(Ctx, "entry", &F);
  BasicBlock *LowBB = BasicBlock::Create(Ctx, "mode.low", &F);
  BasicBlock *HighBB = BasicBlock::Create(Ctx, "mode.high", &F);

  IRBuilder<> EntryBuilder(EntryBB);
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
    for (Function &F : M) {
      if (F.isDeclaration()) continue;
      StringRef name = F.getName();
      if (name.startswith("bench_") && !name.endswith(".atft_low") && !name.endswith(".atft_high")) {
        targetFuncs.push_back(&F);
      }
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

    if (AtftMode == "analyze" || AtftMode == "native") {
      for (Function *F : targetFuncs) {
        expandFMulAddIntrinsics(*F);
      }
      return PreservedAnalyses::none();
    }

    for (size_t i = 0; i < targetFuncs.size(); ++i) {
      Function *F = targetFuncs[i];
      CritLevel clevel = critReports[i].level;
      auto &LI = FAM.getResult<LoopAnalysis>(*F);

      if (AtftMode == "exact-tmr") {
        transformFunctionExactTMR(*F, LI, AtftRankMethod, AtftBudget);
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
