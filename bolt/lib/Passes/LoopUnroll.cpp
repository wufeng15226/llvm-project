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
    BF.loopUnroll2();
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
        std::sort(LoopProfileCount.begin(), LoopProfileCount.end());
        for(int i = 0; i < LoopProfileCount.size(); ++i){
            ++LoopProfileCount[i];
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
        BC.HotLoopCountThreshold = exp(a * (LoopProfileCount.size() - 1) + b) + 1;
        BC.MidLoopCountThreshold = exp(a * (LoopProfileCount.size()/2) + b) + 1;
        BC.ColdLoopCountThreshold = exp(b) + 1;
        outs() << "log linear regression HotLoopCountThreshold:" << BC.HotLoopCountThreshold << "\n";
        outs() << "log linear regression MidLoopCountThreshold:" << BC.MidLoopCountThreshold << "\n";
        outs() << "log linear regression ColdLoopCountThreshold:" << BC.ColdLoopCountThreshold << "\n";
        int hotCount = 0;
        int mildHotCount = 0;
        int mildColdCount = 0;
        int coldCount = 0;
        for (auto v : LoopProfileCount)
        {
            if(v>BC.HotLoopCountThreshold)
                ++hotCount;
            else if(v>BC.MidLoopCountThreshold)
                ++mildHotCount;
            else if(v>BC.ColdLoopCountThreshold)
                ++mildColdCount;
            else
                ++coldCount;
        }
        outs() << "\n";
        outs() << "find " << hotCount << " hot loop in " << LoopProfileCount.size() << " profile loop" << "\n";
        outs() << "find " << mildHotCount << " mild hot loop in " << LoopProfileCount.size() << " profile loop" << "\n";
        outs() << "find " << mildColdCount << " mild cold loop in " << LoopProfileCount.size() << " profile loop" << "\n";
        outs() << "find " << coldCount << " cold loop in " << LoopProfileCount.size() << " profile loop" << "\n";
    }
    for(auto& binaryFunctionPair: BC.getBinaryFunctions()) {
        auto& binaryFunction = binaryFunctionPair.second;
        runOnFunction(binaryFunction);
    }
}

} // end namespace bolt
} // end namespace llvm