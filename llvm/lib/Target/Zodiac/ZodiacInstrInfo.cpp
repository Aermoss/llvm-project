//===-- ZodiacInstrInfo.cpp - Zodiac Instruction Information ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the Zodiac implementation of the TargetInstrInfo class.
//
//===----------------------------------------------------------------------===//

#include "ZodiacInstrInfo.h"
#include "ZodiacAluCode.h"
#include "ZodiacCondCode.h"
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

  BuildMI(MBB, Position, DL, get(Zodiac::OR_I_LO), DestinationRegister)
      .addReg(SourceRegister, getKillRegState(KillSource))
      .addImm(0);
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
  BuildMI(MBB, Position, DL, get(Zodiac::SW_RI))
      .addReg(SourceRegister, getKillRegState(IsKill))
      .addFrameIndex(FrameIndex)
      .addImm(0)
      .addImm(LPAC::ADD);
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
  BuildMI(MBB, Position, DL, get(Zodiac::LDW_RI), DestinationRegister)
      .addFrameIndex(FrameIndex)
      .addImm(0)
      .addImm(LPAC::ADD);
}

bool ZodiacInstrInfo::areMemAccessesTriviallyDisjoint(
    const MachineInstr &MIa, const MachineInstr &MIb) const {
  assert(MIa.mayLoadOrStore() && "MIa must be a load or store.");
  assert(MIb.mayLoadOrStore() && "MIb must be a load or store.");

  if (MIa.hasUnmodeledSideEffects() || MIb.hasUnmodeledSideEffects() ||
      MIa.hasOrderedMemoryRef() || MIb.hasOrderedMemoryRef())
    return false;

  // Retrieve the base register, offset from the base register and width. Width
  // is the size of memory that is being loaded/stored (e.g. 1, 2, 4).  If
  // base registers are identical, and the offset of a lower memory access +
  // the width doesn't overlap the offset of a higher memory access,
  // then the memory accesses are different.
  const TargetRegisterInfo *TRI = &getRegisterInfo();
  const MachineOperand *BaseOpA = nullptr, *BaseOpB = nullptr;
  int64_t OffsetA = 0, OffsetB = 0;
  LocationSize WidthA = LocationSize::precise(0),
               WidthB = LocationSize::precise(0);
  if (getMemOperandWithOffsetWidth(MIa, BaseOpA, OffsetA, WidthA, TRI) &&
      getMemOperandWithOffsetWidth(MIb, BaseOpB, OffsetB, WidthB, TRI)) {
    if (BaseOpA->isIdenticalTo(*BaseOpB)) {
      int LowOffset = std::min(OffsetA, OffsetB);
      int HighOffset = std::max(OffsetA, OffsetB);
      LocationSize LowWidth = (LowOffset == OffsetA) ? WidthA : WidthB;
      if (LowWidth.hasValue() &&
          LowOffset + (int)LowWidth.getValue() <= HighOffset)
        return true;
    }
  }
  return false;
}

bool ZodiacInstrInfo::expandPostRAPseudo(MachineInstr & /*MI*/) const {
  return false;
}

static LPCC::CondCode getOppositeCondition(LPCC::CondCode CC) {
  switch (CC) {
  case LPCC::ICC_T: //  true
    return LPCC::ICC_F;
  case LPCC::ICC_F: //  false
    return LPCC::ICC_T;
  case LPCC::ICC_HI: //  high
    return LPCC::ICC_LS;
  case LPCC::ICC_LS: //  low or same
    return LPCC::ICC_HI;
  case LPCC::ICC_CC: //  carry cleared
    return LPCC::ICC_CS;
  case LPCC::ICC_CS: //  carry set
    return LPCC::ICC_CC;
  case LPCC::ICC_NE: //  not equal
    return LPCC::ICC_EQ;
  case LPCC::ICC_EQ: //  equal
    return LPCC::ICC_NE;
  case LPCC::ICC_VC: //  oVerflow cleared
    return LPCC::ICC_VS;
  case LPCC::ICC_VS: //  oVerflow set
    return LPCC::ICC_VC;
  case LPCC::ICC_PL: //  plus (note: 0 is "minus" too here)
    return LPCC::ICC_MI;
  case LPCC::ICC_MI: //  minus
    return LPCC::ICC_PL;
  case LPCC::ICC_GE: //  greater than or equal
    return LPCC::ICC_LT;
  case LPCC::ICC_LT: //  less than
    return LPCC::ICC_GE;
  case LPCC::ICC_GT: //  greater than
    return LPCC::ICC_LE;
  case LPCC::ICC_LE: //  less than or equal
    return LPCC::ICC_GT;
  default:
    llvm_unreachable("Invalid condtional code");
  }
}

