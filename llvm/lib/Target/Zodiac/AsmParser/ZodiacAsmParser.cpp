//===-- ZodiacAsmParser.cpp - Parse Zodiac assembly to MCInst --------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/ZodiacMCAsmInfo.h"
#include "MCTargetDesc/ZodiacMCTargetDesc.h"
#include "TargetInfo/ZodiacTargetInfo.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCParser/AsmLexer.h"
#include "llvm/MC/MCParser/MCAsmParser.h"
#include "llvm/MC/MCParser/MCParsedAsmOperand.h"
#include "llvm/MC/MCParser/MCTargetAsmParser.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/SMLoc.h"
#include "llvm/Support/raw_ostream.h"
#include <cassert>
#include <memory>

using namespace llvm;

static MCRegister MatchRegisterName(StringRef Name);

namespace {

struct ZodiacOperand;

class ZodiacAsmParser : public MCTargetAsmParser {
  MCAsmParser &Parser;
  AsmLexer &Lexer;

  std::unique_ptr<ZodiacOperand> parseRegister();

  ParseStatus parseOperand(OperandVector &Operands, StringRef Mnemonic);

  bool parseInstruction(ParseInstructionInfo &Info, StringRef Name,
                        SMLoc NameLoc, OperandVector &Operands) override;

  bool parseRegister(MCRegister &Reg, SMLoc &StartLoc, SMLoc &EndLoc) override;
  ParseStatus tryParseRegister(MCRegister &Reg, SMLoc &StartLoc,
                               SMLoc &EndLoc) override;

  bool matchAndEmitInstruction(SMLoc IdLoc, unsigned &Opcode,
                               OperandVector &Operands, MCStreamer &Out,
                               uint64_t &ErrorInfo,
                               bool MatchingInlineAsm) override;

#define GET_ASSEMBLER_HEADER
#include "ZodiacGenAsmMatcher.inc"

public:
  ZodiacAsmParser(const MCSubtargetInfo &STI, MCAsmParser &Parser,
                  const MCInstrInfo &MII)
      : MCTargetAsmParser(STI, MII), Parser(Parser), Lexer(Parser.getLexer()) {
    setAvailableFeatures(
        ComputeAvailableFeatures(STI.getFeatureBits()));
  }
};

struct ZodiacOperand : public MCParsedAsmOperand {
  enum KindTy {
    TOKEN,
    REGISTER,
    IMMEDIATE
  } Kind;

  SMLoc StartLoc, EndLoc;

  union {
    struct {
      const char *Data;
      unsigned Length;
    } Tok;

    struct {
      MCRegister RegNum;
    } Reg;

    struct {
      const MCExpr *Val;
    } Imm;
  };

  ZodiacOperand(KindTy K) : Kind(K) {}

  bool isToken() const override { return Kind == TOKEN; }
  bool isReg() const override { return Kind == REGISTER; }
  bool isImm() const override { return Kind == IMMEDIATE; }
  bool isMem() const override { return false; }

  SMLoc getStartLoc() const override { return StartLoc; }
  SMLoc getEndLoc() const override { return EndLoc; }

  MCRegister getReg() const override { return Reg.RegNum; }
  const MCExpr *getImm() const { return Imm.Val; }
  StringRef getToken() const { return StringRef(Tok.Data, Tok.Length); }

  bool isGPR() const { return isReg(); }
  bool isSimm16() const { return isImm(); }
  bool isUimm21() const { return isImm(); }
  bool isBrTarget16() const { return isImm(); }
  bool isBrTarget21() const { return isImm(); }
  bool isBrTarget26() const { return isImm(); }
  bool isCallTarget() const { return isImm(); }

  void addRegOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    Inst.addOperand(MCOperand::createReg(getReg()));
  }

