//==============================================================================
// FILE:
//    PassInterface.h
//
// DESCRIPTION:
//    Declares the DynamicBranchCounter pass for the new and the legacy pass
//    managers.
//
// License: MIT
//==============================================================================
#ifndef LLVM_TUTOR_INSTRUMENT_BASIC_H
#define LLVM_TUTOR_INSTRUMENT_BASIC_H

#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"

//------------------------------------------------------------------------------
// DynamicBranchCounter interface: new PM
//------------------------------------------------------------------------------
struct DynamicBranchCounter : public llvm::PassInfoMixin<DynamicBranchCounter> {
  llvm::PreservedAnalyses run(llvm::Module &M,
                              llvm::ModuleAnalysisManager &);
  bool runOnModule(llvm::Module &M);

  // Without isRequired returning true, this pass will be skipped for functions
  // decorated with the optnone LLVM attribute. Note that clang -O0 decorates
  // all functions with optnone.
  static bool isRequired() { return true; }
};

//------------------------------------------------------------------------------
// TargetBranchCounter interface: new PM
//------------------------------------------------------------------------------
// struct TargetBranchCounter : public llvm::PassInfoMixin<TargetBranchCounter> {
//   llvm::PreservedAnalyses run(llvm::Module &M,
//                               llvm::ModuleAnalysisManager &);
//   bool runOnModule(llvm::Module &M);
//   static bool isRequired() { return true; }
// };

#endif