std::pair<unsigned, unsigned>
ZodiacInstrInfo::decomposeMachineOperandsTargetFlags(unsigned TF) const {
  return std::make_pair(TF, 0u);
}

ArrayRef<std::pair<unsigned, const char *>>
ZodiacInstrInfo::getSerializableDirectMachineOperandTargetFlags() const {
  using namespace ZodiacII;
  static const std::pair<unsigned, const char *> TargetFlags[] = {
      {MO_ABS_HI, "zodiac-hi"},
      {MO_ABS_LO, "zodiac-lo"},
      {MO_NO_FLAG, "zodiac-nf"}};
  return ArrayRef(TargetFlags);
}

bool ZodiacInstrInfo::analyzeCompare(const MachineInstr &MI, Register &SrcReg,
                                    Register &SrcReg2, int64_t &CmpMask,
                                    int64_t &CmpValue) const {
  switch (MI.getOpcode()) {
  default:
    break;
  case Zodiac::SFSUB_F_RI_LO:
  case Zodiac::SFSUB_F_RI_HI:
    SrcReg = MI.getOperand(0).getReg();
    SrcReg2 = Register();
    CmpMask = ~0;
    CmpValue = MI.getOperand(1).getImm();
    return true;
  case Zodiac::SFSUB_F_RR:
    SrcReg = MI.getOperand(0).getReg();
    SrcReg2 = MI.getOperand(1).getReg();
    CmpMask = ~0;
    CmpValue = 0;
    return true;
  }

  return false;
}

// isRedundantFlagInstr - check whether the first instruction, whose only
// purpose is to update flags, can be made redundant.
// * SFSUB_F_RR can be made redundant by SUB_RI if the operands are the same.
// * SFSUB_F_RI can be made redundant by SUB_I if the operands are the same.
inline static bool isRedundantFlagInstr(MachineInstr *CmpI, unsigned SrcReg,
                                        unsigned SrcReg2, int64_t ImmValue,
                                        MachineInstr *OI) {
  if (CmpI->getOpcode() == Zodiac::SFSUB_F_RR &&
      OI->getOpcode() == Zodiac::SUB_R &&
      ((OI->getOperand(1).getReg() == SrcReg &&
        OI->getOperand(2).getReg() == SrcReg2) ||
       (OI->getOperand(1).getReg() == SrcReg2 &&
        OI->getOperand(2).getReg() == SrcReg)))
    return true;

  if (((CmpI->getOpcode() == Zodiac::SFSUB_F_RI_LO &&
        OI->getOpcode() == Zodiac::SUB_I_LO) ||
       (CmpI->getOpcode() == Zodiac::SFSUB_F_RI_HI &&
        OI->getOpcode() == Zodiac::SUB_I_HI)) &&
      OI->getOperand(1).getReg() == SrcReg &&
      OI->getOperand(2).getImm() == ImmValue)
    return true;
  return false;
}

