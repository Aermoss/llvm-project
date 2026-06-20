//===- ZodiacDisassembler.cpp - Disassembler for Zodiac -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ZodiacDisassembler.h"
#include "ZodiacInstrInfo.h"
#include "TargetInfo/ZodiacTargetInfo.h"
#include "llvm/MC/MCDecoder.h"
#include "llvm/MC/MCDecoderOps.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/MathExtras.h"

#define DEBUG_TYPE "zodiac-disassembler"

using namespace llvm;
using namespace llvm::MCD;

typedef MCDisassembler::DecodeStatus DecodeStatus;

static MCDisassembler *createZodiacDisassembler(const Target & /*T*/,
                                               const MCSubtargetInfo &STI,
                                               MCContext &Ctx) {
  return new ZodiacDisassembler(STI, Ctx);
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeZodiacDisassembler() {
  TargetRegistry::RegisterMCDisassembler(getTheZodiacTarget(),
                                         createZodiacDisassembler);
}

ZodiacDisassembler::ZodiacDisassembler(const MCSubtargetInfo &STI, MCContext &Ctx)
    : MCDisassembler(STI, Ctx) {}

static const unsigned GPRDecoderTable[] = {
  Zodiac::X0,  Zodiac::X1,  Zodiac::X2,  Zodiac::X3,  Zodiac::X4,  Zodiac::X5,
  Zodiac::X6,  Zodiac::X7,  Zodiac::X8,  Zodiac::X9,  Zodiac::X10, Zodiac::X11,
  Zodiac::X12, Zodiac::X13, Zodiac::X14, Zodiac::X15, Zodiac::X16, Zodiac::X17,
  Zodiac::X18, Zodiac::X19, Zodiac::X20, Zodiac::X21, Zodiac::X22, Zodiac::X23,
  Zodiac::X24, Zodiac::X25, Zodiac::X26, Zodiac::X27, Zodiac::X28, Zodiac::X29,
  Zodiac::X30, Zodiac::X31
};

static DecodeStatus DecodeGPRRegisterClass(MCInst &Inst, unsigned RegNo,
                                    uint64_t /*Address*/,
                                    const MCDisassembler * /*Decoder*/) {
  if (RegNo > 31)
    return MCDisassembler::Fail;
  Inst.addOperand(MCOperand::createReg(GPRDecoderTable[RegNo]));
  return MCDisassembler::Success;
}

static DecodeStatus decodeSimm16(MCInst &Inst, unsigned Val,
                                 uint64_t Address,
                                 const MCDisassembler *Decoder) {
  Inst.addOperand(MCOperand::createImm(SignExtend32<16>(Val)));
  return MCDisassembler::Success;
}

static DecodeStatus decodeUimm21(MCInst &Inst, unsigned Val,
                                 uint64_t Address,
                                 const MCDisassembler *Decoder) {
  Inst.addOperand(MCOperand::createImm(Val));
  return MCDisassembler::Success;
}

static DecodeStatus decodeBrTarget16(MCInst &Inst, unsigned Val,
                                     uint64_t Address,
                                     const MCDisassembler *Decoder) {
  Inst.addOperand(MCOperand::createImm(SignExtend32<16>(Val)));
  return MCDisassembler::Success;
}

static DecodeStatus decodeBrTarget21(MCInst &Inst, unsigned Val,
                                     uint64_t Address,
                                     const MCDisassembler *Decoder) {
  Inst.addOperand(MCOperand::createImm(SignExtend32<21>(Val)));
  return MCDisassembler::Success;
}

static DecodeStatus decodeBrTarget26(MCInst &Inst, unsigned Val,
                                     uint64_t Address,
                                     const MCDisassembler *Decoder) {
  Inst.addOperand(MCOperand::createImm(SignExtend32<26>(Val)));
  return MCDisassembler::Success;
}

#include "ZodiacGenDisassemblerTables.inc"

static DecodeStatus readInstruction32(ArrayRef<uint8_t> Bytes, uint64_t &Size,
                                      uint32_t &Insn) {
  if (Bytes.size() < 4) {
    Size = 0;
    return MCDisassembler::Fail;
  }
  Insn = (Bytes[0] << 24) | (Bytes[1] << 16) | (Bytes[2] << 8) | (Bytes[3] << 0);
  return MCDisassembler::Success;
}

DecodeStatus
ZodiacDisassembler::getInstruction(MCInst &Instr, uint64_t &Size,
                                  ArrayRef<uint8_t> Bytes, uint64_t Address,
                                  raw_ostream & /*CStream*/) const {
  uint32_t Insn;
  DecodeStatus Result = readInstruction32(Bytes, Size, Insn);
  if (Result == MCDisassembler::Fail)
    return MCDisassembler::Fail;

  Result = decodeInstruction(DecoderTableZodiac32, Instr, Insn, Address, this, STI);
  if (Result != MCDisassembler::Fail) {
    Size = 4;
    return Result;
  }

  return MCDisassembler::Fail;
}
