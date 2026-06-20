//===-- ZodiacMCCodeEmitter.cpp - Convert Zodiac code to machine code -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/ZodiacBaseInfo.h"
#include "MCTargetDesc/ZodiacFixupKinds.h"
#include "MCTargetDesc/ZodiacMCAsmInfo.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/EndianStream.h"
#include <cassert>
#include <cstdint>

#define DEBUG_TYPE "mccodeemitter"

STATISTIC(MCNumEmitted, "Number of MC instructions emitted");

namespace llvm {

namespace {

class ZodiacMCCodeEmitter : public MCCodeEmitter {
public:
  ZodiacMCCodeEmitter(const MCInstrInfo &MCII, MCContext &C) {}
  ZodiacMCCodeEmitter(const ZodiacMCCodeEmitter &) = delete;
  void operator=(const ZodiacMCCodeEmitter &) = delete;
  ~ZodiacMCCodeEmitter() override = default;

  uint64_t getBinaryCodeForInstr(const MCInst &Inst,
                                 SmallVectorImpl<MCFixup> &Fixups,
                                 const MCSubtargetInfo &SubtargetInfo) const;

  unsigned getMachineOpValue(const MCInst &Inst, const MCOperand &MCOp,
                             SmallVectorImpl<MCFixup> &Fixups,
                             const MCSubtargetInfo &SubtargetInfo) const;

  unsigned getBranchTarget16OpValue(const MCInst &Inst, unsigned OpNo,
                                    SmallVectorImpl<MCFixup> &Fixups,
                                    const MCSubtargetInfo &SubtargetInfo) const;

  unsigned getBranchTarget21OpValue(const MCInst &Inst, unsigned OpNo,
                                    SmallVectorImpl<MCFixup> &Fixups,
                                    const MCSubtargetInfo &SubtargetInfo) const;

  unsigned getBranchTarget26OpValue(const MCInst &Inst, unsigned OpNo,
                                    SmallVectorImpl<MCFixup> &Fixups,
                                    const MCSubtargetInfo &SubtargetInfo) const;

  void encodeInstruction(const MCInst &Inst, SmallVectorImpl<char> &CB,
                         SmallVectorImpl<MCFixup> &Fixups,
                         const MCSubtargetInfo &SubtargetInfo) const override;
};

} // end anonymous namespace

static Zodiac::Fixups FixupKind(const MCExpr *Expr) {
  if (isa<MCSymbolRefExpr>(Expr))
    return Zodiac::FIXUP_ZODIAC_LO11; // default to lower bits if no specifier
  if (const MCSpecifierExpr *McExpr = dyn_cast<MCSpecifierExpr>(Expr)) {
    Zodiac::Specifier ExprKind = McExpr->getSpecifier();
    switch (ExprKind) {
    case Zodiac::S_None:
      return Zodiac::FIXUP_ZODIAC_LO11;
    case Zodiac::S_ABS_HI:
      return Zodiac::FIXUP_ZODIAC_HI21;
    case Zodiac::S_ABS_LO:
      return Zodiac::FIXUP_ZODIAC_LO11;
    }
  }
  return Zodiac::Fixups(0);
}

unsigned ZodiacMCCodeEmitter::getMachineOpValue(
    const MCInst &Inst, const MCOperand &MCOp, SmallVectorImpl<MCFixup> &Fixups,
    const MCSubtargetInfo &SubtargetInfo) const {
  if (MCOp.isReg())
    return getZodiacRegisterNumbering(MCOp.getReg());
  if (MCOp.isImm())
    return static_cast<unsigned>(MCOp.getImm());

  assert(MCOp.isExpr());
  const MCExpr *Expr = MCOp.getExpr();

  if (Expr->getKind() == MCExpr::Binary) {
    const MCBinaryExpr *BinaryExpr = static_cast<const MCBinaryExpr *>(Expr);
    Expr = BinaryExpr->getLHS();
  }

  assert(isa<MCSpecifierExpr>(Expr) || Expr->getKind() == MCExpr::SymbolRef);
  Fixups.push_back(
      MCFixup::create(0, MCOp.getExpr(), MCFixupKind(FixupKind(Expr))));
  return 0;
}

unsigned ZodiacMCCodeEmitter::getBranchTarget16OpValue(
    const MCInst &Inst, unsigned OpNo, SmallVectorImpl<MCFixup> &Fixups,
    const MCSubtargetInfo &SubtargetInfo) const {
  const MCOperand &MCOp = Inst.getOperand(OpNo);
  if (MCOp.isReg() || MCOp.isImm())
    return getMachineOpValue(Inst, MCOp, Fixups, SubtargetInfo);

  Fixups.push_back(MCFixup::create(0, MCOp.getExpr(), Zodiac::FIXUP_ZODIAC_IMM16));
  return 0;
}

unsigned ZodiacMCCodeEmitter::getBranchTarget21OpValue(
    const MCInst &Inst, unsigned OpNo, SmallVectorImpl<MCFixup> &Fixups,
    const MCSubtargetInfo &SubtargetInfo) const {
  const MCOperand &MCOp = Inst.getOperand(OpNo);
  if (MCOp.isReg() || MCOp.isImm())
    return getMachineOpValue(Inst, MCOp, Fixups, SubtargetInfo);

  Fixups.push_back(MCFixup::create(0, MCOp.getExpr(), Zodiac::FIXUP_ZODIAC_IMM21));
  return 0;
}

unsigned ZodiacMCCodeEmitter::getBranchTarget26OpValue(
    const MCInst &Inst, unsigned OpNo, SmallVectorImpl<MCFixup> &Fixups,
    const MCSubtargetInfo &SubtargetInfo) const {
  const MCOperand &MCOp = Inst.getOperand(OpNo);
  if (MCOp.isReg() || MCOp.isImm())
    return getMachineOpValue(Inst, MCOp, Fixups, SubtargetInfo);

  Fixups.push_back(MCFixup::create(0, MCOp.getExpr(), Zodiac::FIXUP_ZODIAC_IMM26));
  return 0;
}

void ZodiacMCCodeEmitter::encodeInstruction(
    const MCInst &Inst, SmallVectorImpl<char> &CB,
    SmallVectorImpl<MCFixup> &Fixups,
    const MCSubtargetInfo &SubtargetInfo) const {
  unsigned Value = getBinaryCodeForInstr(Inst, Fixups, SubtargetInfo);
  ++MCNumEmitted;
  support::endian::write<uint32_t>(CB, Value, llvm::endianness::big);
}

#include "ZodiacGenMCCodeEmitter.inc"

} // end namespace llvm

llvm::MCCodeEmitter *
llvm::createZodiacMCCodeEmitter(const MCInstrInfo &InstrInfo,
                               MCContext &context) {
  return new ZodiacMCCodeEmitter(InstrInfo, context);
}