inline static unsigned flagSettingOpcodeVariant(unsigned OldOpcode) {
  switch (OldOpcode) {
  case Zodiac::ADD_I_HI:
    return Zodiac::ADD_F_I_HI;
  case Zodiac::ADD_I_LO:
    return Zodiac::ADD_F_I_LO;
  case Zodiac::ADD_R:
    return Zodiac::ADD_F_R;
  case Zodiac::ADDC_I_HI:
    return Zodiac::ADDC_F_I_HI;
  case Zodiac::ADDC_I_LO:
    return Zodiac::ADDC_F_I_LO;
  case Zodiac::ADDC_R:
    return Zodiac::ADDC_F_R;
  case Zodiac::AND_I_HI:
    return Zodiac::AND_F_I_HI;
  case Zodiac::AND_I_LO:
    return Zodiac::AND_F_I_LO;
  case Zodiac::AND_R:
    return Zodiac::AND_F_R;
  case Zodiac::OR_I_HI:
    return Zodiac::OR_F_I_HI;
  case Zodiac::OR_I_LO:
    return Zodiac::OR_F_I_LO;
  case Zodiac::OR_R:
    return Zodiac::OR_F_R;
  case Zodiac::SL_I:
    return Zodiac::SL_F_I;
  case Zodiac::SRL_R:
    return Zodiac::SRL_F_R;
  case Zodiac::SA_I:
    return Zodiac::SA_F_I;
  case Zodiac::SRA_R:
    return Zodiac::SRA_F_R;
  case Zodiac::SUB_I_HI:
    return Zodiac::SUB_F_I_HI;
  case Zodiac::SUB_I_LO:
    return Zodiac::SUB_F_I_LO;
  case Zodiac::SUB_R:
    return Zodiac::SUB_F_R;
  case Zodiac::SUBB_I_HI:
    return Zodiac::SUBB_F_I_HI;
  case Zodiac::SUBB_I_LO:
    return Zodiac::SUBB_F_I_LO;
  case Zodiac::SUBB_R:
    return Zodiac::SUBB_F_R;
  case Zodiac::XOR_I_HI:
    return Zodiac::XOR_F_I_HI;
  case Zodiac::XOR_I_LO:
    return Zodiac::XOR_F_I_LO;
  case Zodiac::XOR_R:
    return Zodiac::XOR_F_R;
  default:
    return Zodiac::NOP;
  }
}

