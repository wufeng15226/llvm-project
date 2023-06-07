#ifndef BOLT_PASSES_LoopUnroll_H
#define BOLT_PASSES_LoopUnroll_H

#include "bolt/Passes/BinaryPasses.h"
#include <nlohmann/json.hpp>
#include <fstream>

namespace llvm {
namespace bolt {

class LoopUnrollPass : public BinaryFunctionPass {
public:
  explicit LoopUnrollPass() : BinaryFunctionPass(false) {}

  const char *getName() const override { return "loop-unroll"; }

  /// Pass entry point
  void runOnFunctions(BinaryContext &BC) override;
  bool runOnFunction(BinaryFunction &Function);
  bool runOnFunction(BinaryFunction &Function, raw_ostream &OS);
};

} // namespace bolt
} // namespace llvm


#endif