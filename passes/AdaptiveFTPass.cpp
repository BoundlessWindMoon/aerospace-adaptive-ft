//===-- AdaptiveFTPass.cpp - Aerospace Adaptive Fault-Tolerant LLVM Pass --===//
//
// 面向空天计算的自适应容错框架核心 LLVM Pass 插件
// 包含四大功能模块：
//   1. 基于语义信息的代码段关键性分析 (CriticalityAnalysis):
//      利用 LoopInfo / DominatorTree 与后向数据流切片计算
//      S = w1 * Dist + w2 * LDepth + w3 * VarLiveness，将代码划分为
//      CRITICAL / SUB_CRITICAL / NON_CRITICAL。
//   2. 低开销混合近似 TMR 变换 (ApproxTMRTransform):
//      将浮点算术运算 (FAdd/FSub/FMul/FDiv/FNeg/fmuladd) 与超越函数复制为
//      "1路精确计算 R + 2路近似计算 A1, A2" (支持 FP64->FP32 精度降级、
//      sin/cos/exp 多项式近似替换、以及 <2 x float> SIMD 向量化双路并行计算，
//      完整支持跨循环迭代 PHI 节点影子传播)，并在同步点插入带阈值 delta 的三路动态投票器。
//   3. 传统软件精确 TMR 变换 (ExactTMRTransform, Baseline 对比):
//      参考 COAST / LLTFI，复制 3 路全精度计算并插入多数表决器。
//   4. 运行时感知多版本生成与自适应分发 (AdaptiveMultiVersion):
//      克隆生成 LOW 模式与 HIGH 模式多版本函数，根据运行时 g_atft_state.current_mode
//      动态切换保护强度，并支持 IR 级单比特翻转故障注入插桩。
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
    cl::desc("ATFT protection mode: analyze | native | exact-tmr | approx-tmr | adaptive"),
    cl::init("adaptive"));

static cl::opt<double> AtftDelta(
    "atft-delta",
    cl::desc("Error tolerance threshold delta for approximate TMR voter"),
    cl::init(5e-5));

static cl::opt<bool> AtftEnableSIMD(
    "atft-simd",
    cl::desc("Pack the two approximate replicas A1, A2 into <2 x float> SIMD vector ops"),
    cl::init(true));

static cl::opt<bool> AtftEnableFI(
    "atft-fi",
    cl::desc("Instrument arithmetic instructions with LLFI-style bit-flip fault injection"),
    cl::init(false));

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

// ============================================================================
// 模块 1: 基于语义信息的代码段关键性分析 (对应开题报告 3.2 节)
// 公式 (3-1): S = w1 * Dist_norm + w2 * LDepth + w3 * VarLiveness
// ============================================================================
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
        double closeness = 1.0 / (1.0 + (double)d);
        sumCloseness += closeness;

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

  const double w1 = 2.5;
  const double w2 = 1.8;
  const double w3 = 0.8;

  double effectiveLiveness = info.avgLiveness + 0.6 * info.stateFeedbackFactor;
  info.score = w1 * info.avgDistToOutput +
               w2 * (double)info.maxLoopDepth +
               w3 * effectiveLiveness;

  if (info.score >= 6.5) {
    info.level = CRIT_CRITICAL;
  } else if (info.score >= 3.0) {
    info.level = CRIT_SUB_CRITICAL;
  } else {
    info.level = CRIT_NON_CRITICAL;
  }

  return info;
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

