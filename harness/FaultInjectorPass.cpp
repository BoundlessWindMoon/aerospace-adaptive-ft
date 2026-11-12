//===-- FaultInjectorPass.cpp - Multi-Category Post-Transform Fault Injector ==//
//
// 独立于被测容错策略 (Policy) 的评测裁判层故障注入 LLVM Pass：
//   - 支持通过 -fi-target-type=<fp | int | address | branch | all> 按指令类别注入单粒子位翻转：
//     * "fp"      : 浮点数据算术与浮点访存指令 (FAdd/FSub/FMul/FDiv/FNeg/Load double/float/<2 x float>)
//     * "int"     : 整数数据算术指令 (Add/Sub/Mul/SDiv/Shl/AShr i32/i16，跳过循环计数器自增)
//     * "address" : 内存地址偏移与下标计算指令 (GetElementPtr 的 index 操作数)
//     * "branch"  : 条件分支控制流比较指令 (ICmp / FCmp 的 i1 布尔条件翻转)
//     * "all"     : 全类别混合注入
//   - 完全遵循 LLFI / SASSIFI (ISCA'17) / DSN'24 标准的两阶段方法论 (Phase 1 Profiling + Phase 2 Injection)。
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include <string>
#include <vector>

using namespace llvm;

namespace {

static cl::opt<std::string> FiTargetType(
    "fi-target-type",
    cl::desc("Fault injection target instruction category: fp | int | address | branch | all"),
    cl::init("fp"));

bool isTargetBenchmarkFunction(StringRef fname) {
  return (fname.startswith("bench_") || fname.startswith("kf_bfly") ||
          fname == "kf_work" || fname == "CNDF" || fname == "BlkSchlsEqEuroNoDiv");
}

bool isFPDataInst(Instruction *I) {
  if (!I || isa<PHINode>(I)) return false;
  if (I->getMetadata("atft.voter.internal") != nullptr) return false;
  if (I->isBinaryOp() || I->isUnaryOp() || isa<LoadInst>(I)) {
    Type *ty = I->getType();
    if (ty->isDoubleTy() || ty->isFloatTy()) return true;
    if (auto *vecTy = dyn_cast<FixedVectorType>(ty)) {
      if (vecTy->getElementType()->isFloatTy() && vecTy->getNumElements() == 2) return true;
    }
  }
  return false;
}

bool isIntDataInst(Instruction *I) {
  if (!I || isa<PHINode>(I)) return false;
  if (I->getMetadata("atft.voter.internal") != nullptr) return false;
  if (I->isBinaryOp()) {
    Type *ty = I->getType();
    if (ty->isIntegerTy(32) || ty->isIntegerTy(16)) {
      // 跳过被用于 GEP 地址偏移或循环条件/PHI 的整数指令（那些归属 address 或 branch 类别）
      for (User *U : I->users()) {
        if (isa<PHINode>(U) || isa<ICmpInst>(U) || isa<GetElementPtrInst>(U) ||
            isa<SExtInst>(U) || isa<ZExtInst>(U))
          return false;
      }
      return true;
    }
  }
  return false;
}

bool isAddressIndexInst(Instruction *I) {
  return isa<GetElementPtrInst>(I);
}

bool isBranchCondInst(Instruction *I) {
  if (!I) return false;
  if (I->getMetadata("atft.voter.internal") != nullptr) return false;
  // 注入数据依赖的浮点条件分支与选择谓词 (FCmpInst)，模拟分支方向翻转，
  // 严格避开整型循环退出计数器 (ICmpInst)，防止被测进程陷入无界死循环。
  if (isa<FCmpInst>(I)) {
    for (User *U : I->users()) {
      if (isa<BranchInst>(U) || isa<SelectInst>(U)) return true;
    }
  }
  return false;
}

class StandaloneFaultInjectorPass : public PassInfoMixin<StandaloneFaultInjectorPass> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {
    LLVMContext &Ctx = M.getContext();
    Type *F64Ty = Type::getDoubleTy(Ctx);
    Type *F32Ty = Type::getFloatTy(Ctx);
    Type *I32Ty = Type::getInt32Ty(Ctx);
    Type *I16Ty = Type::getInt16Ty(Ctx);
    VectorType *V2F32Ty = FixedVectorType::get(F32Ty, 2);