bool ZodiacInstrInfo::optimizeCompareInstr(
    MachineInstr &CmpInstr, Register SrcReg, Register SrcReg2,
    int64_t /*CmpMask*/, int64_t CmpValue,
    const MachineRegisterInfo *MRI) const {
  // Get the unique definition of SrcReg.
  MachineInstr *MI = MRI->getUniqueVRegDef(SrcReg);
  if (!MI)
    return false;

  // Get ready to iterate backward from CmpInstr.
  MachineBasicBlock::iterator I = CmpInstr, E = MI,
                              B = CmpInstr.getParent()->begin();

  // Early exit if CmpInstr is at the beginning of the BB.
  if (I == B)
    return false;

  // There are two possible candidates which can be changed to set SR:
  // One is MI, the other is a SUB instruction.
  // * For SFSUB_F_RR(r1,r2), we are looking for SUB(r1,r2) or SUB(r2,r1).
  // * For SFSUB_F_RI(r1, CmpValue), we are looking for SUB(r1, CmpValue).
  MachineInstr *Sub = nullptr;
  if (SrcReg2 != 0)
    // MI is not a candidate to transform into a flag setting instruction.
    MI = nullptr;
  else if (MI->getParent() != CmpInstr.getParent() || CmpValue != 0) {
    // Conservatively refuse to convert an instruction which isn't in the same
    // BB as the comparison. Don't return if SFSUB_F_RI and CmpValue != 0 as Sub
    // may still be a candidate.
    if (CmpInstr.getOpcode() == Zodiac::SFSUB_F_RI_LO)
      MI = nullptr;
    else
      return false;
  }

  // Check that SR isn't set between the comparison instruction and the
  // instruction we want to change while searching for Sub.
  const TargetRegisterInfo *TRI = &getRegisterInfo();
  for (--I; I != E; --I) {
    const MachineInstr &Instr = *I;

    if (Instr.modifiesRegister(Zodiac::SR, TRI) ||
        Instr.readsRegister(Zodiac::SR, TRI))
      // This instruction modifies or uses SR after the one we want to change.
      // We can't do this transformation.
      return false;

    // Check whether CmpInstr can be made redundant by the current instruction.
    if (isRedundantFlagInstr(&CmpInstr, SrcReg, SrcReg2, CmpValue, &*I)) {
      Sub = &*I;
      break;
    }

    // Don't search outside the containing basic block.
    if (I == B)
      return false;
  }

  // Return false if no candidates exist.
  if (!MI && !Sub)
    return false;

  // The single candidate is called MI.
  if (!MI)
    MI = Sub;

  if (flagSettingOpcodeVariant(MI->getOpcode()) != Zodiac::NOP) {
    bool isSafe = false;

    SmallVector<std::pair<MachineOperand *, LPCC::CondCode>, 4>
        OperandsToUpdate;
    I = CmpInstr;
    E = CmpInstr.getParent()->end();
    while (!isSafe && ++I != E) {
      const MachineInstr &Instr = *I;
      for (unsigned IO = 0, EO = Instr.getNumOperands(); !isSafe && IO != EO;
           ++IO) {
        const MachineOperand &MO = Instr.getOperand(IO);
        if (MO.isRegMask() && MO.clobbersPhysReg(Zodiac::SR)) {
          isSafe = true;
          break;
        }
        if (!MO.isReg() || MO.getReg() != Zodiac::SR)
          continue;
        if (MO.isDef()) {
          isSafe = true;
          break;
        }
        // Condition code is after the operand before SR.
        LPCC::CondCode CC;
        CC = (LPCC::CondCode)Instr.getOperand(IO - 1).getImm();

        if (Sub) {
          LPCC::CondCode NewCC = getOppositeCondition(CC);
          if (NewCC == LPCC::ICC_T)
            return false;
          // If we have SUB(r1, r2) and CMP(r2, r1), the condition code based on
          // CMP needs to be updated to be based on SUB.  Push the condition
          // code operands to OperandsToUpdate.  If it is safe to remove
          // CmpInstr, the condition code of these operands will be modified.
          if (SrcReg2 != 0 && Sub->getOperand(1).getReg() == SrcReg2 &&
              Sub->getOperand(2).getReg() == SrcReg) {
            OperandsToUpdate.push_back(
                std::make_pair(&((*I).getOperand(IO - 1)), NewCC));
          }
        } else {
          // No Sub, so this is x = <op> y, z; cmp x, 0.
          switch (CC) {
          case LPCC::ICC_EQ: // Z
          case LPCC::ICC_NE: // Z
          case LPCC::ICC_MI: // N
          case LPCC::ICC_PL: // N
          case LPCC::ICC_F:  // none
          case LPCC::ICC_T:  // none
            // SR can be used multiple times, we should continue.
            break;
          case LPCC::ICC_CS: // C
          case LPCC::ICC_CC: // C
          case LPCC::ICC_VS: // V
          case LPCC::ICC_VC: // V
          case LPCC::ICC_HI: // C Z
          case LPCC::ICC_LS: // C Z
          case LPCC::ICC_GE: // N V
          case LPCC::ICC_LT: // N V
          case LPCC::ICC_GT: // Z N V
          case LPCC::ICC_LE: // Z N V
            // The instruction uses the V bit or C bit which is not safe.
            return false;
          case LPCC::UNKNOWN:
            return false;
          }
        }
      }
    }

    // If SR is not killed nor re-defined, we should check whether it is
    // live-out. If it is live-out, do not optimize.
    if (!isSafe) {
      MachineBasicBlock *MBB = CmpInstr.getParent();
      for (const MachineBasicBlock *Succ : MBB->successors())
        if (Succ->isLiveIn(Zodiac::SR))
          return false;
    }

    // Toggle the optional operand to SR.
    MI->setDesc(get(flagSettingOpcodeVariant(MI->getOpcode())));
    MI->addRegisterDefined(Zodiac::SR);
    CmpInstr.eraseFromParent();
    return true;
  }

  return false;
}

// Identify instructions that can be folded into a SELECT instruction, and
// return the defining instruction.
static MachineInstr *canFoldIntoSelect(Register Reg,
                                       const MachineRegisterInfo &MRI) {
  if (!Reg.isVirtual())
    return nullptr;
  if (!MRI.hasOneNonDBGUse(Reg))
    return nullptr;
  MachineInstr *MI = MRI.getVRegDef(Reg);
  if (!MI)
    return nullptr;
  // MI is folded into the SELECT by predicating it.
  if (!MI->isPredicable())
    return nullptr;
  // Check if MI has any non-dead defs or physreg uses. This also detects
  // predicated instructions which will be reading SR.
  for (const MachineOperand &MO : llvm::drop_begin(MI->operands(), 1)) {
    // Reject frame index operands.
    if (MO.isFI() || MO.isCPI() || MO.isJTI())
      return nullptr;
    if (!MO.isReg())
      continue;
    // MI can't have any tied operands, that would conflict with predication.
    if (MO.isTied())
      return nullptr;
    if (MO.getReg().isPhysical())
      return nullptr;
    if (MO.isDef() && !MO.isDead())
      return nullptr;
  }
  bool DontMoveAcrossStores = true;
  if (!MI->isSafeToMove(DontMoveAcrossStores))
    return nullptr;
  return MI;
}