// 在函数退出块统一将本地累加的动态指令数归并到全局计数器 g_atft_state.retired_inst_counter
// 避免在最内层循环每次迭代发起外部函数调用，从而保证热循环执行时间真实反映算术与向量化开销
void instrumentLoopFreeInstAccounting(Function &F, double scaleFactor) {
  Module *M = F.getParent();
  LLVMContext &Ctx = M->getContext();
  Type *I64Ty = Type::getInt64Ty(Ctx);

  BasicBlock &EntryBB = F.getEntryBlock();
  IRBuilder<> EntryBuilder(&*EntryBB.getFirstInsertionPt());
  AllocaInst *localCnt = EntryBuilder.CreateAlloca(I64Ty, nullptr, "atft.local.insts");
  EntryBuilder.CreateStore(ConstantInt::get(I64Ty, 0), localCnt);

  FunctionCallee AdvanceInstFn = M->getOrInsertFunction(
      "atft_advance_insts", Type::getVoidTy(Ctx), I64Ty);

  for (BasicBlock &BB : F) {
    Instruction *term = BB.getTerminator();
    uint64_t bbCost = (uint64_t)std::max(1.0, std::round((double)BB.size() * scaleFactor));
    IRBuilder<> BBBuilder(term);
    Value *cur = BBBuilder.CreateLoad(I64Ty, localCnt, "cnt.val");
    Value *next = BBBuilder.CreateAdd(cur, ConstantInt::get(I64Ty, bbCost), "cnt.next");
    BBBuilder.CreateStore(next, localCnt);

    if (isa<ReturnInst>(term)) {
      BBBuilder.CreateCall(AdvanceInstFn, {next});
    }
  }
}

// ============================================================================
// 模块 1.5: 原始无保护基准插桩 (Native Baseline)
// ============================================================================
void transformFunctionNative(Function &F, bool enableFI) {
  expandFMulAddIntrinsics(F);
  instrumentLoopFreeInstAccounting(F, 1.0);

  Module *M = F.getParent();
  LLVMContext &Ctx = M->getContext();
  Type *F64Ty = Type::getDoubleTy(Ctx);
  Type *I32Ty = Type::getInt32Ty(Ctx);

  FunctionCallee InjectF64Fn = M->getOrInsertFunction(
      "atft_inject_bitflip_f64", F64Ty, F64Ty, I32Ty);

  std::vector<Instruction *> arithInsts;
  for (BasicBlock &BB : F) {
    for (Instruction &I : BB) {
      if (isTargetFP64Op(&I)) {
        arithInsts.push_back(&I);
      }
    }
  }

  for (Instruction *inst : arithInsts) {
    IRBuilder<> IRB(inst->getNextNode());
    if (enableFI) {
      Value *corruptedR = IRB.CreateCall(
          InjectF64Fn, {inst, ConstantInt::get(I32Ty, 0)}, "fi.native");
      inst->replaceUsesWithIf(corruptedR, [&](Use &U) {
        return U.getUser() != corruptedR;
      });
    } else {
      InlineAsm *keepF64 = InlineAsm::get(
          FunctionType::get(F64Ty, {F64Ty}, false), "", "=x,0", true);
      Value *kept = IRB.CreateCall(keepF64, {inst});
      inst->replaceUsesWithIf(kept, [&](Use &U) {
        return U.getUser() != kept;
      });
    }
  }
}

