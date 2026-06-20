//===-- ZodiacBaseInfo.h - Top level definitions for Zodiac MC ----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains helper functions and enum definitions for the Zodiac target.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ZODIAC_MCTARGETDESC_ZODIACBASEINFO_H
#define LLVM_LIB_TARGET_ZODIAC_MCTARGETDESC_ZODIACBASEINFO_H

#include "ZodiacMCTargetDesc.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/Support/DataTypes.h"
#include "llvm/Support/ErrorHandling.h"

namespace llvm {

namespace ZodiacII {
enum TOF {
  MO_NO_FLAG,
  MO_HI21,
  MO_LO11
};
} // namespace ZodiacII

static inline unsigned getZodiacRegisterNumbering(MCRegister Reg) {
  switch (Reg.id()) {
  case Zodiac::X0: return 0;
  case Zodiac::X1: return 1;
  case Zodiac::X2: return 2;
  case Zodiac::X3: return 3;
  case Zodiac::X4: return 4;
  case Zodiac::X5: return 5;
  case Zodiac::X6: return 6;
  case Zodiac::X7: return 7;
  case Zodiac::X8: return 8;
  case Zodiac::X9: return 9;
  case Zodiac::X10: return 10;
  case Zodiac::X11: return 11;
  case Zodiac::X12: return 12;
  case Zodiac::X13: return 13;
  case Zodiac::X14: return 14;
  case Zodiac::X15: return 15;
  case Zodiac::X16: return 16;
  case Zodiac::X17: return 17;
  case Zodiac::X18: return 18;
  case Zodiac::X19: return 19;
  case Zodiac::X20: return 20;
  case Zodiac::X21: return 21;
  case Zodiac::X22: return 22;
  case Zodiac::X23: return 23;
  case Zodiac::X24: return 24;
  case Zodiac::X25: return 25;
  case Zodiac::X26: return 26;
  case Zodiac::X27: return 27;
  case Zodiac::X28: return 28;
  case Zodiac::X29: return 29;
  case Zodiac::X30: return 30;
  case Zodiac::X31: return 31;
  default:
    llvm_unreachable("Unknown register number!");
  }
}

} // namespace llvm

#endif // LLVM_LIB_TARGET_ZODIAC_MCTARGETDESC_ZODIACBASEINFO_H
