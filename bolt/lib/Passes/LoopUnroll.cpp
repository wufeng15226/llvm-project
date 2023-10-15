#include "bolt/Passes/LoopUnroll.h"

using namespace llvm;

namespace opts {
extern cl::OptionCategory BoltCategory;

static cl::opt<bool> LoopUnroll(
    "loop-unroll",
    cl::desc("loop unroll optimization"),
    cl::init(false), cl::cat(BoltCategory));

static cl::opt<bool> LoopUnrollProfile(
    "loop-unroll-profile",
    cl::desc("loop unroll using PGO"),
    cl::init(false), cl::cat(BoltCategory));
} // namespace opts

namespace llvm {
namespace bolt {

bool LoopUnrollPass::runOnFunction(BinaryFunction &BF) {
    BF.loopUnroll();
    return true;
}

bool LoopUnrollPass::LoopProfile(BinaryFunction &BF) {
    BF.loopProfile(LoopProfileCount);
    return true;
}

void LoopUnrollPass::runOnFunctions(BinaryContext &BC) {
    if (!opts::LoopUnroll)
        return;
    if(opts::LoopUnrollProfile){
        for(auto& binaryFunctionPair: BC.getBinaryFunctions()) {
            auto& binaryFunction = binaryFunctionPair.second;
            LoopProfile(binaryFunction);
        }
        std::sort(LoopProfileCount.begin(), LoopProfileCount.end(), std::greater<int>());
        for(int i = LoopProfileCount.size()-1; i >= 0; --i){
            if(LoopProfileCount[i] == 0){
                LoopProfileCount.erase(LoopProfileCount.begin()+i);
            }
            else{
                break;
            }
        }
        for (auto& count: LoopProfileCount){
            outs() << count << " ";
        }
        outs() << "\n";

        // log linear regression
        double sumX = 0, sumY = 0, sumXY = 0, sumXX = 0;
        for (int i = 0; i < LoopProfileCount.size(); i++) {
            sumX += i;
            sumY += log(LoopProfileCount[i]);
            // sumY += LoopProfileCount[i];
            sumXY += i * log(LoopProfileCount[i]);
            // sumXY += i * LoopProfileCount[i];
            sumXX += i * i;
            // outs() << log(LoopProfileCount[i]) << " ";
            // outs() << LoopProfileCount[i] << " ";
        }
        // outs() << "\n";
        double a = (sumXY - sumX * sumY / LoopProfileCount.size()) /
                (sumXX - sumX * sumX / LoopProfileCount.size());
        double b = sumY / LoopProfileCount.size() - a * sumX / LoopProfileCount.size();
        outs() << "a:" << a << " b:" << b << "\n";
        BC.HotLoopCountThreshold = exp(a * (LoopProfileCount.size() - 1) + b);
        // BC.HotLoopCountThreshold = a * (LoopProfileCount.size() - 1) + b;
        outs() << "log linear regression HotLoopCountThreshold:" << BC.HotLoopCountThreshold << "\n";
        // outs() << "linear regression HotLoopCountThreshold:" << BC.HotLoopCountThreshold << "\n";
        int hotCount = 0;
        for (auto v : LoopProfileCount)
        {
            if(v>BC.HotLoopCountThreshold)
                hotCount++;
            // outs() << (v>BC.HotLoopCountThreshold) << " ";
        }
        outs() << "\n";
        outs() << "find " << hotCount << " hot loop in " << LoopProfileCount.size() << " profile loop" << "\n";
    }
    for(auto& binaryFunctionPair: BC.getBinaryFunctions()) {
        auto& binaryFunction = binaryFunctionPair.second;
        runOnFunction(binaryFunction);
    }
}

} // end namespace bolt
} // end namespace llvm