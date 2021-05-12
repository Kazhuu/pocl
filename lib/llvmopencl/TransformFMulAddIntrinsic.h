#ifndef _POCL_TRANSFORM_FMULADD_INTRINSIC_H
#define _POCL_TRANSFORM_FMULADD_INTRINSIC_H

#include "llvm/IR/Function.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Pass.h"

namespace pocl {

class TransformFMulAddIntrinsic : public llvm::FunctionPass {
public:
  static char ID;

  TransformFMulAddIntrinsic() : FunctionPass(ID) {}
  virtual bool runOnFunction(llvm::Function &F) override;
  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;

private:
  bool transformIntrinsic(llvm::IntrinsicInst *Inst);
};

} // namespace pocl

#endif
