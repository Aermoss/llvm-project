//===- ZodiacInstrFormats.td - Zodiac Instruction Formats ---*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Zodiac instruction formats.
//
// All instructions are 32 bits wide with a 6-bit opcode in bits [31:26].
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Base instruction class
//===----------------------------------------------------------------------===//

class InstZodiac<dag outs, dag ins, string asmstr, list<dag> pattern>
    : Instruction {
  field bits<32> Inst;
  field bits<32> SoftFail = 0;
  let Size = 4;

  let Namespace = "Zodiac";
  let DecoderNamespace = "Zodiac";

  bits<6> Opcode;
  let Inst{31 - 26} = Opcode;

  dag OutOperandList = outs;
  dag InOperandList = ins;
  let AsmString = asmstr;
  let Pattern = pattern;
}

// Pseudo instructions (no encoding, codegen only)
class Pseudo<dag outs, dag ins, list<dag> pattern>
    : InstZodiac<outs, ins, "", pattern> {
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let Inst{31 - 0} = 0;
}

//===----------------------------------------------------------------------===//
// R-type: Register-Register ALU operations
//===----------------------------------------------------------------------===//
// Encoding: [op:6][rd:5][rs1:5][rs2:5][unused:11]
//
// Action: rd <- rs1 op rs2
//
class InstR<bits<6> op, dag outs, dag ins, string asmstr, list<dag> pattern>
    : InstZodiac<outs, ins, asmstr, pattern> {
  bits<5> rd;
  bits<5> rs1;
  bits<5> rs2;

  let Opcode = op;
  let Inst{25 - 21} = rd;
  let Inst{20 - 16} = rs1;
  let Inst{15 - 11} = rs2;
  let Inst{10 - 0} = 0;
}

//===----------------------------------------------------------------------===//
// I-type: Register-Immediate operations and Loads
//===----------------------------------------------------------------------===//
// Encoding: [op:6][rd:5][rs1:5][imm16:16]
//
// Action: rd <- rs1 op sign_extend(imm16)
//   For loads: rd <- Memory[rs1 + sign_extend(imm16)]
//
class InstI<bits<6> op, dag outs, dag ins, string asmstr, list<dag> pattern>
    : InstZodiac<outs, ins, asmstr, pattern> {
  bits<5> rd;
  bits<5> rs1;
  bits<16> imm16;

  let Opcode = op;
  let Inst{25 - 21} = rd;
  let Inst{20 - 16} = rs1;
  let Inst{15 - 0} = imm16;
}

//===----------------------------------------------------------------------===//
// S-type: Store operations
//===----------------------------------------------------------------------===//
// Encoding: [op:6][rs2:5][rs1:5][imm16:16]
//
// Action: Memory[rs1 + sign_extend(imm16)] <- rs2
//   rs2 is the value source (in the reg0 / bits[25:21] position)
//   rs1 is the base address register
//
class InstS<bits<6> op, dag outs, dag ins, string asmstr, list<dag> pattern>
    : InstZodiac<outs, ins, asmstr, pattern> {
  bits<5> rs2;   // value to store (reg0 position)
  bits<5> rs1;   // base address register
  bits<16> imm16;

  let Opcode = op;
  let Inst{25 - 21} = rs2;
  let Inst{20 - 16} = rs1;
  let Inst{15 - 0} = imm16;
}

//===----------------------------------------------------------------------===//
// U-type: Upper-immediate operations (LI, LUI, AUIPC)
//===----------------------------------------------------------------------===//
// Encoding: [op:6][rd:5][imm21:21]
//
// Action:
//   LI:    rd <- zero_extend(imm21)
//   LUI:   rd <- imm21 << 11
//   AUIPC: rd <- PC + (imm21 << 11)
//
class InstU<bits<6> op, dag outs, dag ins, string asmstr, list<dag> pattern>
    : InstZodiac<outs, ins, asmstr, pattern> {
  bits<5> rd;
  bits<21> imm21;

  let Opcode = op;
  let Inst{25 - 21} = rd;
  let Inst{20 - 0} = imm21;
}

//===----------------------------------------------------------------------===//
// B-type: Conditional branches (BEQ, BNE, BLT, BLTU, BGE, BGEU)
//===----------------------------------------------------------------------===//
// Encoding: [op:6][rs1:5][rs2:5][imm16:16]
//
// Action: if (rs1 cond rs2) PC <- PC + sign_extend(imm16)
//         else PC <- PC + 4
//
class InstB<bits<6> op, dag outs, dag ins, string asmstr, list<dag> pattern>
    : InstZodiac<outs, ins, asmstr, pattern> {
  bits<5> rs1;
  bits<5> rs2;
  bits<16> imm16;

  let Opcode = op;
  let Inst{25 - 21} = rs1;
  let Inst{20 - 16} = rs2;
  let Inst{15 - 0} = imm16;

  let isBranch = 1;
  let isTerminator = 1;
}

//===----------------------------------------------------------------------===//
// J-type: Branch and Link (BL)
//===----------------------------------------------------------------------===//
// Encoding: [op:6][rd:5][imm21:21]
//
// Action: rd <- PC + 4; PC <- PC + sign_extend(imm21)
//
class InstJ<bits<6> op, dag outs, dag ins, string asmstr, list<dag> pattern>
    : InstZodiac<outs, ins, asmstr, pattern> {
  bits<5> rd;
  bits<21> imm21;

  let Opcode = op;
  let Inst{25 - 21} = rd;
  let Inst{20 - 0} = imm21;
}

//===----------------------------------------------------------------------===//
// J26-type: Unconditional absolute branch (B)
//===----------------------------------------------------------------------===//
// Encoding: [op:6][imm26:26]
//
// Action: PC <- imm26
//
class InstJ26<bits<6> op, dag outs, dag ins, string asmstr, list<dag> pattern>
    : InstZodiac<outs, ins, asmstr, pattern> {
  bits<26> imm26;

  let Opcode = op;
  let Inst{25 - 0} = imm26;

  let isBranch = 1;
  let isTerminator = 1;
  let isBarrier = 1;
}

//===----------------------------------------------------------------------===//
// BR-type: Branch register (indirect jump)
//===----------------------------------------------------------------------===//
// Encoding: [op:6][rs1:5][zeros:21]
//
// Action: PC <- rs1
//
class InstBR<bits<6> op, dag outs, dag ins, string asmstr, list<dag> pattern>
    : InstZodiac<outs, ins, asmstr, pattern> {
  bits<5> rs1;

  let Opcode = op;
  let Inst{25 - 21} = rs1;
  let Inst{20 - 0} = 0;

  let isBranch = 1;
  let isTerminator = 1;
  let isBarrier = 1;
  let isIndirectBranch = 1;
}

//===----------------------------------------------------------------------===//
// Z-type: Zero-operand instructions (NOP, HLT)
//===----------------------------------------------------------------------===//
// Encoding: [op:6][zeros:26]
//
class InstZ<bits<6> op, dag outs, dag ins, string asmstr, list<dag> pattern>
    : InstZodiac<outs, ins, asmstr, pattern> {
  let Opcode = op;
  let Inst{25 - 0} = 0;
}
