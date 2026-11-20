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
#include <set>
#include <vector>
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

// 注错范围必须与策略层的保护范围一致：同样取"从 bench_* 入口出发的调用图闭包"。
// 旧版本是一张硬编码函数名白名单 (bench_* / kf_bfly* / kf_work / CNDF /
// BlkSchlsEqEuroNoDiv)，凡是不在名单里的上游函数（sobel.c 的 sobel_filtering、
// crc16.c 的 crc16、quicksort.c 的 quick_sort）都拿不到任何注入点，
// 其 SDC 率会被记成 0% —— 那是"没注入"，不是"没损坏"。
bool isOutOfInjectionScope(StringRef n) {
  return n.startswith("atft_") || n.startswith("harness_") ||
         n.startswith("__harness_") || n.startswith("llvm.") ||
         n.startswith("polybench_") || n.endswith("_unused_main") || n == "main";
}

// 若函数直接调用动态内存管理，则不作为注错目标：这类函数（如 KissFFT 的
// kiss_fft_alloc）属于一次性初始化而非被测计算内核，往其整数通路注错只会
// 破坏 harness 自身的堆状态（libc abort: free(): invalid pointer），
// 使同一进程内后续试验不可信。属于已记录的效度边界。
bool allocatesMemory(Function *F) {
  for (BasicBlock &BB : *F) {
    for (Instruction &I : BB) {
      auto *CB = dyn_cast<CallBase>(&I);
      if (!CB) continue;
      Function *callee = CB->getCalledFunction();
      if (!callee) continue;
      StringRef n = callee->getName();
      if (n == "malloc" || n == "calloc" || n == "realloc" || n == "free") return true;
    }
  }
  return false;
}

std::set<Function *> computeInjectionClosure(Module &M) {
  std::set<Function *> closure;
  std::vector<Function *> worklist;
  for (Function &F : M) {
    if (F.isDeclaration()) continue;
    if (F.getName().startswith("bench_") && !isOutOfInjectionScope(F.getName())) {
      closure.insert(&F);
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
        if (callee->isIntrinsic() || isOutOfInjectionScope(callee->getName())) continue;
        if (allocatesMemory(callee)) continue;
        if (closure.insert(callee).second) worklist.push_back(callee);
      }
    }
  }
  return closure;
}

// ============================================================================
// 故障点分类：七个**互斥**类别
//
// 分类只看指令自身的种类与结果类型，**不看它的使用者**。早期版本用"流入
// 比较/GEP 则排除"来避免类别重叠，副作用是容错变换给被保护指令挂上内联
// 表决（ICmp/Select）之后，那条指令就变成不可注入 —— 保护越多、注入点越少、
// 可靠性被系统性虚高。按指令种类分类从根上消除了这个攻击面。
//
// 与文献的对应关系：
//   LLFI/PINFI (DSN'14) 的 arithmetic / cast / cmp / load 四类
//   SASSIFI (ISPASS'17) 的 IOV（指令输出值）与 IOA（输出寄存器索引与 store 地址）
// 在 RV64GCV 上的落点：
//   FP64 -> fadd.d/fmul.d/fld      FP32 -> fadd.s/vfadd.vv/flw
//   INT  -> add/mul/sll/and        CAST -> fcvt.*/sext.w/slli+srai
//   LOAD -> lw/ld/flw/fld 目的寄存器   ADDR -> sw/sd/lw 的地址寄存器
//   CMP  -> slt/feq.d + beq/bne 的条件
// ============================================================================
enum FiCategory {
  FIC_NONE = 0,
  FIC_FP64,
  FIC_FP32,
  FIC_INT_ARITH,
  FIC_CAST,
  FIC_LOAD,
  FIC_ADDR,
  FIC_CMP_BRANCH,
  FIC_COUNT
};

static const char *FI_CATEGORY_NAMES[FIC_COUNT] = {
  "none", "fp64", "fp32", "int_arith", "cast", "load", "addr", "cmp_branch"
};

