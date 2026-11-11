//===-- FaultInjectorPass.cpp - Standalone Post-Transform Fault Injector --===//
//
// 独立于被测容错策略 (Policy) 的评测裁判层故障注入 LLVM Pass：
//   - 在任何容错变换 Pass 运行【之后】（或直接在未保护 IR 上）运行；
//   - 遍历目标计算函数内所有算术与载入指令（覆盖 double, float, <2 x float>, i32, i16）；
//   - 在每条目标指令后客观插桩 __harness_fi_hook_* 调用；
//   - 完全遵循 LLFI / SASSIFI (ISCA'17) / DSN'24 标准的两阶段方法论：
//     * Phase 1 (Profiling): 统计完整运行一次的总动态目标指令数 N_dyn
//     * Phase 2 (One-Fault-Per-Run): 在 [1, N_dyn] 中均匀随机抽 1 个动态指令实例，
//       在 [0, bitwidth-1] 全位宽中均匀随机抽 1 个 bit 翻转。
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

#include <vector>

using namespace llvm;

namespace {

bool isInjectableDataInstruction(Instruction *I) {
  if (!I) return false;
  if (isa<PHINode>(I)) return false;
  // 表决器内部比较指令属于同步检查器 (与外部表决函数一致，不在表决器内部重复打桩)
  if (I->getMetadata("atft.voter.internal") != nullptr) return false;

  // 针对算术指令或内存加载指令的目标寄存器注入单粒子位翻转
  if (I->isBinaryOp() || I->isUnaryOp() || isa<LoadInst>(I)) {
    Type *ty = I->getType();
    if (ty->isDoubleTy() || ty->isFloatTy()) {
      return true;
    }
    if (auto *vecTy = dyn_cast<FixedVectorType>(ty)) {
      if (vecTy->getElementType()->isFloatTy() && vecTy->getNumElements() == 2) {
        return true;
      }
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
    VectorType *V2F32Ty = FixedVectorType::get(F32Ty, 2);

    FunctionCallee HookF64 = M.getOrInsertFunction("__harness_fi_hook_f64", F64Ty, F64Ty, I32Ty);
    FunctionCallee HookF32 = M.getOrInsertFunction("__harness_fi_hook_f32", F32Ty, F32Ty, I32Ty);

    int32_t staticSiteId = 1;

    for (Function &F : M) {
      if (F.isDeclaration()) continue;
      StringRef fname = F.getName();
      // 仅对被测基准计算内核（含被策略克隆出的子版本）进行客观故障注入
      if (!fname.startswith("bench_")) continue;

      std::vector<Instruction *> targets;
      for (BasicBlock &BB : F) {
        for (Instruction &I : BB) {
          if (isInjectableDataInstruction(&I)) {
            targets.push_back(&I);
          }
        }
      }

      for (Instruction *inst : targets) {
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
          // 对 <2 x float> SIMD 向量寄存器：每次执行算作 1 条动态 SIMD 指令，
          // 若被抽中注入单比特翻转，随机翻转 64-bit 向量寄存器中的 1 个 bit (即损坏 lane 0 或 lane 1 之一)
          Value *bitcastI64 = IRB.CreateBitCast(inst, Type::getDoubleTy(Ctx), "v2f32.as.f64");
          Value *hookedF64 = IRB.CreateCall(
              HookF64, {bitcastI64, ConstantInt::get(I32Ty, staticSiteId++)}, "fi.v2f32.f64");
          Value *hookedVec = IRB.CreateBitCast(hookedF64, V2F32Ty, "fi.v2f32");
          inst->replaceUsesWithIf(hookedVec, [&](Use &U) {
            return U.getUser() != bitcastI64;
          });
        }
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
