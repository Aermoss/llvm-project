//===-- ZodiacBaseInfo.h - Top level definitions for Zodiac MC ----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains small standalone helper functions and enum definitions for
// the Zodiac target useful for the compiler back-end and the MC libraries.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ZODIAC_MCTARGETDESC_ZODIACBASEINFO_H
#define LLVM_LIB_TARGET_ZODIAC_MCTARGETDESC_ZODIACBASEINFO_H

#include "ZodiacMCTargetDesc.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/Support/DataTypes.h"
#include "llvm/Support/ErrorHandling.h"

namespace llvm {

// ZodiacII - This namespace holds all of the target specific flags that
// instruction info tracks.
namespace ZodiacII {
// Target Operand Flag enum.
enum TOF {
  //===------------------------------------------------------------------===//
  // Zodiac Specific MachineOperand flags.
  MO_NO_FLAG,

  // MO_ABS_HI/LO - Represents the hi or low part of an absolute symbol
  // address.
  MO_ABS_HI,
  MO_ABS_LO,
};
} // namespace ZodiacII

static inline unsigned getZodiacRegisterNumbering(MCRegister Reg) {
  switch (Reg.id()) {
  case Zodiac::R0:
    return 0;
  case Zodiac::R1:
    return 1;
  case Zodiac::R2:
  case Zodiac::PC:
    return 2;
  case Zodiac::R3:
    return 3;
  case Zodiac::R4:
  case Zodiac::SP:
    return 4;
  case Zodiac::R5:
  case Zodiac::FP:
    return 5;
  case Zodiac::R6:
    return 6;
  case Zodiac::R7:
    return 7;
  case Zodiac::R8:
  case Zodiac::RV:
    return 8;
  case Zodiac::R9:
    return 9;
  case Zodiac::R10:
  case Zodiac::RR1:
    return 10;
  case Zodiac::R11:
  case Zodiac::RR2:
    return 11;
  case Zodiac::R12:
    return 12;
  case Zodiac::R13:
    return 13;
  case Zodiac::R14:
    return 14;
  case Zodiac::R15:
  case Zodiac::RCA:
    return 15;
  case Zodiac::R16:
    return 16;
  case Zodiac::R17:
    return 17;
  case Zodiac::R18:
    return 18;
  case Zodiac::R19:
    return 19;
  case Zodiac::R20:
    return 20;
  case Zodiac::R21:
    return 21;
  case Zodiac::R22:
    return 22;
  case Zodiac::R23:
    return 23;
  case Zodiac::R24:
    return 24;
  case Zodiac::R25:
    return 25;
  case Zodiac::R26:
    return 26;
  case Zodiac::R27:
    return 27;
  case Zodiac::R28:
    return 28;
  case Zodiac::R29:
    return 29;
  case Zodiac::R30:
    return 30;
  case Zodiac::R31:
    return 31;
  default:
    llvm_unreachable("Unknown register number!");
  }
}
} // namespace llvm
#endif // LLVM_LIB_TARGET_ZODIAC_MCTARGETDESC_ZODIACBASEINFO_H
