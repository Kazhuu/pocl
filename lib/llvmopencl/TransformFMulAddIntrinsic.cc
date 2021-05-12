#include "TransformFMulAddIntrinsic.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

using namespace llvm;

namespace pocl {

char TransformFMulAddIntrinsic::ID = 0;

static RegisterPass<TransformFMulAddIntrinsic>
    X("transform-fmuladd-intrinsic",
      "Transform llvm.fmuladd intrinsic to fmul and fadd instructions");

bool TransformFMulAddIntrinsic::runOnFunction(llvm::Function &F) {
  bool Changed = false;
  for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E;) {
    IntrinsicInst *IntrInst = dyn_cast<IntrinsicInst>(&*I);
    if (IntrInst && IntrInst->getIntrinsicID() == Intrinsic::fmuladd) {
      ++I;
      errs() << "replacing llvm.fmuladd.* intrinsic with fmul and fadd "
                "instructions\n";
      Changed = transformIntrinsic(IntrInst);
    } else {
      ++I;
    }
  }
  return Changed;
}

void TransformFMulAddIntrinsic::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesCFG();
}

bool TransformFMulAddIntrinsic::transformIntrinsic(llvm::IntrinsicInst *Inst) {
  IRBuilder<> Builder(Inst);
  Value *FMul = Builder.CreateBinOp(Instruction::BinaryOps::FMul,
                                    Inst->getOperand(0), Inst->getOperand(1));
  Value *FAdd = Builder.CreateBinOp(Instruction::BinaryOps::FAdd, FMul,
                                    Inst->getOperand(2));
  Inst->replaceAllUsesWith(FAdd);
  Inst->eraseFromParent();
  return true;
}

} // namespace pocl