  void addImmOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    const MCExpr *Expr = getImm();
    if (const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(Expr))
      Inst.addOperand(MCOperand::createImm(CE->getValue()));
    else
      Inst.addOperand(MCOperand::createExpr(Expr));
  }

  void print(raw_ostream &OS, const MCAsmInfo &MAI) const override {
    switch (Kind) {
    case TOKEN: OS << "Token: " << getToken(); break;
    case REGISTER: OS << "Reg: " << getReg(); break;
    case IMMEDIATE: OS << "Imm: "; MAI.printExpr(OS, *getImm()); break;
    }
  }

  static std::unique_ptr<ZodiacOperand> createToken(StringRef Str, SMLoc S) {
    auto Op = std::make_unique<ZodiacOperand>(TOKEN);
    Op->Tok.Data = Str.data();
    Op->Tok.Length = Str.size();
    Op->StartLoc = S;
    Op->EndLoc = S;
    return Op;
  }

  static std::unique_ptr<ZodiacOperand> createReg(MCRegister Reg, SMLoc S, SMLoc E) {
    auto Op = std::make_unique<ZodiacOperand>(REGISTER);
    Op->Reg.RegNum = Reg;
    Op->StartLoc = S;
    Op->EndLoc = E;
    return Op;
  }

  static std::unique_ptr<ZodiacOperand> createImm(const MCExpr *Val, SMLoc S, SMLoc E) {
    auto Op = std::make_unique<ZodiacOperand>(IMMEDIATE);
    Op->Imm.Val = Val;
    Op->StartLoc = S;
    Op->EndLoc = E;
    return Op;
  }
};

} // end anonymous namespace

static MCRegister MatchRegisterAltName(StringRef Name) {
  return StringSwitch<MCRegister>(Name.lower())
      .Case("zero", Zodiac::X0)
      .Case("fp", Zodiac::X29)
      .Case("lr", Zodiac::X30)
      .Case("sp", Zodiac::X31)
      .Default(0);
}

std::unique_ptr<ZodiacOperand> ZodiacAsmParser::parseRegister() {
  SMLoc S = Parser.getTok().getLoc();
  if (Parser.getTok().isNot(AsmToken::Identifier))
    return nullptr;
  StringRef Name = Parser.getTok().getIdentifier();
  MCRegister RegNum = MatchRegisterName(Name);
  if (RegNum == 0)
    RegNum = MatchRegisterAltName(Name);
  if (RegNum == 0)
    return nullptr;
  Parser.Lex();
  return ZodiacOperand::createReg(RegNum, S, Parser.getTok().getLoc());
}

ParseStatus ZodiacAsmParser::parseOperand(OperandVector &Operands,
                                         StringRef Mnemonic) {
  SMLoc S = Parser.getTok().getLoc();

  // Case 1: Memory operand of the form "(register)"
  if (Lexer.is(AsmToken::LParen)) {
    Parser.Lex(); // Consume '('
    auto BaseRegOp = parseRegister();
    if (!BaseRegOp)
      return Error(Parser.getTok().getLoc(), "expected register inside '(' ')'");
    if (Lexer.isNot(AsmToken::RParen))
      return Error(Parser.getTok().getLoc(), "expected ')'");
    SMLoc RParenLoc = Lexer.getLoc();
    Parser.Lex(); // Consume ')'

    const MCExpr *OffsetExpr = MCConstantExpr::create(0, getContext());
    Operands.push_back(ZodiacOperand::createImm(OffsetExpr, S, S));
    Operands.push_back(ZodiacOperand::createToken("(", S));
    Operands.push_back(std::move(BaseRegOp));
    Operands.push_back(ZodiacOperand::createToken(")", RParenLoc));
    return ParseStatus::Success;
  }

  // Case 2: Try to parse a register first
  auto RegOp = parseRegister();
  if (RegOp) {
    Operands.push_back(std::move(RegOp));
    return ParseStatus::Success;
  }

  // Case 3: Parse an expression (could be an immediate or memory offset)
  const MCExpr *ExprVal = nullptr;
  if (Parser.parseExpression(ExprVal)) {
    return ParseStatus::Failure;
  }

  // If the next token is '(', then it is a memory operand "expression(register)"
  if (Lexer.is(AsmToken::LParen)) {
    SMLoc LParenLoc = Lexer.getLoc();
    Parser.Lex(); // Consume '('
    auto BaseRegOp = parseRegister();
    if (!BaseRegOp)
      return Error(Parser.getTok().getLoc(), "expected register inside '(' ')'");
    if (Lexer.isNot(AsmToken::RParen))
      return Error(Parser.getTok().getLoc(), "expected ')'");
    SMLoc RParenLoc = Lexer.getLoc();
    Parser.Lex(); // Consume ')'

    Operands.push_back(ZodiacOperand::createImm(ExprVal, S, LParenLoc));
    Operands.push_back(ZodiacOperand::createToken("(", LParenLoc));
    Operands.push_back(std::move(BaseRegOp));
    Operands.push_back(ZodiacOperand::createToken(")", RParenLoc));
    return ParseStatus::Success;
  }

  // Otherwise, it is just a plain immediate
  Operands.push_back(ZodiacOperand::createImm(ExprVal, S, Parser.getTok().getLoc()));
  return ParseStatus::Success;
}

