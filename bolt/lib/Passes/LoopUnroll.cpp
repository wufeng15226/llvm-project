#include "bolt/Passes/LoopUnroll.h"

using namespace llvm;

namespace opts {
extern cl::OptionCategory BoltCategory;

static cl::opt<bool> LoopUnroll(
    "loop-unroll",
    cl::desc("loop unroll optimization"),
    cl::init(false), cl::cat(BoltCategory));

static cl::opt<bool> PrintProfilerLoop(
    "print-profiler-loop",
    cl::desc("print profiler loop"),
    cl::init(false), cl::cat(BoltCategory));

static cl::opt<std::string> SerializeProfilerLoopFileName(
    "specify-serialize-profiler-loop-file-name",
	cl::desc("serialize profiler loop file name"));
} // namespace opts

namespace llvm {
namespace bolt {

bool LoopUnrollPass::runOnFunction(BinaryFunction &BF) {
    BF.loopUnroll();
    return true;
}

bool LoopUnrollPass::runOnFunction(BinaryFunction &BF, raw_ostream &OS) {
    BF.printLoopProfiler(OS);
    return true;
}

bool LoopUnrollPass::runOnFunction(BinaryFunction &BF, nlohmann::json &json) {
    BF.serializeLoopProfiler(json);
    return true;
}

void LoopUnrollPass::runOnFunctions(BinaryContext &BC) {
    if (!opts::LoopUnroll)
        return;
    if (opts::PrintProfilerLoop) {
        for(auto& binaryFunctionPair: BC.getBinaryFunctions()) {
            auto& binaryFunction = binaryFunctionPair.second;
            runOnFunction(binaryFunction, outs());
        }
    }
    if (!opts::SerializeProfilerLoopFileName.empty()) {
        std::ofstream jsonFile;
        jsonFile.open(opts::SerializeProfilerLoopFileName);
        nlohmann::json json;

        for(auto& binaryFunctionPair: BC.getBinaryFunctions()) {
            auto& binaryFunction = binaryFunctionPair.second;
            runOnFunction(binaryFunction, json);
        }
        jsonFile<<json;
        jsonFile.close();
    }
}

} // end namespace bolt
} // end namespace llvm