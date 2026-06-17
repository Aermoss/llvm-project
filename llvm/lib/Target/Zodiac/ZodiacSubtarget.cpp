//===- ZodiacSubtarget.cpp - Zodiac Subtarget Information -----------*- C++ -*-=//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the Zodiac specific subclass of TargetSubtarget.
//
//===----------------------------------------------------------------------===//

#include "ZodiacSubtarget.h"

#define DEBUG_TYPE "zodiac-subtarget"

#define GET_SUBTARGETINFO_TARGET_DESC
#define GET_SUBTARGETINFO_CTOR
#include "ZodiacGenSubtargetInfo.inc"

using namespace llvm;

void ZodiacSubtarget::initSubtargetFeatures(StringRef CPU, StringRef FS) {
  std::string CPUName = std::string(CPU);
  if (CPUName.empty())
    CPUName = "generic";

  ParseSubtargetFeatures(CPUName, /*TuneCPU*/ CPUName, FS);
}

ZodiacSubtarget &ZodiacSubtarget::initializeSubtargetDependencies(StringRef CPU,
                                                                StringRef FS) {
  initSubtargetFeatures(CPU, FS);
  return *this;
}

ZodiacSubtarget::ZodiacSubtarget(const Triple &TargetTriple, StringRef Cpu,
                               StringRef FeatureString, const TargetMachine &TM,
                               const TargetOptions & /*Options*/,
                               CodeModel::Model /*CodeModel*/,
                               CodeGenOptLevel /*OptLevel*/)
    : ZodiacGenSubtargetInfo(TargetTriple, Cpu, /*TuneCPU*/ Cpu, FeatureString),
      InstrInfo(initializeSubtargetDependencies(Cpu, FeatureString)),
      FrameLowering(*this), TLInfo(TM, *this) {}
