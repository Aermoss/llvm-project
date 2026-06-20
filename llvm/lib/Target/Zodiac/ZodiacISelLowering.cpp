//===-- ZodiacISelLowering.cpp - Zodiac DAG Lowering Implementation ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ZodiacISelLowering.h"
#include "ZodiacMachineFunctionInfo.h"
#include "ZodiacSubtarget.h"
#include "MCTargetDesc/ZodiacBaseInfo.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineMemOperand.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/CodeGen/SelectionDAGNodes.h"
#include "llvm/CodeGen/TargetCallingConv.h"
#include "llvm/CodeGen/ValueTypes.h"
#include "llvm/CodeGenTypes/MachineValueType.h"
#include "llvm/IR/CallingConv.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/CodeGen.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/KnownBits.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "zodiac-lower"

ZodiacTargetLowering::ZodiacTargetLowering(const TargetMachine &TM,
                                         const ZodiacSubtarget &STI)
    : TargetLowering(TM, STI), Subtarget(STI), TRI(STI.getRegisterInfo()) {
  addRegisterClass(MVT::i32, &Zodiac::GPRRegClass);

  setStackPointerRegisterToSaveRestore(Zodiac::X31);

  setBooleanContents(ZeroOrOneBooleanContent);

  for (auto Op : {ISD::GlobalAddress, ISD::BlockAddress, ISD::ConstantPool,
                  ISD::JumpTable, ISD::BR_CC, ISD::VASTART}) {
    setOperationAction(Op, MVT::i32, Custom);
  }

  for (auto Op : {ISD::SELECT, ISD::STACKSAVE, ISD::STACKRESTORE}) {
    setOperationAction(Op, MVT::i32, Expand);
  }
  setOperationAction(ISD::SELECT_CC, MVT::i32, Custom);

  // Custom lower SETCC to handle all condition codes using SLT/SLTU/SLTI/SLTIU
  setOperationAction(ISD::SETCC, MVT::i32, Custom);

  // Hardware supports multiplication/division
  for (auto Op : {ISD::MUL, ISD::MULHS, ISD::MULHU, ISD::SDIV, ISD::UDIV,
                  ISD::SREM, ISD::UREM}) {
    setOperationAction(Op, MVT::i32, Legal);
  }

  setMinFunctionAlignment(Align(4));
  computeRegisterProperties(TRI);
}

#include "ZodiacGenCallingConv.inc"

SDValue ZodiacTargetLowering::LowerOperation(SDValue Op, SelectionDAG &DAG) const {
  switch (Op.getOpcode()) {
  case ISD::GlobalAddress:
    return LowerGlobalAddress(Op, DAG);
  case ISD::BlockAddress:
    return LowerBlockAddress(Op, DAG);
  case ISD::ConstantPool:
    return LowerConstantPool(Op, DAG);
  case ISD::JumpTable:
    return LowerJumpTable(Op, DAG);
  case ISD::BR_CC:
    return LowerBR_CC(Op, DAG);
  case ISD::VASTART:
    return LowerVASTART(Op, DAG);
  case ISD::SETCC:
    return LowerSETCC(Op, DAG);
  case ISD::SELECT_CC:
    return LowerSELECT_CC(Op, DAG);
  default:
    llvm_unreachable("unimplemented operand lowering");
  }
}

SDValue ZodiacTargetLowering::LowerGlobalAddress(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  const GlobalValue *GV = cast<GlobalAddressSDNode>(Op)->getGlobal();
  int64_t Offset = cast<GlobalAddressSDNode>(Op)->getOffset();

  SDValue GAHi = DAG.getTargetGlobalAddress(GV, DL, MVT::i32, Offset, ZodiacII::MO_HI21);
  SDValue GALo = DAG.getTargetGlobalAddress(GV, DL, MVT::i32, Offset, ZodiacII::MO_LO11);

  SDValue LUI = SDValue(DAG.getMachineNode(Zodiac::LUI, DL, MVT::i32, GAHi), 0);
  return SDValue(DAG.getMachineNode(Zodiac::ORI, DL, MVT::i32, LUI, GALo), 0);
}

