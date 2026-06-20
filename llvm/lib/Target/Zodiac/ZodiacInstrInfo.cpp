//===-- ZodiacInstrInfo.cpp - Zodiac Instruction Information ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ZodiacInstrInfo.h"
#include "ZodiacSubtarget.h"
#include "MCTargetDesc/ZodiacBaseInfo.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define GET_INSTRINFO_CTOR_DTOR
#include "ZodiacGenInstrInfo.inc"

ZodiacInstrInfo::ZodiacInstrInfo(const ZodiacSubtarget &STI)
    : ZodiacGenInstrInfo(STI, RegisterInfo, Zodiac::ADJCALLSTACKDOWN,
                         Zodiac::ADJCALLSTACKUP),
      RegisterInfo() {}

void ZodiacInstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                                 MachineBasicBlock::iterator Position,
                                 const DebugLoc &DL,
                                 Register DestinationRegister,
                                 Register SourceRegister, bool KillSource,
                                 bool RenamableDest, bool RenamableSrc) const {
  if (!Zodiac::GPRRegClass.contains(DestinationRegister, SourceRegister)) {
    llvm_unreachable("Impossible reg-to-reg copy");
  }

  BuildMI(MBB, Position, DL, get(Zodiac::ADD), DestinationRegister)
      .addReg(SourceRegister, getKillRegState(KillSource))
      .addReg(Zodiac::X0);
}

void ZodiacInstrInfo::storeRegToStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator Position,
    Register SourceRegister, bool IsKill, int FrameIndex,
    const TargetRegisterClass *RegisterClass, Register /*VReg*/,
    MachineInstr::MIFlag /*Flags*/) const {
  DebugLoc DL;
  if (Position != MBB.end()) {
    DL = Position->getDebugLoc();
  }

  if (!Zodiac::GPRRegClass.hasSubClassEq(RegisterClass)) {
    llvm_unreachable("Can't store this register to stack slot");
  }
  BuildMI(MBB, Position, DL, get(Zodiac::SW))
      .addReg(SourceRegister, getKillRegState(IsKill))
      .addFrameIndex(FrameIndex)
      .addImm(0);
}

void ZodiacInstrInfo::loadRegFromStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator Position,
    Register DestinationRegister, int FrameIndex,
    const TargetRegisterClass *RegisterClass, Register /*VReg*/,
    unsigned /*SubReg*/, MachineInstr::MIFlag /*Flags*/) const {
  DebugLoc DL;
  if (Position != MBB.end()) {
    DL = Position->getDebugLoc();
  }

  if (!Zodiac::GPRRegClass.hasSubClassEq(RegisterClass)) {
    llvm_unreachable("Can't load this register from stack slot");
  }
  BuildMI(MBB, Position, DL, get(Zodiac::LW), DestinationRegister)
      .addFrameIndex(FrameIndex)
      .addImm(0);
}

bool ZodiacInstrInfo::areMemAccessesTriviallyDisjoint(
    const MachineInstr &MIa, const MachineInstr &MIb) const {
  return false;
}

bool ZodiacInstrInfo::expandPostRAPseudo(MachineInstr & /*MI*/) const {
  return false;
}

std::pair<unsigned, unsigned>
ZodiacInstrInfo::decomposeMachineOperandsTargetFlags(unsigned TF) const {
  return std::make_pair(TF, 0u);
}

ArrayRef<std::pair<unsigned, const char *>>
ZodiacInstrInfo::getSerializableDirectMachineOperandTargetFlags() const {
  return {};
}

bool ZodiacInstrInfo::analyzeCompare(const MachineInstr &MI, Register &SrcReg,
                                    Register &SrcReg2, int64_t &CmpMask,
                                    int64_t &CmpValue) const {
  return false;
}

bool ZodiacInstrInfo::optimizeCompareInstr(
    MachineInstr &CmpInstr, Register SrcReg, Register SrcReg2,
    int64_t /*CmpMask*/, int64_t CmpValue,
    const MachineRegisterInfo *MRI) const {
  return false;
}

MachineInstr *
ZodiacInstrInfo::optimizeSelect(MachineInstr &MI,
                               SmallPtrSetImpl<MachineInstr *> &SeenMIs,
                               bool /*PreferFalse*/) const {
  return nullptr;
}

