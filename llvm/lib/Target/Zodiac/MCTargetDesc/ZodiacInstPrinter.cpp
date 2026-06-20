//===-- ZodiacInstPrinter.cpp - Convert Zodiac MCInst to asm syntax ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ZodiacInstPrinter.h"
#include "MCTargetDesc/ZodiacMCTargetDesc.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "asm-printer"

// Include the auto-generated portion of the assembly writer.
#define PRINT_ALIAS_INSTR
#include "ZodiacGenAsmWriter.inc"

void ZodiacInstPrinter::printRegName(raw_ostream &OS, MCRegister Reg) {
  OS << StringRef(getRegisterName(Reg)).lower();
}

void ZodiacInstPrinter::printInst(const MCInst *MI, uint64_t Address,
                                 StringRef Annot, const MCSubtargetInfo &STI,
                                 raw_ostream &OS) {
  // Print to a temporary buffer so we can convert tabs to spaces.
  SmallString<128> Buf;
  raw_svector_ostream TmpOS(Buf);
  if (!printAliasInstr(MI, Address, TmpOS))
    printInstruction(MI, Address, TmpOS);

  // Replace leading tab with 4 spaces and convert remaining tabs to spaces.
  StringRef S = TmpOS.str();
  if (S.starts_with("\t")) {
    OS << "    ";
    S = S.drop_front(1);
  }
  for (char C : S)
    OS << (C == '\t' ? ' ' : C);

  printAnnotation(OS, Annot);
}

void ZodiacInstPrinter::printOperand(const MCInst *MI, unsigned OpNo,
                                     raw_ostream &O) {
  const MCOperand &Op = MI->getOperand(OpNo);
  if (Op.isReg()) {
    printRegName(O, Op.getReg());
  } else if (Op.isImm()) {
    O << Op.getImm();
  } else {
    assert(Op.isExpr() && "unknown operand kind in printOperand");
    MAI.printExpr(O, *Op.getExpr());
  }
}
