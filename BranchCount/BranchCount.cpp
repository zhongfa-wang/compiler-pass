#include "PassInterface.h"

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

using namespace llvm;

#define DEBUG_TYPE "dynamic-cc"

Constant *CreateGlobalCounter(Module &M, StringRef GlobalVarName) {
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

//-----------------------------------------------------------------------------
// DynamicBranchCounter implementation
//-----------------------------------------------------------------------------
bool DynamicBranchCounter::runOnModule(Module &M) {
  // bool Instrumented = false;

  /* Counter map <--> IR variable that holds the call counter */
  llvm::StringMap<Constant *> CounterMap;
  /* Counter name map <--> IR variable that holds the function name */
  // llvm::StringMap<Constant *> CounterNameMap;

  auto &CTX = M.getContext();
  std::string CounterName1 = std::string("branch_all");
  // std::string CounterName2 = std::string("branch_target");

  /* STEP 1: For each function in the module, inject a call-counting code */
  /* --------------------------------------------------------------------*/
  /* Counting the total number of branch instructions */
  for (auto &F : M) {

    /* Initialize the global variable: the number of all branches */
    Constant *branch_counter_all = CreateGlobalCounter(M, CounterName1);
    CounterMap[CounterName1] = branch_counter_all;
    /* Initialize the global variable: the number of target branches */
    // Constant *branch_counter_target = CreateGlobalCounter(M, CounterName2);
    // CounterMap[CounterName2] = branch_counter_target;

    for (auto &B : F) {
      for (auto &I : B) {
        IRBuilder<> InstBuilder(&I);
        if (BranchInst *branchInst = dyn_cast<BranchInst>(&I)) {
          if (branchInst->isConditional()){
            /* Increment the global variable */
          LoadInst *Load_B_C = InstBuilder.CreateLoad(
              IntegerType::getInt32Ty(CTX), branch_counter_all);
          Value *Value_B_C =
              InstBuilder.CreateAdd(InstBuilder.getInt32(1), Load_B_C);
          InstBuilder.CreateStore(Value_B_C, branch_counter_all);
          LLVM_DEBUG(dbgs() << "Instrumented: " << I.getOpcodeName() << "\n");
          }
        }
      }
    }
  }

  /* STEP 2: Inject the declaration of printf */
  /* ---------------------------------------- */
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
  /* ------------------------------------------------------------------------ */
  /* format string for the counter of all branches */
  llvm::Constant *ResultFormatStr_all = llvm::ConstantDataArray::getString(
      CTX, "The number of all branch instructions is: %-10lu\n");
  Constant *ResultFormatStrVar_all =
      M.getOrInsertGlobal("ResultFormatStrIR_all", ResultFormatStr_all->getType());
  dyn_cast<GlobalVariable>(ResultFormatStrVar_all)
      ->setInitializer(ResultFormatStr_all);
  /* format string for the counter of target branches */
  llvm::Constant *ResultFormatStr_target = llvm::ConstantDataArray::getString(
      CTX, "The number of multiplying instructions is: %-10lu\n");
  Constant *ResultFormatStrVar_target = M.getOrInsertGlobal(
      "ResultFormatStrIR_target", ResultFormatStr_target->getType());
  dyn_cast<GlobalVariable>(ResultFormatStrVar_target)
      ->setInitializer(ResultFormatStr_target);

  // STEP 4: Define a printf wrapper that will print the results
  // -----------------------------------------------------------
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
  // (printf requires i8*, so cast the input strings accordingly)
  /* print the number of all branches */
  llvm::Value *ResultFormatStrPtr_all =
      Builder.CreatePointerCast(ResultFormatStrVar_all, PrintfArgTy);

  Builder.CreateCall(Printf, {ResultFormatStrPtr_all,
                              Builder.CreateLoad(IntegerType::getInt32Ty(CTX),
                                                 CounterMap[CounterName1])});
  /* print the number of target branches */
  // llvm::Value *ResultFormatStrPtr_target =
  //     Builder.CreatePointerCast(ResultFormatStrVar_target, PrintfArgTy);

  // Builder.CreateCall(Printf, {ResultFormatStrPtr_target,
  //                             Builder.CreateLoad(IntegerType::getInt32Ty(CTX),
  //                                                CounterMap[CounterName2])});

  /* Finally, insert return instruction */
  Builder.CreateRetVoid();

  // STEP 5: Call `printf_wrapper` at the very end of this module
  // ------------------------------------------------------------
  appendToGlobalDtors(M, PrintfWrapperF, /*Priority=*/0);

  return true;
}

PreservedAnalyses DynamicBranchCounter::run(llvm::Module &M,
                                          llvm::ModuleAnalysisManager &) {
  bool Changed = runOnModule(M);

  return (Changed ? llvm::PreservedAnalyses::none()
                  : llvm::PreservedAnalyses::all());
}

//-----------------------------------------------------------------------------
// New PM Registration of DynamicBranchCounter
//-----------------------------------------------------------------------------
llvm::PassPluginLibraryInfo getDynamicBranchCounterPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "branch-count-pass", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "branch-count-pass") {
                    MPM.addPass(DynamicBranchCounter());
                    return true;
                  }
                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getDynamicBranchCounterPluginInfo();
}