SDValue ZodiacTargetLowering::LowerBlockAddress(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  const BlockAddress *BA = cast<BlockAddressSDNode>(Op)->getBlockAddress();
  int64_t Offset = cast<BlockAddressSDNode>(Op)->getOffset();

  SDValue GAHi = DAG.getTargetBlockAddress(BA, MVT::i32, Offset, ZodiacII::MO_HI21);
  SDValue GALo = DAG.getTargetBlockAddress(BA, MVT::i32, Offset, ZodiacII::MO_LO11);

  SDValue LUI = SDValue(DAG.getMachineNode(Zodiac::LUI, DL, MVT::i32, GAHi), 0);
  return SDValue(DAG.getMachineNode(Zodiac::ORI, DL, MVT::i32, LUI, GALo), 0);
}

SDValue ZodiacTargetLowering::LowerConstantPool(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  ConstantPoolSDNode *CP = cast<ConstantPoolSDNode>(Op);

  SDValue GAHi = DAG.getTargetConstantPool(CP->getConstVal(), MVT::i32, CP->getAlign(), CP->getOffset(), ZodiacII::MO_HI21);
  SDValue GALo = DAG.getTargetConstantPool(CP->getConstVal(), MVT::i32, CP->getAlign(), CP->getOffset(), ZodiacII::MO_LO11);

  SDValue LUI = SDValue(DAG.getMachineNode(Zodiac::LUI, DL, MVT::i32, GAHi), 0);
  return SDValue(DAG.getMachineNode(Zodiac::ORI, DL, MVT::i32, LUI, GALo), 0);
}

SDValue ZodiacTargetLowering::LowerJumpTable(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  JumpTableSDNode *JT = cast<JumpTableSDNode>(Op);

  SDValue GAHi = DAG.getTargetJumpTable(JT->getIndex(), MVT::i32, ZodiacII::MO_HI21);
  SDValue GALo = DAG.getTargetJumpTable(JT->getIndex(), MVT::i32, ZodiacII::MO_LO11);

  SDValue LUI = SDValue(DAG.getMachineNode(Zodiac::LUI, DL, MVT::i32, GAHi), 0);
  return SDValue(DAG.getMachineNode(Zodiac::ORI, DL, MVT::i32, LUI, GALo), 0);
}

SDValue ZodiacTargetLowering::LowerBR_CC(SDValue Op, SelectionDAG &DAG) const {
  SDValue Chain = Op.getOperand(0);
  SDValue Cond = Op.getOperand(1);
  SDValue LHS = Op.getOperand(2);
  SDValue RHS = Op.getOperand(3);
  SDValue Dest = Op.getOperand(4);
  return DAG.getNode(ZodiacISD::BR_CC, SDLoc(Op), MVT::Other, Chain, Cond, LHS, RHS, Dest);
}

SDValue ZodiacTargetLowering::LowerVASTART(SDValue Op, SelectionDAG &DAG) const {
  MachineFunction &MF = DAG.getMachineFunction();
  ZodiacMachineFunctionInfo *FuncInfo = MF.getInfo<ZodiacMachineFunctionInfo>();
  SDLoc DL(Op);
  SDValue FI = DAG.getFrameIndex(FuncInfo->getVarArgsFrameIndex(), getPointerTy(DAG.getDataLayout()));
  const Value *SV = cast<SrcValueSDNode>(Op.getOperand(2))->getValue();
  return DAG.getStore(Op.getOperand(0), DL, FI, Op.getOperand(1), MachinePointerInfo(SV));
}

SDValue ZodiacTargetLowering::LowerFormalArguments(
    SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
    const SmallVectorImpl<ISD::InputArg> &Ins, const SDLoc &DL,
    SelectionDAG &DAG, SmallVectorImpl<SDValue> &InVals) const {
  return LowerCCCArguments(Chain, CallConv, IsVarArg, Ins, DL, DAG, InVals);
}