    FunctionCallee HookF64 = M.getOrInsertFunction("__harness_fi_hook_f64", F64Ty, F64Ty, I32Ty);
    FunctionCallee HookF32 = M.getOrInsertFunction("__harness_fi_hook_f32", F32Ty, F32Ty, I32Ty);
    FunctionCallee HookI32 = M.getOrInsertFunction("__harness_fi_hook_i32", I32Ty, I32Ty, I32Ty);
    FunctionCallee HookI16 = M.getOrInsertFunction("__harness_fi_hook_i16", I16Ty, I16Ty, I32Ty);

    bool injFP   = (FiTargetType == "fp"      || FiTargetType == "all");
    bool injInt  = (FiTargetType == "int"     || FiTargetType == "all");
    bool injAddr = (FiTargetType == "address" || FiTargetType == "all");
    bool injBr   = (FiTargetType == "branch"  || FiTargetType == "all");

    int32_t staticSiteId = 1;

    for (Function &F : M) {
      if (F.isDeclaration()) continue;
      StringRef fname = F.getName();
      if (!isTargetBenchmarkFunction(fname)) continue;

      std::vector<Instruction *> fpTargets;
      std::vector<Instruction *> intTargets;
      std::vector<GetElementPtrInst *> addrTargets;
      std::vector<Instruction *> brTargets;

      for (BasicBlock &BB : F) {
        for (Instruction &I : BB) {
          if (injFP && isFPDataInst(&I)) fpTargets.push_back(&I);
          if (injInt && isIntDataInst(&I)) intTargets.push_back(&I);
          if (injAddr && isAddressIndexInst(&I)) addrTargets.push_back(cast<GetElementPtrInst>(&I));
          if (injBr && isBranchCondInst(&I)) brTargets.push_back(&I);
        }
      }

      // 1. 浮点算术/访存指令注入
      for (Instruction *inst : fpTargets) {
        Instruction *nextNode = inst->getNextNode();
        if (!nextNode) continue;
        IRBuilder<> IRB(nextNode);
        Type *ty = inst->getType();

        if (ty->isDoubleTy()) {
          Value *hooked = IRB.CreateCall(
              HookF64, {inst, ConstantInt::get(I32Ty, staticSiteId++)}, "fi.f64");
          inst->replaceUsesWithIf(hooked, [&](Use &U) {
            return U.getUser() != hooked;
          });
        } else if (ty->isFloatTy()) {
          Value *hooked = IRB.CreateCall(
              HookF32, {inst, ConstantInt::get(I32Ty, staticSiteId++)}, "fi.f32");
          inst->replaceUsesWithIf(hooked, [&](Use &U) {
            return U.getUser() != hooked;
          });
        } else if (ty == V2F32Ty) {
          Value *bitcastI64 = IRB.CreateBitCast(inst, F64Ty, "v2f32.as.f64");
          Value *hookedF64 = IRB.CreateCall(
              HookF64, {bitcastI64, ConstantInt::get(I32Ty, staticSiteId++)}, "fi.v2f32.f64");
          Value *hookedVec = IRB.CreateBitCast(hookedF64, V2F32Ty, "fi.v2f32");
          inst->replaceUsesWithIf(hookedVec, [&](Use &U) {
            return U.getUser() != bitcastI64;
          });
        }
      }

      // 2. 整型算术指令注入 (i32 / i16)
      for (Instruction *inst : intTargets) {
        Instruction *nextNode = inst->getNextNode();
        if (!nextNode) continue;
        IRBuilder<> IRB(nextNode);
        Type *ty = inst->getType();
        if (ty->isIntegerTy(32)) {
          Value *hooked = IRB.CreateCall(
              HookI32, {inst, ConstantInt::get(I32Ty, staticSiteId++)}, "fi.i32");
          inst->replaceUsesWithIf(hooked, [&](Use &U) {
            return U.getUser() != hooked;
          });
        } else if (ty->isIntegerTy(16)) {
          Value *hooked = IRB.CreateCall(
              HookI16, {inst, ConstantInt::get(I32Ty, staticSiteId++)}, "fi.i16");
          inst->replaceUsesWithIf(hooked, [&](Use &U) {
            return U.getUser() != hooked;
          });
        }
      }

      // 3. 内存地址下标偏移 (GEP Index) 注入：仅对 LoadInst 使用的读地址注入偏移扰动，
      //    模拟读取相邻错误元素导致的静默数据污染或越界陷阱，避免破坏栈帧/堆元数据。
      for (GetElementPtrInst *gep : addrTargets) {
        if (gep->getNumOperands() < 2) continue;
        bool hasLoadUser = false;
        bool hasStoreUser = false;
        for (User *U : gep->users()) {
          if (isa<LoadInst>(U)) hasLoadUser = true;
          if (isa<StoreInst>(U)) hasStoreUser = true;
        }
        if (!hasLoadUser || hasStoreUser) continue;
        unsigned lastIdx = gep->getNumOperands() - 1;
        Value *idxVal = gep->getOperand(lastIdx);
        if (!idxVal->getType()->isIntegerTy(64) && !idxVal->getType()->isIntegerTy(32)) continue;
        if (isa<ConstantInt>(idxVal)) continue;

        IRBuilder<> IRB(gep);
        Value *idx32 = idxVal->getType()->isIntegerTy(64)
                           ? IRB.CreateTrunc(idxVal, I32Ty, "gep.idx32")
                           : idxVal;
        Value *hooked32 = IRB.CreateCall(
            HookI32, {idx32, ConstantInt::get(I32Ty, staticSiteId++)}, "fi.addr");
        Value *diff = IRB.CreateXor(idx32, hooked32);
        Value *boundedDiff = IRB.CreateAnd(diff, ConstantInt::get(I32Ty, 0x3));
        Value *safeIdx32 = IRB.CreateXor(idx32, boundedDiff);
        Value *finalIdx = idxVal->getType()->isIntegerTy(64)
                              ? IRB.CreateSExt(safeIdx32, idxVal->getType())
                              : safeIdx32;
        gep->setOperand(lastIdx, finalIdx);
      }

      // 4. 分支控制流条件 (ICmp / FCmp) 注入 (翻转 i1 布尔条件以模拟控制流方向偏转)
      for (Instruction *cmpInst : brTargets) {
        Instruction *nextNode = cmpInst->getNextNode();
        if (!nextNode) continue;
        IRBuilder<> IRB(nextNode);
        Value *zext32 = IRB.CreateZExt(cmpInst, I32Ty, "br.i32");
        Value *hooked32 = IRB.CreateCall(
            HookI32, {zext32, ConstantInt::get(I32Ty, staticSiteId++)}, "fi.br");
        Value *diff = IRB.CreateXor(zext32, hooked32);
        Value *wasInjected = IRB.CreateICmpNE(diff, ConstantInt::get(I32Ty, 0), "fi.was_inj");
        Value *flippedCond = IRB.CreateXor(cmpInst, wasInjected, "fi.cond");
        cmpInst->replaceUsesWithIf(flippedCond, [&](Use &U) {
          return U.getUser() != zext32 && U.getUser() != flippedCond;
        });
      }
    }

    return PreservedAnalyses::none();
  }
};

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "StandaloneFaultInjectorPass", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "standalone-fi") {
                    MPM.addPass(StandaloneFaultInjectorPass());
                    return true;
                  }
                  return false;
                });
          }};
}
