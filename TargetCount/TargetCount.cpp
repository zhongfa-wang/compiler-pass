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

/* Declare the required passes */
// void getAnalysisUsage(AnalysisUsage &AU) {
//     // AU.addRequired<MemorySSAWrapperPass>();
//     // AU.addPreserved<MemorySSAWrapperPass>();
//     AU.addRequiredTransitive<MemorySSAWrapperPass>();
//     AU.addPreserved<MemorySSAWrapperPass>();
//   }

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
  /* Register the pass required by memorySSA */

  /* STEP 1: For each function in the module, inject a call-counting code */
  /* --------------------------------------------------------------------*/
  /* Counting the total number of branch instructions */
  /* Initialize the global variable: the number of target branches */
  Constant *branch_counter_target = CreateGlobalCounter(M, CounterName2);
  CounterMap[CounterName2] = branch_counter_target;

  /* Get result from function analyses */
  // FunctionAnalysisManager &FAM =
  //   AM.getResult<FunctionAnalysisManagerFunctionProxy>(M).getManager();
  /* Register MemorySSA analysis */
  FAM.registerPass([&]
                   { return MemorySSAAnalysis(); });
  // FAM.registerPass([] { return MemorySSAAnalysis(); });

  for (auto &F : M)
  {
    /* Get the MemorySSAAnalysis results for the function */
    // FunctionAnalysisManager FAM;
    // FAM.registerPass([]
    //                  { return PassInstrumentationAnalysis(); });
    // FAM.registerPass([]
    //                  { return MemorySSAAnalysis(); });
    // FAM.registerPass([] { return DominatorTreeAnalysis(); });
    // FAM.registerPass([] { return AAManager(); });
    // FAM.registerPass([] {return TargetLibraryAnalysis(); });
    // MemorySSA &MSSA = FAM.getResult<MemorySSAAnalysis>(F).getMSSA();
    // FAM.registerPass([] {return DominatorTreeAnalysis(); });
    // DominatorTree &DT = FAM.getResult<DominatorTreeAnalysis>(F);

    // Register MemorySSA analysis
    // AM.registerPass([] { return MemorySSAAnalysis(); });

    /* test: registering pass */
    // MemorySSA &MSSA = AM.getResult<MemorySSAAnalysis>(M).getMSSA();
    // DominatorTree &DT = AM.getResult<DominatorTreeAnalysis>(M);
    // LoopInfo &LI = AM.getResult<LoopAnalysis>(M);

    /* Get the MemorySSA for the current function */
    // MemorySSA &MSSA = FAM.getResult<MemorySSAAnalysis>(F).getMSSA();
    // MemorySSA *MSSA = EnableMSSALoopDependency
    //                      ? &FAM.getResult<MemorySSAAnalysis>(F).getMSSA()
    //                      : nullptr;

    for (auto &B : F)
    {
      /* Count the add instruction in basic blocks followed by branch instructions */
      Instruction *Terminator = B.getTerminator();
      if (BranchInst *Branch = dyn_cast<BranchInst>(Terminator))
      {
        if (Branch->isConditional())
        {
          /* get memory access of the branch instruction */
          // auto *BrMA = MSSA.getMemoryAccess(Branch);
          for (unsigned i = 0, e = Branch->getNumSuccessors(); i != e; ++i)
          {
            BasicBlock *SuccessorBB = Branch->getSuccessor(i);
            for (Instruction &II : *SuccessorBB)
            {
              /* get memory access of the current instruction*/
              // auto *LoadMA = MSSA.getMemoryAccess(&II);
              IRBuilder<> InstBuilder(&II);
              if (auto Load = dyn_cast<LoadInst>(&II))
              {

                /* The targeted branch__ test */
                // if (MSSA.dominates(BrMA, LoadMA))
                // {
                LoadInst *Load_B_C_T = InstBuilder.CreateLoad(
                    IntegerType::getInt32Ty(CTX), branch_counter_target);
                Value *Value_B_C_T =
                    InstBuilder.CreateAdd(InstBuilder.getInt32(1),
                                          Load_B_C_T);
                InstBuilder.CreateStore(Value_B_C_T, branch_counter_target);
                LLVM_DEBUG(dbgs()
                           << "Instrumented: " << II.getOpcodeName() << "\n");
                // }

                /* Test: increment by 1 on each load instruction */
                // LoadInst *Load_B_C_T = InstBuilder.CreateLoad(
                //     IntegerType::getInt32Ty(CTX), branch_counter_target);
                // Value *Value_B_C_T =
                //     InstBuilder.CreateAdd(InstBuilder.getInt32(1),
                //                           Load_B_C_T);
                // InstBuilder.CreateStore(Value_B_C_T, branch_counter_target);
                // LLVM_DEBUG(dbgs()
                //            << "Instrumented: " << II.getOpcodeName() << "\n");
              }

              // IRBuilder<> InstBuilder(&II);
              // if (std::string(II.getOpcodeName()) == "mul")
              // {
              //   // The targeted branch__ test
              //   LoadInst *Load_B_C_T = InstBuilder.CreateLoad(
              //       IntegerType::getInt32Ty(CTX), branch_counter_target);
              //   Value *Value_B_C_T =
              //       InstBuilder.CreateAdd(InstBuilder.getInt32(1),
              //                             Load_B_C_T);
              //   InstBuilder.CreateStore(Value_B_C_T, branch_counter_target);
              //   LLVM_DEBUG(dbgs()
              //              << "Instrumented: " << II.getOpcodeName() << "\n");
              // }
            }
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
  /* format string for the counter of target branches */
  llvm::Constant *ResultFormatStr_target = llvm::ConstantDataArray::getString(
      CTX, "The number of load instructions is: %-10lu\n");
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
  /* print the number of target branches */
  llvm::Value *ResultFormatStrPtr_target =
      Builder.CreatePointerCast(ResultFormatStrVar_target, PrintfArgTy);

  Builder.CreateCall(Printf, {ResultFormatStrPtr_target,
                              Builder.CreateLoad(IntegerType::getInt32Ty(CTX),
                                                 CounterMap[CounterName2])});

  /* Finally, insert return instruction */
  Builder.CreateRetVoid();

  // STEP 5: Call `printf_wrapper` at the very end of this module
  // ------------------------------------------------------------
  appendToGlobalDtors(M, PrintfWrapperF, /*Priority=*/0);

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