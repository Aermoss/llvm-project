//===-- ZodiacInstrInfo.td - Zodiac Instruction defs ---------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file describes the Zodiac instructions in TableGen format.
// Opcode values match the cpu.sv RTL definition.
//
//===----------------------------------------------------------------------===//

include "ZodiacInstrFormats.td"

// ===----------------------------------------------------------------------===//
// Zodiac-specific ISD nodes
// ===----------------------------------------------------------------------===//

def SDT_ZodiacCall : SDTypeProfile<0, -1, [SDTCisVT<0, i32>]>;
def SDT_ZodiacCallSeqStart : SDCallSeqStart<[SDTCisVT<0, i32>,
                                              SDTCisVT<1, i32>]>;
def SDT_ZodiacCallSeqEnd   : SDCallSeqEnd<[SDTCisVT<0, i32>,
                                            SDTCisVT<1, i32>]>;

def ZodiacCall       : SDNode<"ZodiacISD::CALL", SDT_ZodiacCall,
                              [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue,
                               SDNPVariadic]>;
def ZodiacRetGlue    : SDNode<"ZodiacISD::RET_GLUE", SDTNone,
                              [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;
def CallSeqStart     : SDNode<"ISD::CALLSEQ_START", SDT_ZodiacCallSeqStart,
                              [SDNPHasChain, SDNPOutGlue]>;
def CallSeqEnd       : SDNode<"ISD::CALLSEQ_END", SDT_ZodiacCallSeqEnd,
                              [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue]>;

def ZodiacHi21       : SDNode<"ZodiacISD::HI21", SDTIntUnaryOp>;
def ZodiacLo11       : SDNode<"ZodiacISD::LO11", SDTIntUnaryOp>;

def SDT_ZodiacBrCC : SDTypeProfile<0, 4, [SDTCisVT<0, OtherVT>, SDTCisVT<1, i32>, SDTCisVT<2, i32>, SDTCisVT<3, OtherVT>]>;
def ZodiacBrCC : SDNode<"ZodiacISD::BR_CC", SDT_ZodiacBrCC, [SDNPHasChain]>;

def SDT_ZodiacSelectCC : SDTypeProfile<1, 5, [SDTCisSameAs<0, 1>, SDTCisSameAs<1, 2>, SDTCisVT<3, i32>, SDTCisVT<4, i32>, SDTCisVT<5, OtherVT>]>;
def ZodiacSelectCC : SDNode<"ZodiacISD::SELECT_CC", SDT_ZodiacSelectCC>;

// ===----------------------------------------------------------------------===//
// Operand types
// ===----------------------------------------------------------------------===//

// Signed 16-bit immediate
def simm16 : Operand<i32>, ImmLeaf<i32, [{return isInt<16>(Imm);}]> {
  let DecoderMethod = "decodeSimm16";
}

// Unsigned 21-bit immediate
def uimm21 : Operand<i32>, ImmLeaf<i32, [{return isUInt<21>(Imm);}]> {
  let DecoderMethod = "decodeUimm21";
}

// Branch target operands (PC-relative)
def BrTarget16 : Operand<OtherVT> {
  let EncoderMethod = "getBranchTarget16OpValue";
  let DecoderMethod = "decodeBrTarget16";
}

def BrTarget21 : Operand<OtherVT> {
  let EncoderMethod = "getBranchTarget21OpValue";
  let DecoderMethod = "decodeBrTarget21";
}

def BrTarget26 : Operand<OtherVT> {
  let EncoderMethod = "getBranchTarget26OpValue";
  let DecoderMethod = "decodeBrTarget26";
}

def CallTarget : Operand<i32> {
  let EncoderMethod = "getBranchTarget21OpValue";
  let DecoderMethod = "decodeBrTarget21";
}

// Address matching for loads/stores: base + signed 16-bit offset
def ADDRri : ComplexPattern<i32, 2, "selectAddrRI", [frameindex]>;

// SDNodeXForms for constant materialization
def HI21 : SDNodeXForm<imm, [{
  return CurDAG->getTargetConstant(
    ((uint64_t)N->getZExtValue() >> 11) & 0x1FFFFF, SDLoc(N), MVT::i32);
}]>;

def LO11 : SDNodeXForm<imm, [{
  return CurDAG->getTargetConstant(
    (uint64_t)N->getZExtValue() & 0x7FF, SDLoc(N), MVT::i32);
}]>;

// Immediate predicates
def immSExt16 : PatLeaf<(imm), [{return isInt<16>(N->getSExtValue());}]>;
def immUExt21 : PatLeaf<(imm), [{return isUInt<21>(N->getZExtValue());}]>;

// ===----------------------------------------------------------------------===//
// Pseudo instructions
// ===----------------------------------------------------------------------===//

let Defs = [X31], Uses = [X31] in {
  def ADJCALLSTACKDOWN : Pseudo<(outs), (ins i32imm:$amt, i32imm:$amt2),
                                [(CallSeqStart timm:$amt, timm:$amt2)]>;
  def ADJCALLSTACKUP   : Pseudo<(outs), (ins i32imm:$amt, i32imm:$amt2),
                                [(CallSeqEnd timm:$amt, timm:$amt2)]>;
}

// Return pseudo - expands to BR X30
let isReturn = 1, isTerminator = 1, isBarrier = 1, Uses = [X30] in
  def RET : Pseudo<(outs), (ins), [(ZodiacRetGlue)]>;

// Call pseudo - expands to BL X30, target
let isCall = 1, Defs = [X30], Uses = [X31] in {
  def CALL : Pseudo<(outs), (ins CallTarget:$target),
                    [(ZodiacCall tglobaladdr:$target)]>;
  def CALLR : Pseudo<(outs), (ins GPR:$rs1),
                     [(ZodiacCall GPR:$rs1)]>;
}

// Select_CC pseudo - expanded during EmitInstrWithCustomInserter
let usesCustomInserter = 1 in
  def SELECT_CC : Pseudo<(outs GPR:$rd),
                         (ins GPR:$trueval, GPR:$falseval, GPR:$lhs, GPR:$rhs, i32imm:$cc), []>;

// ===----------------------------------------------------------------------===//
// NOP and HLT (Z-type)
// ===----------------------------------------------------------------------===//

let hasSideEffects = 0, mayLoad = 0, mayStore = 0 in
  def NOP : InstZ<0b000000, (outs), (ins), "nop", []>;

let hasSideEffects = 1, mayLoad = 0, mayStore = 0,
    isTerminator = 1, isBarrier = 1 in
  def HLT : InstZ<0b111111, (outs), (ins), "hlt", []>;

// ===----------------------------------------------------------------------===//
// ALU instruction multiclasses
// ===----------------------------------------------------------------------===//

// R-type ALU: rd = rs1 op rs2
class ALU_rr<bits<6> op, string mnemonic, SDNode OpNode>
    : InstR<op, (outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2),
            !strconcat(mnemonic, "\t$rd, $rs1, $rs2"),
            [(set GPR:$rd, (OpNode GPR:$rs1, GPR:$rs2))]>,
      Sched<[WriteALU]>;

// I-type ALU: rd = rs1 op sign_extend(imm16)
class ALU_ri<bits<6> op, string mnemonic, SDNode OpNode>
    : InstI<op, (outs GPR:$rd), (ins GPR:$rs1, simm16:$imm16),
            !strconcat(mnemonic, "\t$rd, $rs1, $imm16"),
            [(set GPR:$rd, (OpNode GPR:$rs1, simm16:$imm16))]>,
      Sched<[WriteALU]>;

// I-type ALU without pattern (for SUBI which LLVM canonicalizes away)
class ALU_ri_np<bits<6> op, string mnemonic>
    : InstI<op, (outs GPR:$rd), (ins GPR:$rs1, simm16:$imm16),
            !strconcat(mnemonic, "\t$rd, $rs1, $imm16"), []>,
      Sched<[WriteALU]>;

// R-type ALU without pattern
class ALU_rr_np<bits<6> op, string mnemonic>
    : InstR<op, (outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2),
            !strconcat(mnemonic, "\t$rd, $rs1, $rs2"), []>,
      Sched<[WriteALU]>;

// ===----------------------------------------------------------------------===//
// Arithmetic instructions (opcodes 25-28)
// ===----------------------------------------------------------------------===//

def ADD  : ALU_rr<25, "add",  add>;
def ADDI : ALU_ri<26, "addi", add>;
def SUB  : ALU_rr<27, "sub",  sub>;
// SUBI: rd = rs1 - sign_extend(imm16). LLVM canonicalizes (sub x, C) to
// (add x, -C), so ADDI handles most cases. SUBI has no auto-match pattern.
def SUBI : ALU_ri_np<28, "subi">;

// ===----------------------------------------------------------------------===//
// Multiply / Divide instructions (opcodes 29-36)
// ===----------------------------------------------------------------------===//

def MUL    : ALU_rr<29, "mul",    mul>;
def MULH   : InstR<30, (outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2),
                   "mulh\t$rd, $rs1, $rs2",
                   [(set GPR:$rd, (mulhs GPR:$rs1, GPR:$rs2))]>,
             Sched<[WriteALU]>;
def MULHSU : ALU_rr_np<31, "mulhsu">;
def MULHU  : InstR<32, (outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2),
                   "mulhu\t$rd, $rs1, $rs2",
                   [(set GPR:$rd, (mulhu GPR:$rs1, GPR:$rs2))]>,
             Sched<[WriteALU]>;
def DIV    : ALU_rr<33, "div",  sdiv>;
def DIVU   : ALU_rr<34, "divu", udiv>;
def REM    : ALU_rr<35, "rem",  srem>;
def REMU   : ALU_rr<36, "remu", urem>;

// ===----------------------------------------------------------------------===//
// Logical instructions (opcodes 37-42)
// ===----------------------------------------------------------------------===//

def AND  : ALU_rr<37, "and",  and>;
def ANDI : ALU_ri<38, "andi", and>;
def OR   : ALU_rr<39, "or",   or>;
def ORI  : ALU_ri<40, "ori",  or>;
def XOR  : ALU_rr<41, "xor",  xor>;
def XORI : ALU_ri<42, "xori", xor>;

// ===----------------------------------------------------------------------===//
// Shift instructions (opcodes 43-48)
// ===----------------------------------------------------------------------===//

def SLL  : ALU_rr<43, "sll",  shl>;
def SLLI : ALU_ri<44, "slli", shl>;
def SRL  : ALU_rr<45, "srl",  srl>;
def SRLI : ALU_ri<46, "srli", srl>;
def SRA  : ALU_rr<47, "sra",  sra>;
def SRAI : ALU_ri<48, "srai", sra>;

// ===----------------------------------------------------------------------===//
// Set-less-than instructions (opcodes 21-24)
// ===----------------------------------------------------------------------===//
// Patterns are handled via custom lowering of ISD::SETCC

def SLT   : ALU_rr_np<21, "slt">;
def SLTU  : ALU_rr_np<22, "sltu">;
def SLTI  : ALU_ri_np<23, "slti">;
def SLTIU : ALU_ri_np<24, "sltiu">;

// ===----------------------------------------------------------------------===//
// Load instructions (opcodes 1-5) — I-type
// ===----------------------------------------------------------------------===//

let mayLoad = 1, hasSideEffects = 0 in {
  def LB  : InstI<1, (outs GPR:$rd), (ins GPR:$rs1, simm16:$imm16),
                  "lb\t$rd, ${imm16}(${rs1})", []>;
  def LBU : InstI<2, (outs GPR:$rd), (ins GPR:$rs1, simm16:$imm16),
                  "lbu\t$rd, ${imm16}(${rs1})", []>;
  def LH  : InstI<3, (outs GPR:$rd), (ins GPR:$rs1, simm16:$imm16),
                  "lh\t$rd, ${imm16}(${rs1})", []>;
  def LHU : InstI<4, (outs GPR:$rd), (ins GPR:$rs1, simm16:$imm16),
                  "lhu\t$rd, ${imm16}(${rs1})", []>;
  def LW  : InstI<5, (outs GPR:$rd), (ins GPR:$rs1, simm16:$imm16),
                  "lw\t$rd, ${imm16}(${rs1})", []>;
}

// ===----------------------------------------------------------------------===//
// Store instructions (opcodes 9-11) — S-type
// ===----------------------------------------------------------------------===//

let mayStore = 1, hasSideEffects = 0 in {
  def SB : InstS<9,  (outs), (ins GPR:$rs2, GPR:$rs1, simm16:$imm16),
                 "sb\t$rs2, ${imm16}(${rs1})", []>;
  def SH : InstS<10, (outs), (ins GPR:$rs2, GPR:$rs1, simm16:$imm16),
                 "sh\t$rs2, ${imm16}(${rs1})", []>;
  def SW : InstS<11, (outs), (ins GPR:$rs2, GPR:$rs1, simm16:$imm16),
                 "sw\t$rs2, ${imm16}(${rs1})", []>;
}

// ===----------------------------------------------------------------------===//
// Load / Store patterns
// ===----------------------------------------------------------------------===//

// Word load/store
def : Pat<(load (ADDRri GPR:$base, simm16:$off)),
          (LW GPR:$base, simm16:$off)>;
def : Pat<(store GPR:$val, (ADDRri GPR:$base, simm16:$off)),
          (SW GPR:$val, GPR:$base, simm16:$off)>;

// Half-word loads
def : Pat<(sextloadi16 (ADDRri GPR:$base, simm16:$off)),
          (LH GPR:$base, simm16:$off)>;
def : Pat<(zextloadi16 (ADDRri GPR:$base, simm16:$off)),
          (LHU GPR:$base, simm16:$off)>;
def : Pat<(extloadi16 (ADDRri GPR:$base, simm16:$off)),
          (LHU GPR:$base, simm16:$off)>;

// Byte loads
def : Pat<(sextloadi8 (ADDRri GPR:$base, simm16:$off)),
          (LB GPR:$base, simm16:$off)>;
def : Pat<(zextloadi8 (ADDRri GPR:$base, simm16:$off)),
          (LBU GPR:$base, simm16:$off)>;
def : Pat<(extloadi8 (ADDRri GPR:$base, simm16:$off)),
          (LBU GPR:$base, simm16:$off)>;

// Truncating stores
def : Pat<(truncstorei16 GPR:$val, (ADDRri GPR:$base, simm16:$off)),
          (SH GPR:$val, GPR:$base, simm16:$off)>;
def : Pat<(truncstorei8 GPR:$val, (ADDRri GPR:$base, simm16:$off)),
          (SB GPR:$val, GPR:$base, simm16:$off)>;

// ===----------------------------------------------------------------------===//
// Upper-immediate instructions (opcodes 6-8) — U-type
// ===----------------------------------------------------------------------===//

let hasSideEffects = 0, mayLoad = 0, mayStore = 0 in {
  def LI    : InstU<6, (outs GPR:$rd), (ins uimm21:$imm21),
                    "li\t$rd, $imm21", []>;
  def LUI   : InstU<7, (outs GPR:$rd), (ins uimm21:$imm21),
                    "lui\t$rd, $imm21", []>;
  def AUIPC : InstU<8, (outs GPR:$rd), (ins uimm21:$imm21),
                    "auipc\t$rd, $imm21", []>;
}

// ===----------------------------------------------------------------------===//
// Constant materialization patterns
// ===----------------------------------------------------------------------===//

// Small signed constants [-32768, 32767]: ADDI x0, imm
let AddedComplexity = 3 in
def : Pat<(i32 immSExt16:$imm), (ADDI X0, simm16:$imm)>;

// Unsigned 21-bit constants [0, 2097151]: LI
let AddedComplexity = 2 in
def : Pat<(i32 immUExt21:$imm), (LI uimm21:$imm)>;

// General 32-bit constants: LUI + ORI
def : Pat<(i32 imm:$imm), (ORI (LUI (HI21 imm:$imm)), (LO11 imm:$imm))>;

// Global address materialization
def : Pat<(ZodiacHi21 tglobaladdr:$addr), (LUI tglobaladdr:$addr)>;
def : Pat<(ZodiacLo11 tglobaladdr:$addr), (ORI X0, tglobaladdr:$addr)>;
def : Pat<(ZodiacHi21 tblockaddress:$addr), (LUI tblockaddress:$addr)>;
def : Pat<(ZodiacLo11 tblockaddress:$addr), (ORI X0, tblockaddress:$addr)>;

// ===----------------------------------------------------------------------===//
// Branch instructions (opcodes 12-20)
// ===----------------------------------------------------------------------===//

// Unconditional absolute branch
let isBranch = 1, isTerminator = 1, isBarrier = 1 in
  def B : InstJ26<12, (outs), (ins BrTarget26:$imm26),
                  "b\t$imm26", [(br bb:$imm26)]>;

// Branch register (indirect jump)
let isBranch = 1, isTerminator = 1, isBarrier = 1, isIndirectBranch = 1 in
  def BR : InstBR<13, (outs), (ins GPR:$rs1),
                  "br\t$rs1", [(brind GPR:$rs1)]>;

// Branch and link (function call instruction)
let isCall = 1 in
  def BL : InstJ<14, (outs GPR:$rd), (ins BrTarget21:$imm21),
                 "bl\t$rd, $imm21", []>;

// Conditional branches — patterns handled by custom lowering of BR_CC
let isBranch = 1, isTerminator = 1 in {
  def BEQ  : InstB<15, (outs), (ins GPR:$rs1, GPR:$rs2, BrTarget16:$imm16),
                   "beq\t$rs1, $rs2, $imm16", []>;
  def BNE  : InstB<16, (outs), (ins GPR:$rs1, GPR:$rs2, BrTarget16:$imm16),
                   "bne\t$rs1, $rs2, $imm16", []>;
  def BLT  : InstB<17, (outs), (ins GPR:$rs1, GPR:$rs2, BrTarget16:$imm16),
                   "blt\t$rs1, $rs2, $imm16", []>;
  def BLTU : InstB<18, (outs), (ins GPR:$rs1, GPR:$rs2, BrTarget16:$imm16),
                   "bltu\t$rs1, $rs2, $imm16", []>;
  def BGE  : InstB<19, (outs), (ins GPR:$rs1, GPR:$rs2, BrTarget16:$imm16),
                   "bge\t$rs1, $rs2, $imm16", []>;
  def BGEU : InstB<20, (outs), (ins GPR:$rs1, GPR:$rs2, BrTarget16:$imm16),
                   "bgeu\t$rs1, $rs2, $imm16", []>;
}

// Patterns for conditional branches
def : Pat<(ZodiacBrCC SETEQ, GPR:$rs1, GPR:$rs2, bb:$imm16),
          (BEQ GPR:$rs1, GPR:$rs2, bb:$imm16)>;
def : Pat<(ZodiacBrCC SETNE, GPR:$rs1, GPR:$rs2, bb:$imm16),
          (BNE GPR:$rs1, GPR:$rs2, bb:$imm16)>;
def : Pat<(ZodiacBrCC SETLT, GPR:$rs1, GPR:$rs2, bb:$imm16),
          (BLT GPR:$rs1, GPR:$rs2, bb:$imm16)>;
def : Pat<(ZodiacBrCC SETULT, GPR:$rs1, GPR:$rs2, bb:$imm16),
          (BLTU GPR:$rs1, GPR:$rs2, bb:$imm16)>;
def : Pat<(ZodiacBrCC SETGE, GPR:$rs1, GPR:$rs2, bb:$imm16),
          (BGE GPR:$rs1, GPR:$rs2, bb:$imm16)>;
def : Pat<(ZodiacBrCC SETUGE, GPR:$rs1, GPR:$rs2, bb:$imm16),
          (BGEU GPR:$rs1, GPR:$rs2, bb:$imm16)>;

// Swapped operand patterns for conditions not directly supported by hardware
def : Pat<(ZodiacBrCC SETGT, GPR:$rs1, GPR:$rs2, bb:$imm16),
          (BLT GPR:$rs2, GPR:$rs1, bb:$imm16)>;
def : Pat<(ZodiacBrCC SETUGT, GPR:$rs1, GPR:$rs2, bb:$imm16),
          (BLTU GPR:$rs2, GPR:$rs1, bb:$imm16)>;
def : Pat<(ZodiacBrCC SETLE, GPR:$rs1, GPR:$rs2, bb:$imm16),
          (BGE GPR:$rs2, GPR:$rs1, bb:$imm16)>;
def : Pat<(ZodiacBrCC SETULE, GPR:$rs1, GPR:$rs2, bb:$imm16),
          (BGEU GPR:$rs2, GPR:$rs1, bb:$imm16)>;

// Patterns for setcc (SETLT and SETULT)
def : Pat<(setcc GPR:$rs1, GPR:$rs2, SETLT),
          (SLT GPR:$rs1, GPR:$rs2)>;
def : Pat<(setcc GPR:$rs1, simm16:$imm16, SETLT),
          (SLTI GPR:$rs1, simm16:$imm16)>;
def : Pat<(setcc GPR:$rs1, GPR:$rs2, SETULT),
          (SLTU GPR:$rs1, GPR:$rs2)>;
def : Pat<(setcc GPR:$rs1, simm16:$imm16, SETULT),
          (SLTIU GPR:$rs1, simm16:$imm16)>;