// 可被注入的标量/向量类型：决定用哪个 hook
static bool isHookableTy(Type *T) {
  if (T->isDoubleTy() || T->isFloatTy()) return true;
  if (T->isIntegerTy(16) || T->isIntegerTy(32) || T->isIntegerTy(64)) return true;
  if (auto *vecTy = dyn_cast<FixedVectorType>(T)) {
    return vecTy->getNumElements() == 2 &&
           (vecTy->getElementType()->isFloatTy() || vecTy->getElementType()->isDoubleTy());
  }
  return false;
}

static bool isCastOp(Instruction *I) {
  switch (I->getOpcode()) {
    case Instruction::Trunc:
    case Instruction::ZExt:
    case Instruction::SExt:
    case Instruction::FPTrunc:
    case Instruction::FPExt:
    case Instruction::FPToSI:
    case Instruction::FPToUI:
    case Instruction::SIToFP:
    case Instruction::UIToFP:
      return true;
    default:
      return false;
  }
}

// 比较结果是否真正驱动控制流或选择
static bool cmpDrivesControlFlow(Instruction *I) {
  for (User *U : I->users()) {
    if (isa<BranchInst>(U) || isa<SelectInst>(U) || isa<SwitchInst>(U)) return true;
  }
  return false;
}

// 地址类注入点：带可变下标的 GEP。读地址与写地址都算 —— 子进程隔离之后，
// 写地址被改坏只会杀死该次试验的子进程并记为 DUE，不再污染父进程的堆。
static bool isAddrSite(Instruction *I) {
  auto *gep = dyn_cast<GetElementPtrInst>(I);
  if (!gep || gep->getNumOperands() < 2) return false;
  Value *idx = gep->getOperand(gep->getNumOperands() - 1);
  if (isa<ConstantInt>(idx)) return false;
  return idx->getType()->isIntegerTy(32) || idx->getType()->isIntegerTy(64);
}

// 盲注入模式:忽略 atft.voter.internal 豁免,让故障也能打到 pass 插入的保护代码
// (voter / 代数校验 / cmp-DMR),对齐 SASSIFI/LLFI 的"全程序均匀注入"口径,
// 测得出保护机制自身被打中时的真实残余。默认关闭(保持冻结实验口径)。
static cl::opt<bool> FiBlind(
    "fi-blind",
    cl::desc("Blind injection: do NOT exempt protection code (atft.voter.internal); "
             "inject uniformly across the whole program incl. FT mechanisms (SASSIFI/LLFI style)"),
    cl::init(false));

// 唯一的分类入口：按指令种类定优先级，保证互斥
FiCategory classifyFaultSite(Instruction *I) {
  if (!I || isa<PHINode>(I)) return FIC_NONE;
  if (!FiBlind && I->getMetadata("atft.voter.internal") != nullptr) return FIC_NONE;

  if (isAddrSite(I))                              return FIC_ADDR;
  if (isa<LoadInst>(I))
    return isHookableTy(I->getType()) ? FIC_LOAD : FIC_NONE;
  if (isCastOp(I))
    return isHookableTy(I->getType()) ? FIC_CAST : FIC_NONE;
  if (isa<ICmpInst>(I) || isa<FCmpInst>(I))
    // 仅标量 i1 分支条件算控制流注入点；向量谓词(<N x i1>，向量化 select 的掩码)
    // 不是控制流、且标量翻转注入器处理不了向量 → 排除，避免生成坏 IR。
    return (!I->getType()->isVectorTy() && cmpDrivesControlFlow(I)) ? FIC_CMP_BRANCH : FIC_NONE;

  if (I->isBinaryOp() || I->isUnaryOp()) {
    Type *ty = I->getType();
    if (ty->isDoubleTy()) return FIC_FP64;
    if (ty->isFloatTy())  return FIC_FP32;
    if (auto *vecTy = dyn_cast<FixedVectorType>(ty)) {
      if (vecTy->getNumElements() == 2 && vecTy->getElementType()->isFloatTy())
        return FIC_FP32;
      if (vecTy->getNumElements() == 2 && vecTy->getElementType()->isDoubleTy())
        return FIC_FP64;
    }
    if (ty->isIntegerTy(16) || ty->isIntegerTy(32) || ty->isIntegerTy(64))
      return FIC_INT_ARITH;
  }
  return FIC_NONE;
}

