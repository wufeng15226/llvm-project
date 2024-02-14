//===- bolt/Core/BinaryLoop.h - Loop info at low-level IR -------*- C++ -*-===//
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

#ifndef BOLT_CORE_BINARY_LOOP_H
#define BOLT_CORE_BINARY_LOOP_H

#include "llvm/Support/GenericLoopInfoImpl.h"

namespace llvm {
namespace bolt {

class BinaryBasicBlock;

class BinaryLoop : public LoopBase<BinaryBasicBlock, BinaryLoop> {
public:
  typedef struct MemoryOperand {
    unsigned BaseRegNum = 0;
    int64_t ScaleValue = 0;
    unsigned IndexRegNum = 0;
    int64_t DispValue = 0;
    unsigned SegRegNum = 0;
    const MCExpr *DispExpr = nullptr;
    unsigned UnrollOpcode = 0;
  } MemoryOperand;

  BinaryLoop() : LoopBase<BinaryBasicBlock, BinaryLoop>() {}

  static std::unordered_map<int64_t, int64_t> regMap;

  int64_t iterInstructionInd{-1};

  int64_t cmpInstructionInd{-1};

  // induction variable reg num
  int64_t inductionRegNum{-1};

  // iteration stride
  int64_t stride{0};

  // iteration start
  int64_t iterationBegin{0};

  bool iterationBeginValid{false};

  // iteration end
  int64_t iterationEnd{0};

  bool iterationEndValid{false};

  // The total count of all the back edges of this loop.
  uint64_t TotalBackEdgeCount{0};

  // The times the loop is entered from outside.
  uint64_t EntryCount{0};

  // The times the loop is exited.
  uint64_t ExitCount{0};

  // Most of the public interface is provided by LoopBase.

  // bool GetLoopIterReg(unsigned &LoopIterReg);
  bool GetLoopIterReg2(unsigned &LoopIterReg);
  bool getLoopUnrollFactor(
      const unsigned LoopIterReg, int64_t &LoopUnrollFactor,
      int64_t &LoopUnrollStep, int64_t &LoopUnrollStart, MemoryOperand &MemOp,
      std::vector<MemoryOperand> *ExcludeMemoryOperands = nullptr);

  // Move loop update inst to the end and update related address.
  bool dispatchLoopUpdateInst(const unsigned LoopIterReg,
                              BinaryBasicBlock::iterator &UpdatePos);

  // Take correlation analysis on unrolled loop and fold it.
  bool correlationAnalysis(std::vector<MCInst> &Instructions,
                           unsigned LoopIterReg, int64_t LoopUnrollFactor,
                           int64_t LoopUnrollStep, int64_t LoopUnrollStart);

  void loopUnroll();

  bool iterationAnalysis();

  bool checkInductionReg();

  bool checkCmpInstruction();

  uint64_t getUnrollCount();

  void setIterationBegin(int64_t num){
    iterationBegin = num;
    iterationBeginValid = true;
  }

  void setIterationEnd(int64_t num){
    iterationEnd = num;
    iterationEndValid = true;
  }

  bool isBoundValid(){
    return iterationBeginValid && iterationEndValid;
  }

protected:
  friend class LoopInfoBase<BinaryBasicBlock, BinaryLoop>;
  explicit BinaryLoop(BinaryBasicBlock *BB)
      : LoopBase<BinaryBasicBlock, BinaryLoop>(BB) {}
  void printX86MemoryOperand(const MemoryOperand &MemOp);

  bool compareMemExceptDisp(const MemoryOperand &MemOp1,
                            const MemoryOperand &MemOp2) {

    std::string s1, s2;
    auto getSymbolName = [](const MemoryOperand &MemOp) {
      std::string s;
      if (MemOp.DispExpr != nullptr) {
        if (MemOp.DispExpr->getKind() == MCExpr::Binary) {
          const MCBinaryExpr *BE =
              static_cast<const MCBinaryExpr *>(MemOp.DispExpr);
          const MCExpr * Expr = BE->getLHS();
          // const MCExpr * RHS = BE->getRHS();
          if (Expr->getKind() != MCExpr::SymbolRef) {
            const MCSymbolRefExpr *Ref =
                static_cast<const MCSymbolRefExpr *>(Expr);
            s = Ref->getSymbol().getName().str();
          }
        }
      }
      return s;
    };

    s1 = getSymbolName(MemOp1);
    s2 = getSymbolName(MemOp1);
    if ((!s1.empty() || !s2.empty()) && (s1 != s2))
      return false;
    if (MemOp1.BaseRegNum == MemOp2.BaseRegNum &&
        MemOp1.ScaleValue == MemOp2.ScaleValue &&
        MemOp1.IndexRegNum == MemOp2.IndexRegNum &&
        MemOp1.SegRegNum == MemOp2.SegRegNum)
      return true;
    return false;
  };
};

class BinaryLoopInfo : public LoopInfoBase<BinaryBasicBlock, BinaryLoop> {
public:
  BinaryLoopInfo() {}

  unsigned OuterLoops{0};
  unsigned TotalLoops{0};
  unsigned MaximumDepth{0};

  // Most of the public interface is provided by LoopInfoBase.
};

} // namespace bolt
} // namespace llvm

#endif