bool ZodiacInstrInfo::analyzeBranch(MachineBasicBlock &MBB,
                                   MachineBasicBlock *&TrueBlock,
                                   MachineBasicBlock *&FalseBlock,
                                   SmallVectorImpl<MachineOperand> &Condition,
                                   bool AllowModify) const {
  MachineBasicBlock::iterator I = MBB.end();
  while (I != MBB.begin()) {
    --I;
    if (I->isDebugInstr())
      continue;
    if (!I->isTerminator())
      break;

    if (!I->isBranch())
      return true;

    if (I->isIndirectBranch())
      return true;

    if (I->getOpcode() == Zodiac::B) {
      if (!AllowModify) {
        TrueBlock = I->getOperand(0).getMBB();
        continue;
      }
      MBB.erase(std::next(I), MBB.end());
      Condition.clear();
      FalseBlock = nullptr;

      if (MBB.isLayoutSuccessor(I->getOperand(0).getMBB())) {
        TrueBlock = nullptr;
        I->eraseFromParent();
        I = MBB.end();
        continue;
      }
      TrueBlock = I->getOperand(0).getMBB();
      continue;
    }

    unsigned Opcode = I->getOpcode();
    if (Opcode == Zodiac::BEQ || Opcode == Zodiac::BNE ||
        Opcode == Zodiac::BLT || Opcode == Zodiac::BGE ||
        Opcode == Zodiac::BLTU || Opcode == Zodiac::BGEU) {
      if (!Condition.empty())
        return true;

      if (TrueBlock)
        FalseBlock = TrueBlock;
      TrueBlock = I->getOperand(2).getMBB();
      Condition.push_back(MachineOperand::CreateImm(Opcode));
      Condition.push_back(I->getOperand(0));
      Condition.push_back(I->getOperand(1));
      continue;
    }

    return true;
  }
  return false;
}

unsigned ZodiacInstrInfo::removeBranch(MachineBasicBlock &MBB,
                                       int *BytesRemoved) const {
  assert(!BytesRemoved && "BytesRemoved not supported");
  MachineBasicBlock::iterator I = MBB.end();
  unsigned Count = 0;
  while (I != MBB.begin()) {
    --I;
    if (I->isDebugInstr())
      continue;
    if (!I->isBranch())
      break;
    I->eraseFromParent();
    I = MBB.end();
    ++Count;
  }
  return Count;
}

unsigned ZodiacInstrInfo::insertBranch(MachineBasicBlock &MBB,
                                       MachineBasicBlock *TrueBlock,
                                       MachineBasicBlock *FalseBlock,
                                       ArrayRef<MachineOperand> Condition,
                                       const DebugLoc &DL,
                                       int *BytesAdded) const {
  assert(!BytesAdded && "BytesAdded not supported");

  if (Condition.empty()) {
    assert(TrueBlock && "TrueBlock must be non-null");
    BuildMI(&MBB, DL, get(Zodiac::B)).addMBB(TrueBlock);
    return 1;
  }

  assert(Condition.size() == 3 && "Invalid condition vector!");
  unsigned Opcode = Condition[0].getImm();
  Register Reg1 = Condition[1].getReg();
  Register Reg2 = Condition[2].getReg();

  BuildMI(&MBB, DL, get(Opcode)).addReg(Reg1).addReg(Reg2).addMBB(TrueBlock);

  if (FalseBlock) {
    BuildMI(&MBB, DL, get(Zodiac::B)).addMBB(FalseBlock);
    return 2;
  }
  return 1;
}

bool ZodiacInstrInfo::reverseBranchCondition(
    SmallVectorImpl<MachineOperand> &Condition) const {
  assert(Condition.size() == 3 && "Invalid condition vector!");
  unsigned Opcode = Condition[0].getImm();
  switch (Opcode) {
  case Zodiac::BEQ:  Condition[0].setImm(Zodiac::BNE); break;
  case Zodiac::BNE:  Condition[0].setImm(Zodiac::BEQ); break;
  case Zodiac::BLT:  Condition[0].setImm(Zodiac::BGE); break;
  case Zodiac::BGE:  Condition[0].setImm(Zodiac::BLT); break;
  case Zodiac::BLTU: Condition[0].setImm(Zodiac::BGEU); break;
  case Zodiac::BGEU: Condition[0].setImm(Zodiac::BLTU); break;
  default: return true;
  }
  return false;
}

bool ZodiacInstrInfo::getMemOperandsWithOffsetWidth(
    const MachineInstr &LdSt,
    SmallVectorImpl<const MachineOperand *> &BaseOps, int64_t &Offset,
    bool &OffsetIsScalable, LocationSize &Width,
    const TargetRegisterInfo *TRI) const {
  return false;
}

Register ZodiacInstrInfo::isLoadFromStackSlot(const MachineInstr &MI,
                                             int &FrameIndex) const {
  if (MI.getOpcode() == Zodiac::LW) {
    if (MI.getOperand(1).isFI()) {
      FrameIndex = MI.getOperand(1).getIndex();
      return MI.getOperand(0).getReg();
    }
  }
  return Register();
}

Register ZodiacInstrInfo::isLoadFromStackSlotPostFE(const MachineInstr &MI,
                                                   int &FrameIndex) const {
  return Register();
}

Register ZodiacInstrInfo::isStoreToStackSlot(const MachineInstr &MI,
                                            int &FrameIndex) const {
  if (MI.getOpcode() == Zodiac::SW) {
    if (MI.getOperand(1).isFI()) {
      FrameIndex = MI.getOperand(1).getIndex();
      return MI.getOperand(0).getReg();
    }
  }
  return Register();
}
