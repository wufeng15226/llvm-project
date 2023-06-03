#include "bolt/Passes/LoopFold.h"

using namespace llvm;

namespace opts {
extern cl::OptionCategory BoltCategory;

static cl::opt<bool> LoopFold(
    "loop-fold",
    cl::desc("loop folding optimization"),
    cl::init(false), cl::cat(BoltCategory));

static cl::opt<bool> PrintLoopInstructions(
    "print-loop-instructions",
    cl::desc("print loop instructions"),
    cl::init(false), cl::cat(BoltCategory));

static cl::opt<std::string> SerializeLoopFileName(
    "specify-serialize-loop-file-name",
	cl::desc("serialize loop file name"));
} // namespace opts

namespace llvm {
namespace bolt {

bool LoopFoldPass::runOnFunction(BinaryFunction &BF) {
    return true;
}

bool LoopFoldPass::runOnFunction(BinaryFunction &BF, raw_ostream &OS) {
    BF.printLoopInstructions(OS);
    return true;
}

bool LoopFoldPass::runOnFunction(BinaryFunction &BF, nlohmann::json &json) {
    BF.serializeLoopInstructions(json);
    return true;
}

void LoopFoldPass::runOnFunctions(BinaryContext &BC) {
    if (!opts::LoopFold)
        return;
    if (opts::PrintLoopInstructions) {
        outs() << "PrintLoopInstructions Begin\n";
        for(auto& binaryFunctionPair: BC.getBinaryFunctions()) {
            auto& binaryFunction = binaryFunctionPair.second;
            runOnFunction(binaryFunction, outs());
        }
        outs() << "PrintLoopInstructions End\n";
    }
    if (!opts::SerializeLoopFileName.empty()) {
        std::ofstream jsonFile;
        jsonFile.open(opts::SerializeLoopFileName);
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