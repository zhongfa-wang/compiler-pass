// #include "PassInterface.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"
#include <llvm/IR/Constant.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/Debug.h>
#include <string>
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/IR/CFG.h" //predecessor of BB
#include "llvm-c/Core.h"
#include "llvm/IR/PassManager.h" //ModuleAnalysisManager

using namespace llvm;

#define DEBUG_TYPE "dynamic-cc"

//------------------------------------------------------------------------------
// TargetBranchCounter interface: new PM
//------------------------------------------------------------------------------
struct TargetBranchCounter : public llvm::PassInfoMixin<TargetBranchCounter>
{
  llvm::PreservedAnalyses run(llvm::Function &F,
                              llvm::FunctionAnalysisManager &);
  // bool runOnFunction(llvm::Function &F);
  /* test: registering pass */
  bool runOnFunction(llvm::Function &F, FunctionAnalysisManager &FAM);

  // Without isRequired returning true, this pass will be skipped for functions
  // decorated with the optnone LLVM attribute. Note that clang -O0 decorates
  // all functions with optnone.
  static bool isRequired() { return true; }
};

Constant *CreateGlobalCounter(Module &M, StringRef GlobalVarName)
{
  auto &CTX = M.getContext();

  /* This will insert a declaration into M */
  Constant *NewGlobalVar =
      M.getOrInsertGlobal(GlobalVarName, IntegerType::getInt32Ty(CTX));

  /* This will change the declaration into definition (and initialise to 0) */
  GlobalVariable *NewGV = M.getNamedGlobal(GlobalVarName);
  NewGV->setLinkage(GlobalValue::CommonLinkage);
  NewGV->setAlignment(MaybeAlign(4));
  NewGV->setInitializer(llvm::ConstantInt::get(CTX, APInt(32, 0)));

  return NewGlobalVar;
}

// void infoprint(Module &M, StringRef PreStr, Constant *GlobalVar, Instruction *Ins)
void infoprint(Module &M, StringRef PreStr, Constant *GlobalVar)
{
  auto &CTX = M.getContext();
  /* STEP 2: Inject the declaration of printf */
  PointerType *PrintfArgTy = PointerType::getUnqual(Type::getInt8Ty(CTX));
  FunctionType *PrintfTy =
      FunctionType::get(IntegerType::getInt32Ty(CTX), PrintfArgTy,
                        /*IsVarArgs=*/true);
  FunctionCallee Printf = M.getOrInsertFunction("printf", PrintfTy);
  // Set attributes as per inferLibFuncAttributes in BuildLibCalls.cpp
  Function *PrintfF = dyn_cast<Function>(Printf.getCallee());
  PrintfF->setDoesNotThrow();
  PrintfF->addParamAttr(0, Attribute::NoCapture);
  PrintfF->addParamAttr(0, Attribute::ReadOnly);
  /* STEP 3: Inject a global variable that will hold the printf format string */
  llvm::Constant *ResultFormatStr_target = llvm::ConstantDataArray::getString(
      CTX, PreStr);
  Constant *ResultFormatStrVar_target = M.getOrInsertGlobal(
      "ResultFormatStrIR_target", ResultFormatStr_target->getType());
  dyn_cast<GlobalVariable>(ResultFormatStrVar_target)
      ->setInitializer(ResultFormatStr_target);
  // STEP 4: Define a printf wrapper that will print the results
  FunctionType *PrintfWrapperTy =
      FunctionType::get(llvm::Type::getVoidTy(CTX), {},
                        /*IsVarArgs=*/false);
  Function *PrintfWrapperF = dyn_cast<Function>(
      M.getOrInsertFunction("printf_wrapper", PrintfWrapperTy).getCallee());
  // Create the entry basic block for printf_wrapper ...
  llvm::BasicBlock *RetBlock =
      llvm::BasicBlock::Create(CTX, "enter", PrintfWrapperF);
  IRBuilder<> Builder(RetBlock);
  // ... and start inserting calls to printf
  llvm::Value *ResultFormatStrPtr_target =
      Builder.CreatePointerCast(ResultFormatStrVar_target, PrintfArgTy);
  Builder.CreateCall(Printf, {ResultFormatStrPtr_target,
                              Builder.CreateLoad(IntegerType::getInt32Ty(CTX),
                                                 GlobalVar)});
  /* Finally, insert return instruction */
  Builder.CreateRetVoid();
  // STEP 5: Call `printf_wrapper` at the very end of this module
  appendToGlobalDtors(M, PrintfWrapperF, /*Priority=*/0);
}

