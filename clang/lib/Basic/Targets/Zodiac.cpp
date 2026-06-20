//===--- Zodiac.cpp - Implement Zodiac target feature support -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements Zodiac TargetInfo objects.
//
//===----------------------------------------------------------------------===//

#include "Zodiac.h"
#include "clang/Basic/MacroBuilder.h"

using namespace clang;
using namespace clang::targets;

const char *const ZodiacTargetInfo::GCCRegNames[] = {
    "x0",  "x1",  "x2",  "x3",  "x4",  "x5",  "x6",  "x7",
    "x8",  "x9",  "x10", "x11", "x12", "x13", "x14", "x15",
    "x16", "x17", "x18", "x19", "x20", "x21", "x22", "x23",
    "x24", "x25", "x26", "x27", "x28", "x29", "x30", "x31"
};

ArrayRef<const char *> ZodiacTargetInfo::getGCCRegNames() const {
  return llvm::ArrayRef(GCCRegNames);
}

void ZodiacTargetInfo::getTargetDefines(const LangOptions &Opts,
                                        MacroBuilder &Builder) const {
  Builder.defineMacro("zodiac");
  Builder.defineMacro("__zodiac__");
}