// 把命令行给的类别名解析成选择集合。保留旧名字作别名，避免老配置失效。
static void parseCategorySelection(StringRef spec, bool *enabled) {
  for (int c = 0; c < FIC_COUNT; c++) enabled[c] = false;
  if (spec == "all") {
    for (int c = FIC_FP64; c < FIC_COUNT; c++) enabled[c] = true;
    return;
  }
  SmallVector<StringRef, 8> parts;
  spec.split(parts, ',', -1, false);
  for (StringRef raw : parts) {
    StringRef p = raw.trim();
    if (p == "fp") { enabled[FIC_FP64] = true; enabled[FIC_FP32] = true; continue; }
    if (p == "int")     { enabled[FIC_INT_ARITH] = true; continue; }
    if (p == "address") { enabled[FIC_ADDR] = true; continue; }
    if (p == "branch")  { enabled[FIC_CMP_BRANCH] = true; continue; }
    bool matched = false;
    for (int c = FIC_FP64; c < FIC_COUNT; c++) {
      if (p == FI_CATEGORY_NAMES[c]) { enabled[c] = true; matched = true; break; }
    }
    if (!matched) {
      errs() << "[standalone-fi] 未知的注错类别: " << p << "\n";
    }
  }
}

// 配对注入：只往原程序本来就有的故障点注入，跳过策略层引入的冗余副本
// （副本带 atft.replica 标记）。这样各策略的故障空间逐点相同、动态计数也相同，
// 同一随机种子抽到的是同一组逻辑故障，策略间比较变成配对比较。
// 默认关闭，关闭时行为与冻结实验逐位一致。
static cl::opt<bool> FiOriginOnly(
    "fi-origin-only",
    cl::desc("Inject only into original-program sites, skipping replicas marked atft.replica"),
    cl::init(false));

class StandaloneFaultInjectorPass : public PassInfoMixin<StandaloneFaultInjectorPass> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {
    LLVMContext &Ctx = M.getContext();
    Type *F64Ty = Type::getDoubleTy(Ctx);
    Type *F32Ty = Type::getFloatTy(Ctx);
    Type *I16Ty = Type::getInt16Ty(Ctx);
    Type *I32Ty = Type::getInt32Ty(Ctx);
    Type *I64Ty = Type::getInt64Ty(Ctx);
    VectorType *V2F32Ty = FixedVectorType::get(F32Ty, 2);
    VectorType *V2F64Ty = FixedVectorType::get(F64Ty, 2);

    FunctionCallee HookF64 = M.getOrInsertFunction("__harness_fi_hook_f64", F64Ty, F64Ty, I32Ty);
    FunctionCallee HookV2F64 = M.getOrInsertFunction(
        "__harness_fi_hook_v2f64", V2F64Ty, V2F64Ty, I32Ty);
    FunctionCallee HookF32 = M.getOrInsertFunction("__harness_fi_hook_f32", F32Ty, F32Ty, I32Ty);
    FunctionCallee HookI16 = M.getOrInsertFunction("__harness_fi_hook_i16", I16Ty, I16Ty, I32Ty);
    FunctionCallee HookI32 = M.getOrInsertFunction("__harness_fi_hook_i32", I32Ty, I32Ty, I32Ty);
    FunctionCallee HookAddrI32 = M.getOrInsertFunction("__harness_fi_hook_addr_i32", I32Ty, I32Ty, I32Ty);
    FunctionCallee HookI64 = M.getOrInsertFunction("__harness_fi_hook_i64", I64Ty, I64Ty, I32Ty);

