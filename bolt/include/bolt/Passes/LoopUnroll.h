#ifndef BOLT_PASSES_LOOPUNROLL_H
#define BOLT_PASSES_LOOPUNROLL_H

#include "bolt/Passes/BinaryPasses.h"

namespace llvm {
namespace bolt {

class LoopUnrollPass : public BinaryFunctionPass {
public:
  explicit LoopUnrollPass() : BinaryFunctionPass(false) {}

  const char *getName() const override { return "loop-unroll"; }
  std::vector<int> LoopProfileCount;

  /// Pass entry point
  void runOnFunctions(BinaryContext &BC) override;
  bool runOnFunction(BinaryFunction &Function);
  bool LoopProfile(BinaryFunction &Function);
};

} // namespace bolt
} // namespace llvm


#endif