SDValue ZodiacTargetLowering::LowerCCCArguments(
    SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
    const SmallVectorImpl<ISD::InputArg> &Ins, const SDLoc &DL,
    SelectionDAG &DAG, SmallVectorImpl<SDValue> &InVals) const {
  MachineFunction &MF = DAG.getMachineFunction();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  MachineRegisterInfo &RegInfo = MF.getRegInfo();
  ZodiacMachineFunctionInfo *ZodiacMFI = MF.getInfo<ZodiacMachineFunctionInfo>();

  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, IsVarArg, DAG.getMachineFunction(), ArgLocs,
                 *DAG.getContext());
  CCInfo.AnalyzeFormalArguments(Ins, CC_Zodiac32);

  for (const CCValAssign &VA : ArgLocs) {
    if (VA.isRegLoc()) {
      EVT RegVT = VA.getLocVT();
      Register VReg = RegInfo.createVirtualRegister(&Zodiac::GPRRegClass);
      RegInfo.addLiveIn(VA.getLocReg(), VReg);
      SDValue ArgValue = DAG.getCopyFromReg(Chain, DL, VReg, RegVT);

      if (VA.getLocInfo() == CCValAssign::SExt)
        ArgValue = DAG.getNode(ISD::AssertSext, DL, RegVT, ArgValue,
                               DAG.getValueType(VA.getValVT()));
      else if (VA.getLocInfo() == CCValAssign::ZExt)
        ArgValue = DAG.getNode(ISD::AssertZext, DL, RegVT, ArgValue,
                               DAG.getValueType(VA.getValVT()));

      if (VA.getLocInfo() != CCValAssign::Full)
        ArgValue = DAG.getNode(ISD::TRUNCATE, DL, VA.getValVT(), ArgValue);

      InVals.push_back(ArgValue);
    } else {
      assert(VA.isMemLoc());
      unsigned ObjSize = VA.getLocVT().getSizeInBits() / 8;
      int FI = MFI.CreateFixedObject(ObjSize, VA.getLocMemOffset(), true);
      SDValue FIN = DAG.getFrameIndex(FI, MVT::i32);
      InVals.push_back(DAG.getLoad(
          VA.getLocVT(), DL, Chain, FIN,
          MachinePointerInfo::getFixedStack(DAG.getMachineFunction(), FI)));
    }
  }

  if (MF.getFunction().hasStructRetAttr()) {
    Register Reg = ZodiacMFI->getSRetReturnReg();
    if (!Reg) {
      Reg = MF.getRegInfo().createVirtualRegister(getRegClassFor(MVT::i32));
      ZodiacMFI->setSRetReturnReg(Reg);
    }
    SDValue Copy = DAG.getCopyToReg(DAG.getEntryNode(), DL, Reg, InVals[0]);
    Chain = DAG.getNode(ISD::TokenFactor, DL, MVT::Other, Copy, Chain);
  }

  if (IsVarArg) {
    int FI = MFI.CreateFixedObject(4, CCInfo.getStackSize(), true);
    ZodiacMFI->setVarArgsFrameIndex(FI);
  }

  return Chain;
}

SDValue ZodiacTargetLowering::LowerCall(TargetLowering::CallLoweringInfo &CLI,
                                       SmallVectorImpl<SDValue> &InVals) const {
  SelectionDAG &DAG = CLI.DAG;
  SDLoc &DL = CLI.DL;
  SmallVectorImpl<ISD::OutputArg> &Outs = CLI.Outs;
  SmallVectorImpl<SDValue> &OutVals = CLI.OutVals;
  SmallVectorImpl<ISD::InputArg> &Ins = CLI.Ins;
  SDValue Chain = CLI.Chain;
  SDValue Callee = CLI.Callee;
  bool &IsTailCall = CLI.IsTailCall;
  CallingConv::ID CallConv = CLI.CallConv;
  bool IsVarArg = CLI.IsVarArg;

  IsTailCall = false;

  return LowerCCCCallTo(Chain, Callee, CallConv, IsVarArg, IsTailCall, Outs,
                        OutVals, Ins, DL, DAG, InVals);
}