    bool enabled[FIC_COUNT];
    parseCategorySelection(FiTargetType, enabled);

    int32_t staticSiteId = 1;
    unsigned siteCount[FIC_COUNT] = {0};

    std::set<Function *> injectionScope = computeInjectionClosure(M);

    // 先收集再改写：注入会插入新指令，边遍历边改写会把新指令也当成候选
    std::vector<std::pair<Instruction *, FiCategory>> sites;
    auto allocateSiteId = [&](Instruction *inst, FiCategory cat) {
      int32_t id = staticSiteId++;
      bool feedsBranch = false;
      bool feedsStore = false;
      bool feedsStoreAddr = false;
      bool feedsCall = false;
      bool feedsGEP = false;
      bool feedsPhi = false;
      for (User *U : inst->users()) {
        auto *UI = dyn_cast<Instruction>(U);
        if (!UI) continue;
        if (isa<BranchInst>(UI) || isa<SelectInst>(UI) || isa<SwitchInst>(UI))
          feedsBranch = true;
        if (auto *SI = dyn_cast<StoreInst>(UI)) {
          if (SI->getValueOperand() == inst) feedsStore = true;
          if (SI->getPointerOperand() == inst) feedsStoreAddr = true;
        }
        if (isa<CallBase>(UI)) feedsCall = true;
        if (isa<GetElementPtrInst>(UI)) feedsGEP = true;
        if (isa<PHINode>(UI)) feedsPhi = true;
      }
      std::string siteKey;
      if (MDNode *N = inst->getMetadata("atft.site.key")) {
        if (N->getNumOperands() > 0) {
          if (auto *S = dyn_cast<MDString>(N->getOperand(0)))
            siteKey = S->getString().str();
        }
      }
      errs() << "FI_SITE_MAP," << id << "," << FI_CATEGORY_NAMES[cat]
             << "," << inst->getFunction()->getName() << ","
             << inst->getParent()->getName() << "," << inst->getOpcodeName()
             << "," << (feedsBranch ? 1 : 0) << "," << (feedsStore ? 1 : 0)
             << "," << (feedsStoreAddr ? 1 : 0) << "," << (feedsCall ? 1 : 0)
             << "," << (feedsGEP ? 1 : 0) << "," << (feedsPhi ? 1 : 0)
             << "," << siteKey << "\n";
      return ConstantInt::get(I32Ty, id);
    };
    for (Function &F : M) {
      if (F.isDeclaration()) continue;
      if (!injectionScope.count(&F)) continue;
      for (BasicBlock &BB : F) {
        for (Instruction &I : BB) {
          FiCategory c = classifyFaultSite(&I);
          if (c == FIC_NONE) continue;
          if (FiOriginOnly && I.getMetadata("atft.replica") != nullptr) continue;
          siteCount[c]++;
          if (!enabled[c]) continue;
          sites.push_back({&I, c});
        }
      }
    }

