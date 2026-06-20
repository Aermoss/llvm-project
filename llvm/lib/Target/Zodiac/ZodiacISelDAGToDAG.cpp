//===-- ZodiacISelDAGToDAG.cpp - A dag to dag inst selector for Zodiac ------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ZodiacTargetMachine.h"
#include "llvm/CodeGen/MachineConstantPool.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "zodiac-isel"
#define PASS_NAME "Zodiac DAG->DAG Pattern Instruction Selection"

namespace {

class ZodiacDAGToDAGISel : public SelectionDAGISel {
public:
  ZodiacDAGToDAGISel() = delete;

  explicit ZodiacDAGToDAGISel(ZodiacTargetMachine &TargetMachine)
      : SelectionDAGISel(TargetMachine) {}

  bool SelectInlineAsmMemoryOperand(const SDValue &Op,
                                    InlineAsm::ConstraintCode ConstraintCode,
                                    std::vector<SDValue> &OutOps) override;

private:
#include "ZodiacGenDAGISel.inc"

  void Select(SDNode *N) override;
  void selectFrameIndex(SDNode *N);

  bool selectAddrRI(SDValue Addr, SDValue &Base, SDValue &Offset);

  inline SDValue getI32Imm(unsigned Imm, const SDLoc &DL) {
    return CurDAG->getTargetConstant(Imm, DL, MVT::i32);
  }
};

class ZodiacDAGToDAGISelLegacy : public SelectionDAGISelLegacy {
public:
  static char ID;
  explicit ZodiacDAGToDAGISelLegacy(ZodiacTargetMachine &TM)
      : SelectionDAGISelLegacy(ID, std::make_unique<ZodiacDAGToDAGISel>(TM)) {}
};

} // namespace

char ZodiacDAGToDAGISelLegacy::ID = 0;

INITIALIZE_PASS(ZodiacDAGToDAGISelLegacy, DEBUG_TYPE, PASS_NAME, false, false)

bool ZodiacDAGToDAGISel::selectAddrRI(SDValue Addr, SDValue &Base, SDValue &Offset) {
  SDLoc DL(Addr);

  if (FrameIndexSDNode *FIN = dyn_cast<FrameIndexSDNode>(Addr)) {
    Base = CurDAG->getTargetFrameIndex(
        FIN->getIndex(),
        getTargetLowering()->getPointerTy(CurDAG->getDataLayout()));
    Offset = CurDAG->getTargetConstant(0, DL, MVT::i32);
    return true;
  }

  if (Addr.getOpcode() == ISD::ADD) {
    if (ConstantSDNode *CN = dyn_cast<ConstantSDNode>(Addr.getOperand(1))) {
      if (isInt<16>(CN->getSExtValue())) {
        if (FrameIndexSDNode *FIN = dyn_cast<FrameIndexSDNode>(Addr.getOperand(0))) {
          Base = CurDAG->getTargetFrameIndex(
              FIN->getIndex(),
              getTargetLowering()->getPointerTy(CurDAG->getDataLayout()));
        } else {
          Base = Addr.getOperand(0);
        }
        Offset = CurDAG->getTargetConstant(CN->getSExtValue() & 0xFFFFFFFF, DL, MVT::i32);
        return true;
      }
    }
  }

  if (ConstantSDNode *CN = dyn_cast<ConstantSDNode>(Addr)) {
    if (isInt<16>(CN->getSExtValue())) {
      Base = CurDAG->getRegister(Zodiac::X0, CN->getValueType(0));
      Offset = CurDAG->getTargetConstant(CN->getSExtValue() & 0xFFFFFFFF, DL, CN->getValueType(0));
      return true;
    }
  }

  Base = Addr;
  Offset = CurDAG->getTargetConstant(0, DL, MVT::i32);
  return true;
}

bool ZodiacDAGToDAGISel::SelectInlineAsmMemoryOperand(
    const SDValue &Op, InlineAsm::ConstraintCode ConstraintCode,
    std::vector<SDValue> &OutOps) {
  SDValue Base, Offset;
  switch (ConstraintCode) {
  default:
    return true;
  case InlineAsm::ConstraintCode::m:
    if (!selectAddrRI(Op, Base, Offset))
      return true;
    break;
  }

  OutOps.push_back(Base);
  OutOps.push_back(Offset);
  return false;
}

void ZodiacDAGToDAGISel::Select(SDNode *Node) {
  unsigned Opcode = Node->getOpcode();

  if (Node->isMachineOpcode()) {
    LLVM_DEBUG(errs() << "== "; Node->dump(CurDAG); errs() << "\n");
    return;
  }

  EVT VT = Node->getValueType(0);
  switch (Opcode) {
  case ISD::Constant:
    if (VT == MVT::i32) {
      ConstantSDNode *ConstNode = cast<ConstantSDNode>(Node);
      if (ConstNode->isZero()) {
        SDValue New = CurDAG->getCopyFromReg(CurDAG->getEntryNode(),
                                             SDLoc(Node), Zodiac::X0, MVT::i32);
        return ReplaceNode(Node, New.getNode());
      }
    }
    break;
  case ISD::FrameIndex:
    selectFrameIndex(Node);
    return;
  case ZodiacISD::SELECT_CC: {
    SDValue TrueVal = Node->getOperand(0);
    SDValue FalseVal = Node->getOperand(1);
    SDValue LHS = Node->getOperand(2);
    SDValue RHS = Node->getOperand(3);
    SDValue CC = Node->getOperand(4);
    SDLoc dl(Node);
    unsigned CCVal = cast<CondCodeSDNode>(CC)->get();
    SDValue TargetCC = CurDAG->getTargetConstant(CCVal, dl, MVT::i32);
    SDValue Ops[] = { TrueVal, FalseVal, LHS, RHS, TargetCC };
    SDNode *NewNode = CurDAG->getMachineNode(Zodiac::SELECT_CC, dl, Node->getValueType(0), Ops);
    ReplaceNode(Node, NewNode);
    return;
  }
  default:
    break;
  }

  SelectCode(Node);
}

void ZodiacDAGToDAGISel::selectFrameIndex(SDNode *Node) {
  SDLoc DL(Node);
  SDValue Imm = CurDAG->getTargetConstant(0, DL, MVT::i32);
  int FI = cast<FrameIndexSDNode>(Node)->getIndex();
  EVT VT = Node->getValueType(0);
  SDValue TFI = CurDAG->getTargetFrameIndex(FI, VT);
  unsigned Opc = Zodiac::ADDI;
  if (Node->hasOneUse()) {
    CurDAG->SelectNodeTo(Node, Opc, VT, TFI, Imm);
    return;
  }
  ReplaceNode(Node, CurDAG->getMachineNode(Opc, DL, VT, TFI, Imm));
}

FunctionPass *llvm::createZodiacISelDag(ZodiacTargetMachine &TM) {
  return new ZodiacDAGToDAGISelLegacy(TM);
}