SDValue ZodiacTargetLowering::LowerCCCCallTo(
    SDValue Chain, SDValue Callee, CallingConv::ID CallConv, bool IsVarArg,
    bool /*IsTailCall*/, const SmallVectorImpl<ISD::OutputArg> &Outs,
    const SmallVectorImpl<SDValue> &OutVals,
    const SmallVectorImpl<ISD::InputArg> &Ins, const SDLoc &DL,
    SelectionDAG &DAG, SmallVectorImpl<SDValue> &InVals) const {
  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, IsVarArg, DAG.getMachineFunction(), ArgLocs,
                 *DAG.getContext());
  CCInfo.AnalyzeCallOperands(Outs, CC_Zodiac32);

  unsigned NumBytes = CCInfo.getStackSize();
  Chain = DAG.getCALLSEQ_START(Chain, NumBytes, 0, DL);

  SmallVector<std::pair<unsigned, SDValue>, 8> RegsToPass;
  SmallVector<SDValue, 8> MemOpChains;

  SDValue StackPtr;

  for (unsigned i = 0, e = ArgLocs.size(); i != e; ++i) {
    CCValAssign &VA = ArgLocs[i];
    SDValue Arg = OutVals[i];

    switch (VA.getLocInfo()) {
    default: llvm_unreachable("Unknown loc info!");
    case CCValAssign::Full: break;
    case CCValAssign::SExt:
      Arg = DAG.getNode(ISD::SIGN_EXTEND, DL, VA.getLocVT(), Arg);
      break;
    case CCValAssign::ZExt:
      Arg = DAG.getNode(ISD::ZERO_EXTEND, DL, VA.getLocVT(), Arg);
      break;
    case CCValAssign::AExt:
      Arg = DAG.getNode(ISD::ANY_EXTEND, DL, VA.getLocVT(), Arg);
      break;
    }

    if (VA.isRegLoc()) {
      RegsToPass.push_back(std::make_pair(VA.getLocReg(), Arg));
    } else {
      assert(VA.isMemLoc());
      if (!StackPtr.getNode())
        StackPtr = DAG.getCopyFromReg(Chain, DL, Zodiac::X31, getPointerTy(DAG.getDataLayout()));
      SDValue PtrOff = DAG.getNode(ISD::ADD, DL, getPointerTy(DAG.getDataLayout()), StackPtr,
                                   DAG.getIntPtrConstant(VA.getLocMemOffset(), DL));
      MemOpChains.push_back(DAG.getStore(
          Chain, DL, Arg, PtrOff,
          MachinePointerInfo::getStack(DAG.getMachineFunction(), VA.getLocMemOffset())));
    }
  }

  if (!MemOpChains.empty())
    Chain = DAG.getNode(ISD::TokenFactor, DL, MVT::Other, MemOpChains);

  SDValue Glue;
  for (unsigned i = 0, e = RegsToPass.size(); i != e; ++i) {
    Chain = DAG.getCopyToReg(Chain, DL, RegsToPass[i].first, RegsToPass[i].second, Glue);
    Glue = Chain.getValue(1);
  }

  if (GlobalAddressSDNode *G = dyn_cast<GlobalAddressSDNode>(Callee)) {
    Callee = DAG.getTargetGlobalAddress(G->getGlobal(), DL, MVT::i32, 0);
  } else if (ExternalSymbolSDNode *E = dyn_cast<ExternalSymbolSDNode>(Callee)) {
    Callee = DAG.getTargetExternalSymbol(E->getSymbol(), MVT::i32);
  }

  SDVTList NodeTys = DAG.getVTList(MVT::Other, MVT::Glue);
  SmallVector<SDValue, 8> Ops;
  Ops.push_back(Chain);
  Ops.push_back(Callee);

  for (unsigned i = 0, e = RegsToPass.size(); i != e; ++i)
    Ops.push_back(DAG.getRegister(RegsToPass[i].first, RegsToPass[i].second.getValueType()));

  // Add the register mask operand to denote which registers are preserved/clobbered
  const TargetRegisterInfo *TRI = DAG.getSubtarget().getRegisterInfo();
  const uint32_t *Mask = TRI->getCallPreservedMask(DAG.getMachineFunction(), CallConv);
  assert(Mask && "Missing call preserved mask");
  Ops.push_back(DAG.getRegisterMask(Mask));

  if (Glue.getNode())
    Ops.push_back(Glue);

  Chain = DAG.getNode(ZodiacISD::CALL, DL, NodeTys, Ops);
  Glue = Chain.getValue(1);

  Chain = DAG.getCALLSEQ_END(Chain, NumBytes, 0, Glue, DL);
  Glue = Chain.getValue(1);

  return LowerCallResult(Chain, Glue, CallConv, IsVarArg, Ins, DL, DAG, InVals);
}

SDValue ZodiacTargetLowering::LowerCallResult(
    SDValue Chain, SDValue InGlue, CallingConv::ID CallConv, bool IsVarArg,
    const SmallVectorImpl<ISD::InputArg> &Ins, const SDLoc &DL,
    SelectionDAG &DAG, SmallVectorImpl<SDValue> &InVals) const {
  SmallVector<CCValAssign, 16> RVLocs;
  CCState CCInfo(CallConv, IsVarArg, DAG.getMachineFunction(), RVLocs,
                 *DAG.getContext());
  CCInfo.AnalyzeCallResult(Ins, RetCC_Zodiac32);

  for (unsigned i = 0; i != RVLocs.size(); ++i) {
    CCValAssign &VA = RVLocs[i];
    assert(VA.isRegLoc() && "Can only return in registers!");
    SDValue Val = DAG.getCopyFromReg(Chain, DL, VA.getLocReg(), VA.getLocVT(), InGlue);
    Chain = Val.getValue(1);
    InGlue = Val.getValue(2);
    InVals.push_back(Val.getValue(0));
  }

  return Chain;
}

