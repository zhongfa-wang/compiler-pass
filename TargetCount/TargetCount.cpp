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
  /* Instruction List <--> IR variable that holds the instruction */
  llvm::StringMap<Instruction *> InsMap;
  llvm::StringMap<MemoryUseOrDef *> InsMemMap;

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
  MemorySSAWalker *Walker = MSSA.getWalker();

  for (auto &B : F)
  {
    for (Instruction &I : B)
    {
      // IRBuilder<> InstBuilder(&I);
      if (auto *BrIns = dyn_cast<BranchInst>(&I))
      {
        if (BrIns->isConditional())
        {
          /* Find the load op */
          for (BasicBlock::reverse_iterator RI = ++I.getReverseIterator(), RE = B.rend(); RI != RE; ++RI)
          {
            if (auto *CmpPreBrIns = dyn_cast<CmpInst>(&*RI))
            {
              errs() << "Op: " << std::string(RI->getOpcodeName());
              errs() << ", Code: " << *RI << "\n";
              for (Use &U : CmpPreBrIns->operands())
              {
                if (Instruction *CmpOpLoad = dyn_cast<Instruction>(U))
                {
                  if (auto *IsLoad = dyn_cast<LoadInst>(&*CmpOpLoad))
                  {
                    errs() << "Found the load before the branch: " << std::string(IsLoad->getOpcodeName());
                    errs() << ", Code: " << *IsLoad << "\n";
                  }
                  InsMap["compare_operand_load"] = &*CmpOpLoad;                                        // Store instruction to map
                  InsMemMap["compare_operand_load_memory_access"] = MSSA.getMemoryAccess(&*CmpOpLoad); // Store memory access to map
                }
                break;
              }
            }
          }
          /* Retrieve the instruction and fetch the memory access */
          // MemoryUseOrDef *CmpOpLoadMemUse = MSSA.getMemoryAccess(InsMap["compare_operand_load"]);
          // errs() << "Check memoryaccess: " << *CmpOpLoadMemUse << "\n";
          /* Get the memory access from map */
          // MemoryUseOrDef *CmpOpLoadMemUse = InsMemMap["compare_operand_load_memory_access"];
          // errs() << "Check memoryaccess: " << *InsMemMap["compare_operand_load_memory_access"] << "\n";

          Value *PcedBrLd_Op = InsMap["compare_operand_load"]->getOperand(0);//PcedBrLd_Op: the operand of the load preceding the branch
          errs() << "Get the branch-preceding load's operand: " << *PcedBrLd_Op << "\n";
          /* Get the succeeding basic block */
          BasicBlock *BrScedBB = BrIns->getSuccessor(0);
          for (Instruction &II : *BrScedBB)
          {
            IRBuilder<> InstBuilder(&II);
            if (LoadInst *ScedBBLoad = dyn_cast<LoadInst>(&II))
            {
              errs() << "Found the load in the successor basic block: " << std::string(ScedBBLoad->getOpcodeName());
              errs() << ", Code: " << *ScedBBLoad << "\n";
              Instruction *NextInsLoad = II.getNextNode(); //NextInsLoad: the very next instruction of the load
              if (auto *NextInsLoad_isCmp = dyn_cast<CmpInst>(NextInsLoad)){
                errs() << "This load: " << II << "is related to a comparing: " << *NextInsLoad << ". \n";
                break;
              }
              
              Value *ScedBrLd_Op = ScedBBLoad->getOperand(0); //ScedBrLd_Op: the operand of the load succeeding the branch
              errs() << "Get the branch-succeeding load's operand: " << *ScedBrLd_Op << "\n";
              if (isa<Instruction>(*ScedBrLd_Op))
              {
                if (isa<Instruction>(*PcedBrLd_Op))
                {
                Instruction *PcedBrLd_Op_Inst = dyn_cast<Instruction>(PcedBrLd_Op);
                Instruction *ScedBrLd_Op_Inst = dyn_cast<Instruction>(ScedBrLd_Op);
                // errs() << "Check if the operand Preceding the BR is an instruction: " << *PcedBrLd_Op_Inst << "\n";
                // errs() << "Check if the operand succeeding the BR is an instruction: " << *ScedBrLd_Op_Inst << "\n";
                  if (ScedBrLd_Op_Inst == PcedBrLd_Op_Inst) {
                   errs() << "Found the data dependence!" << *ScedBrLd_Op_Inst << "\n";
                   LoadInst *Load_B_C_T = InstBuilder.CreateLoad(
                      IntegerType::getInt32Ty(CTX), branch_counter_target);
                  Value *Value_B_C_T =
                      InstBuilder.CreateAdd(InstBuilder.getInt32(1),
                                            Load_B_C_T);
                  InstBuilder.CreateStore(Value_B_C_T, branch_counter_target);
                  break;
                  }
                }
              }

              // if (MemoryUseOrDef *BrMemUse = MSSA.getMemoryAccess(ScedBBLoad))
              // {
              //   errs() << "Check memoryaccess: " << *BrMemUse << "\n";
              //   if (MSSA.dominates(CmpOpLoadMemUse, BrMemUse))
              //   {
              //     errs() << "Found data dependence!"
              //            << "\n";
              //     LoadInst *Load_B_C_T = InstBuilder.CreateLoad(
              //         IntegerType::getInt32Ty(CTX), branch_counter_target);
              //     Value *Value_B_C_T =
              //         InstBuilder.CreateAdd(InstBuilder.getInt32(1),
              //                               Load_B_C_T);
              //     InstBuilder.CreateStore(Value_B_C_T, branch_counter_target);
              //     break;
              //   }
              // }
            }
          }
        }
      }

      /* Fail to get clobbering instruction, only the clobbering memory access */
      // if (MemoryAccess *ClobberingAccess = Walker->getClobberingMemoryAccess(BrMemUse))
      // {
      //   if (isa<MemoryDef>(ClobberingAccess))
      //   {
      //     MemoryDef *ClobberingDef = cast<MemoryDef>(ClobberingAccess);
      //     Instruction *ClobberingIns = ClobberingDef->getMemoryInst();
      //     dbgs() << "Clobbering instruction for instruction: ";
      //     I.print(dbgs());
      //     dbgs() << " is";
      //     errs() << ClobberingIns << "\n";
      //     // ClobberingIns->print(dbgs());
      //     dbgs() << "\n";
      //   }
      //   // errs() << "ClobberingAccess: " << *ClobberingAccess;
      //   // errs() << ", Code: " << ClobberingIns << "\n";
      // }

      /* Failed to get the clobbering instruction of the cmpl */
      // if (auto *CmpIns = dyn_cast<CmpInst>(&I))
      // {
      //   errs() << "Found a compare instruction: " << *CmpIns << "\n";

      //   // Use &OpUse = CmpIns->getOperandUse(0);
      //   MemoryAccess *MemAccess = MSSA.getMemoryAccess(CmpIns);
      //   errs() << "Check memoryaccess: " << *CmpIns << "\n";

      //   // if (MemoryAccess *MemAccess = MSSA.getMemoryAccess(CmpIns))
      //   // {
      //   //   errs() << "Check memoryaccess: " << *MemAccess << "\n";

      //   //   if (MemoryUseOrDef *Def = dyn_cast<MemoryUseOrDef>(MemAccess))
      //   //   {
      //   //     if (MemoryAccess *OperandAccess = Def->getDefiningAccess())
      //   //     {
      //   //       if (Instruction *OperandInst = dyn_cast<Instruction>(OperandAccess))
      //   //       {
      //   //         errs() << "Operand instruction: " << *OperandInst << "\n";
      //   //       }
      //   //     }
      //   //     errs() << "Operand of compare instruction: " << *Def->getMemoryInst() << "\n";
      //   //   }
      //   // }
      // }

      // if (auto *StoreIns = dyn_cast<StoreInst>(&I))
      // if (auto *StoreIns = dyn_cast<BranchInst>(&I))
      // {
      //   IRBuilder<> InstBuilder(&I);
      //   LoadInst *Load_B_C_T = InstBuilder.CreateLoad(
      //       IntegerType::getInt32Ty(CTX), branch_counter_target);
      //   Value *Value_B_C_T =
      //       InstBuilder.CreateAdd(InstBuilder.getInt32(1),
      //                             Load_B_C_T);
      //   InstBuilder.CreateStore(Value_B_C_T, branch_counter_target);

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

      //       //   std::string PrintString = "The number of dangerous branch instructions is: %-10lu\n";
      //       //   FunctionType *InfoprintType = FunctionType::get(Type::getVoidTy(F.getContext()), /* ... */);
      //       //   Function *InfoprintFunc = F.getParent()->getFunction("infoprint");
      //       // }

      //       }
      //     }
      //   }
      // }
      // }
    }
  }

  /* Print */
  std::string PrintString = "The number of dangerous branch instructions is: %-10lu\n";
  infoprint(M, PrintString, branch_counter_target);

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