//===-- ZodiacMCInstLower.cpp - Convert Zodiac MachineInstr to an MCInst ------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ZodiacMCInstLower.h"
#include "MCTargetDesc/ZodiacBaseInfo.h"
#include "MCTargetDesc/ZodiacMCAsmInfo.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

MCSymbol *
ZodiacMCInstLower::GetGlobalAddressSymbol(const MachineOperand &MO) const {
  return Printer.getSymbol(MO.getGlobal());
}

MCSymbol *
ZodiacMCInstLower::GetBlockAddressSymbol(const MachineOperand &MO) const {
  return Printer.GetBlockAddressSymbol(MO.getBlockAddress());
}

MCSymbol *
ZodiacMCInstLower::GetExternalSymbolSymbol(const MachineOperand &MO) const {
  return Printer.GetExternalSymbolSymbol(MO.getSymbolName());
}

MCSymbol *ZodiacMCInstLower::GetJumpTableSymbol(const MachineOperand &MO) const {
  SmallString<256> Name;
  raw_svector_ostream(Name)
      << Printer.MAI.getInternalSymbolPrefix() << "JTI"
      << Printer.getFunctionNumber() << '_' << MO.getIndex();
  return Ctx.getOrCreateSymbol(Name.str());
}

MCSymbol *
ZodiacMCInstLower::GetConstantPoolIndexSymbol(const MachineOperand &MO) const {
  SmallString<256> Name;
  raw_svector_ostream(Name)
      << Printer.MAI.getInternalSymbolPrefix() << "CPI"
      << Printer.getFunctionNumber() << '_' << MO.getIndex();
  return Ctx.getOrCreateSymbol(Name.str());
}

MCOperand ZodiacMCInstLower::LowerSymbolOperand(const MachineOperand &MO,
                                               MCSymbol *Sym) const {
  Zodiac::Specifier Kind;
  switch (MO.getTargetFlags()) {
  case ZodiacII::MO_NO_FLAG:
    Kind = Zodiac::S_None;
    break;
  case ZodiacII::MO_HI21:
    Kind = Zodiac::S_ABS_HI;
    break;
  case ZodiacII::MO_LO11:
    Kind = Zodiac::S_ABS_LO;
    break;
  default:
    llvm_unreachable("Unknown target flag on GV operand");
  }

  const MCExpr *Expr = MCSymbolRefExpr::create(Sym, Ctx);
  if (!MO.isJTI() && MO.getOffset())
    Expr = MCBinaryExpr::createAdd(
        Expr, MCConstantExpr::create(MO.getOffset(), Ctx), Ctx);
  Expr = MCSpecifierExpr::create(Expr, Kind, Ctx);
  return MCOperand::createExpr(Expr);
}

void ZodiacMCInstLower::Lower(const MachineInstr *MI, MCInst &OutMI) const {
  OutMI.setOpcode(MI->getOpcode());

  for (const MachineOperand &MO : MI->operands()) {
    MCOperand MCOp;
    switch (MO.getType()) {
    case MachineOperand::MO_Register:
      if (MO.isImplicit())
        continue;
      MCOp = MCOperand::createReg(MO.getReg());
      break;
    case MachineOperand::MO_Immediate:
      MCOp = MCOperand::createImm(MO.getImm());
      break;
    case MachineOperand::MO_MachineBasicBlock:
      MCOp = MCOperand::createExpr(
          MCSymbolRefExpr::create(MO.getMBB()->getSymbol(), Ctx));
      break;
    case MachineOperand::MO_RegisterMask:
      continue;
    case MachineOperand::MO_GlobalAddress:
      MCOp = LowerSymbolOperand(MO, GetGlobalAddressSymbol(MO));
      break;
    case MachineOperand::MO_BlockAddress:
      MCOp = LowerSymbolOperand(MO, GetBlockAddressSymbol(MO));
      break;
    case MachineOperand::MO_ExternalSymbol:
      MCOp = LowerSymbolOperand(MO, GetExternalSymbolSymbol(MO));
      break;
    case MachineOperand::MO_JumpTableIndex:
      MCOp = LowerSymbolOperand(MO, GetJumpTableSymbol(MO));
      break;
    case MachineOperand::MO_ConstantPoolIndex:
      MCOp = LowerSymbolOperand(MO, GetConstantPoolIndexSymbol(MO));
      break;
    default:
      MI->print(errs());
      llvm_unreachable("unknown operand type");
    }

    OutMI.addOperand(MCOp);
  }
}