bool ZodiacTargetLowering::CanLowerReturn(
    CallingConv::ID CallConv, MachineFunction &MF, bool IsVarArg,
    const SmallVectorImpl<ISD::OutputArg> &Outs, LLVMContext &Context,
    const Type *RetTy) const {
  SmallVector<CCValAssign, 16> RVLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, RVLocs, Context);
  return CCInfo.CheckReturn(Outs, RetCC_Zodiac32);
}

SDValue
ZodiacTargetLowering::LowerReturn(SDValue Chain, CallingConv::ID CallConv,
                                 bool IsVarArg,
                                 const SmallVectorImpl<ISD::OutputArg> &Outs,
                                 const SmallVectorImpl<SDValue> &OutVals,
                                 const SDLoc &DL, SelectionDAG &DAG) const {
  SmallVector<CCValAssign, 16> RVLocs;
  CCState CCInfo(CallConv, IsVarArg, DAG.getMachineFunction(), RVLocs,
                 *DAG.getContext());
  CCInfo.AnalyzeReturn(Outs, RetCC_Zodiac32);

  SDValue Glue;
  SmallVector<SDValue, 4> RetOps(1, Chain);

  for (unsigned i = 0; i != RVLocs.size(); ++i) {
    CCValAssign &VA = RVLocs[i];
    assert(VA.isRegLoc() && "Can only return in registers!");

    Chain = DAG.getCopyToReg(Chain, DL, VA.getLocReg(), OutVals[i], Glue);
    Glue = Chain.getValue(1);
    RetOps.push_back(DAG.getRegister(VA.getLocReg(), VA.getLocVT()));
  }

  if (DAG.getMachineFunction().getFunction().hasStructRetAttr()) {
    MachineFunction &MF = DAG.getMachineFunction();
    ZodiacMachineFunctionInfo *ZodiacMFI = MF.getInfo<ZodiacMachineFunctionInfo>();
    Register Reg = ZodiacMFI->getSRetReturnReg();
    assert(Reg && "SRetReturnReg should have been set.");
    SDValue Val =
        DAG.getCopyFromReg(Chain, DL, Reg, getPointerTy(DAG.getDataLayout()));

    Chain = DAG.getCopyToReg(Chain, DL, Zodiac::X1, Val, Glue);
    Glue = Chain.getValue(1);
    RetOps.push_back(
        DAG.getRegister(Zodiac::X1, getPointerTy(DAG.getDataLayout())));
  }

  RetOps[0] = Chain;

  unsigned Opc = ZodiacISD::RET_GLUE;
  if (Glue.getNode())
    RetOps.push_back(Glue);

  return DAG.getNode(Opc, DL, MVT::Other, ArrayRef<SDValue>(&RetOps[0], RetOps.size()));
}

Register ZodiacTargetLowering::getRegisterByName(
  const char *RegName, LLT /*VT*/,
  const MachineFunction & /*MF*/) const {
  Register Reg = StringSwitch<Register>(RegName)
                     .Case("sp", Zodiac::X31)
                     .Case("fp", Zodiac::X29)
                     .Case("lr", Zodiac::X30)
                     .Default(Register());
  return Reg;
}

std::pair<unsigned, const TargetRegisterClass *>
ZodiacTargetLowering::getRegForInlineAsmConstraint(const TargetRegisterInfo *TRI,
                                                  StringRef Constraint,
                                                  MVT VT) const {
  if (Constraint.size() == 1) {
    switch (Constraint[0]) {
    case 'r':
      return std::make_pair(0U, &Zodiac::GPRRegClass);
    default:
      break;
    }
  }
  return TargetLowering::getRegForInlineAsmConstraint(TRI, Constraint, VT);
}

