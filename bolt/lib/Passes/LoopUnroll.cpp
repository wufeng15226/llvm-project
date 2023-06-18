#include "bolt/Passes/LoopUnroll.h"

using namespace llvm;

namespace opts {
extern cl::OptionCategory BoltCategory;

static cl::opt<bool> LoopUnroll(
    "loop-unroll",
    cl::desc("loop unroll optimization"),
    cl::init(false), cl::cat(BoltCategory));
} // namespace opts

namespace llvm {
namespace bolt {

bool LoopUnrollPass::runOnFunction(BinaryFunction &BF) {
    BF.loopUnroll();
    return true;
}

bool LoopUnrollPass::runOnFunction(BinaryFunction &BF, raw_ostream &OS) {
    return true;
}

void LoopUnrollPass::runOnFunctions(BinaryContext &BC) {
    if (!opts::LoopUnroll)
        return;
    for(auto& binaryFunctionPair: BC.getBinaryFunctions()) {
        auto& binaryFunction = binaryFunctionPair.second;
        runOnFunction(binaryFunction);
    }
}

} // end namespace bolt
} // end namespace llvm