MachineInstr *
ZodiacInstrInfo::optimizeSelect(MachineInstr &MI,
                               SmallPtrSetImpl<MachineInstr *> &SeenMIs,
                               bool /*PreferFalse*/) const {
  assert(MI.getOpcode() == Zodiac::SELECT && "unknown select instruction");
  MachineRegisterInfo &MRI = MI.getParent()->getParent()->getRegInfo();
  MachineInstr *DefMI = canFoldIntoSelect(MI.getOperand(1).getReg(), MRI);
  bool Invert = !DefMI;
  if (!DefMI)
    DefMI = canFoldIntoSelect(MI.getOperand(2).getReg(), MRI);
  if (!DefMI)
    return nullptr;

  // Find new register class to use.
  MachineOperand FalseReg = MI.getOperand(Invert ? 1 : 2);
  Register DestReg = MI.getOperand(0).getReg();
  const TargetRegisterClass *PreviousClass = MRI.getRegClass(FalseReg.getReg());
  if (!MRI.constrainRegClass(DestReg, PreviousClass))
    return nullptr;

  // Create a new predicated version of DefMI.
  MachineInstrBuilder NewMI =
      BuildMI(*MI.getParent(), MI, MI.getDebugLoc(), DefMI->getDesc(), DestReg);

  // Copy all the DefMI operands, excluding its (null) predicate.
  const MCInstrDesc &DefDesc = DefMI->getDesc();
  for (unsigned i = 1, e = DefDesc.getNumOperands();
       i != e && !DefDesc.operands()[i].isPredicate(); ++i)
    NewMI.add(DefMI->getOperand(i));

  unsigned CondCode = MI.getOperand(3).getImm();
  if (Invert)
    NewMI.addImm(getOppositeCondition(LPCC::CondCode(CondCode)));
  else
    NewMI.addImm(CondCode);
  NewMI.copyImplicitOps(MI);

  // The output register value when the predicate is false is an implicit
  // register operand tied to the first def.  The tie makes the register
  // allocator ensure the FalseReg is allocated the same register as operand 0.
  FalseReg.setImplicit();
  NewMI.add(FalseReg);
  NewMI->tieOperands(0, NewMI->getNumOperands() - 1);

  // Update SeenMIs set: register newly created MI and erase removed DefMI.
  SeenMIs.insert(NewMI);
  SeenMIs.erase(DefMI);

  // If MI is inside a loop, and DefMI is outside the loop, then kill flags on
  // DefMI would be invalid when transferred inside the loop.  Checking for a
  // loop is expensive, but at least remove kill flags if they are in different
  // BBs.
  if (DefMI->getParent() != MI.getParent())
    NewMI->clearKillInfo();

  // The caller will erase MI, but not DefMI.
  DefMI->eraseFromParent();
  return NewMI;
}

