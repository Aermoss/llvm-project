//===-- ZodiacRegisterInfo.cpp - Zodiac Register Information ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the Zodiac implementation of the TargetRegisterInfo class.
//
//===----------------------------------------------------------------------===//

#include "ZodiacRegisterInfo.h"
#include "ZodiacAluCode.h"
#include "ZodiacCondCode.h"
#include "ZodiacFrameLowering.h"
#include "ZodiacInstrInfo.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/RegisterScavenging.h"
#include "llvm/CodeGen/TargetFrameLowering.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/ErrorHandling.h"

#define GET_REGINFO_TARGET_DESC
#include "ZodiacGenRegisterInfo.inc"

using namespace llvm;

ZodiacRegisterInfo::ZodiacRegisterInfo() : ZodiacGenRegisterInfo(Zodiac::RCA) {}

const uint16_t *
ZodiacRegisterInfo::getCalleeSavedRegs(const MachineFunction * /*MF*/) const {
  return CSR_SaveList;
}

BitVector ZodiacRegisterInfo::getReservedRegs(const MachineFunction &MF) const {
  BitVector Reserved(getNumRegs());

  Reserved.set(Zodiac::R0);
  Reserved.set(Zodiac::R1);
  Reserved.set(Zodiac::PC);
  Reserved.set(Zodiac::R2);
  Reserved.set(Zodiac::SP);
  Reserved.set(Zodiac::R4);
  Reserved.set(Zodiac::FP);
  Reserved.set(Zodiac::R5);
  Reserved.set(Zodiac::RR1);
  Reserved.set(Zodiac::R10);
  Reserved.set(Zodiac::RR2);
  Reserved.set(Zodiac::R11);
  Reserved.set(Zodiac::RCA);
  Reserved.set(Zodiac::R15);
  if (hasBasePointer(MF))
    Reserved.set(getBaseRegister());
  return Reserved;
}

bool ZodiacRegisterInfo::requiresRegisterScavenging(
    const MachineFunction & /*MF*/) const {
  return true;
}

static bool isALUArithLoOpcode(unsigned Opcode) {
  switch (Opcode) {
  case Zodiac::ADD_I_LO:
  case Zodiac::SUB_I_LO:
  case Zodiac::ADD_F_I_LO:
  case Zodiac::SUB_F_I_LO:
  case Zodiac::ADDC_I_LO:
  case Zodiac::SUBB_I_LO:
  case Zodiac::ADDC_F_I_LO:
  case Zodiac::SUBB_F_I_LO:
    return true;
  default:
    return false;
  }
}

static unsigned getOppositeALULoOpcode(unsigned Opcode) {
  switch (Opcode) {
  case Zodiac::ADD_I_LO:
    return Zodiac::SUB_I_LO;
  case Zodiac::SUB_I_LO:
    return Zodiac::ADD_I_LO;
  case Zodiac::ADD_F_I_LO:
    return Zodiac::SUB_F_I_LO;
  case Zodiac::SUB_F_I_LO:
    return Zodiac::ADD_F_I_LO;
  case Zodiac::ADDC_I_LO:
    return Zodiac::SUBB_I_LO;
  case Zodiac::SUBB_I_LO:
    return Zodiac::ADDC_I_LO;
  case Zodiac::ADDC_F_I_LO:
    return Zodiac::SUBB_F_I_LO;
  case Zodiac::SUBB_F_I_LO:
    return Zodiac::ADDC_F_I_LO;
  default:
    llvm_unreachable("Invalid ALU lo opcode");
  }
}

static unsigned getRRMOpcodeVariant(unsigned Opcode) {
  switch (Opcode) {
  case Zodiac::LDBs_RI:
    return Zodiac::LDBs_RR;
  case Zodiac::LDBz_RI:
    return Zodiac::LDBz_RR;
  case Zodiac::LDHs_RI:
    return Zodiac::LDHs_RR;
  case Zodiac::LDHz_RI:
    return Zodiac::LDHz_RR;
  case Zodiac::LDW_RI:
    return Zodiac::LDW_RR;
  case Zodiac::STB_RI:
    return Zodiac::STB_RR;
  case Zodiac::STH_RI:
    return Zodiac::STH_RR;
  case Zodiac::SW_RI:
    return Zodiac::SW_RR;
  default:
    llvm_unreachable("Opcode has no RRM variant");
  }
}

