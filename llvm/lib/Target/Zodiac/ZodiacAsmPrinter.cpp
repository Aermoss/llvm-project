//===-- ZodiacAsmPrinter.cpp - Zodiac LLVM assembly writer ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ZodiacMCInstLower.h"
#include "ZodiacTargetMachine.h"
#include "MCTargetDesc/ZodiacInstPrinter.h"
#include "TargetInfo/ZodiacTargetInfo.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineConstantPool.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/IR/Mangler.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstBuilder.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetLoweringObjectFile.h"

#define DEBUG_TYPE "asm-printer"

using namespace llvm;

namespace {
class ZodiacAsmPrinter : public AsmPrinter {
public:
  explicit ZodiacAsmPrinter(TargetMachine &TM,
                           std::unique_ptr<MCStreamer> Streamer)
      : AsmPrinter(TM, std::move(Streamer), ID) {}

  StringRef getPassName() const override { return "Zodiac Assembly Printer"; }

  void printOperand(const MachineInstr *MI, int OpNum, raw_ostream &O);
  bool PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
                       const char *ExtraCode, raw_ostream &O) override;
  void emitInstruction(const MachineInstr *MI) override;
  bool isBlockOnlyReachableByFallthrough(
      const MachineBasicBlock *MBB) const override;

  // Suppress all ELF/OS directives for bare-metal output.
  void emitStartOfAsmFile(Module &M) override {} // No .text
  void emitEndOfAsmFile(Module &M) override {}   // No .section ".note.GNU-stack"
  void emitFunctionHeader() override;            // Just the label, no .globl/.p2align/.type
  void emitFunctionBodyEnd() override {}         // No .Lfunc_end / .size

public:
  static char ID;
};
} // end of anonymous namespace

void ZodiacAsmPrinter::printOperand(const MachineInstr *MI, int OpNum,
                                   raw_ostream &O) {
  const MachineOperand &MO = MI->getOperand(OpNum);

  switch (MO.getType()) {
  case MachineOperand::MO_Register:
    O << ZodiacInstPrinter::getRegisterName(MO.getReg());
    break;

  case MachineOperand::MO_Immediate:
    O << MO.getImm();
    break;

  case MachineOperand::MO_MachineBasicBlock:
    O << *MO.getMBB()->getSymbol();
    break;

  case MachineOperand::MO_GlobalAddress:
    O << *getSymbol(MO.getGlobal());
    break;

  case MachineOperand::MO_BlockAddress: {
    MCSymbol *BA = GetBlockAddressSymbol(MO.getBlockAddress());
    O << BA->getName();
    break;
  }

  case MachineOperand::MO_ExternalSymbol:
    O << *GetExternalSymbolSymbol(MO.getSymbolName());
    break;

  case MachineOperand::MO_JumpTableIndex:
    O << MAI.getInternalSymbolPrefix() << "JTI" << getFunctionNumber() << '_'
      << MO.getIndex();
    break;

  case MachineOperand::MO_ConstantPoolIndex:
    O << MAI.getInternalSymbolPrefix() << "CPI" << getFunctionNumber() << '_'
      << MO.getIndex();
    return;

  default:
    llvm_unreachable("<unknown operand type>");
  }
}

bool ZodiacAsmPrinter::PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
                                      const char *ExtraCode, raw_ostream &O) {
  printOperand(MI, OpNo, O);
  return false;
}

void ZodiacAsmPrinter::emitInstruction(const MachineInstr *MI) {
  Zodiac_MC::verifyInstructionPredicates(MI->getOpcode(),
                                        getSubtargetInfo().getFeatureBits());

  MCSubtargetInfo STI = getSubtargetInfo();
  ZodiacMCInstLower MCInstLowering(OutContext, *this);
  MCInst TmpInst;

  switch (MI->getOpcode()) {
  case Zodiac::CALL: {
    // CALL target -> BL X30, target
    MCInst BlInst;
    BlInst.setOpcode(Zodiac::BL);
    BlInst.addOperand(MCOperand::createReg(Zodiac::X30));

    MCInst TmpCall;
    MCInstLowering.Lower(MI, TmpCall);
    BlInst.addOperand(TmpCall.getOperand(0));

    OutStreamer->emitInstruction(BlInst, STI);
    return;
  }
  case Zodiac::CALLR: {
    // CALLR rs1 ->
    //   AUIPC X30, 0
    //   ADDI X30, X30, 8
    //   BR rs1
    Register Reg = MI->getOperand(0).getReg();
    OutStreamer->emitInstruction(MCInstBuilder(Zodiac::AUIPC)
                                     .addReg(Zodiac::X30)
                                     .addImm(0),
                                 STI);
    OutStreamer->emitInstruction(MCInstBuilder(Zodiac::ADDI)
                                     .addReg(Zodiac::X30)
                                     .addReg(Zodiac::X30)
                                     .addImm(8),
                                 STI);
    OutStreamer->emitInstruction(MCInstBuilder(Zodiac::BR)
                                     .addReg(Reg),
                                 STI);
    return;
  }
  case Zodiac::RET: {
    // RET -> BR X30
    OutStreamer->emitInstruction(MCInstBuilder(Zodiac::BR)
                                     .addReg(Zodiac::X30),
                                 STI);
    return;
  }
  default:
    MCInstLowering.Lower(MI, TmpInst);
    OutStreamer->emitInstruction(TmpInst, STI);
    return;
  }
}

void ZodiacAsmPrinter::emitFunctionHeader() {
  // Emit just the bare label — no .globl, no .p2align, no .type.
  // The default emitFunctionHeader emits linkage, alignment, type directives,
  // and section switches — none of which are needed for bare-metal.
  //
  // We must still set the section internally so the MC layer doesn't crash.
  const Function &F = MF->getFunction();
  MF->setSection(getObjFileLowering().SectionForGlobal(&F, TM));
  OutStreamer->switchSection(MF->getSection());

  OutStreamer->emitLabel(CurrentFnSym);
}

bool ZodiacAsmPrinter::isBlockOnlyReachableByFallthrough(
    const MachineBasicBlock *MBB) const {
  const MachineBasicBlock *Pred = *MBB->pred_begin();
  if (const BasicBlock *B = Pred->getBasicBlock())
    if (isa<SwitchInst>(B->getTerminator()))
      return false;

  if (!AsmPrinter::isBlockOnlyReachableByFallthrough(MBB))
    return false;

  MachineBasicBlock::const_iterator I = Pred->end();
  while (I != Pred->begin() && !(--I)->isTerminator()) {
  }

  return !I->isBarrier();
}

char ZodiacAsmPrinter::ID = 0;

INITIALIZE_PASS(ZodiacAsmPrinter, "zodiac-asm-printer", "Zodiac Assembly Printer",
                false, false)

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeZodiacAsmPrinter() {
  RegisterAsmPrinter<ZodiacAsmPrinter> X(getTheZodiacTarget());
}
