#include "llvm/Analysis/InstCount.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/InitializePasses.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/IR/InstVisitor.h"

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include <llvm/IR/BasicBlock.h>
#include <map>
#include <typeinfo>
#include <iostream>

using namespace llvm;
using namespace std;

namespace
{
    struct InstCount : public FunctionPass
    {

        static char ID;
        InstCount() : FunctionPass(ID) {}
        bool runOnFunction(Function &F) override{
            map<string, int> opcode_map;
            std::cout << typeid(F.getName()).name() << std::endl;
            for (Function::iterator BB=F.begin(), BEnd=F.end(); BB != BEnd; ++BB) {
                for (BasicBlock::iterator instBegin = BB->begin(), instEnd = BB->end(); instBegin != instEnd; ++instBegin) {
                    if(opcode_map.find(instBegin->getOpcodeName()) == opcode_map.end()) {
                        opcode_map[instBegin->getOpcodeName()] = 1;
                    } else {
                        opcode_map[instBegin->getOpcodeName()] += 1;
                    }
                }
            }

            for(map<string, int>::iterator i = opcode_map.begin(), e = opcode_map.end(); i != e; ++i) {
                errs() << i->first << ":::" << i->second << "\n";

            }

            opcode_map.clear();

            return false;
        } 

    }; // end of struct TestHello
} // end of anonymous namespace

char InstCount::ID = 0;

static RegisterPass<InstCount> X("InstCount", "Test Hello World Pass",
                                 false /*Only loooks at CFG*/,
                                 false /*Analysis Pass*/);

static RegisterStandardPasses Y(
    PassManagerBuilder::EP_EarlyAsPossible,
    [](const PassManagerBuilder &Builder, legacy::PassManagerBase &PM)
    { PM.add(new InstCount()); });