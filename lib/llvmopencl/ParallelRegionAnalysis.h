#ifndef _POCL_PARALLEL_REGION_ANALYSIS_H
#define _POCL_PARALLEL_REGION_ANALYSIS_H

#include "ParallelRegion.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/IR/Instructions.h"
#include <llvm/IR/Dominators.h>

namespace pocl {

/**
 * Missing early exit cases:
 * - When early exit has other comparisons than less than.
 * - Lower bound support (i > 2). Start loop from 2 rather 0.
 * - Exact bound (i == 0). Execute iteration only once. gramschmidt kernel has
 *   this.
 * - Parameter +- some constant (i < ni - 1). Add this calculation to bound
 *   check.
 * - When parameter is below zero which basically don't execute the kernel at
 *   all but with current modification loop body is executed at least once
 *   before exit.
 * - When && is used for multiple comparison sometimes there exists another
 *   early exit branch right before the loop body after the first one. This is
 *   currently missed.  fdtd2d kernel 2 is example of this.
 */
class ParallelRegionAnalysis {
public:
  ParallelRegionAnalysis(ParallelRegion *region, llvm::DominatorTree *DT,
                         llvm::PostDominatorTree *PDT);
  llvm::Value *getXDimensionUpperBound() const;
  llvm::Value *getYDimensionUpperBound() const;
  llvm::Value *getZDimensionUpperBound() const;
  void removeXUpperBound();
  void removeYUpperBound();

private:
  /**
   * Find early exit branch instruction if parallel region has one, nullptr
   * otherwise.
   */
  llvm::BranchInst *getEarlyExitBranch();
  void findCompareInstructions(llvm::Instruction *Inst);
  /**
   * Find used get_global_id() returned variable that given compare instruction
   * uses. Search from given compare instruction and only very simple use of
   * global variable is supported. This is to avoid transformation in cases like
   * get_global_id() + constant.
   */
  void findUsedGlobalVariable(llvm::CmpInst *CompareInst);
  void processCompareInstruction(llvm::CmpInst *CompareInst);
  void processGlobalLoadInstuction(llvm::LoadInst *globalLoadInst, llvm::CmpInst *CompareInst);
  //void findUsedGlobalVariable(llvm::Instruction *BeginInst, llvm::CmpInst *CompareInst);
  /**
   * Find successor block that is not a simple block and return it. Here simple
   * meaning that block only has only one successor and predecessor and has one
   * unconditional jump instruction. If block is simple then try next successor
   * in the line until non-simple one is found.
   */
  llvm::BasicBlock *getNonSimpleSuccessor(llvm::BasicBlock *Begin);

  ParallelRegion *parallelRegion;
  llvm::DominatorTree *DT;
  llvm::PostDominatorTree *PDT;
  llvm::CmpInst *xDimensionUpperBound;
  llvm::CmpInst *yDimensionUpperBound;
  llvm::CmpInst *zDimensionUpperBound;
  bool safeTransformation;
};

} // namespace pocl

#endif
