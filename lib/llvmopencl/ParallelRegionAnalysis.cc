#include "ParallelRegionAnalysis.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

namespace pocl {

using namespace llvm;

ParallelRegionAnalysis::ParallelRegionAnalysis(ParallelRegion *region,
                                               llvm::DominatorTree *DT,
                                               llvm::PostDominatorTree *PDT)
    : parallelRegion(region), DT(DT), PDT(PDT), xDimensionUpperBound(nullptr),
      yDimensionUpperBound(nullptr), zDimensionUpperBound(nullptr), safeTransformation(true) {
  BranchInst *Br = getEarlyExitBranch();
  if (Br) {
    auto *CondInst = dyn_cast<Instruction>(Br->getCondition());
    findCompareInstructions(CondInst);
    // TODO: Better condition here. This is missed in lu kernel when early exit
    // is found but no global load instruction cannot be found.
    if (!safeTransformation) {
      errs() << "early exit branch transformation not safe or not understood\n";
    }
  } else {
    errs() << "no early exit found\n";
  }
}

llvm::Value *ParallelRegionAnalysis::getXDimensionUpperBound() const {
  if (xDimensionUpperBound && safeTransformation) {
    return xDimensionUpperBound->getOperand(1);
  }
  return nullptr;
}

llvm::Value *ParallelRegionAnalysis::getYDimensionUpperBound() const {
  if (xDimensionUpperBound && yDimensionUpperBound && safeTransformation) {
    return yDimensionUpperBound->getOperand(1);
  }
  return nullptr;
}

llvm::Value *ParallelRegionAnalysis::getZDimensionUpperBound() const {
  if (xDimensionUpperBound && yDimensionUpperBound && zDimensionUpperBound && safeTransformation) {
    return zDimensionUpperBound->getOperand(1);
  }
  return nullptr;
}

void ParallelRegionAnalysis::removeXUpperBound() {
  if (xDimensionUpperBound && safeTransformation) {
    llvm::errs() << "replacing x upper bound comparison instruction with constant\n";
    BasicBlock::iterator ii(xDimensionUpperBound);
    Value *Replacement = ConstantInt::getTrue(xDimensionUpperBound->getContext());
    ReplaceInstWithValue(xDimensionUpperBound->getParent()->getInstList(), ii, Replacement);
  }
}

void ParallelRegionAnalysis::removeYUpperBound() {
  if (xDimensionUpperBound && yDimensionUpperBound && safeTransformation) {
    llvm::errs() << "replacing y upper bound comparison instruction with constant\n";
    BasicBlock::iterator ii(yDimensionUpperBound);
    Value *Replacement = ConstantInt::getTrue(yDimensionUpperBound->getContext());
    ReplaceInstWithValue(yDimensionUpperBound->getParent()->getInstList(), ii, Replacement);
  }
}

BranchInst *ParallelRegionAnalysis::getEarlyExitBranch() {
  BranchInst *EarlyExitBr = nullptr;
  BranchInst *Br = nullptr;

  // Iterate over basic blocks and find the first branch terminator instruction.
  // This would be the early exit we're looking for.
  for (auto *BB : *parallelRegion) {
    Br = dyn_cast<BranchInst>(BB->getTerminator());
    if (Br && Br->isConditional() && Br->getNumSuccessors() == 2) {
      EarlyExitBr = Br;
      break;
    }
  }
  // Return if branch terminator wasn't found.
  if (!EarlyExitBr)
    return nullptr;
  assert(Br->getNumSuccessors() == 2 &&
         "early exit branch must have two successors basic blocks");

  auto *ConditionBB = Br->getParent();
  auto *FirstSuccessor = getNonSimpleSuccessor(Br->getSuccessor(0));
  auto *SecondSuccessor = getNonSimpleSuccessor(Br->getSuccessor(1));
  BasicBlock *ExitBB = nullptr;
  // If one of the successors blocks post-dominates the branch begin block, then
  // that block is the early exit block.
  if (PDT->dominates(FirstSuccessor, ConditionBB)) {
    ExitBB = FirstSuccessor;
  } else if (PDT->dominates(SecondSuccessor, ConditionBB)) {
    ExitBB = SecondSuccessor;
  }
  if (ExitBB) {
    return Br;
  }
  return nullptr;
}

void ParallelRegionAnalysis::findCompareInstructions(llvm::Instruction *Inst) {
  auto *CompareInst = dyn_cast<CmpInst>(Inst);
  auto *BinaryInst = dyn_cast<BinaryOperator>(Inst);
  // Check is this instruction the supported compare instruction.
  if (CompareInst) {
    processCompareInstruction(CompareInst);

  // Else instruction needs to be and operator so modification we want is
  // possible.
  } else if (BinaryInst &&
             BinaryInst->getOpcode() == Instruction::BinaryOps::And) {
    // This instruction was not the one we're looking for. Iterate recursively
    // over current instruction operand variables and check the instructions
    // that defined the used comparison variables.
    for (const Use &U : Inst->operands()) {
      Value *V = U.get();
      findCompareInstructions(dyn_cast<Instruction>(V));
    }
  } else {
    safeTransformation = false;
  }
}

void ParallelRegionAnalysis::processCompareInstruction(
    llvm::CmpInst *CompareInst) {
  if (CompareInst->getPredicate() == CmpInst::Predicate::ICMP_ULT ||
      CompareInst->getPredicate() == CmpInst::Predicate::ICMP_SLT) {
    // One of the early exit compare instruction operands should be parameter
    // variable to the function.
    if (dyn_cast<Argument>(CompareInst->getOperand(1))) {
      findUsedGlobalVariable(CompareInst);
      return;
    }
  } else {
    safeTransformation = false;
  }
}

void ParallelRegionAnalysis::findUsedGlobalVariable(llvm::CmpInst *CompareInst) {
  Instruction *Current = dyn_cast<Instruction>(CompareInst->getOperand(0));
  if (!Current) {
    return;
  }
  // Skip truncate instruction if kernel parameter is 32 bit instead of 64 bits.
  auto *Truncate = dyn_cast<TruncInst>(Current);
  if (Truncate) {
    Current = dyn_cast<Instruction>(Truncate->getOperand(0));
  }
  Current = dyn_cast<BinaryOperator>(Current);
  // Next instruction is supposed to be add which will add offset to the global
  // variable.
  if (!Current &&
             Current->getOpcode() != Instruction::BinaryOps::Add) {
    return;
  }
  // This add might use global load variable so try both its operands.
  processGlobalLoadInstuction(dyn_cast<LoadInst>(Current->getOperand(0)), CompareInst);
  processGlobalLoadInstuction(dyn_cast<LoadInst>(Current->getOperand(1)), CompareInst);
}

void ParallelRegionAnalysis::processGlobalLoadInstuction(llvm::LoadInst *globalLoadInst, llvm::CmpInst *CompareInst) {
  if (globalLoadInst) {
    StringRef name = globalLoadInst->getPointerOperand()->getName();
    if (name == POCL_LOCAL_ID_X_GLOBAL) {
      xDimensionUpperBound = CompareInst;
      errs() << "kernel using x upper bound:";
      CompareInst->dump();
    } else if (name == POCL_LOCAL_ID_Y_GLOBAL) {
      yDimensionUpperBound = CompareInst;
      errs() << "kernel using y upper bound:";
      CompareInst->dump();
    } else if (name == POCL_LOCAL_ID_Z_GLOBAL) {
      zDimensionUpperBound = CompareInst;
      errs() << "kernel using z upper bound:";
      CompareInst->dump();
    }
  }
}

//void ParallelRegionAnalysis::findUsedGlobalVariable(
    //llvm::Instruction *BeginInst, llvm::CmpInst *CompareInst) {
  //if (!BeginInst) {
    //return;
  //}
  //LoadInst *LoadI = dyn_cast<LoadInst>(BeginInst);
  //if (LoadI) {
    //StringRef name = LoadI->getPointerOperand()->getName();
    //if (name == POCL_LOCAL_ID_X_GLOBAL) {
      //xDimensionUpperBound = CompareInst;
      //errs() << "kernel using x upper bound:";
      //CompareInst->dump();
    //} else if (name == POCL_LOCAL_ID_Y_GLOBAL) {
      //yDimensionUpperBound = CompareInst;
      //errs() << "kernel using y upper bound:";
      //CompareInst->dump();
    //} else if (name == POCL_LOCAL_ID_Z_GLOBAL) {
      //zDimensionUpperBound = CompareInst;
      //errs() << "kernel using z upper bound:";
      //CompareInst->dump();
    //}
    //return;
  //}
  //for (const Use &U : BeginInst->operands()) {
    //Value *v = U.get();
    //findUsedGlobalVariable(dyn_cast<Instruction>(v), CompareInst);
  //}
//}

BasicBlock *
ParallelRegionAnalysis::getNonSimpleSuccessor(llvm::BasicBlock *Begin) {
  BasicBlock *Current = Begin;
  while (Current->size() == 1 && Current->getSinglePredecessor() != nullptr &&
         Current->getSingleSuccessor() != nullptr) {
    Current = Current->getSingleSuccessor();
  };
  return Current;
}

} // namespace pocl
