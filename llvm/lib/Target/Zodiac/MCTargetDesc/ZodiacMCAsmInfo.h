//=====-- ZodiacMCAsmInfo.h - Zodiac asm properties -----------*- C++ -*--====//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the declaration of the ZodiacMCAsmInfo class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ZODIAC_MCTARGETDESC_ZODIACMCASMINFO_H
#define LLVM_LIB_TARGET_ZODIAC_MCTARGETDESC_ZODIACMCASMINFO_H

#include "llvm/MC/MCAsmInfo.h"

namespace llvm {
class Triple;

class ZodiacMCAsmInfo : public MCAsmInfo {
  void anchor();

public:
  explicit ZodiacMCAsmInfo(const Triple &TheTriple,
                          const MCTargetOptions &Options);
  void printSpecifierExpr(raw_ostream &OS,
                          const MCSpecifierExpr &Expr) const override;
};

namespace Zodiac {
using Specifier = uint8_t;
enum { S_None, S_ABS_HI, S_ABS_LO };
} // namespace Zodiac

} // namespace llvm

#endif // LLVM_LIB_TARGET_ZODIAC_MCTARGETDESC_ZODIACMCASMINFO_H