TargetLowering::ConstraintWeight
ZodiacTargetLowering::getSingleConstraintMatchWeight(
    AsmOperandInfo &Info, const char *Constraint) const {
  ConstraintWeight Weight = CW_Invalid;
  Value *CallOperandVal = Info.CallOperandVal;
  if (CallOperandVal == nullptr)
    return CW_Default;
  switch (*Constraint) {
  case 'I':
  case 'J':
  case 'K':
  case 'L':
    if (isa<ConstantInt>(CallOperandVal))
      Weight = CW_Constant;
    break;
  default:
    Weight = TargetLowering::getSingleConstraintMatchWeight(Info, Constraint);
    break;
  }
  return Weight;
}

void ZodiacTargetLowering::LowerAsmOperandForConstraint(
    SDValue Op, StringRef Constraint, std::vector<SDValue> &Ops,
    SelectionDAG &DAG) const {
  SDValue Result;
  if (Constraint.size() > 1)
    return;

  char ConstraintLetter = Constraint[0];
  switch (ConstraintLetter) {
  case 'I':
    if (ConstantSDNode *C = dyn_cast<ConstantSDNode>(Op)) {
      if (isInt<16>(C->getSExtValue())) {
        Result = DAG.getTargetConstant(C->getSExtValue(), SDLoc(C),
                                       Op.getValueType());
        break;
      }
    }
    return;
  default:
    break;
  }

  if (Result.getNode()) {
    Ops.push_back(Result);
    return;
  }

  TargetLowering::LowerAsmOperandForConstraint(Op, Constraint, Ops, DAG);
}

SDValue ZodiacTargetLowering::LowerSETCC(SDValue Op, SelectionDAG &DAG) const {
  SDLoc dl(Op);
  SDValue LHS = Op.getOperand(0);
  SDValue RHS = Op.getOperand(1);
  ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(2))->get();

  if (CC == ISD::SETEQ) {
    // a == b  -->  sltiu (xor a, b), 1
    SDValue Xor = DAG.getNode(ISD::XOR, dl, MVT::i32, LHS, RHS);
    return DAG.getSetCC(dl, Op.getValueType(), Xor, DAG.getConstant(1, dl, MVT::i32), ISD::SETULT);
  }
  if (CC == ISD::SETNE) {
    // a != b  -->  sltu x0, (xor a, b)
    SDValue Xor = DAG.getNode(ISD::XOR, dl, MVT::i32, LHS, RHS);
    SDValue Zero = DAG.getRegister(Zodiac::X0, MVT::i32);
    return DAG.getSetCC(dl, Op.getValueType(), Zero, Xor, ISD::SETULT);
  }
  if (CC == ISD::SETGT) {
    // a > b  -->  slt b, a
    return DAG.getSetCC(dl, Op.getValueType(), RHS, LHS, ISD::SETLT);
  }
  if (CC == ISD::SETUGT) {
    // a >u b  -->  sltu b, a
    return DAG.getSetCC(dl, Op.getValueType(), RHS, LHS, ISD::SETULT);
  }
  if (CC == ISD::SETLE) {
    // a <= b  -->  (slt b, a) ^ 1
    SDValue Slt = DAG.getSetCC(dl, Op.getValueType(), RHS, LHS, ISD::SETLT);
    return DAG.getNode(ISD::XOR, dl, Op.getValueType(), Slt, DAG.getConstant(1, dl, Op.getValueType()));
  }
  if (CC == ISD::SETULE) {
    // a <=u b  -->  (sltu b, a) ^ 1
    SDValue Sltu = DAG.getSetCC(dl, Op.getValueType(), RHS, LHS, ISD::SETULT);
    return DAG.getNode(ISD::XOR, dl, Op.getValueType(), Sltu, DAG.getConstant(1, dl, Op.getValueType()));
  }
  if (CC == ISD::SETGE) {
    // a >= b  -->  (slt a, b) ^ 1
    SDValue Slt = DAG.getSetCC(dl, Op.getValueType(), LHS, RHS, ISD::SETLT);
    return DAG.getNode(ISD::XOR, dl, Op.getValueType(), Slt, DAG.getConstant(1, dl, Op.getValueType()));
  }
  if (CC == ISD::SETUGE) {
    // a >=u b  -->  (sltu a, b) ^ 1
    SDValue Sltu = DAG.getSetCC(dl, Op.getValueType(), LHS, RHS, ISD::SETULT);
    return DAG.getNode(ISD::XOR, dl, Op.getValueType(), Sltu, DAG.getConstant(1, dl, Op.getValueType()));
  }

  // Otherwise, let it be (for SETLT and SETULT)
  return SDValue();
}

SDValue ZodiacTargetLowering::PerformDAGCombine(SDNode *N, DAGCombinerInfo &DCI) const {
  return SDValue();
}