// The analyzeBranch function is used to examine conditional instructions and
// remove unnecessary instructions. This method is used by BranchFolder and
// IfConverter machine function passes to improve the CFG.
// - TrueBlock is set to the destination if condition evaluates true (it is the
//   nullptr if the destination is the fall-through branch);
// - FalseBlock is set to the destination if condition evaluates to false (it
//   is the nullptr if the branch is unconditional);
// - condition is populated with machine operands needed to generate the branch
//   to insert in insertBranch;
// Returns: false if branch could successfully be analyzed.
bool ZodiacInstrInfo::analyzeBranch(MachineBasicBlock &MBB,
                                   MachineBasicBlock *&TrueBlock,
                                   MachineBasicBlock *&FalseBlock,
                                   SmallVectorImpl<MachineOperand> &Condition,
                                   bool AllowModify) const {
  // Iterator to current instruction being considered.
  MachineBasicBlock::iterator Instruction = MBB.end();

  // Start from the bottom of the block and work up, examining the
  // terminator instructions.
  while (Instruction != MBB.begin()) {
    --Instruction;

    // Skip over debug instructions.
    if (Instruction->isDebugInstr())
      continue;

    // Working from the bottom, when we see a non-terminator
    // instruction, we're done.
    if (!isUnpredicatedTerminator(*Instruction))
      break;

    // A terminator that isn't a branch can't easily be handled
    // by this analysis.
    if (!Instruction->isBranch())
      return true;

    // Handle unconditional branches.
    if (Instruction->getOpcode() == Zodiac::BT) {
      if (!AllowModify) {
        TrueBlock = Instruction->getOperand(0).getMBB();
        continue;
      }

      // If the block has any instructions after a branch, delete them.
      MBB.erase(std::next(Instruction), MBB.end());

      Condition.clear();
      FalseBlock = nullptr;

      // Delete the jump if it's equivalent to a fall-through.
      if (MBB.isLayoutSuccessor(Instruction->getOperand(0).getMBB())) {
        TrueBlock = nullptr;
        Instruction->eraseFromParent();
        Instruction = MBB.end();
        continue;
      }

      // TrueBlock is used to indicate the unconditional destination.
      TrueBlock = Instruction->getOperand(0).getMBB();
      continue;
    }

    // Handle conditional branches
    unsigned Opcode = Instruction->getOpcode();
    if (Opcode != Zodiac::BRCC)
      return true; // Unknown opcode.

    // Multiple conditional branches are not handled here so only proceed if
    // there are no conditions enqueued.
    if (Condition.empty()) {
      LPCC::CondCode BranchCond =
          static_cast<LPCC::CondCode>(Instruction->getOperand(1).getImm());

      // TrueBlock is the target of the previously seen unconditional branch.
      FalseBlock = TrueBlock;
      TrueBlock = Instruction->getOperand(0).getMBB();
      Condition.push_back(MachineOperand::CreateImm(BranchCond));
      continue;
    }

    // Multiple conditional branches are not handled.
    return true;
  }

  // Return false indicating branch successfully analyzed.
  return false;
}

// reverseBranchCondition - Reverses the branch condition of the specified
// condition list, returning false on success and true if it cannot be
// reversed.
bool ZodiacInstrInfo::reverseBranchCondition(
    SmallVectorImpl<llvm::MachineOperand> &Condition) const {
  assert((Condition.size() == 1) &&
         "Zodiac branch conditions should have one component.");

  LPCC::CondCode BranchCond =
      static_cast<LPCC::CondCode>(Condition[0].getImm());
  Condition[0].setImm(getOppositeCondition(BranchCond));
  return false;
}

// Insert the branch with condition specified in condition and given targets
// (TrueBlock and FalseBlock). This function returns the number of machine
// instructions inserted.
unsigned ZodiacInstrInfo::insertBranch(MachineBasicBlock &MBB,
                                      MachineBasicBlock *TrueBlock,
                                      MachineBasicBlock *FalseBlock,
                                      ArrayRef<MachineOperand> Condition,
                                      const DebugLoc &DL,
                                      int *BytesAdded) const {
  // Shouldn't be a fall through.
  assert(TrueBlock && "insertBranch must not be told to insert a fallthrough");
  assert(!BytesAdded && "code size not handled");

  // If condition is empty then an unconditional branch is being inserted.
  if (Condition.empty()) {
    assert(!FalseBlock && "Unconditional branch with multiple successors!");
    BuildMI(&MBB, DL, get(Zodiac::BT)).addMBB(TrueBlock);
    return 1;
  }

  // Else a conditional branch is inserted.
  assert((Condition.size() == 1) &&
         "Zodiac branch conditions should have one component.");
  unsigned ConditionalCode = Condition[0].getImm();
  BuildMI(&MBB, DL, get(Zodiac::BRCC)).addMBB(TrueBlock).addImm(ConditionalCode);

  // If no false block, then false behavior is fall through and no branch needs
  // to be inserted.
  if (!FalseBlock)
    return 1;

  BuildMI(&MBB, DL, get(Zodiac::BT)).addMBB(FalseBlock);
  return 2;
}

unsigned ZodiacInstrInfo::removeBranch(MachineBasicBlock &MBB,
                                      int *BytesRemoved) const {
  assert(!BytesRemoved && "code size not handled");

  MachineBasicBlock::iterator Instruction = MBB.end();
  unsigned Count = 0;

  while (Instruction != MBB.begin()) {
    --Instruction;
    if (Instruction->isDebugInstr())
      continue;
    if (Instruction->getOpcode() != Zodiac::BT &&
        Instruction->getOpcode() != Zodiac::BRCC) {
      break;
    }

    // Remove the branch.
    Instruction->eraseFromParent();
    Instruction = MBB.end();
    ++Count;
  }

  return Count;
}

