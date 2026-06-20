//===-- ZodiacMCAsmInfo.cpp - Zodiac asm properties -----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the declarations of the ZodiacMCAsmInfo properties.
//
//===----------------------------------------------------------------------===//

#include "ZodiacMCAsmInfo.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/TargetParser/Triple.h"

using namespace llvm;

void ZodiacMCAsmInfo::anchor() {}

ZodiacMCAsmInfo::ZodiacMCAsmInfo(const Triple & /*TheTriple*/,
                               const MCTargetOptions &Options)
    : MCAsmInfo(Options) {
  IsLittleEndian = false;

  // Use '__L' as internal prefix since the assembler's lexer
  // treats '.' as an invalid symbol character.
  InternalSymbolPrefix = "__L";

  // Suppress all ELF directives — Zodiac is bare-metal, no OS, no ELF loader.
  HasDotTypeDotSizeDirective = false;  // No .type / .size
  HasSingleParameterDotFile = false;   // No .file "name"

  // No exception handling or debug info on bare metal.
  ExceptionsType = ExceptionHandling::None;
  SupportsDebugInformation = false;

  // Use ';' as comment string to match the Zodiac assembler.
  CommentString = ";";

  // Set the instruction alignment.
  MinInstAlignment = 4;
}

void ZodiacMCAsmInfo::printSpecifierExpr(raw_ostream &OS,
                                        const MCSpecifierExpr &Expr) const {
  if (Expr.getSpecifier() == 0) {
    printExpr(OS, *Expr.getSubExpr());
    return;
  }

  switch (Expr.getSpecifier()) {
  default:
    llvm_unreachable("Invalid kind!");
  case Zodiac::S_ABS_HI:
    OS << "hi";
    break;
  case Zodiac::S_ABS_LO:
    OS << "lo";
    break;
  }

  OS << '(';
  printExpr(OS, *Expr.getSubExpr());
  OS << ')';
}