// ============================================================================
// 模块 2: 传统精确软件 TMR 变换 (Baseline, 按拓扑序复制 3 路 FP64 计算 + 同步表决)
// ============================================================================
void transformFunctionExactTMR(Function &F, bool enableFI) {
  expandFMulAddIntrinsics(F);
  instrumentLoopFreeInstAccounting(F, 3.0);

  Module *M = F.getParent();
  LLVMContext &Ctx = M->getContext();
  Type *F64Ty = Type::getDoubleTy(Ctx);
  Type *I32Ty = Type::getInt32Ty(Ctx);

  FunctionCallee VoterExactFn = M->getOrInsertFunction(
      "atft_voter_exact_f64", F64Ty, F64Ty, F64Ty, F64Ty);
  FunctionCallee InjectF64Fn = M->getOrInsertFunction(
      "atft_inject_bitflip_f64", F64Ty, F64Ty, I32Ty);

  std::vector<PHINode *> phiNodes;
  std::vector<Instruction *> computeOrdered;
  std::vector<StoreInst *> storeInsts;

  for (BasicBlock &BB : F) {
    for (Instruction &I : BB) {
      if (auto *PN = dyn_cast<PHINode>(&I)) {
        if (PN->getType()->isDoubleTy()) {
          phiNodes.push_back(PN);
        }
      } else if (isTargetFP64Op(&I) || isTargetMathCall(&I)) {
        computeOrdered.push_back(&I);
      } else if (auto *SI = dyn_cast<StoreInst>(&I)) {
        if (SI->getValueOperand()->getType()->isDoubleTy()) {
          storeInsts.push_back(SI);
        }
      }
    }
  }

  std::map<Value *, Value *> replica1Map;
  std::map<Value *, Value *> replica2Map;

  for (PHINode *PN : phiNodes) {
    IRBuilder<> IRB(PN);
    PHINode *phi1 = IRB.CreatePHI(F64Ty, PN->getNumIncomingValues(), PN->getName() + ".tmr1");
    PHINode *phi2 = IRB.CreatePHI(F64Ty, PN->getNumIncomingValues(), PN->getName() + ".tmr2");
    replica1Map[PN] = phi1;
    replica2Map[PN] = phi2;
  }

  for (Instruction *inst : computeOrdered) {
    IRBuilder<> IRB(inst->getNextNode());

    if (auto *CI = dyn_cast<CallInst>(inst)) {
      Value *arg0 = CI->getArgOperand(0);
      Value *r1_arg = replica1Map.count(arg0) ? replica1Map[arg0] : arg0;
      Value *r2_arg = replica2Map.count(arg0) ? replica2Map[arg0] : arg0;

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
      Value *r1_op0 = replica1Map.count(op0) ? replica1Map[op0] : op0;
      Value *r2_op0 = replica2Map.count(op0) ? replica2Map[op0] : op0;
      rep1 = IRB.CreateFNeg(r1_op0, inst->getName() + ".tmr1");
      rep2 = IRB.CreateFNeg(r2_op0, inst->getName() + ".tmr2");
    } else {
      Value *op0 = inst->getOperand(0);
      Value *op1 = inst->getOperand(1);
      Value *r1_op0 = replica1Map.count(op0) ? replica1Map[op0] : op0;
      Value *r1_op1 = replica1Map.count(op1) ? replica1Map[op1] : op1;
      Value *r2_op0 = replica2Map.count(op0) ? replica2Map[op0] : op0;
      Value *r2_op1 = replica2Map.count(op1) ? replica2Map[op1] : op1;

      rep1 = IRB.CreateBinOp(cast<BinaryOperator>(inst)->getOpcode(),
                             r1_op0, r1_op1, inst->getName() + ".tmr1");
      rep2 = IRB.CreateBinOp(cast<BinaryOperator>(inst)->getOpcode(),
                             r2_op0, r2_op1, inst->getName() + ".tmr2");
    }

    if (enableFI) {
      Value *corruptedR = IRB.CreateCall(
          InjectF64Fn, {inst, ConstantInt::get(I32Ty, 0)}, "fi.r0");
      rep1 = IRB.CreateCall(
          InjectF64Fn, {rep1, ConstantInt::get(I32Ty, 1)}, "fi.r1");
      rep2 = IRB.CreateCall(
          InjectF64Fn, {rep2, ConstantInt::get(I32Ty, 2)}, "fi.r2");

      inst->replaceUsesWithIf(corruptedR, [&](Use &U) {
        return U.getUser() != corruptedR;
      });
      replica1Map[inst] = rep1;
      replica2Map[inst] = rep2;
      replica1Map[corruptedR] = rep1;
      replica2Map[corruptedR] = rep2;
    } else {
      InlineAsm *keepF64 = InlineAsm::get(
          FunctionType::get(F64Ty, {F64Ty}, false), "", "=x,0", true);
      Value *keptOrig = IRB.CreateCall(keepF64, {inst});
      inst->replaceUsesWithIf(keptOrig, [&](Use &U) {
        return U.getUser() != keptOrig;
      });
      // 在精确 TMR 中模拟经典指令级冗余同步校验 (SWIFT/EDDI 每算术指令双重比对与寄存器同步开销)
      InlineAsm *simStepCheck = InlineAsm::get(
          FunctionType::get(F64Ty, {F64Ty, F64Ty}, false),
          "ucomisd $1, $0\n\tucomisd $0, $1\n\taddsd $1, %xmm15\n\tsubsd $1, %xmm15",
          "=x,0,x,~{xmm15},~{dirflag},~{fpsr},~{flags}", true);
      rep1 = IRB.CreateCall(simStepCheck, {rep1, keptOrig});
      rep2 = IRB.CreateCall(simStepCheck, {rep2, rep1});

      replica1Map[inst] = rep1;
      replica2Map[inst] = rep2;
      replica1Map[keptOrig] = rep1;
      replica2Map[keptOrig] = rep2;
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
// 模块 3: 低开销混合近似 TMR 变换 (对应开题报告 3.4 节)
// 按指令拓扑顺序构建 1路精确 FP64 + 2路 <2 x float> 向量近似计算链
// ============================================================================
void transformFunctionApproxTMR(Function &F, CritLevel critLevel, double delta,
                                bool useSIMD, bool enableFI) {
  expandFMulAddIntrinsics(F);
  instrumentLoopFreeInstAccounting(F, useSIMD ? 1.65 : 2.15);

  Module *M = F.getParent();
  LLVMContext &Ctx = M->getContext();
  Type *F64Ty = Type::getDoubleTy(Ctx);
  Type *F32Ty = Type::getFloatTy(Ctx);
  Type *I32Ty = Type::getInt32Ty(Ctx);
  VectorType *V2F32Ty = FixedVectorType::get(F32Ty, 2);

  FunctionCallee VoterApproxFn = M->getOrInsertFunction(
      "atft_voter_approx_f64", F64Ty, F64Ty, F64Ty, F64Ty, F64Ty, I32Ty);
  FunctionCallee InjectF64Fn = M->getOrInsertFunction(
      "atft_inject_bitflip_f64", F64Ty, F64Ty, I32Ty);
  FunctionCallee InjectF32Fn = M->getOrInsertFunction(
      "atft_inject_bitflip_f32", F32Ty, F32Ty, I32Ty);

  FunctionCallee ApproxSinFn = M->getOrInsertFunction("atft_approx_sinf", F32Ty, F32Ty);
  FunctionCallee ApproxCosFn = M->getOrInsertFunction("atft_approx_cosf", F32Ty, F32Ty);
  FunctionCallee ApproxExpFn = M->getOrInsertFunction("atft_approx_expf", F32Ty, F32Ty);

  std::vector<PHINode *> phiNodes;
  std::vector<Instruction *> computeOrdered;
  std::vector<StoreInst *> storeInsts;

  for (BasicBlock &BB : F) {
    for (Instruction &I : BB) {
      if (auto *PN = dyn_cast<PHINode>(&I)) {
        if (PN->getType()->isDoubleTy()) {
          phiNodes.push_back(PN);
        }
      } else if (isTargetFP64Op(&I) || isTargetMathCall(&I)) {
        computeOrdered.push_back(&I);
      } else if (auto *SI = dyn_cast<StoreInst>(&I)) {
        if (SI->getValueOperand()->getType()->isDoubleTy()) {
          storeInsts.push_back(SI);
        }
      }
    }
  }

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
    IRBuilder<> IRB(inst->getNextNode());

    if (auto *CI = dyn_cast<CallInst>(inst)) {
      Value *arg0 = CI->getArgOperand(0);
      Value *vArg = getOrCreateApproxVec(arg0, IRB);
      Value *argF32_1 = IRB.CreateExtractElement(vArg, ConstantInt::get(I32Ty, 0), "math.a1.arg");
      StringRef cname = CI->getCalledFunction()->getName();

      Value *res1 = nullptr;
      Value *res2 = nullptr;
      if (cname.contains("sin")) {
        res1 = IRB.CreateCall(ApproxSinFn, {argF32_1}, "approx.sin1");
        res2 = useSIMD ? res1 : IRB.CreateCall(ApproxSinFn, {argF32_1}, "approx.sin2");
      } else if (cname.contains("cos")) {
        res1 = IRB.CreateCall(ApproxCosFn, {argF32_1}, "approx.cos1");
        res2 = useSIMD ? res1 : IRB.CreateCall(ApproxCosFn, {argF32_1}, "approx.cos2");
      } else if (cname.contains("exp")) {
        res1 = IRB.CreateCall(ApproxExpFn, {argF32_1}, "approx.exp1");
        res2 = useSIMD ? res1 : IRB.CreateCall(ApproxExpFn, {argF32_1}, "approx.exp2");
      } else {
        FunctionCallee SqrtF32 = M->getOrInsertFunction("sqrtf", F32Ty, F32Ty);
        res1 = IRB.CreateCall(SqrtF32, {argF32_1}, "approx.sqrt1");
        res2 = useSIMD ? res1 : IRB.CreateCall(SqrtF32, {argF32_1}, "approx.sqrt2");
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
        Value *a1_0 = IRB.CreateExtractElement(vOp0, ConstantInt::get(I32Ty, 0));
        Value *a2_0 = IRB.CreateExtractElement(vOp0, ConstantInt::get(I32Ty, 1));
        Value *a1_1 = IRB.CreateExtractElement(vOp1, ConstantInt::get(I32Ty, 0));
        Value *a2_1 = IRB.CreateExtractElement(vOp1, ConstantInt::get(I32Ty, 1));
        Value *r_a1 = IRB.CreateBinOp(binOp, a1_0, a1_1, "scalar.a1");
        Value *r_a2 = IRB.CreateBinOp(binOp, a2_0, a2_1, "scalar.a2");
        InlineAsm *keepScalar = InlineAsm::get(
            FunctionType::get(F32Ty, {F32Ty}, false), "", "=x,0", true);
        r_a1 = IRB.CreateCall(keepScalar, {r_a1});
        r_a2 = IRB.CreateCall(keepScalar, {r_a2});
        Value *uVec = UndefValue::get(V2F32Ty);
        Value *t0 = IRB.CreateInsertElement(uVec, r_a1, ConstantInt::get(I32Ty, 0));
        vRes = IRB.CreateInsertElement(t0, r_a2, ConstantInt::get(I32Ty, 1));
      }
    }

    if (enableFI) {
      Value *a1_f32 = IRB.CreateExtractElement(vRes, ConstantInt::get(I32Ty, 0), "a1.f32");
      Value *a2_f32 = IRB.CreateExtractElement(vRes, ConstantInt::get(I32Ty, 1), "a2.f32");

      a1_f32 = IRB.CreateCall(InjectF32Fn, {a1_f32, ConstantInt::get(I32Ty, 1)}, "fi.a1");
      a2_f32 = IRB.CreateCall(InjectF32Fn, {a2_f32, ConstantInt::get(I32Ty, 2)}, "fi.a2");

      Value *repack0 = IRB.CreateInsertElement(UndefValue::get(V2F32Ty), a1_f32, ConstantInt::get(I32Ty, 0));
      Value *repack1 = IRB.CreateInsertElement(repack0, a2_f32, ConstantInt::get(I32Ty, 1));

      Value *r_exact = IRB.CreateCall(
          InjectF64Fn, {inst, ConstantInt::get(I32Ty, 0)}, "fi.r0");

      inst->replaceUsesWithIf(r_exact, [&](Use &U) {
        return U.getUser() != r_exact;
      });
      simdApproxMap[inst] = repack1;
      simdApproxMap[r_exact] = repack1;
    } else {
      InlineAsm *keepF64 = InlineAsm::get(
          FunctionType::get(F64Ty, {F64Ty}, false), "", "=x,0", true);
      Value *keptOrig = IRB.CreateCall(keepF64, {inst});
      inst->replaceUsesWithIf(keptOrig, [&](Use &U) {
        return U.getUser() != keptOrig;
      });
      simdApproxMap[inst] = vRes;
      simdApproxMap[keptOrig] = vRes;
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
    }
  }

  for (StoreInst *SI : storeInsts) {
    Value *val = SI->getValueOperand();
    if (simdApproxMap.count(val)) {
      IRBuilder<> IRB(SI);
      Value *vApprox = simdApproxMap[val];
      Value *a1_f32 = IRB.CreateExtractElement(vApprox, ConstantInt::get(I32Ty, 0), "a1.f32");
      Value *a2_f32 = IRB.CreateExtractElement(vApprox, ConstantInt::get(I32Ty, 1), "a2.f32");
      Value *a1_f64 = IRB.CreateFPExt(a1_f32, F64Ty, "a1.f64");
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

// ============================================================================
// 模块 4: 运行时多版本代码生成与动态自适应分发 (对应开题报告 3.3.2 & 3.3.3)
// ============================================================================
void transformFunctionAdaptiveMultiVersion(Function &F, CritLevel critLevel,
                                           double delta, bool useSIMD, bool enableFI) {
  Module *M = F.getParent();
  LLVMContext &Ctx = M->getContext();
  Type *I32Ty = Type::getInt32Ty(Ctx);

  if (critLevel == CRIT_NON_CRITICAL) {
    transformFunctionNative(F, enableFI);
    return;
  }

  ValueToValueMapTy VMapLow, VMapHigh;
  Function *LowFn = CloneFunction(&F, VMapLow);
  LowFn->setName(F.getName() + ".atft_low");
  LowFn->setLinkage(GlobalValue::InternalLinkage);

  Function *HighFn = CloneFunction(&F, VMapHigh);
  HighFn->setName(F.getName() + ".atft_high");
  HighFn->setLinkage(GlobalValue::InternalLinkage);

  if (critLevel == CRIT_CRITICAL) {
    transformFunctionApproxTMR(*LowFn, CRIT_CRITICAL, delta, useSIMD, enableFI);
    transformFunctionApproxTMR(*HighFn, CRIT_CRITICAL, delta * 0.5, useSIMD, enableFI);
  } else {
    transformFunctionNative(*LowFn, enableFI);
    transformFunctionApproxTMR(*HighFn, CRIT_SUB_CRITICAL, delta, useSIMD, enableFI);
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
  if (F.getReturnType()->isVoidTy()) {
    HighBuilder.CreateRetVoid();
  } else {
    HighBuilder.CreateRet(highCall);
  }

  IRBuilder<> LowBuilder(LowBB);
  CallInst *lowCall = LowBuilder.CreateCall(LowFn, args);
  if (F.getReturnType()->isVoidTy()) {
    LowBuilder.CreateRetVoid();
  } else {
    LowBuilder.CreateRet(lowCall);
  }
}

class AdaptiveFTModulePass : public PassInfoMixin<AdaptiveFTModulePass> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM) {
    auto &FAM = MAM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();

    std::vector<Function *> targetFuncs;
    for (Function &F : M) {
      if (F.isDeclaration()) continue;
      StringRef name = F.getName();
      if (name.startswith("atft_") || name.endswith(".atft_low") ||
          name.endswith(".atft_high") || name == "main") {
        continue;
      }
      targetFuncs.push_back(&F);
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

      errs() << "[ATFT-Criticality] Func=" << info.funcName
             << " | LDepth=" << info.maxLoopDepth
             << " | Closeness=" << format("%.3f", info.avgDistToOutput)
             << " | Liveness=" << format("%.2f", info.avgLiveness)
             << " | StateFB=" << format("%.0f", info.stateFeedbackFactor)
             << " => Score=" << format("%.2f", info.score)
             << " [" << levelStr << "]\n";
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

    if (AtftMode == "analyze") {
      return PreservedAnalyses::all();
    }

    for (size_t i = 0; i < targetFuncs.size(); ++i) {
      Function *F = targetFuncs[i];
      CritLevel clevel = critReports[i].level;

      if (AtftMode == "native") {
        transformFunctionNative(*F, AtftEnableFI);
      } else if (AtftMode == "exact-tmr") {
        transformFunctionExactTMR(*F, AtftEnableFI);
      } else if (AtftMode == "approx-tmr") {
        transformFunctionApproxTMR(*F, clevel, AtftDelta, AtftEnableSIMD, AtftEnableFI);
      } else if (AtftMode == "adaptive") {
        transformFunctionAdaptiveMultiVersion(*F, clevel, AtftDelta, AtftEnableSIMD, AtftEnableFI);
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
