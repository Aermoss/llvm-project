//===-- ZodiacFrameLowering.cpp - Zodiac Frame Information ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ZodiacFrameLowering.h"
#include "ZodiacInstrInfo.h"
#include "ZodiacSubtarget.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

using namespace llvm;

void ZodiacFrameLowering::determineFrameLayout(MachineFunction &MF) const {
  MachineFrameInfo &MFI = MF.getFrameInfo();
  const ZodiacRegisterInfo *LRI = STI.getRegisterInfo();

  unsigned FrameSize = MFI.getStackSize();
  Align StackAlign =
      LRI->hasStackRealignment(MF) ? MFI.getMaxAlign() : getStackAlign();

  unsigned MaxCallFrameSize = MFI.getMaxCallFrameSize();
  if (MFI.hasVarSizedObjects())
    MaxCallFrameSize = alignTo(MaxCallFrameSize, StackAlign);

  MFI.setMaxCallFrameSize(MaxCallFrameSize);

  if (!(hasReservedCallFrame(MF) && MFI.adjustsStack()))
    FrameSize += MaxCallFrameSize;

  FrameSize = alignTo(FrameSize, StackAlign);
  MFI.setStackSize(FrameSize);
}

void ZodiacFrameLowering::emitPrologue(MachineFunction &MF,
                                      MachineBasicBlock &MBB) const {
  assert(&MF.front() == &MBB && "Shrink-wrapping not yet supported");

  MachineFrameInfo &MFI = MF.getFrameInfo();
  const ZodiacInstrInfo &TII = *STI.getInstrInfo();
  MachineBasicBlock::iterator MBBI = MBB.begin();
  DebugLoc DL;

  determineFrameLayout(MF);
  unsigned StackSize = MFI.getStackSize();
  if (StackSize == 0 && !MFI.adjustsStack())
    return;

  // Allocate space on stack: addi x31, x31, -StackSize
  if (StackSize != 0) {
    BuildMI(MBB, MBBI, DL, TII.get(Zodiac::ADDI), Zodiac::X31)
        .addReg(Zodiac::X31)
        .addImm(-static_cast<int32_t>(StackSize))
        .setMIFlag(MachineInstr::FrameSetup);
  }

  // Set FP (x29) = SP + StackSize (old SP)
  if (hasFP(MF)) {
    // Update FP after CSR spills.
    // CSR spills are marked with FrameSetup flag.
    MachineBasicBlock::iterator FPInsertPoint = MBB.begin();
    while (FPInsertPoint != MBB.end() && FPInsertPoint->getFlag(MachineInstr::FrameSetup))
      ++FPInsertPoint;

    BuildMI(MBB, FPInsertPoint, DL, TII.get(Zodiac::ADDI), Zodiac::X29)
        .addReg(Zodiac::X31)
        .addImm(StackSize)
        .setMIFlag(MachineInstr::FrameSetup);
  }
}

void ZodiacFrameLowering::emitEpilogue(MachineFunction &MF,
                                      MachineBasicBlock &MBB) const {
  MachineFrameInfo &MFI = MF.getFrameInfo();
  const ZodiacInstrInfo &TII = *STI.getInstrInfo();
  MachineBasicBlock::iterator MBBI = MBB.getFirstTerminator();
  DebugLoc DL;
  if (MBBI != MBB.end())
    DL = MBBI->getDebugLoc();

  unsigned StackSize = MFI.getStackSize();
  if (StackSize == 0 && !MFI.adjustsStack())
    return;

  if (hasFP(MF)) {
    // Find the first CSR restore instruction to insert SP restoration before it.
    // Callee-saved restores are marked with FrameDestroy flag.
    while (MBBI != MBB.begin() &&
           std::prev(MBBI)->getFlag(MachineInstr::FrameDestroy))
      --MBBI;

    BuildMI(MBB, MBBI, DL, TII.get(Zodiac::ADDI), Zodiac::X31)
        .addReg(Zodiac::X29)
        .addImm(0)
        .setMIFlag(MachineInstr::FrameDestroy);
  } else if (StackSize != 0) {
    // Insert SP restoration after CSR restores (at the terminator).
    BuildMI(MBB, MBBI, DL, TII.get(Zodiac::ADDI), Zodiac::X31)
        .addReg(Zodiac::X31)
        .addImm(StackSize)
        .setMIFlag(MachineInstr::FrameDestroy);
  }
}

MachineBasicBlock::iterator ZodiacFrameLowering::eliminateCallFramePseudoInstr(
    MachineFunction & /*MF*/, MachineBasicBlock &MBB,
    MachineBasicBlock::iterator I) const {
  return MBB.erase(I);
}

void ZodiacFrameLowering::determineCalleeSaves(MachineFunction &MF,
                                               BitVector &SavedRegs,
                                               RegScavenger *RS) const {
  TargetFrameLowering::determineCalleeSaves(MF, SavedRegs, RS);

  if (MF.getFrameInfo().adjustsStack()) {
    SavedRegs.set(Zodiac::X30); // Save link register
  }
  if (hasFP(MF)) {
    SavedRegs.set(Zodiac::X29); // Save frame pointer
  }
}

bool ZodiacFrameLowering::hasFPImpl(const MachineFunction &MF) const {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  return MFI.hasVarSizedObjects() || MFI.isFrameAddressTaken();
}
