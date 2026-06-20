//===-- ZodiacRegisterInfo.cpp - Zodiac Register Information ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ZodiacRegisterInfo.h"
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

ZodiacRegisterInfo::ZodiacRegisterInfo() : ZodiacGenRegisterInfo(Zodiac::X30) {}

const uint16_t *
ZodiacRegisterInfo::getCalleeSavedRegs(const MachineFunction * /*MF*/) const {
  return CSR_SaveList;
}

BitVector ZodiacRegisterInfo::getReservedRegs(const MachineFunction &MF) const {
  BitVector Reserved(getNumRegs());

  Reserved.set(Zodiac::X0);  // zero register
  Reserved.set(Zodiac::X31); // SP
  Reserved.set(Zodiac::X30); // LR / RA
  Reserved.set(Zodiac::X29); // FP

  if (hasBasePointer(MF))
    Reserved.set(getBaseRegister());

  return Reserved;
}

bool ZodiacRegisterInfo::requiresRegisterScavenging(
    const MachineFunction & /*MF*/) const {
  return true;
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

  Register FrameReg = getFrameRegister(MF);
  if (!HasFP || (hasStackRealignment(MF) && FrameIndex >= 0)) {
    Offset += MF.getFrameInfo().getStackSize();
    FrameReg = Zodiac::X31; // SP
  }

  if (FrameIndex >= 0 && hasBasePointer(MF)) {
    FrameReg = getBaseRegister(); // BP
  }

  if (isInt<16>(Offset)) {
    MI.getOperand(FIOperandNum).ChangeToRegister(FrameReg, /*isDef=*/false);
    MI.getOperand(FIOperandNum + 1).ChangeToImmediate(Offset);
    return false;
  }

  assert(RS && "Register scavenging must be on for large offsets");
  Register Reg = RS->FindUnusedReg(&Zodiac::GPRRegClass);
  if (!Reg)
    Reg = RS->scavengeRegisterBackwards(Zodiac::GPRRegClass, II, false, SPAdj);
  assert(Reg && "Register scavenger failed");

  // Materialize Offset in Reg: LUI + ORI
  BuildMI(*MI.getParent(), II, DL, TII->get(Zodiac::LUI), Reg)
      .addImm((static_cast<uint32_t>(Offset) >> 11) & 0x1FFFFF);
  BuildMI(*MI.getParent(), II, DL, TII->get(Zodiac::ORI), Reg)
      .addReg(Reg)
      .addImm(Offset & 0x7FF);

  // Reg = FrameReg + Reg
  BuildMI(*MI.getParent(), II, DL, TII->get(Zodiac::ADD), Reg)
      .addReg(FrameReg)
      .addReg(Reg);

  MI.getOperand(FIOperandNum).ChangeToRegister(Reg, /*isDef=*/false);
  MI.getOperand(FIOperandNum + 1).ChangeToImmediate(0);
  return false;
}

bool ZodiacRegisterInfo::hasBasePointer(const MachineFunction &MF) const {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  return hasStackRealignment(MF) && MFI.hasVarSizedObjects();
}

unsigned ZodiacRegisterInfo::getRARegister() const { return Zodiac::X30; }

Register
ZodiacRegisterInfo::getFrameRegister(const MachineFunction & /*MF*/) const {
  return Zodiac::X29;
}

Register ZodiacRegisterInfo::getBaseRegister() const { return Zodiac::X28; }

const uint32_t *
ZodiacRegisterInfo::getCallPreservedMask(const MachineFunction & /*MF*/,
                                        CallingConv::ID /*CC*/) const {
  return CSR_RegMask;
}