bool ZodiacRegisterInfo::eliminateFrameIndex(MachineBasicBlock::iterator II,
                                            int SPAdj, unsigned FIOperandNum,
                                            RegScavenger *RS) const {
  assert(SPAdj == 0 && "Unexpected");

  MachineInstr &MI = *II;
  MachineFunction &MF = *MI.getParent()->getParent();
  const TargetInstrInfo *TII = MF.getSubtarget().getInstrInfo();
  const TargetFrameLowering *TFI = MF.getSubtarget().getFrameLowering();
  bool HasFP = TFI->hasFP(MF);
  DebugLoc DL = MI.getDebugLoc();

  int FrameIndex = MI.getOperand(FIOperandNum).getIndex();

  int Offset = MF.getFrameInfo().getObjectOffset(FrameIndex) +
               MI.getOperand(FIOperandNum + 1).getImm();

  // Addressable stack objects are addressed using neg. offsets from fp
  // or pos. offsets from sp/basepointer
  if (!HasFP || (hasStackRealignment(MF) && FrameIndex >= 0))
    Offset += MF.getFrameInfo().getStackSize();

  Register FrameReg = getFrameRegister(MF);
  if (FrameIndex >= 0) {
    if (hasBasePointer(MF))
      FrameReg = getBaseRegister();
    else if (hasStackRealignment(MF))
      FrameReg = Zodiac::SP;
  }

  // Replace frame index with a frame pointer reference.
  // If the offset is small enough to fit in the immediate field, directly
  // encode it.
  // Otherwise scavenge a register and encode it into a MOVHI, OR_I_LO sequence.
  if ((isSPLSOpcode(MI.getOpcode()) && !isInt<10>(Offset)) ||
      !isInt<16>(Offset)) {
    assert(RS && "Register scavenging must be on");
    Register Reg = RS->FindUnusedReg(&Zodiac::GPRRegClass);
    if (!Reg)
      Reg = RS->scavengeRegisterBackwards(Zodiac::GPRRegClass, II, false, SPAdj);
    assert(Reg && "Register scavenger failed");

    bool HasNegOffset = false;
    // ALU ops have unsigned immediate values. If the Offset is negative, we
    // negate it here and reverse the opcode later.
    if (Offset < 0) {
      HasNegOffset = true;
      Offset = -Offset;
    }

    if (!isInt<16>(Offset)) {
      // Reg = hi(offset) | lo(offset)
      BuildMI(*MI.getParent(), II, DL, TII->get(Zodiac::MOVHI), Reg)
          .addImm(static_cast<uint32_t>(Offset) >> 16);
      BuildMI(*MI.getParent(), II, DL, TII->get(Zodiac::OR_I_LO), Reg)
          .addReg(Reg)
          .addImm(Offset & 0xffffU);
    } else {
      // Reg = mov(offset)
      BuildMI(*MI.getParent(), II, DL, TII->get(Zodiac::ADD_I_LO), Reg)
          .addImm(0)
          .addImm(Offset);
    }
    // Reg = FrameReg OP Reg
    if (MI.getOpcode() == Zodiac::ADD_I_LO) {
      BuildMI(*MI.getParent(), II, DL,
              HasNegOffset ? TII->get(Zodiac::SUB_R) : TII->get(Zodiac::ADD_R),
              MI.getOperand(0).getReg())
          .addReg(FrameReg)
          .addReg(Reg)
          .addImm(LPCC::ICC_T);
      MI.eraseFromParent();
      return true;
    }
    if (isSPLSOpcode(MI.getOpcode()) || isRMOpcode(MI.getOpcode())) {
      MI.setDesc(TII->get(getRRMOpcodeVariant(MI.getOpcode())));
      if (HasNegOffset) {
        // Change the ALU op (operand 3) from LPAC::ADD (the default) to
        // LPAC::SUB with the already negated offset.
        assert((MI.getOperand(3).getImm() == LPAC::ADD) &&
               "Unexpected ALU op in RRM instruction");
        MI.getOperand(3).setImm(LPAC::SUB);
      }
    } else
      llvm_unreachable("Unexpected opcode in frame index operation");

    MI.getOperand(FIOperandNum).ChangeToRegister(FrameReg, /*isDef=*/false);
    MI.getOperand(FIOperandNum + 1)
        .ChangeToRegister(Reg, /*isDef=*/false, /*isImp=*/false,
                          /*isKill=*/true);
    return false;
  }

  // ALU arithmetic ops take unsigned immediates. If the offset is negative,
  // we replace the instruction with one that inverts the opcode and negates
  // the immediate.
  if ((Offset < 0) && isALUArithLoOpcode(MI.getOpcode())) {
    unsigned NewOpcode = getOppositeALULoOpcode(MI.getOpcode());
    // We know this is an ALU op, so we know the operands are as follows:
    // 0: destination register
    // 1: source register (frame register)
    // 2: immediate
    BuildMI(*MI.getParent(), II, DL, TII->get(NewOpcode),
            MI.getOperand(0).getReg())
        .addReg(FrameReg)
        .addImm(-Offset);
    MI.eraseFromParent();
    return true;
  }

  MI.getOperand(FIOperandNum).ChangeToRegister(FrameReg, /*isDef=*/false);
  MI.getOperand(FIOperandNum + 1).ChangeToImmediate(Offset);
  return false;
}

bool ZodiacRegisterInfo::hasBasePointer(const MachineFunction &MF) const {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  // When we need stack realignment and there are dynamic allocas, we can't
  // reference off of the stack pointer, so we reserve a base pointer.
  if (hasStackRealignment(MF) && MFI.hasVarSizedObjects())
    return true;

  return false;
}

unsigned ZodiacRegisterInfo::getRARegister() const { return Zodiac::RCA; }

Register
ZodiacRegisterInfo::getFrameRegister(const MachineFunction & /*MF*/) const {
  return Zodiac::FP;
}

Register ZodiacRegisterInfo::getBaseRegister() const { return Zodiac::R14; }

const uint32_t *
ZodiacRegisterInfo::getCallPreservedMask(const MachineFunction & /*MF*/,
                                        CallingConv::ID /*CC*/) const {
  return CSR_RegMask;
}
