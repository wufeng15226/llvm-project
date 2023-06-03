#ifndef BOLT_PASSES_LOOPFOLD_H
#define BOLT_PASSES_LOOPFOLD_H

#include "bolt/Passes/BinaryPasses.h"
#include <nlohmann/json.hpp>
#include <fstream>

namespace llvm {
namespace bolt {

class LoopFoldPass : public BinaryFunctionPass {
public:
  explicit LoopFoldPass() : BinaryFunctionPass(false) {}

  const char *getName() const override { return "loop-fold"; }

  /// Pass entry point
  void runOnFunctions(BinaryContext &BC) override;
  bool runOnFunction(BinaryFunction &Function);
  bool runOnFunction(BinaryFunction &Function, raw_ostream &OS);
  bool runOnFunction(BinaryFunction &Function, nlohmann::json &json);
};

} // namespace bolt
} // namespace llvm


#endif