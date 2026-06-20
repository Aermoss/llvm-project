//===-- ZodiacMCTargetDesc.cpp - Zodiac Target Descriptions -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file provides Zodiac specific target descriptions.
//
//===----------------------------------------------------------------------===//

#include "ZodiacMCTargetDesc.h"
#include "ZodiacInstPrinter.h"
#include "ZodiacMCAsmInfo.h"
#include "TargetInfo/ZodiacTargetInfo.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrAnalysis.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/TargetParser/Triple.h"
#include <cstdint>
#include <string>

#define GET_INSTRINFO_MC_DESC
#define ENABLE_INSTR_PREDICATE_VERIFIER
#include "ZodiacGenInstrInfo.inc"

#define GET_SUBTARGETINFO_MC_DESC
#include "ZodiacGenSubtargetInfo.inc"

#define GET_REGINFO_MC_DESC
#include "ZodiacGenRegisterInfo.inc"

using namespace llvm;

static MCInstrInfo *createZodiacMCInstrInfo() {
  MCInstrInfo *X = new MCInstrInfo();
  InitZodiacMCInstrInfo(X);
  return X;
}

static MCRegisterInfo *createZodiacMCRegisterInfo(const Triple & /*TT*/) {
  MCRegisterInfo *X = new MCRegisterInfo();
  InitZodiacMCRegisterInfo(X, Zodiac::X30, 0, 0, Zodiac::X30);
  return X;
}

static MCSubtargetInfo *
createZodiacMCSubtargetInfo(const Triple &TT, StringRef CPU, StringRef FS) {
  std::string CPUName = std::string(CPU);
  if (CPUName.empty())
    CPUName = "generic";

  return createZodiacMCSubtargetInfoImpl(TT, CPUName, /*TuneCPU*/ CPUName, FS);
}

static MCStreamer *createMCStreamer(const Triple &T, MCContext &Context,
                                    std::unique_ptr<MCAsmBackend> &&MAB,
                                    std::unique_ptr<MCObjectWriter> &&OW,
                                    std::unique_ptr<MCCodeEmitter> &&Emitter) {
  if (!T.isOSBinFormatELF())
    llvm_unreachable("OS not supported");

  return createELFStreamer(Context, std::move(MAB), std::move(OW),
                           std::move(Emitter));
}

static MCInstPrinter *createZodiacMCInstPrinter(const Triple & /*T*/,
                                               unsigned SyntaxVariant,
                                               const MCAsmInfo &MAI,
                                               const MCInstrInfo &MII,
                                               const MCRegisterInfo &MRI) {
  if (SyntaxVariant == 0)
    return new ZodiacInstPrinter(MAI, MII, MRI);
  return nullptr;
}

static MCRelocationInfo *createZodiacElfRelocation(const Triple &TheTriple,
                                                  MCContext &Ctx) {
  return createMCRelocationInfo(TheTriple, Ctx);
}

namespace {

class ZodiacMCInstrAnalysis : public MCInstrAnalysis {
public:
  explicit ZodiacMCInstrAnalysis(const MCInstrInfo *Info)
      : MCInstrAnalysis(Info) {}

  bool evaluateBranch(const MCInst &Inst, uint64_t Addr, uint64_t Size,
                      uint64_t &Target) const override {
    return false;
  }
};

} // end anonymous namespace

static MCInstrAnalysis *createZodiacInstrAnalysis(const MCInstrInfo *Info) {
  return new ZodiacMCInstrAnalysis(Info);
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeZodiacTargetMC() {
  // Register the MC asm info.
  RegisterMCAsmInfo<ZodiacMCAsmInfo> X(getTheZodiacTarget());

  // Register the MC instruction info.
  TargetRegistry::RegisterMCInstrInfo(getTheZodiacTarget(),
                                      createZodiacMCInstrInfo);

  // Register the MC register info.
  TargetRegistry::RegisterMCRegInfo(getTheZodiacTarget(),
                                    createZodiacMCRegisterInfo);

  // Register the MC subtarget info.
  TargetRegistry::RegisterMCSubtargetInfo(getTheZodiacTarget(),
                                          createZodiacMCSubtargetInfo);

  // Register the MC code emitter
  TargetRegistry::RegisterMCCodeEmitter(getTheZodiacTarget(),
                                        createZodiacMCCodeEmitter);

  // Register the ASM Backend
  TargetRegistry::RegisterMCAsmBackend(getTheZodiacTarget(),
                                       createZodiacAsmBackend);

  // Register the MCInstPrinter.
  TargetRegistry::RegisterMCInstPrinter(getTheZodiacTarget(),
                                        createZodiacMCInstPrinter);

  // Register the ELF streamer.
  TargetRegistry::RegisterELFStreamer(getTheZodiacTarget(), createMCStreamer);

  // Register the MC relocation info.
  TargetRegistry::RegisterMCRelocationInfo(getTheZodiacTarget(),
                                           createZodiacElfRelocation);

  // Register the MC instruction analyzer.
  TargetRegistry::RegisterMCInstrAnalysis(getTheZodiacTarget(),
                                          createZodiacInstrAnalysis);
}