//   static bool isRequired() { return true; }
// };
//-----------------------------------------------------------------------------
// TargetBranchCounter implementation
//-----------------------------------------------------------------------------
// bool TargetBranchCounter::runOnFunction(Function &F)
bool TargetBranchCounter::runOnFunction(Function &F, FunctionAnalysisManager &FAM)
{
  /* Counter map <--> IR variable that holds the call counter */
  llvm::StringMap<Constant *> CounterMap;

  Module &M = *F.getParent();
  auto &CTX = M.getContext();
  std::string CounterName2 = std::string("branch_target");

  /* STEP 1: For each function in the module, inject a call-counting code */
  Constant *branch_counter_target = CreateGlobalCounter(M, CounterName2);
  CounterMap[CounterName2] = branch_counter_target;

  /* Register MemorySSA analysis */
  FAM.registerPass([&]
                   { return MemorySSAAnalysis(); });
  MemorySSA &MSSA = FAM.getResult<MemorySSAAnalysis>(F).getMSSA();

  for (auto &B : F)
  {
    for (Instruction &I : B)
    {
      // MemoryUseOrDef *BrMemUse = MSSA.getMemoryAccess(&I);
      // errs() << "Op: " << std::string(I.getOpcodeName());
      // errs() << ", Pointer: " << BrMemUse << "\n";
      if (auto *CmpIns = dyn_cast<CmpInst>(&I))
      {
        errs() << "Found a compare instruction: " << *CmpIns << "\n";
        // Use &OpUse = CmpIns->getOperandUse(0);
        // MemoryAccess *MemAccess = MSSA.getMemoryAccess(CmpIns);
        // errs() << "Check memoryaccess: " << *CmpIns << "\n";

        if (MemoryAccess *MemAccess = MSSA.getMemoryAccess(CmpIns))
        {
            errs() << "Check memoryaccess: " << *MemAccess << "\n";

          // if (MemoryUseOrDef *Def = dyn_cast<MemoryUseOrDef>(MemAccess))
          // {
          //   if (MemoryAccess *OperandAccess = Def->getDefiningAccess())
          //   {
          //     if (Instruction *OperandInst = dyn_cast<Instruction>(OperandAccess))
          //     {
          //       errs() << "Operand instruction: " << *OperandInst << "\n";
          //     }
          //   }
            // errs() << "Operand of compare instruction: " << *Def->getMemoryInst() << "\n";
          // }
        }

        // IRBuilder<> InstBuilder(&I);
        // if (MemoryUseOrDef *BrMemUse = MSSA.getMemoryAccess(StoreIns))
        // {
        //   for (Instruction &NextI : llvm::make_range(std::next(I.getIterator()), B.end()))
        //   {
        //     if (auto *LoadIns = dyn_cast<LoadInst>(&NextI))
        //     {
        //       // if (MemoryUseOrDef *UseOrDefB = MSSA.getMemoryAccess(&NextI))
        //       // {
        //       MemoryUseOrDef *UseOrDefB = MSSA.getMemoryAccess(LoadIns);
        //       if (MSSA.dominates(BrMemUse, UseOrDefB))
        //       {
        //         // errs() << "Op: " << std::string(I.getOpcodeName());
        //         // errs() << ", Pointer: " << StoreIns << ". ";
        //         // errs() << "Op: " << std::string(NextI.getOpcodeName());
        //         // errs() << ", Pointer: " << LoadIns << ".\n";
        //         LoadInst *Load_B_C_T = InstBuilder.CreateLoad(
        //             IntegerType::getInt32Ty(CTX), branch_counter_target);
        //         Value *Value_B_C_T =
        //             InstBuilder.CreateAdd(InstBuilder.getInt32(1),
        //                                   Load_B_C_T);
        //         InstBuilder.CreateStore(Value_B_C_T, branch_counter_target);

        //         // std::string PrintString = "The number of dangerous branch instructions is: %-10lu\n";
        //         // FunctionType *InfoprintType = FunctionType::get(Type::getVoidTy(F.getContext()), /* ... */);
        //         // Function *InfoprintFunc = F.getParent()->getFunction("infoprint");
        //       }
        //       // }
        //     }
        //   }
        // }
      }
    }
  }

  /* Print */
  // std::string PrintString = "The number of dangerous branch instructions is: %-10lu\n";
  // infoprint(M, PrintString, branch_counter_target);

  return true;
}

PreservedAnalyses TargetBranchCounter::run(llvm::Function &F,
                                           //  llvm::FunctionAnalysisManager &)
                                           llvm::FunctionAnalysisManager &FAM) // test: registering pass
{
  // bool Changed = runOnFunction(F);
  /* test: registering pass */
  bool Changed = runOnFunction(F, FAM);

  return (Changed ? llvm::PreservedAnalyses::none()
                  : llvm::PreservedAnalyses::all());
}

//-----------------------------------------------------------------------------
// New PM Registration of TargetBranchCounter
//-----------------------------------------------------------------------------
llvm::PassPluginLibraryInfo getTargetBranchCounterPluginInfo()
{
  return {LLVM_PLUGIN_API_VERSION, "target-count-pass", LLVM_VERSION_STRING,
          [](PassBuilder &PB)
          {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>)
                {
                  if (Name == "target-count-pass")
                  {
                    MPM.addPass(TargetBranchCounter());
                    return true;
                  }
                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo()
{
  return getTargetBranchCounterPluginInfo();
}