Register ZodiacInstrInfo::isLoadFromStackSlot(const MachineInstr &MI,
                                             int &FrameIndex) const {
  if (MI.getOpcode() == Zodiac::LDW_RI)
    if (MI.getOperand(1).isFI() && MI.getOperand(2).isImm() &&
        MI.getOperand(2).getImm() == 0) {
      FrameIndex = MI.getOperand(1).getIndex();
      return MI.getOperand(0).getReg();
    }
  return 0;
}

Register ZodiacInstrInfo::isLoadFromStackSlotPostFE(const MachineInstr &MI,
                                                   int &FrameIndex) const {
  if (MI.getOpcode() == Zodiac::LDW_RI) {
    unsigned Reg;
    if ((Reg = isLoadFromStackSlot(MI, FrameIndex)))
      return Reg;
    // Check for post-frame index elimination operations
    SmallVector<const MachineMemOperand *, 1> Accesses;
    if (hasLoadFromStackSlot(MI, Accesses)){
      FrameIndex =
          cast<FixedStackPseudoSourceValue>(Accesses.front()->getPseudoValue())
              ->getFrameIndex();
      return 1;
    }
  }
  return 0;
}

Register ZodiacInstrInfo::isStoreToStackSlot(const MachineInstr &MI,
                                            int &FrameIndex) const {
  if (MI.getOpcode() == Zodiac::SW_RI)
    if (MI.getOperand(0).isFI() && MI.getOperand(1).isImm() &&
        MI.getOperand(1).getImm() == 0) {
      FrameIndex = MI.getOperand(0).getIndex();
      return MI.getOperand(2).getReg();
    }
  return 0;
}

bool ZodiacInstrInfo::getMemOperandWithOffsetWidth(
    const MachineInstr &LdSt, const MachineOperand *&BaseOp, int64_t &Offset,
    LocationSize &Width, const TargetRegisterInfo * /*TRI*/) const {
  // Handle only loads/stores with base register followed by immediate offset
  // and with add as ALU op.
  if (LdSt.getNumOperands() != 4)
    return false;
  if (!LdSt.getOperand(1).isReg() || !LdSt.getOperand(2).isImm() ||
      !(LdSt.getOperand(3).isImm() && LdSt.getOperand(3).getImm() == LPAC::ADD))
    return false;

  switch (LdSt.getOpcode()) {
  default:
    return false;
  case Zodiac::LDW_RI:
  case Zodiac::LDW_RR:
  case Zodiac::SW_RR:
  case Zodiac::SW_RI:
    Width = LocationSize::precise(4);
    break;
  case Zodiac::LDHs_RI:
  case Zodiac::LDHz_RI:
  case Zodiac::STH_RI:
    Width = LocationSize::precise(2);
    break;
  case Zodiac::LDBs_RI:
  case Zodiac::LDBz_RI:
  case Zodiac::STB_RI:
    Width = LocationSize::precise(1);
    break;
  }

  BaseOp = &LdSt.getOperand(1);
  Offset = LdSt.getOperand(2).getImm();

  if (!BaseOp->isReg())
    return false;

  return true;
}

bool ZodiacInstrInfo::getMemOperandsWithOffsetWidth(
    const MachineInstr &LdSt, SmallVectorImpl<const MachineOperand *> &BaseOps,
    int64_t &Offset, bool &OffsetIsScalable, LocationSize &Width,
    const TargetRegisterInfo *TRI) const {
  switch (LdSt.getOpcode()) {
  default:
    return false;
  case Zodiac::LDW_RI:
  case Zodiac::LDW_RR:
  case Zodiac::SW_RR:
  case Zodiac::SW_RI:
  case Zodiac::LDHs_RI:
  case Zodiac::LDHz_RI:
  case Zodiac::STH_RI:
  case Zodiac::LDBs_RI:
  case Zodiac::LDBz_RI:
    const MachineOperand *BaseOp;
    OffsetIsScalable = false;
    if (!getMemOperandWithOffsetWidth(LdSt, BaseOp, Offset, Width, TRI))
      return false;
    BaseOps.push_back(BaseOp);
    return true;
  }
}
