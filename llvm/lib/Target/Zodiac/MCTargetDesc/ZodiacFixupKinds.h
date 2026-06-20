//===-- ZodiacFixupKinds.h - Zodiac Specific Fixup Entries --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ZODIAC_MCTARGETDESC_ZODIACFIXUPKINDS_H
#define LLVM_LIB_TARGET_ZODIAC_MCTARGETDESC_ZODIACFIXUPKINDS_H

#include "llvm/MC/MCFixup.h"

namespace llvm {
namespace Zodiac {
enum Fixups {
  // Results in R_Zodiac_NONE
  FIXUP_ZODIAC_NONE = FirstTargetFixupKind,

  FIXUP_ZODIAC_LO11,  // lower 11 bits (relocation for ORI/ADDI)
  FIXUP_ZODIAC_HI21,  // upper 21 bits (relocation for LUI)
  FIXUP_ZODIAC_IMM16, // 16-bit offset/imm (relocation for loads/stores/branches)
  FIXUP_ZODIAC_IMM21, // 21-bit offset/imm (relocation for BL / J-type)
  FIXUP_ZODIAC_IMM26, // 26-bit target (relocation for B / J26-type)
  FIXUP_ZODIAC_32,    // general 32-bit relocation

  // Marker
  LastTargetFixupKind,
  NumTargetFixupKinds = LastTargetFixupKind - FirstTargetFixupKind
};
} // namespace Zodiac
} // namespace llvm

#endif // LLVM_LIB_TARGET_ZODIAC_MCTARGETDESC_ZODIACFIXUPKINDS_H
