//===--- BinaryLoop.h - Interface for machine-level loop ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the BinaryLoop class, which represents a loop in the
// CFG of a binary function, and the BinaryLoopInfo class, which stores
// information about all the loops of a binary function.
//
//===----------------------------------------------------------------------===//

#include "bolt/Core/BinaryBasicBlock.h"
#include "bolt/Core/BinaryLoop.h"
#include "bolt/Core/BinaryContext.h"
#include "bolt/Core/BinaryFunction.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/MC/MCInst.h"
#include "bolt/Utils/Utils.h"
#include "bolt/Utils/CommandLineOpts.h"
#include <queue>

#undef DEBUG_TYPE
#define DEBUG_TYPE "bolt"

using namespace llvm;
using namespace bolt;

namespace opts {
  extern cl::opt<bool> PrintLoopFold;
  extern cl::opt<bool> RemoveSuffixTree;
  extern cl::opt<bool> RemoveSubDDG;
}

namespace llvm {
namespace bolt {

// bool BinaryLoop::GetLoopIterReg(unsigned &LoopIterReg) {
//   // Loop iteration register usually located at the second last Inst.
//   LoopIterReg = 0;
//   assert(getBlocks().size() == 1 && "Loop should have only one block");
//   BinaryBasicBlock *LastBB = getBlocks()[getBlocks().size() - 1];
//   const BinaryContext &BC = LastBB->getFunction()->getBinaryContext();
//   // Some loops only have a jump instruction.
//   if (LastBB->size() < 2)
//     return false;
  
//   MCInst ComInst = LastBB->getInstructionAtIndex(LastBB->size() - 2);
//   if(!(BC.MIB->isCompare(ComInst)) || !(ComInst.getNumOperands() == 2)) {
//     // No compare instruction but computations also change flags used by the
//     // jump instruction.
//     // From end to start, check if the last ADDri instruction has LoopIterReg.
//     for (auto II = (LastBB->rbegin() + 1); II != LastBB->rend(); ++II) {
//       if (BC.MIB->isADDri(*II)) {
//         LoopIterReg = II->getOperand(0).getReg();
//         // Check if the register is the real loop iterator.
//         auto isRegUsedByInst = [&](const MCInst &Inst, unsigned Reg) {
//           BitVector GoalRegs(BC.MRI->getNumRegs(), false);
//           GoalRegs |= BC.MIB->getAliases(Reg, /*OnlySmaller=*/true);
//           BitVector SrcRegs(BC.MRI->getNumRegs(), false);
//           BC.MIB->getSrcRegs(Inst, SrcRegs);
//           for (unsigned n = 0; n != BC.MRI->getNumRegs(); ++n) {
//             if (SrcRegs[n] == 1 && GoalRegs[n] == 1) {
//               return true;
//             }
//           }
//           return false;
//         };
//         // I believe that the LoopIterReg will be used by the first instruction.
//         if(isRegUsedByInst(LastBB->getInstructionAtIndex(0), LoopIterReg))
//           return true;
//         else 
//           continue;
//       } 
//       // No LoopIterReg found if no more ADDri.
//       return false;
//     }
//   } else {
//     // In fact, sometimes the operand in compare instruction isn't the loop
//     // iteration register. But now we skip this situation.
//     MCOperand firstOperand = ComInst.getOperand(0);
//     MCOperand secondOperand = ComInst.getOperand(1);
//     if (firstOperand.isImm()) {
//       assert(secondOperand.isReg() && "Second operand should be register");
//       LoopIterReg = secondOperand.getReg();
//       return true;
//     }
//     if (secondOperand.isImm()) {
//       assert(firstOperand.isReg() && "First operand should be register");
//       LoopIterReg = firstOperand.getReg();
//       return true;
//     }

//     assert(firstOperand.isReg() && secondOperand.isReg() &&
//           "All operands should be register");

//     // Try to find first Operand
//     BitVector FirstRegs(BC.MRI->getNumRegs(), false);
//     FirstRegs |= BC.MIB->getAliases(firstOperand.getReg(), false);
//     BitVector SecondRegs(BC.MRI->getNumRegs(), false);
//     SecondRegs |= BC.MIB->getAliases(secondOperand.getReg(), false);

//     // Traverse all BB to find loop iteration register
//     for (unsigned m = 0; m < getBlocks().size(); ++m) {
//       BinaryBasicBlock *BB = getBlocks()[m];
//       for (auto II = BB->begin(); II != BB->end(); ++II) {
//         if (m == getBlocks().size() - 1 && II + 2 >= BB->end())
//           break;
//         BitVector SrcRegs(BC.MRI->getNumRegs(), false);
//         BC.MIB->getSrcRegs(*II, SrcRegs);
//         for (unsigned n = 0; n != BC.MRI->getNumRegs(); ++n) {
//           if ((SrcRegs[n] & FirstRegs[n]) != 0) {
//             LoopIterReg = firstOperand.getReg();
//             return true;
//           }
//         }
//       }
//     }

//     for (unsigned m = 0; m < getBlocks().size(); ++m) {
//       BinaryBasicBlock *BB = getBlocks()[m];
//       for (auto II = BB->begin(); II != BB->end(); ++II) {
//         if (m == getBlocks().size() - 1 && II + 2 >= BB->end())
//           break;
//         BitVector SrcRegs(BC.MRI->getNumRegs(), false);
//         BC.MIB->getSrcRegs(*II, SrcRegs);
//         for (unsigned n = 0; n != BC.MRI->getNumRegs(); ++n) {
//           if ((SrcRegs[n] & SecondRegs[n]) != 0) {
//             LoopIterReg = secondOperand.getReg();
//             return true;
//           }
//         }
//       }
//     }
//     return false;
//   }
//   return false;
// }

bool BinaryLoop::GetLoopIterReg2(unsigned &LoopIterReg) {
  // Sometimes the operand in compare instruction isn't the loop
  // iteration register. So we can only find the loop iteration register in
  // Addri instruction.
  LoopIterReg = 0;
  assert(getBlocks().size() == 1 && "Loop should have only one block");
  BinaryBasicBlock *LastBB = getBlocks()[getBlocks().size() - 1];
  const BinaryContext &BC = LastBB->getFunction()->getBinaryContext();
  // Some loops only have a jump instruction. Skip it.
  if (LastBB->size() < 2)
    return false;

  // Try to find more IteratorRegister if failed.
  // auto isIteratorRegister = [&](unsigned Reg) {
  //   MemoryOperand FoundMemOperand;
  //   BitVector Regs(BC.MRI->getNumRegs(), false);
  //   Regs |= BC.MIB->getAliases(Reg, false);
  //   bool foundOnce = false;
  //   int64_t baseOrIndex = 0;
  //   unsigned UnrollOpcode = 0;

  //   for (auto II = LastBB->begin(); II != LastBB->end(); ++II) {
  //     MemoryOperand MemOperand;
  //     if (BC.MIB->evaluateX86MemoryOperand(
  //             *II, &MemOperand.BaseRegNum, &MemOperand.ScaleValue,
  //             &MemOperand.IndexRegNum, &MemOperand.DispValue,
  //             &MemOperand.SegRegNum, &MemOperand.DispExpr)) {
  //       printX86MemoryOperand(MemOperand);
  //       for (unsigned i = 0; i != BC.MRI->getNumRegs(); ++i) {
  //         // LoopIterReg is used in BaseReg.
  //         if (Regs[i] != 0 &&
  //             (i == MemOperand.BaseRegNum || i == MemOperand.IndexRegNum)) {
  //           if (i == MemOperand.BaseRegNum)
  //             baseOrIndex = -1;
  //           else
  //             baseOrIndex = 1;
  //           FoundMemOperand = MemOperand;
  //           UnrollOpcode = II->getOpcode();
  //           for (auto III = II + 1; III != LastBB->end(); ++III) {
  //             MemoryOperand MemOperand;
  //             if (BC.MIB->evaluateX86MemoryOperand(
  //                     *II, &MemOperand.BaseRegNum, &MemOperand.ScaleValue,
  //                     &MemOperand.IndexRegNum, &MemOperand.DispValue,
  //                     &MemOperand.SegRegNum, &MemOperand.DispExpr)) {
  //               for (unsigned i = 0; i != BC.MRI->getNumRegs(); ++i) {
  //                 // LoopIterReg is used in BaseReg.
  //                 if (Regs[i] != 0 &&
  //                     ((i == MemOperand.BaseRegNum && baseOrIndex == -1) ||
  //                      (i == MemOperand.IndexRegNum && baseOrIndex == 1))) {
  //                   if (compareMemExceptDisp(FoundMemOperand, MemOperand)) {
  //                     if (II->getOpcode() != UnrollOpcode)
  //                       break;
  //                     return true;
  //                   }
  //                 }
  //               }
  //             }
  //           }
  //           baseOrIndex = 0;
  //           break;
  //         }
  //       }
  //     }
  //   }
  //   return false;
  // };

  auto isIteratorRegister = [&](unsigned Reg) {
    MemoryOperand FoundMemOperand;

    BitVector Regs(BC.MRI->getNumRegs(), false);
    Regs |= BC.MIB->getAliases(Reg, false);
    bool foundOnce = false;
    int64_t baseOrIndex = 0;
    unsigned UnrollOpcode = 0;

    for (auto II = LastBB->begin(); II != LastBB->end(); ++II) {
      MemoryOperand MemOperand;
      if (BC.MIB->evaluateX86MemoryOperand(
              *II, &MemOperand.BaseRegNum, &MemOperand.ScaleValue,
              &MemOperand.IndexRegNum, &MemOperand.DispValue,
              &MemOperand.SegRegNum, &MemOperand.DispExpr)) {
        for (unsigned i = 0; i != BC.MRI->getNumRegs(); ++i) {
          // LoopIterReg is used in BaseReg.
          if (Regs[i] != 0 && i == MemOperand.BaseRegNum && baseOrIndex <= 0) {
            if (!foundOnce) {
              foundOnce = true;
              baseOrIndex = -1;
              FoundMemOperand = MemOperand;
              UnrollOpcode = II->getOpcode();
            } else {
              if (compareMemExceptDisp(FoundMemOperand, MemOperand)) {
                if (II->getOpcode() != UnrollOpcode)
                  break;
                return true;
              }
            }
            break;
          }
          // LoopIterReg is used in IndexReg.
          if (Regs[i] != 0 && i == MemOperand.IndexRegNum && baseOrIndex >= 0) {
            if (!foundOnce) {
              foundOnce = true;
              baseOrIndex = 1;
              FoundMemOperand = MemOperand;
              UnrollOpcode = II->getOpcode();
            } else {
              if (compareMemExceptDisp(FoundMemOperand, MemOperand)) {
                if (II->getOpcode() != UnrollOpcode)
                  break;
                return true;
              }
            }
            break;
          }
        }
      }
    }
    return false;
  };

  // From end to start, check if the ADDri instruction has LoopIterReg.
  for (auto II = LastBB->rbegin(); II != LastBB->rend(); ++II) {
    if (!BC.MIB->isADDri(*II))
      continue;
    BC.printInstruction(outs(), *II);
    if (II->getOpcode() == 389 && II->getNumOperands() == 1) { // X86::ADD64i32
      MCOperand m = II->getOperand(0);
      II->clear();
      II->setOpcode(393);                       // X86::ADD64ri32
      II->addOperand(MCOperand::createReg(49)); // X86::RAX
      II->addOperand(MCOperand::createReg(49)); // X86::RAX
      II->addOperand(m);
    }
    if(II->getOperand(0).isReg()){
      unsigned CandidateReg = II->getOperand(0).getReg();
      if (isIteratorRegister(CandidateReg)) {
        LoopIterReg = CandidateReg;
        return true;
      }
    }
  }
  return false;
}

bool BinaryLoop::getLoopUnrollFactor(
    const unsigned LoopIterReg, int64_t &LoopUnrollFactor,
    int64_t &LoopUnrollStep, int64_t &LoopUnrollStart, MemoryOperand &MemOp,
    std::vector<MemoryOperand> *ExcludeMemoryOperands) {
  LoopUnrollFactor = 1;
  LoopUnrollStep = 0;
  LoopUnrollStart = 0;

  assert(getBlocks().size() == 1 && "Loop should have only one block");
  BinaryBasicBlock *BB = getBlocks()[0];
  const BinaryContext &BC = BB->getFunction()->getBinaryContext();

  // LoopIterReg may occur in BaseReg or IndexReg, -1 means BaseReg, 1 means
  // IndexReg, 0 means unknown.
  int64_t baseOrIndex = 0;
  std::vector<int64_t> DispValues;
  unsigned UnrollOpcode = 0;

  MemoryOperand FoundMemOperand;

  // For each instruction in the loop, check if it is a memory instruction
  // related to LoopIterReg.
  for (auto II = BB->begin(); II != BB->end(); ++II) {
    BitVector LoopIterRegs(BC.MRI->getNumRegs(), false);
    LoopIterRegs |= BC.MIB->getAliases(LoopIterReg, false);

    MemoryOperand MemOperand;
    if (BC.MIB->evaluateX86MemoryOperand(
            *II, &MemOperand.BaseRegNum, &MemOperand.ScaleValue,
            &MemOperand.IndexRegNum, &MemOperand.DispValue,
            &MemOperand.SegRegNum, &MemOperand.DispExpr)) {
      //  DispValue can increase or decrease by step;

      bool skipII = false;
      if (ExcludeMemoryOperands != nullptr) {
        for (auto ExcludeMemOperand : *ExcludeMemoryOperands) {
          if (compareMemExceptDisp(ExcludeMemOperand, MemOperand) &&
              ExcludeMemOperand.UnrollOpcode == II->getOpcode()) {
            skipII = true;
          }
        }
        if (skipII)
          continue;
      }
      for (unsigned i = 0; i != BC.MRI->getNumRegs(); ++i) {
        // LoopIterReg is used in BaseReg.
        if (LoopIterRegs[i] != 0 && i == MemOperand.BaseRegNum &&
            baseOrIndex <= 0) {
          int dispExprValue = 0;
          if (MemOperand.DispExpr != NULL) {
            if (MemOperand.DispExpr->getKind() == MCExpr::Binary) {
              auto *BE = static_cast<const MCBinaryExpr *>(MemOperand.DispExpr);
              if (isa<MCConstantExpr>(BE->getRHS())) {
                dispExprValue =
                    static_cast<const MCConstantExpr *>(BE->getRHS())
                        ->getValue();
              }
            }
          }
          int offsetValue = MemOperand.DispValue + dispExprValue;
          if (DispValues.empty()) {
            // Once we get the memory operand, we should remember it. Then all
            // operands after it should be the same except dispValue.
            // DispValues are do not contain zero.
            DispValues.emplace_back(offsetValue);
            UnrollOpcode = II->getOpcode();
            baseOrIndex = -1;
            FoundMemOperand = MemOperand;
            MemOp = FoundMemOperand;
            MemOp.UnrollOpcode = UnrollOpcode;
            tge_log("FoundMemOperand: ", RESET);
            printX86MemoryOperand(MemOperand);
            BC.printInstruction(outs(), *II);
          } else {
            if (compareMemExceptDisp(FoundMemOperand, MemOperand) &&
                II->getOpcode() == UnrollOpcode) {
              auto it =
                  std::find(DispValues.begin(), DispValues.end(), offsetValue);
              if (it == DispValues.end()) {
                DispValues.emplace_back(offsetValue);
              }
            }
          }
          break;
        }
        // LoopIterReg is used in IndexReg.
        if (LoopIterRegs[i] != 0 && i == MemOperand.IndexRegNum &&
            baseOrIndex >= 0) {
          int dispExprValue = 0;
          if (MemOperand.DispExpr != NULL) {
            if (MemOperand.DispExpr->getKind() == MCExpr::Binary) {
              auto *BE = static_cast<const MCBinaryExpr *>(MemOperand.DispExpr);
              if (isa<MCConstantExpr>(BE->getRHS())) {
                dispExprValue =
                    static_cast<const MCConstantExpr *>(BE->getRHS())
                        ->getValue();
              }
            }
          }
          int offsetValue = MemOperand.DispValue + dispExprValue;
          if (DispValues.empty()) {
            DispValues.emplace_back(offsetValue);
            UnrollOpcode = II->getOpcode();
            baseOrIndex = 1;
            FoundMemOperand = MemOperand;
            MemOp = FoundMemOperand;
            MemOp.UnrollOpcode = UnrollOpcode;
            tge_log("FoundMemOperand: ", RESET);
            printX86MemoryOperand(MemOperand);
            BC.printInstruction(outs(), *II);
          } else {
            if (compareMemExceptDisp(FoundMemOperand, MemOperand) &&
                II->getOpcode() == UnrollOpcode) {
              auto it =
                  std::find(DispValues.begin(), DispValues.end(), offsetValue);
              if (it == DispValues.end()) {
                DispValues.emplace_back(offsetValue);
              }
            }
          }
          break;
        }
      }
    }
  }
  if (DispValues.empty())
    return false;
  // Just to output the order of the dispValues.
  std::sort(DispValues.begin(), DispValues.end());
  // tge_log("DispValue: ", RESET);
  // for (auto DispValue : DispValues) {
  //   tge_log(" " << DispValue, RESET);
  // }
  auto isAscending = [](std::vector<int64_t> DispValues, int64_t &Factor,
                        int64_t &Step, int64_t &Start) {
    std::sort(DispValues.begin(), DispValues.end());
    if (DispValues.size() > 1) {
      int step = DispValues[1] - DispValues[0];
      for (unsigned int i = 2; i < DispValues.size(); i++) {
        if (DispValues[i] != DispValues[i - 1] + step) {
          return false;
        }
      }
      if (step == 0 || DispValues.size() == 0)
        return false;
      // TODO(tge): Check divide update variable.
      Factor = DispValues.size();
      Step = step;
      Start = DispValues[0];
      return true;
    }
    return false;
  };

  auto tryDivide = [&](int divide, std::vector<int64_t> DispValues,
                       int64_t &Factor, int64_t &Step, int64_t &Start) {
    std::vector<std::vector<int64_t>> vv(divide);
    if (DispValues.size() % divide == 0) {
      unsigned int v_step = DispValues.size() / divide;
      std::sort(DispValues.begin(), DispValues.end());
      // auto it = DispValues.begin();
      for (int i = 0; i < divide; i++) {

        std::vector<int64_t> v(DispValues.begin() + i * v_step,
                               DispValues.begin() + (i + 1) * v_step);
        vv[i] = v;
        outs() << "v.size() " << v.size() << " " << v_step << "\n";
        assert(v.size() == v_step);
      }
      int64_t Factor_0, Step_0, Start_0, Factor_1, Step_1, Start_1;
      for (int i = 0; i < divide; i++) {
        if (i == 0) {
          if (!isAscending(vv[i], Factor_0, Step_0, Start_0))
            return false;
        } else {
          if (!isAscending(vv[i], Factor_1, Step_1, Start_1) ||
              (Factor_0 != Factor_1 && Step_0 != Step_1))
            return false;
        }
      }
      Factor = Factor_0;
      Step = Step_0;
      Start = Start_0;
      if (Step == 0 || Factor == 0)
        return false;
      return true;
    }
    return false;
  };

  if (isAscending(DispValues, LoopUnrollFactor, LoopUnrollStep,
                  LoopUnrollStart))
    return true;

  // for (int tryNum = 2; tryNum <= 4; tryNum++) {
  //   if (tryDivide(tryNum, DispValues, LoopUnrollFactor, LoopUnrollStep,
  //                 LoopUnrollStart))
  //     return true;
  // }
  return false;
}

bool BinaryLoop::dispatchLoopUpdateInst(const unsigned LoopIterReg,
                                        BinaryBasicBlock::iterator &UpdatePos) {
  MCInst UdpInst;
  bool FoundInst = false;
  bool NoNeedDispatch = false;
  int64_t InstStep = 0;
  assert(getBlocks().size() == 1 && "Loop should have only one block");
  BinaryBasicBlock *BB = getBlocks()[0];
  const BinaryContext &BC = BB->getFunction()->getBinaryContext();

  BitVector GoalRegs(BC.MRI->getNumRegs(), false);
  GoalRegs |= BC.MIB->getAliases(LoopIterReg, /*OnlySmaller=*/true);
  UpdatePos = BB->end();

  assert(BC.MIB->isBranch(*(BB->end() - 1)) && "Last instruction should be branch.");
  int labelIndex = 1;
  for (auto II = BB->rbegin(); II != BB->rend(); ++II, ++labelIndex) {
    if (BC.MIB->isBranch(*II) && BC.MIB->getTargetSymbol(*II) == BB->getLabel())
      break;
  }

  int cmpInstIndex = 0;
  if (BC.MIB->isCompare(*(BB->end() - labelIndex - 1))) {
    // add, cmp, br
    cmpInstIndex = 3;
  } else {
    // add, br
    cmpInstIndex = 2;
  }

  // tge_log("labelIndex:" << labelIndex << " cmpInstIndex:" << cmpInstIndex
  //                       << "\n",
  //         RESET);

  for (auto II = BB->begin(); II != BB->end() - labelIndex + 1; ++II) {
    if ((II + cmpInstIndex - 1) == BB->end() - labelIndex + 1) {
      if (FoundInst) {
        // Move the update instruction before branch instruction.
        // If there is ADDri instruction before branch instruction, add before
        // it;
        if (NoNeedDispatch)
          break;
        if (BC.MIB->isADDri(*(II - 1)))
          BB->insertInstruction(II - 1, UdpInst);
        else
          BB->insertInstruction(II, UdpInst);
      } else {
        assert(false && "No UpdateInstruction found");
      }
      break;
    }
    if (!FoundInst) {
      // Try to find update instruction and delete it.
      if (BC.MIB->isADDri(*II)) {
        if (II->getOpcode() == 389 &&
            II->getNumOperands() == 1) { // X86::ADD64i32
          MCOperand m = II->getOperand(0);
          II->clear();
          II->setOpcode(393);                       // X86::ADD64ri32
          II->addOperand(MCOperand::createReg(49)); // X86::RAX
          II->addOperand(MCOperand::createReg(49)); // X86::RAX
          II->addOperand(m);
        }
        BitVector WrittenRegs(BC.MRI->getNumRegs(), false);
        BC.MIB->getWrittenRegs(*II, WrittenRegs);
        // for (unsigned m = 0; m != BC.MRI->getNumRegs(); ++m) {
        //     if(GoalRegs[m]==1)
        //       outs()<<m<<" ";
        // }
        // outs()<<" GoalRegs\n ";
        // for (unsigned m = 0; m != BC.MRI->getNumRegs(); ++m) {
        //     if(WrittenRegs[m]==1)
        //       outs()<<m<<" ";
        // }
        // outs()<<" WrittenRegs\n ";
        for (unsigned m = 0; m != BC.MRI->getNumRegs(); ++m) {
          if ((WrittenRegs[m] !=0 && GoalRegs[m]) != 0) {
            MCOperand StepOperand = II->getOperand(2);
            MCOperand IterOperand = II->getOperand(1);
            // outs() << "test: " << *II << "\n";
            if (!StepOperand.isImm() || !IterOperand.isReg() ||
                IterOperand.getReg() != LoopIterReg)
              return false;
            assert(StepOperand.isImm() && "First operand should be imm");
            assert(IterOperand.isReg() &&
                    IterOperand.getReg() == LoopIterReg &&
                    "Second operand should be LoopIterReg");
            InstStep = StepOperand.getImm();
            UdpInst = *II;
            if (BC.MIB->isCompare(*(II + 1)) || BC.MIB->isBranch(*(II + 1))) {
              NoNeedDispatch = true;
            } else {
              UpdatePos = BB->eraseInstruction(II);
              II--;
            }
            FoundInst = true;
            break;
          }
        }
      }
    } else {
      // We have found update instruction, and we update all instruction
      // related to LoopIterReg
      unsigned BaseRegNum;
      int64_t ScaleValue;
      unsigned IndexRegNum;
      int64_t DispValue;
      unsigned SegRegNum;
      const MCExpr *DispExpr = nullptr;
      if (BC.MIB->evaluateX86MemoryOperand(*II, &BaseRegNum, &ScaleValue,
                                            &IndexRegNum, &DispValue,
                                            &SegRegNum, &DispExpr)) {
        if (BaseRegNum == LoopIterReg) {
          int64_t temp = InstStep;
          BC.MIB->addToImm(*II, temp, BC.Ctx.get());
        }
      }
    }
  }
  return FoundInst;
}

bool BinaryLoop::correlationAnalysis(std::vector<MCInst> &Instructions,
                                     unsigned LoopIterReg,
                                     int64_t LoopUnrollFactor,
                                     int64_t LoopUnrollStep,
                                     int64_t LoopUnrollStart) {
                                      
  bool decrease = false;
  // After we get all info about the loop, we can start to fold it.
  assert(getBlocks().size() == 1 && "Loop should have only one block");
  BinaryBasicBlock *BB = getBlocks()[0];
  const BinaryContext &BC = BB->getFunction()->getBinaryContext();
  
  assert(BC.MIB->isBranch(*(BB->end() - 1)) &&
         "Last instruction should be branch.");
  int labelIndex = 1;
  for (auto II = BB->rbegin(); II != BB->rend(); ++II, ++labelIndex) {
    if (BC.MIB->isBranch(*II) && BC.MIB->getTargetSymbol(*II) == BB->getLabel())
      break;
  }

  bool hasCmpInst = true;
  int MemoryScale = 1;
  int cmpInstIndex = 0;
  if (BC.MIB->isCompare(*(BB->end() - labelIndex - 1))) {
    // add, cmp, br
    cmpInstIndex = 3;
  } else {
    // add, br
    cmpInstIndex = 2;
  }

  // tge_log("labelIndex:" << labelIndex << " cmpInstIndex:" << cmpInstIndex
  //                       << "\n",
  //         RESET);

  std::vector<int> DispValues;
  for (int i = 0; i < LoopUnrollFactor; i++) {
    DispValues.push_back(LoopUnrollStart + i * LoopUnrollStep);
  }

  // A vector to record an instruction whether it should be in the folded loop.
  std::vector<int> InstructionStatus(BB->size(), 0);
  std::vector<bool> InstructionStatusBlock(BB->size(), false);

  // {def_inst_vector, written_regs}
  std::vector<std::pair<std::vector<int>, BitVector>> InstructionUseChain(
      BB->size(), {std::vector<int>(), BitVector(BC.MRI->getNumRegs(), false)});

  // Update induction variable.
  BinaryBasicBlock::iterator UpdateInstIter;
  UpdateInstIter = BB->end() - labelIndex - cmpInstIndex + 1;
  if (!BC.MIB->isADDri(*UpdateInstIter) ||
      !UpdateInstIter->getOperand(2).isImm()) {
    tge_log("UpdateInstIter is not ADDri\n", RESET);
    return false;
  }
  if (UpdateInstIter->getOperand(1).getReg() != LoopIterReg) {
    UpdateInstIter--;
  }
  if ((UpdateInstIter->getNumOperands()==3 && UpdateInstIter->getOperand(2).getImm() < 0 &&
       BC.MIB->isADDri(*UpdateInstIter)) ||
      (UpdateInstIter->getNumOperands()==3 && UpdateInstIter->getOperand(2).getImm() > 0 &&
       BC.MIB->isSUBri(*UpdateInstIter))) {
    decrease = true;
    LoopUnrollStep = -LoopUnrollStep;
    LoopUnrollStart = LoopUnrollStart - LoopUnrollStep * (LoopUnrollFactor - 1);
    outs() << "Set LoopUnrollStart to " << LoopUnrollStart << "\n";
  }

  outs() << "UpdateInstIter:";
  BC.printInstruction(outs(), *UpdateInstIter);

  struct LoopUnrollInfo {
    MemoryOperand MemOperand;
    int64_t Start;
    int64_t Step;
    int64_t Factor;
    std::vector<int> DispValues;
  };
  std::vector<MemoryOperand> FoundMemoryOperands;
  std::vector<LoopUnrollInfo> LoopUnrollInfos;

  int64_t Start;
  int64_t Step;
  int64_t Factor;
  MemoryOperand MemOp;
  while (getLoopUnrollFactor(LoopIterReg, Factor, Step, Start, MemOp,
                             &FoundMemoryOperands)) {
    FoundMemoryOperands.push_back(MemOp);
    std::vector<int> DispValues;
    if (Factor != LoopUnrollFactor)
      continue;
    if (decrease) {
      Step = -Step;
      Start = Start - Step * (Factor - 1);
    }
    for (int i = 0; i < Factor; i++) {
      DispValues.push_back(Start + i * Step);
    }
    LoopUnrollInfos.push_back({MemOp, Start, Step, Factor, DispValues});
  }

  for (LoopUnrollInfo loopUnrollInfo : LoopUnrollInfos) {
    outs() << "LoopUnrollInfo: start " << loopUnrollInfo.Start << ", step "
           << loopUnrollInfo.Step << ", factor " << loopUnrollInfo.Factor
           << "\n";
    for (int i = 0; i < loopUnrollInfo.DispValues.size(); i++) {
      outs() << loopUnrollInfo.DispValues[i] << " ";
    }
    outs() << "\n";
  }

  for (int i = BB->end() - UpdateInstIter; i < BB->size(); i++) {
    // keep it.
    InstructionStatus[i] = 0;
  }
  // When the srcReg of instruction is defined by a previous instruction,
  // update the previous/latter instruction's status as latter/previous
  // instruction's. Only update the instruction which status is 0.
  auto updateInstructionStatus = [&](BinaryBasicBlock::iterator instIter) {
    std::function<void(BinaryBasicBlock::iterator)> f;
    f = [&](BinaryBasicBlock::iterator instIter) {
      // Back propagation
      int status = InstructionStatus[instIter - BB->begin()];
      std::queue<int> depend_inst;
      for (auto inst : InstructionUseChain[instIter - BB->begin()].first) {
        depend_inst.push(inst);
      }
      while (!depend_inst.empty()) {
        int inst = depend_inst.front();
        depend_inst.pop();
        if (InstructionStatus[inst] == 0) {
          // Update when depend_inst's status is 0.
          outs() << "Back Update InstructionStatus[" << inst << "]=" << status
                 << " (The original status is 0)\n";
          InstructionStatus[inst] = status;
          for (auto inst : InstructionUseChain[inst].first) {
            depend_inst.push(inst);
          }
        } else if (InstructionStatus[inst] < status &&
                   InstructionStatusBlock[inst] == false) {
          // Update when depend_inst's status is less then status.
          bool updateStatus = true;
          for (int isIndex = inst + 1; isIndex < instIter - BB->begin();
               isIndex++) {
            // Be used in other group, not update.
            if (InstructionStatus[isIndex] != status &&
                std::find(InstructionUseChain[isIndex].first.begin(),
                          InstructionUseChain[isIndex].first.end(),
                          inst) != InstructionUseChain[isIndex].first.end()) {
              updateStatus = false;
              break;
            }
          }
          if (updateStatus) {
            // if(InstructionStatusBlock[inst] == true) {
            //   outs() << "Block Update InstructionStatus[" << inst
            //          << "]" << "\n";
            //   continue;
            // }
            outs() << "Back Update InstructionStatus[" << inst << "]=" << status
                   << " (The original status is " << InstructionStatus[inst]
                   << ")\n";
            InstructionStatus[inst] = status;
            f(inst + BB->begin());
          }
        }
      }
      // Forward propagation
      // for (auto fp = BB->begin() + 1; fp < instIter; fp++) {
      //   if (InstructionStatus[fp - BB->begin()] == 0) {
      //     for (auto inst : InstructionUseChain[fp - BB->begin()].first) {
      //       if (InstructionStatus[inst] != 0) {
      //         outs() << "Forward Update InstructionStatus[" << fp - BB->begin()
      //                << "]=" << InstructionStatus[inst]
      //                << " (The forward original status is "
      //                << InstructionStatus[fp - BB->begin()] << ")\n";
      //         InstructionStatus[fp - BB->begin()] = InstructionStatus[inst];
      //         break;
      //       }
      //     }
      //   }
      // }
    };
    return f(instIter);
  };

  // Check whether two BitVectors have common registers.
  auto checkRegs = [&](BitVector a, BitVector b) {
    for (unsigned m = 0; m != BC.MRI->getNumRegs(); ++m) {
      if (a[m] == 1 && b[m] == 1) {
        return true;
      }
    }
    return false;
  };

  auto removeCommonRegs = [&](BitVector a, BitVector &removedRegs) {
    for (unsigned m = 0; m != BC.MRI->getNumRegs(); ++m) {
      if (a[m] == 1 && removedRegs[m] == 1) {
        removedRegs[m]=0;
      }
    }
  };

  for (auto II = BB->begin(); II != BB->end() - labelIndex + 1; ++II) {
    // Ignore the last two instructions in the last BB
    if (II >= UpdateInstIter)
      break;

    // For each instruction, record its WrittenRegs in InstructionUseChain, and
    // try to find the defining instruction of SrcRegs, otherwise -1.
    BitVector WrittenRegs(BC.MRI->getNumRegs(), false);
    BitVector SrcRegs(BC.MRI->getNumRegs(), false);
    BC.MIB->getWrittenRegs(*II, WrittenRegs);
    BC.MIB->getSrcRegs(*II, SrcRegs);
    InstructionUseChain[II - BB->begin()].second |= WrittenRegs;
    bool skipMemCheck = false;
    if (II != BB->begin()) {
      outs() << "Check Instruction[" << II - BB->begin() << "]: ";
      outs() << "Src regs: ";
      BC.printRegistersName(outs(), SrcRegs);
      int status = 0;
      bool block= false;
      for (auto III = II - 1; III >= BB->begin(); --III) {
        // TODO(teg): Early stop.
        if (checkRegs(InstructionUseChain[III - BB->begin()].second, SrcRegs)) {
          outs() <<"["<< III - BB->begin() << "] Prev def regs: ";
          BC.printRegistersName(outs(),
                                InstructionUseChain[III - BB->begin()].second);      
          removeCommonRegs(InstructionUseChain[III - BB->begin()].second,
                           SrcRegs);
          //  II rely on III.
          InstructionUseChain[II - BB->begin()].first.emplace_back(III -
                                                                   BB->begin());
          if (InstructionStatus[II - BB->begin()] == 0 &&
              InstructionStatus[III - BB->begin()] > 0) {
            if (status < InstructionStatus[III - BB->begin()]) {
              status = InstructionStatus[III - BB->begin()];
              block = InstructionStatusBlock[III - BB->begin()];
            }
          }
        }
      }
      // xorps
      if ((II->getOpcode() == 17707) &&
          II->getOperand(0).getReg() == II->getOperand(1).getReg())
        continue;
      if (InstructionStatus[II - BB->begin()] == 0 && status > 0) {
        outs() << "Set InstructionStatus[" << II - BB->begin() << "] to "
               << status << "\n";
        InstructionStatus[II - BB->begin()] = status;
        InstructionStatusBlock[II - BB->begin()] = block;
        updateInstructionStatus(II);
        // skipMemCheck = true;
      }
    }
    // if (skipMemCheck)
    //   continue;

    // Repair SrcRegs
    BC.MIB->getSrcRegs(*II, SrcRegs);

    // Check if memory address is related to LoopIterReg.
    MemoryOperand MemOperand;
    int64_t DispExprValue = 0;
    if (BC.MIB->evaluateX86MemoryOperand(
            *II, &MemOperand.BaseRegNum, &MemOperand.ScaleValue,
            &MemOperand.IndexRegNum, &MemOperand.DispValue,
            &MemOperand.SegRegNum, &MemOperand.DispExpr)) {
      // printX86MemoryOperand(MemOperand);

      if (MemOperand.DispExpr != NULL) {
        if (MemOperand.DispExpr->getKind() == MCExpr::Binary) {
          auto *BE = static_cast<const MCBinaryExpr *>(MemOperand.DispExpr);
          if (isa<MCConstantExpr>(BE->getRHS())) {
            DispExprValue =
                static_cast<const MCConstantExpr *>(BE->getRHS())->getValue();
          }
        }
      }

      BitVector LoopIterRegs(BC.MRI->getNumRegs(), false);
      LoopIterRegs |= BC.MIB->getAliases(LoopIterReg, false);
      for (unsigned i = 0; i != BC.MRI->getNumRegs(); ++i) {
        if (LoopIterRegs[i] != 0 &&
            (MemOperand.BaseRegNum == i || MemOperand.IndexRegNum == i)) {
          outs() << "Memory address :";
          BC.printInstruction(outs(), *II);
          // DispValue should be in DispValues vector
          LoopUnrollInfo *Info = nullptr;
          for (int j = 0; j < LoopUnrollInfos.size(); j++) {
            if (compareMemExceptDisp(MemOperand,
                                     LoopUnrollInfos[j].MemOperand) &&
                II->getOpcode() == LoopUnrollInfos[j].MemOperand.UnrollOpcode) {
              Info = &LoopUnrollInfos[j];
              break;
            }
          }
          if (Info == nullptr) {
            outs() << "No LoopUnrollInfo\n";
            break;
          }

          if (std::find(Info->DispValues.begin(), Info->DispValues.end(),
                        MemOperand.DispValue + DispExprValue) ==
              Info->DispValues.end()) {
            outs() << "Can't find the disp value in the loop unroll info.("
                   << MemOperand.DispValue + DispExprValue << ")\n";
            break;
          }
          int memOffset = MemOperand.DispValue + DispExprValue - Info->Start;
          if (memOffset % std::abs(Info->Step) != 0) {
            outs() << "memOffset \% GroupStep != 0 \n";
            return false;
          }
          // Group number starts from 1.
          int group = std::abs(memOffset / Info->Step) + 1;
          if (group <= Info->Factor) {
            if (MemoryScale == 1 && MemOperand.ScaleValue != 1) {
              tge_log("MemoryScale is set to " << MemOperand.ScaleValue, RESET);
              MemoryScale = MemOperand.ScaleValue;
            }
            InstructionStatus[II - BB->begin()] = group;
            InstructionStatusBlock[II - BB->begin()] = true;;
            outs() << "memOffset:" << memOffset
                   << " LoopUnrollStep:" << Info->Step << "\n";
            outs() << "Set InstructionStatus[" << II - BB->begin() << "] to "
                   << group << "\n";
            updateInstructionStatus(II);
          }
          break;
        }
      }
    }
  }

  bool checkUpdateStep = true;
  // Select expected instructions in folded loop.
  std::vector<MCInst> backupInstructions;
  for (auto Inst : *BB)
    backupInstructions.push_back(Inst);
  auto selectInstructions = [&](std::vector<MCInst> &instructions,
                                int &instructionStatusBorder,
                                int &groupScale) {
    bool success = true;
    auto revertSelectInstructions = [&]() {
      outs() << "Revert BB\n";
      BB->clear();
      BB->addInstructions(backupInstructions);
    };

    for (long unsigned int i = 0; i < InstructionStatus.size(); i++) {
      if (InstructionStatus[i] <= instructionStatusBorder) {
        auto II = BB->begin() + i;
        if (II->getOpcode() == 389 &&
            II->getNumOperands() == 1) { // X86::ADD64i32
          MCOperand m = II->getOperand(0);
          II->clear();
          II->setOpcode(393);                       // X86::ADD64ri32
          II->addOperand(MCOperand::createReg(49)); // X86::RAX
          II->addOperand(MCOperand::createReg(49)); // X86::RAX
          II->addOperand(m);
        }
        if (InstructionStatus[i] == 0 &&
            (BC.MIB->isADDri(*II) || BC.MIB->isSUBri(*II)) && II->getNumOperands() == 3 &&
            II->getOperand(2).isImm()) {
          // Check update register.
          // Update step should match LoopUnrollFactor.
          unsigned LoopIterRegCheck = II->getOperand(1).getReg();
          int64_t LoopUnrollFactorCheck, LoopUnrollStepCheck,
              LoopUnrollStartCheck;
          MemoryOperand MemOp;
          if (getLoopUnrollFactor(LoopIterRegCheck, LoopUnrollFactorCheck,
                                  LoopUnrollStepCheck, LoopUnrollStartCheck,
                                  MemOp, nullptr)) {
            outs() << " Check update register: ";
            BC.printRegisterName(outs(), LoopIterRegCheck);
            outs() << " LoopUnrollFactorCheck: " << LoopUnrollFactorCheck
                   << " LoopUnrollStepCheck: " << LoopUnrollStepCheck
                   << " LoopUnrollStartCheck: " << LoopUnrollStartCheck << "\n";
            //  Check update step.
            //  Consider the scale case.
            if (instructionStatusBorder == 0 &&
                std::abs(LoopUnrollStepCheck * (LoopUnrollFactorCheck + 1)) ==
                    std::abs(II->getOperand(2).getImm() * MemOp.ScaleValue)) {
              LoopUnrollFactorCheck += 1;
              LoopUnrollFactor += 1;
            }
            if (std::abs(LoopUnrollStepCheck * LoopUnrollFactorCheck) !=
                std::abs(II->getOperand(2).getImm() * MemOp.ScaleValue)) {
              if (std::abs(LoopUnrollStepCheck * (LoopUnrollFactorCheck + 1)) ==
                  std::abs(II->getOperand(2).getImm() * MemOp.ScaleValue)) {
                outs() << "skip instructionStatusBorder\n";
                // instructionStatusBorder = 0;
              } else {
                outs() << "Update step is not correct."
                       << II->getOperand(2).getImm() << "*" << MemOp.ScaleValue
                       << "\n";
              }
              success = false;
              break;
            }
          }
          // Update step.
          if (II->getOperand(2).getImm() % (LoopUnrollFactor / groupScale) !=
              0) {
            outs() << "Update step can't be divided.\n";
            if (LoopUnrollFactor % II->getOperand(2).getImm() == 0 &&
                LoopUnrollFactor / II->getOperand(2).getImm() > 0) {
              groupScale = (LoopUnrollFactor / II->getOperand(2).getImm());
            }
            success = false;
            break;
          }
          II->getOperand(2).setImm(II->getOperand(2).getImm() /
                                   (LoopUnrollFactor / groupScale));
        }
        instructions.emplace_back(*II);
      }
    }
    if (!success) {
      revertSelectInstructions();
      instructions.clear();
    }
    return success;
  };

  int GroupScale = 1, InstructionStatusBorder = 1;
  checkUpdateStep =
      selectInstructions(Instructions, InstructionStatusBorder, GroupScale);
  if (!checkUpdateStep && (GroupScale > 1 || InstructionStatusBorder != 1)) {
    outs() << "Scale factor " << GroupScale << "\n";
    outs() << "InstructionStatusBorder " << InstructionStatusBorder << "\n";
    for (int i = 0; i < InstructionStatus.size(); i++) {
      InstructionStatus[i] =
          (InstructionStatus[i] + GroupScale - 1) / GroupScale;
    }
    checkUpdateStep =
        selectInstructions(Instructions, InstructionStatusBorder, GroupScale);
  }

  for (int i = 0; i < InstructionUseChain.size(); i++) {
    outs() << "InstructionUseChain[" << i << "]: ";
    for (auto j : InstructionUseChain[i].first) {
      outs() << j << " ";
    }
    outs() << "\n";
  }
  for (int i = 0; i < InstructionStatusBlock.size(); i++) {
    outs() << "InstructionStatusBlock[" << i
           << "]: " << InstructionStatusBlock[i] << "\n";
  }
  outs() << "\n";

  outs() << "\n";
  for (int i = 0; i < InstructionStatus.size(); i++) {
    outs() << "InstructionStatus[" << i << "]: " << InstructionStatus[i]
           << "\n";
  }
  outs() << "\n";

  if (!checkUpdateStep) {
    tge_log("Update step failed.", RESET);
    return false;
  }
  // Check groups:
  // Each group has the same number of instructions is not enough!
  bool checkGroupResult = true;
  std::map<int, int> group_map;
  for (long unsigned int i = 0; i < InstructionStatus.size(); i++) {
    int group = InstructionStatus[i];
    if (group_map.count(group) == 0)
      group_map.insert({group, 1});
    else
      group_map[group]++;
  }
  int inst_num_check = -1;
  int group_NO = 0;
  int inst_num;
  for (auto iter = group_map.begin(); iter != group_map.end(); iter++) {
    int group_id = iter->first;
    inst_num = iter->second;
    // outs() << "Group " << group_id << ": " << inst_num << "\n";
    if (group_id == 0) {
      group_NO++;
      continue;
    }
    if (group_NO != group_id) {
      checkGroupResult = false;
      // break;
    }
    group_NO++;
    if (inst_num_check < 0)
      inst_num_check = inst_num;
    else if (inst_num_check != inst_num) {
      checkGroupResult = false;
      // break;
    }
  }
  

  if (group_NO == 3 || group_NO == 5) {
    int last_group_1 = -1, last_group_2 = -1;
    for (int i = InstructionStatus.size() - 1; i >= 0; i--) {
      if (last_group_1 != -1 && last_group_2 != -1)
        break;
      if (last_group_1 == -1 && InstructionStatus[i] == 1) {
        last_group_1 = i;
      }
      if (last_group_2 == -1 && InstructionStatus[i] == 2) {
        last_group_2 = i;
      }
    }

    outs() << "Exchange reg. 0 \n";
    outs() << "last_group_1: " << last_group_1
           << " last_group_2: " << last_group_2 << "\n";
    auto it1 =
        std::find(InstructionUseChain[last_group_2].first.begin(),
                  InstructionUseChain[last_group_2].first.end(), last_group_1);
    /// padd addsd paddq
    std::vector<int> opc = {2087, 2089, 394, 416};
    auto it2 = std::find(opc.begin(), opc.end(),
                         backupInstructions[last_group_1].getOpcode());
    auto it3 = std::find(opc.begin(), opc.end(),
                         backupInstructions[last_group_2].getOpcode());
    if (it1 != DispValues.end() && it2 != opc.end() && it3 != opc.end() &&
        it2 == it3) {
      int reg0 = backupInstructions[last_group_1].getOperand(0).getReg();
      int reg1 = backupInstructions[last_group_1].getOperand(2).getReg();
      outs() << "Exchange reg. 1 \n";
      outs() << reg0 << ", " << reg1 << "\n";
      outs() << backupInstructions[last_group_2].getOperand(0).getReg() << ", "
             << backupInstructions[last_group_2].getOperand(2).getReg() << "\n";
      if (reg0 == backupInstructions[last_group_2].getOperand(2).getReg() &&
          reg1 == backupInstructions[last_group_2].getOperand(0).getReg()) {
        return false;
        Instructions[inst_num - 1].getOperand(0).setReg(reg1);
        Instructions[inst_num - 1].getOperand(1).setReg(reg1);
        Instructions[inst_num - 1].getOperand(2).setReg(reg0);
        outs() << "Exchange reg.\n";
      }
    }
  }

  if (!checkGroupResult) {
    tge_log("Group check failed.", RESET);
    return false;
  }

  // Check depandency:
  // Instruction and its dependency should be in the same group.
  bool checkDependencyResult = true;
  for (long unsigned int i = 0; i < InstructionUseChain.size(); i++) {
    auto dependency = InstructionUseChain[i].first;
    for (auto j : dependency) {
      if (InstructionStatus[i] != InstructionStatus[j]) {
        checkDependencyResult = false;
        break;
      }
    }
  }
  for (int i = 1; i < group_map.size(); i++) {
    std::vector<int> check_group;
    for (int j = 0; j < InstructionStatus.size(); j++) {
      if (InstructionStatus[j] == i)
        check_group.emplace_back(j);
    }
    std::vector<int> check_group_delete(check_group.size(), 0);
    std::vector<int> depend;
    depend.push_back(check_group[0]);

    check_group_delete[0] = 1;
    auto have_dependency = [&](int a, int b) {
      auto it1 = std::find(InstructionUseChain[a].first.begin(),
                           InstructionUseChain[a].first.end(), b);
      auto it2 = std::find(InstructionUseChain[b].first.begin(),
                           InstructionUseChain[b].first.end(), a);
      if (it1 != InstructionUseChain[a].first.end() ||
          it2 != InstructionUseChain[b].first.end())
        return true;
      else
        return false;
    };
    auto all_delete = [&]() {
      for (int i = 0; i < check_group_delete.size(); i++) {
        if (check_group_delete[i] == 0)
          return false;
      }
      return true;
    };

    while (!all_delete()) {
      bool modified = false;
      for (int j = 0; j < check_group.size(); j++) {
        if (check_group_delete[j] == 1)
          continue;
        for (auto k : depend) {
          if (have_dependency(check_group[j], k)) {
            depend.push_back(check_group[j]);
            check_group_delete[j] = 1;
            modified = true;
            break;
          }
        }
      }
      if (!modified)
        break;
    }

    if (!all_delete()) {
      // check mov
      checkDependencyResult = false;
      outs() << "all_deleted_but_one.\n";
    }

    if (opts::RemoveSubDDG && !checkDependencyResult) {
      tge_log("Dependency check failed.", RESET);
      return false;
    }
  }

  if (!checkDependencyResult) {
    tge_log("Dependency check failed.", RESET);
    return false;
  }


  // check continuous for compare
  bool checkContinuous = true;
  for (int i = 0; i < InstructionStatus.size(); i++) {
    if (InstructionStatus[i] != 0 &&
        InstructionStatus[i] != i / inst_num_check + 1) {
      outs() << "Group not continuous: InstructionStatus[" << i << "]\n";
      checkContinuous = false;
      break;
    }
  }

  if (checkContinuous) {
    for (int i = 0; i < inst_num_check; i++) {
      for (int j = 1; j < group_map.size() - 1; j++) {
        if ((BB->begin() + i)->getOpcode() !=
            (BB->begin() + i + j * inst_num_check)->getOpcode()) {
          outs() << "Group not continuous: InstructionStatus[" << i << "]\n";
          checkContinuous = false;
          break;
        }
      }
      if (!checkContinuous)
        break;
    }
  }

  outs() << " check depandency and continuous " << checkDependencyResult << " "
         << checkContinuous << "\n";

  if (opts::RemoveSuffixTree && !checkContinuous) {
    tge_log("Group not continuous.", RESET);
    return false;
  }

  // Check Group 0:
  // Theres should only be add, cmp, jmp instructions in group 0.
  bool checkGroupZero = true;
  for (long unsigned int i = 0; i < InstructionStatus.size(); i++) {
    auto II = BB->begin() + i;
    if (InstructionStatus[i] == 0) {
      if (!BC.MIB->isCompare(*II) && !BC.MIB->isBranch(*II) &&
          !BC.MIB->isADDri(*II)) {
        outs() << "Unexpected instruction in Group 0: \n";
        BC.printInstruction(outs(), *II);
        checkGroupZero = false;
        break;
      }
    }
  }
  // If InstructionStatusBorder is 0, it means that some instructions in group
  // one are placed in group zero. So skip check GroupZero.
  outs() << "InstructionStatusBorder " << InstructionStatusBorder << "\n";
  if (!checkGroupZero && InstructionStatusBorder != 0) {
    tge_log("Group zero check failed.", RESET);
    return false;
  }

  auto checkInstruction = [](llvm::bolt::BinaryBasicBlock::iterator II, llvm::bolt::BinaryBasicBlock::iterator checkII){
    if(II->getOpcode() != checkII->getOpcode()) return false;
    if(II->getNumOperands() != checkII->getNumOperands()) return false;
    for(int i=0;i<II->getNumOperands();++i){
      if(II->getOperand(i).isReg() && II->getOperand(i).getReg() != checkII->getOperand(i).getReg()) return false;
    }
    return true;
  };

  // zty_log("----------------------------------begin------------------------------------------\n");
  // for(auto II = BB->begin();II!=BB->end();++II) BC.printInstruction(outs(), *II);
  // zty_log("-------------------after-------------------\n")
  // for(int i = 0;i<Instructions.size();++i) BC.printInstruction(outs(), Instructions[i]);
  // // we need to check every instruction in each group.
  // // group check here is too strict, we need to check it by map other than by vector.
  // bool checkGroupFlag = true;
  // zty_log("IGroups["<<0<<"]");
  // for(int i=0;i<inst_num_check;++i){
  //   BC.printInstruction(outs(), *(BB->begin() + i));
  // }
  // for(int j=1;checkGroupFlag&&j<group_map.size() - 1;++j){
  //   zty_log("IGroups["<<j<<"]");
  //   for(int i=0;checkGroupFlag&&i<inst_num_check;++i){
  //     BC.printInstruction(outs(), *(BB->begin() + i + j * inst_num_check));
  //     // to-do: check the instruction in group j.
  //     bool checkFlag = false;
  //     for(int k=0;k<inst_num_check;++k){
  //       if(checkInstruction(BB->begin() + k, BB->begin() + i + j * inst_num_check)){
  //         checkFlag = true;
  //         break;
  //       }
  //     }
  //     if(!checkFlag){
  //       checkGroupFlag = false;
  //       break;
  //     }
  //   }
    
  // }
  // zty_log("---------------------------------end-------------------------------------------\n");
  // if(!checkGroupFlag) {
  //   zty_log("Group check failed.");
  //   return false;
  // }

  return true;
}

void BinaryLoop::printX86MemoryOperand(const MemoryOperand &MemOp) {
  BinaryBasicBlock *BB = getBlocks()[0];
  const BinaryContext &BC = BB->getFunction()->getBinaryContext();
  outs() << "BaseRegNum: ";
  if (MemOp.BaseRegNum > 0)
    BC.printRegisterName(outs(), MemOp.BaseRegNum);
  else
    outs() << MemOp.BaseRegNum << "\n";
  outs() << "IndexRegNum: ";
  if (MemOp.IndexRegNum > 0)
    BC.printRegisterName(outs(), MemOp.IndexRegNum);
  else
    outs() << MemOp.IndexRegNum << "\n";
  outs() << "SegRegNum: ";
  if (MemOp.SegRegNum > 0)
    BC.printRegisterName(outs(), MemOp.SegRegNum);
  else
    outs() << MemOp.SegRegNum << "\n";
  outs() << "ScaleValue: " << MemOp.ScaleValue << "\n";
  outs() << "DispValue: " << MemOp.DispValue << "\n";
  int64_t DispExprValue = 0;
  if (MemOp.DispExpr != NULL) {
    outs() << "DispExpr: " << *MemOp.DispExpr << "\n";
    if (MemOp.DispExpr->getKind() == MCExpr::Binary) {
      auto *BE = static_cast<const MCBinaryExpr *>(MemOp.DispExpr);
      if (isa<MCConstantExpr>(BE->getRHS())) {
        DispExprValue =
            static_cast<const MCConstantExpr *>(BE->getRHS())->getValue();
        outs() << "(" << DispExprValue << ")\n";
      }
    }
  }
}

} // namespace bolt
} // namespace llvm