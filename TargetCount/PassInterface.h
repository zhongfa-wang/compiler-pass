//==============================================================================
// FILE:
//    PassInterface.h
//
// DESCRIPTION:
//    Declares the TargetBranchCounter pass for the new and the legacy pass
//    managers.
//
// License: MIT
//==============================================================================
#ifndef LLVM_TUTOR_INSTRUMENT_BASIC_H
#define LLVM_TUTOR_INSTRUMENT_BASIC_H

#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"

//------------------------------------------------------------------------------
// TargetBranchCounter interface: new PM
//------------------------------------------------------------------------------
struct TargetBranchCounter : public llvm::PassInfoMixin<TargetBranchCounter> {
  llvm::PreservedAnalyses run(llvm::Module &M,
                              llvm::ModuleAnalysisManager &);
  // bool runOnModule(llvm::Module &M);
  /* test: registering pass */
  bool runOnModule(llvm::Module &M, ModuleAnalysisManager &AM);

  // Without isRequired returning true, this pass will be skipped for functions
  // decorated with the optnone LLVM attribute. Note that clang -O0 decorates
  // all functions with optnone.
  static bool isRequired() { return true; }
};

#endif