    for (auto &entry : sites) {
      Instruction *inst = entry.first;
      FiCategory cat = entry.second;

      // ---- 地址类：扰动 GEP 的可变下标，±3 元素有界位移故障模型 ----
      // P0-4 修复：改用专用 addr hook，每次触发都是非零位移，不再有 bit>=2 被 &0x3
      // 清零的空注入。hook 内部已做 ±3 元素有界，这里直接用返回值。
      if (cat == FIC_ADDR) {
        unsigned lastIdx = inst->getNumOperands() - 1;
        Value *idxVal = inst->getOperand(lastIdx);
        IRBuilder<> IRB(inst);
        bool is64 = idxVal->getType()->isIntegerTy(64);
        Value *idx32 = is64 ? IRB.CreateTrunc(idxVal, I32Ty, "gep.idx32") : idxVal;
        Value *hooked32 = IRB.CreateCall(
            HookAddrI32, {idx32, allocateSiteId(inst, cat)}, "fi.addr");
        Value *finalIdx = is64 ? IRB.CreateSExt(hooked32, idxVal->getType()) : hooked32;
        inst->setOperand(lastIdx, finalIdx);
        continue;
      }

      // ---- 比较类：按注入与否翻转 i1 条件 ----
      if (cat == FIC_CMP_BRANCH) {
        Instruction *nextNode = inst->getNextNode();
        if (!nextNode) continue;
        IRBuilder<> IRB(nextNode);
        Value *zext32 = IRB.CreateZExt(inst, I32Ty, "br.i32");
        Value *hooked32 = IRB.CreateCall(
            HookI32, {zext32, allocateSiteId(inst, cat)}, "fi.br");
        Value *diff = IRB.CreateXor(zext32, hooked32);
        Value *wasInjected = IRB.CreateICmpNE(diff, ConstantInt::get(I32Ty, 0), "fi.was_inj");
        Value *flippedCond = IRB.CreateXor(inst, wasInjected, "fi.cond");
        inst->replaceUsesWithIf(flippedCond, [&](Use &U) {
          return U.getUser() != zext32 && U.getUser() != flippedCond;
        });
        continue;
      }

      // ---- 其余类别：按结果类型选 hook，替换该值的全部使用者 ----
      Instruction *nextNode = inst->getNextNode();
      if (!nextNode) continue;
      IRBuilder<> IRB(nextNode);
      Type *ty = inst->getType();
      Value *hooked = nullptr;

      if (ty->isDoubleTy()) {
        hooked = IRB.CreateCall(HookF64, {inst, allocateSiteId(inst, cat)}, "fi.f64");
      } else if (ty == V2F64Ty) {
        hooked = IRB.CreateCall(
            HookV2F64, {inst, allocateSiteId(inst, cat)}, "fi.v2f64");
      } else if (ty->isFloatTy()) {
        hooked = IRB.CreateCall(HookF32, {inst, allocateSiteId(inst, cat)}, "fi.f32");
      } else if (ty->isIntegerTy(16)) {
        hooked = IRB.CreateCall(HookI16, {inst, allocateSiteId(inst, cat)}, "fi.i16");
      } else if (ty->isIntegerTy(32)) {
        hooked = IRB.CreateCall(HookI32, {inst, allocateSiteId(inst, cat)}, "fi.i32");
      } else if (ty->isIntegerTy(64)) {
        hooked = IRB.CreateCall(HookI64, {inst, allocateSiteId(inst, cat)}, "fi.i64");
      } else if (ty == V2F32Ty) {
        // <2 x float> 近似副本：按 64 位整体翻转一个比特，等价于打中其中一路
        Value *asF64 = IRB.CreateBitCast(inst, F64Ty, "v2f32.as.f64");
        Value *hookedF64 = IRB.CreateCall(
            HookF64, {asF64, allocateSiteId(inst, cat)}, "fi.v2f32.f64");
        Value *hookedVec = IRB.CreateBitCast(hookedF64, V2F32Ty, "fi.v2f32");
        inst->replaceUsesWithIf(hookedVec, [&](Use &U) { return U.getUser() != asF64; });
        continue;
      }

      if (!hooked) continue;
      Value *hookedVal = hooked;
      inst->replaceUsesWithIf(hookedVal, [&](Use &U) { return U.getUser() != hookedVal; });
    }

    // 静态注入点统计：供上层做"注入点守恒"检查，防止通过减少插桩点刷低 SDC
    for (int c = FIC_FP64; c < FIC_COUNT; c++) {
      errs() << "FI_STATIC_SITES," << FI_CATEGORY_NAMES[c] << "," << siteCount[c] << "\n";
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