void ZodiacTargetLowering::computeKnownBitsForTargetNode(
    const SDValue Op, KnownBits &Known, const APInt &DemandedElts,
    const SelectionDAG &DAG, unsigned Depth) const {
  Known.resetAll();
}

SDValue ZodiacTargetLowering::LowerSELECT_CC(SDValue Op, SelectionDAG &DAG) const {
  SDValue LHS = Op.getOperand(0);
  SDValue RHS = Op.getOperand(1);
  SDValue TrueVal = Op.getOperand(2);
  SDValue FalseVal = Op.getOperand(3);
  SDValue CC = Op.getOperand(4);
  SDLoc dl(Op);

  return DAG.getNode(ZodiacISD::SELECT_CC, dl, Op.getValueType(),
                     TrueVal, FalseVal, LHS, RHS, CC);
}

MachineBasicBlock *ZodiacTargetLowering::EmitInstrWithCustomInserter(
    MachineInstr &MI, MachineBasicBlock *BB) const {
  const TargetInstrInfo &TII = *Subtarget.getInstrInfo();
  DebugLoc dl = MI.getDebugLoc();

  assert(MI.getOpcode() == Zodiac::SELECT_CC && "Unexpected instruction to select!");

  const BasicBlock *LLVM_BB = BB->getBasicBlock();
  MachineFunction::iterator It = ++BB->getIterator();

  MachineFunction *F = BB->getParent();
  MachineBasicBlock *TrueBB = F->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *FalseBB = F->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *SinkBB = F->CreateMachineBasicBlock(LLVM_BB);

  F->insert(It, TrueBB);
  F->insert(It, FalseBB);
  F->insert(It, SinkBB);

  // Transfer the remainder of BB and its successor connections to SinkBB.
  SinkBB->splice(SinkBB->begin(), BB, std::next(MachineBasicBlock::iterator(MI)), BB->end());
  SinkBB->transferSuccessorsAndUpdatePHIs(BB);

  // Set up connections:
  BB->addSuccessor(TrueBB);
  BB->addSuccessor(FalseBB);
  TrueBB->addSuccessor(SinkBB);
  FalseBB->addSuccessor(SinkBB);

  // Retrieve operands:
  Register DstReg = MI.getOperand(0).getReg();
  Register TrueVal = MI.getOperand(1).getReg();
  Register FalseVal = MI.getOperand(2).getReg();
  Register LHS = MI.getOperand(3).getReg();
  Register RHS = MI.getOperand(4).getReg();
  unsigned CC = MI.getOperand(5).getImm();

  // Convert CondCode to actual hardware branch opcode.
  unsigned Opc = 0;
  bool Swap = false;

  switch (static_cast<ISD::CondCode>(CC)) {
  case ISD::SETEQ:  Opc = Zodiac::BEQ; break;
  case ISD::SETNE:  Opc = Zodiac::BNE; break;
  case ISD::SETLT:  Opc = Zodiac::BLT; break;
  case ISD::SETULT: Opc = Zodiac::BLTU; break;
  case ISD::SETGE:  Opc = Zodiac::BGE; break;
  case ISD::SETUGE: Opc = Zodiac::BGEU; break;
  case ISD::SETGT:  Opc = Zodiac::BLT; Swap = true; break;
  case ISD::SETUGT: Opc = Zodiac::BLTU; Swap = true; break;
  case ISD::SETLE:  Opc = Zodiac::BGE; Swap = true; break;
  case ISD::SETULE: Opc = Zodiac::BGEU; Swap = true; break;
  default:
    llvm_unreachable("Unexpected condition code!");
  }

  Register BrReg1 = Swap ? RHS : LHS;
  Register BrReg2 = Swap ? LHS : RHS;

  BuildMI(BB, dl, TII.get(Opc))
      .addReg(BrReg1)
      .addReg(BrReg2)
      .addMBB(TrueBB);

  BuildMI(BB, dl, TII.get(Zodiac::B)).addMBB(FalseBB);

  // SinkBB: PHI node merging TrueVal and FalseVal
  BuildMI(*SinkBB, SinkBB->begin(), dl, TII.get(Zodiac::PHI), DstReg)
      .addReg(TrueVal)
      .addMBB(TrueBB)
      .addReg(FalseVal)
      .addMBB(FalseBB);

  MI.eraseFromParent();
  return SinkBB;
}
