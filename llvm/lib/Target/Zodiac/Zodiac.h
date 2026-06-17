//===-- Zodiac.h - Top-level interface for Zodiac representation --*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the entry points for global functions defined in the LLVM
// Zodiac back-end.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ZODIAC_ZODIAC_H
#define LLVM_LIB_TARGET_ZODIAC_ZODIAC_H

#include "llvm/Pass.h"

namespace llvm {
class FunctionPass;
class ZodiacTargetMachine;
class PassRegistry;

// createZodiacISelDag - This pass converts a legalized DAG into a
// Zodiac-specific DAG, ready for instruction scheduling.
FunctionPass *createZodiacISelDag(ZodiacTargetMachine &TM);

// createZodiacDelaySlotFillerPass - This pass fills delay slots
// with useful instructions or nop's
FunctionPass *createZodiacDelaySlotFillerPass(const ZodiacTargetMachine &TM);

// createZodiacMemAluCombinerPass - This pass combines loads/stores and
// arithmetic operations.
FunctionPass *createZodiacMemAluCombinerPass();

// createZodiacSetflagAluCombinerPass - This pass combines SET_FLAG and ALU
// operations.
FunctionPass *createZodiacSetflagAluCombinerPass();

void initializeZodiacAsmPrinterPass(PassRegistry &);
void initializeZodiacDAGToDAGISelLegacyPass(PassRegistry &);
void initializeZodiacMemAluCombinerPass(PassRegistry &);

} // namespace llvm

#endif // LLVM_LIB_TARGET_ZODIAC_ZODIAC_H