bool ZodiacAsmParser::parseInstruction(ParseInstructionInfo &Info,
                                      StringRef Name, SMLoc NameLoc,
                                      OperandVector &Operands) {
  Operands.push_back(ZodiacOperand::createToken(Name, NameLoc));

  if (Lexer.is(AsmToken::EndOfStatement))
    return false;

  while (true) {
    if (!parseOperand(Operands, Name).isSuccess())
      return true;

    if (Lexer.is(AsmToken::Comma)) {
      Parser.Lex();
    } else if (Lexer.is(AsmToken::EndOfStatement)) {
      break;
    } else {
      return Error(Parser.getTok().getLoc(), "unexpected token in operand list");
    }
  }

  return false;
}

bool ZodiacAsmParser::parseRegister(MCRegister &Reg, SMLoc &StartLoc, SMLoc &EndLoc) {
  StartLoc = Parser.getTok().getLoc();
  auto Op = parseRegister();
  if (!Op)
    return true;
  Reg = Op->getReg();
  EndLoc = Op->getEndLoc();
  return false;
}

ParseStatus ZodiacAsmParser::tryParseRegister(MCRegister &Reg, SMLoc &StartLoc,
                                             SMLoc &EndLoc) {
  StartLoc = Parser.getTok().getLoc();
  auto Op = parseRegister();
  if (!Op)
    return ParseStatus::NoMatch;
  Reg = Op->getReg();
  EndLoc = Op->getEndLoc();
  return ParseStatus::Success;
}

bool ZodiacAsmParser::matchAndEmitInstruction(SMLoc IdLoc, unsigned &Opcode,
                                             OperandVector &Operands,
                                             MCStreamer &Out,
                                             uint64_t &ErrorInfo,
                                             bool MatchingInlineAsm) {
  MCInst Inst;
  unsigned MatchResult =
      MatchInstructionImpl(Operands, Inst, ErrorInfo, MatchingInlineAsm);
  switch (MatchResult) {
  case Match_Success:
    Inst.setLoc(IdLoc);
    Out.emitInstruction(Inst, getSTI());
    return false;
  case Match_MissingFeature:
    return Error(IdLoc, "instruction requires a backend feature not enabled");
  case Match_MnemonicFail:
    return Error(IdLoc, "invalid instruction mnemonic");
  case Match_InvalidOperand: {
    SMLoc ErrorLoc = IdLoc;
    if (ErrorInfo != ~0ULL) {
      if (ErrorInfo < Operands.size())
        ErrorLoc = Operands[ErrorInfo]->getStartLoc();
    }
    return Error(ErrorLoc, "invalid operand for instruction");
  }
  }
  llvm_unreachable("Unexpected match result");
}

#define GET_REGISTER_MATCHER
#define GET_MATCHER_IMPLEMENTATION
#include "ZodiacGenAsmMatcher.inc"

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeZodiacAsmParser() {
  RegisterMCAsmParser<ZodiacAsmParser> X(getTheZodiacTarget());
}
