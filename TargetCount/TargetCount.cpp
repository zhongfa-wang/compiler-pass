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

  /* Create global variables */
  Module &M = *F.getParent();
  auto &CTX = M.getContext();
  std::string Countername_inst_all = std::string("all_instruction");
  std::string Countername_br_all = std::string("all_branch");
  std::string Countername_br_target = std::string("dangerous_branch");
  /* Counter of all instructions */
  Constant *Counter_all_instruction = CreateGlobalCounter(M, Countername_inst_all);
  /* Counter of all branches */
  Constant *Counter_all_branch = CreateGlobalCounter(M, Countername_br_all);
  /* Counter of the dangerous branch */
  Constant *Counter_dangerous_branch = CreateGlobalCounter(M, Countername_br_target);

  for (auto &B : F)
  {
    for (Instruction &I : B)
    {
      // IRBuilder<> IRBuilderAll(&I);
      // /* Increment on each instruction */
      // LoadInst *Load_counter_all_inst = IRBuilderAll.CreateLoad(
      //                 IntegerType::getInt32Ty(CTX), Counter_all_instruction);
      // Value *Value_counter_all_inst =
      //                 IRBuilderAll.CreateAdd(IRBuilderAll.getInt32(1),
      //                                       Load_counter_all_inst);
      // IRBuilderAll.CreateStore(Value_counter_all_inst, Counter_all_instruction);
      // if (auto *BrIns = dyn_cast<BranchInst>(&I))
      // {
      //   if (BrIns->isConditional())
      //   {
      //     /* Increment on each conditional branch */
      //     LoadInst *Load_counter_all_br = IRBuilderAll.CreateLoad(
      //                 IntegerType::getInt32Ty(CTX), Counter_all_branch);
      //     Value *Value_counter_all_br =
      //                 IRBuilderAll.CreateAdd(IRBuilderAll.getInt32(1),
      //                                       Load_counter_all_br);
      //     IRBuilderAll.CreateStore(Value_counter_all_br, Counter_all_branch);

      //     // /* Find the load op */
      //     // for (BasicBlock::reverse_iterator RI = ++I.getReverseIterator(), RE = B.rend(); RI != RE; ++RI)
      //     // {
      //     //   if (auto *CmpPreBrIns = dyn_cast<CmpInst>(&*RI))
      //     //   {
      //     //     // errs() << "Op: " << std::string(RI->getOpcodeName());
      //     //     // errs() << ", Code: " << *RI << "\n";
      //     //     for (Use &U : CmpPreBrIns->operands())
      //     //     {
      //     //       if (Instruction *CmpOpLoad = dyn_cast<Instruction>(U))
      //     //       {
      //     //         if (auto *IsLoad = dyn_cast<LoadInst>(&*CmpOpLoad))
      //     //         {
      //     //           // errs() << "Found the load before the branch: " << std::string(IsLoad->getOpcodeName());
      //     //           // errs() << ", Code: " << *IsLoad << "\n";
      //     //         }
      //     //         InsMap["compare_operand_load"] = &*CmpOpLoad;                                        // Store instruction to map
      //     //         // InsMemMap["compare_operand_load_memory_access"] = MSSA.getMemoryAccess(&*CmpOpLoad); // Store memory access to map
      //     //       }
      //     //       break;
      //     //     }
      //     //   }
      //     // }
          

      //     // Value *LoadPcedBr_Op = InsMap["compare_operand_load"]->getOperand(0);//LoadPcedBr_Op: the operand of the load preceding the branch
      //     // // errs() << "Get the branch-preceding load's operand: " << *LoadPcedBr_Op << "\n";
      //     // /* Get the succeeding basic block */
      //     // BasicBlock *BBScedBr = BrIns->getSuccessor(0);
      //     // for (Instruction &II : *BBScedBr)
      //     // {
      //     //   IRBuilder<> IRBuilderScedBr(&II);
      //     //   if (LoadInst *LoadScedBB = dyn_cast<LoadInst>(&II))
      //     //   {
      //     //     // errs() << "Found the load in the successor basic block: " << std::string(LoadScedBB->getOpcodeName());
      //     //     // errs() << ", Code: " << *LoadScedBB << "\n";
      //     //     Instruction *NextIns2Load = II.getNextNode(); //NextIns2Load: the very next instruction of the load
      //     //     if (auto *NextIns2Load_isCmp = dyn_cast<CmpInst>(NextIns2Load)){
      //     //       // errs() << "This load: " << II << "is related to a comparing: " << *NextIns2Load << ". \n";
      //     //       break;
      //     //     }
              
      //     //     Value *LoadScedBr_Op = LoadScedBB->getOperand(0); //LoadScedBr_Op: the operand of the load succeeding the branch
      //     //     // errs() << "Get the branch-succeeding load's operand: " << *LoadScedBr_Op << "\n";
      //     //     if (isa<Instruction>(*LoadScedBr_Op))
      //     //     {
      //     //       if (isa<Instruction>(*LoadPcedBr_Op))
      //     //       {
      //     //       Instruction *LoadPcedBr_Op_Inst = dyn_cast<Instruction>(LoadPcedBr_Op);
      //     //       Instruction *LoadScedBr_Op_Inst = dyn_cast<Instruction>(LoadScedBr_Op);
      //     //         if (LoadScedBr_Op_Inst == LoadPcedBr_Op_Inst) { 
      //     //         //  errs() << "Found the data dependence!" << *LoadScedBr_Op_Inst << "\n";
      //     //          LoadInst *Load_B_C_T = IRBuilderScedBr.CreateLoad(
      //     //             IntegerType::getInt32Ty(CTX), Counter_dangerous_branch);
      //     //         Value *Value_B_C_T =
      //     //             IRBuilderScedBr.CreateAdd(IRBuilderScedBr.getInt32(1),
      //     //                                   Load_B_C_T);
      //     //         IRBuilderScedBr.CreateStore(Value_B_C_T, Counter_dangerous_branch);
      //     //         break;
      //     //         }
      //     //       }
      //     //     }
      //     //   }
      //     // }
      //   }
      // }
    }
  }

  /* Print results */
  /* Print the number of all instructions */
  std::string PrintString_all_inst = "The number of all instructions is: %-10lu\n";
  /* Print the number of all branches */
  std::string PrintString_all_br = "The number of all branches is: %-10lu\n";
  // /* Print the number of dangerous branch*/
  // std::string PrintString_dangerous_br = "The number of dangerous branch instructions is: %-10lu\n";
  
  /* STEP 2: Inject the declaration of printf */
  PointerType *PrintfArgTy = PointerType::getUnqual(Type::getInt8Ty(CTX));
  FunctionType *PrintfTy =
      FunctionType::get(IntegerType::getInt32Ty(CTX), PrintfArgTy,
                        /*IsVarArgs=*/true);
  FunctionCallee Printf = M.getOrInsertFunction("printf", PrintfTy);
  /* Set attributes as per inferLibFuncAttributes in BuildLibCalls.cpp */
  Function *PrintfF = dyn_cast<Function>(Printf.getCallee());
  PrintfF->setDoesNotThrow();
  PrintfF->addParamAttr(0, Attribute::NoCapture);
  PrintfF->addParamAttr(0, Attribute::ReadOnly);

  /* STEP 3: Inject a global variable that will hold the printf format string */
  /* ----Global string variable: all instruction */
  llvm::Constant *FormatStr_all_inst = llvm::ConstantDataArray::getString(
      CTX, PrintString_all_inst);
  Constant *FormatStrGlbVar_all_inst = M.getOrInsertGlobal(
      "ResultFormatStrIR_all_inst", FormatStr_all_inst->getType());
  dyn_cast<GlobalVariable>(FormatStrGlbVar_all_inst)
      ->setInitializer(FormatStr_all_inst);
  /* ----Global string variable: all branch */
  llvm::Constant *FormatStr_all_br = llvm::ConstantDataArray::getString(
      CTX, PrintString_all_br);
  Constant *FormatStrGlbVar_all_br = M.getOrInsertGlobal(
      "ResultFormatStrIR_all_br", FormatStr_all_br->getType());
  dyn_cast<GlobalVariable>(FormatStrGlbVar_all_br)
      ->setInitializer(FormatStr_all_br);
  // /* ----Global string variable: dangerous branch */
  // llvm::Constant *FormatStr_dangerous_br = llvm::ConstantDataArray::getString(
  //     CTX, PrintString_dangerous_br);
  // Constant *FormatStrGlbVar_dangerous_br = M.getOrInsertGlobal(
  //     "ResultFormatStrIR_dangerous_br", FormatStr_dangerous_br->getType());
  // dyn_cast<GlobalVariable>(FormatStrGlbVar_dangerous_br)
  //     ->setInitializer(FormatStr_dangerous_br);

  /* STEP 4: Define a printf wrapper that will print the results */
  FunctionType *PrintfWrapperTy =
      FunctionType::get(llvm::Type::getVoidTy(CTX), {},
                        /*IsVarArgs=*/false);
  Function *PrintfWrapperF = dyn_cast<Function>(
      M.getOrInsertFunction("printf_wrapper", PrintfWrapperTy).getCallee());
  /* Create the entry basic block for printf_wrapper ... */
  llvm::BasicBlock *RetBlock =
      llvm::BasicBlock::Create(CTX, "enter", PrintfWrapperF);
  IRBuilder<> Builder(RetBlock);
  /* and start inserting calls to printf */
  /* ----all instruction */
  llvm::Value *ResultFormatStrPtr_all_inst =
      Builder.CreatePointerCast(FormatStrGlbVar_all_inst, PrintfArgTy);
  Builder.CreateCall(Printf, {ResultFormatStrPtr_all_inst,
                              Builder.CreateLoad(IntegerType::getInt32Ty(CTX),
                                                 Counter_all_instruction)});
  /* ----all branch */
  llvm::Value *ResultFormatStrPtr_all_br =
      Builder.CreatePointerCast(FormatStrGlbVar_all_br, PrintfArgTy);
  Builder.CreateCall(Printf, {ResultFormatStrPtr_all_br,
                              Builder.CreateLoad(IntegerType::getInt32Ty(CTX),
                                                 Counter_all_branch)});
  // /* ----dangerous branch */
  // llvm::Value *ResultFormatStrPtr_dangerous_br =
  //     Builder.CreatePointerCast(FormatStrGlbVar_dangerous_br, PrintfArgTy);
  // Builder.CreateCall(Printf, {ResultFormatStrPtr_dangerous_br,
  //                             Builder.CreateLoad(IntegerType::getInt32Ty(CTX),
  //                                                Counter_dangerous_branch)});
  /* Finally, insert return instruction */
  Builder.CreateRetVoid();
  // STEP 5: Call `printf_wrapper` at the very end of this module
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