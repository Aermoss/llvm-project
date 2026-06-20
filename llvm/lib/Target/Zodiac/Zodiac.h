//=====-- Zodiac.h - Top-level interface for Zodiac ----------*- C++ -*--===//
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

#include "llvm/Target/TargetMachine.h"

#define GET_SDNODE_ENUM
#include "ZodiacGenSDNodeInfo.inc"

namespace llvm {
class ZodiacTargetMachine;
class FunctionPass;
class PassRegistry;

FunctionPass *createZodiacISelDag(ZodiacTargetMachine &TM);

void initializeZodiacAsmPrinterPass(PassRegistry &);
void initializeZodiacDAGToDAGISelLegacyPass(PassRegistry &);

} // namespace llvm

#endif // LLVM_LIB_TARGET_ZODIAC_ZODIAC_H
