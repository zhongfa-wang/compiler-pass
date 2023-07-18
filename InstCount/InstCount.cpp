#include "llvm/Analysis/InstCount.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/InitializePasses.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include <iostream>
#include <llvm-14/llvm/IR/IRBuilder.h>
#include <llvm-14/llvm/IR/Instruction.h>
#include <llvm/IR/BasicBlock.h>
#include <map>
#include <typeinfo>

using namespace llvm;
using namespace std;

namespace {
struct InstCount : public ModulePass {
  static char ID;
  InstCount() : ModulePass(ID) {}
  bool runOnModule(Module &M) override {

    // declare and initialize the global variable BranchNumber
    auto type = IntegerType::getInt32Ty(M.getContext());
    auto constantInt = ConstantInt::getIntegerValue(type, APInt(32, 0));
    auto BranchNumber = new GlobalVariable(
        M, type, false, GlobalValue::CommonLinkage, constantInt, "br_number");

    for (Module::iterator FBegin = M.begin(), FEnd = M.end(); FBegin != FEnd;
         ++FBegin) {
      for (Function::iterator BB = FBegin->begin(), BEnd = FBegin->end();
           BB != BEnd; ++BB) {
        for (BasicBlock::iterator instBegin = BB->begin(), instEnd = BB->end();
             instBegin != instEnd; ++instBegin) {

          // Insert at the point where the instruction `op` appears.
          IRBuilder<> IR(dyn_cast<Instruction>(instBegin));
          // If the instruction is a branch, the BranchNumber++
          if (std::string(instBegin->getOpcodeName()) == "br") {
            LoadInst *Load = IR.CreateLoad(type, BranchNumber, "br_number");
            Value *Inc = IR.CreateAdd(IR.getInt32(1), Load);
            StoreInst *Store = IR.CreateStore(Inc, BranchNumber);
          }
        }
      }
    }

    return false;
  }

}; // End of struct InstCount

} // end of anonymous namespace

char InstCount::ID = 0;

static RegisterPass<InstCount> X("InstCount", "Test Hello World Pass",
                                 false /*Only loooks at CFG*/,
                                 false /*Analysis Pass*/);

static RegisterStandardPasses Y(PassManagerBuilder::EP_EarlyAsPossible,
                                [](const PassManagerBuilder &Builder,
                                   legacy::PassManagerBase &PM) {
                                  PM.add(new InstCount());
                                });