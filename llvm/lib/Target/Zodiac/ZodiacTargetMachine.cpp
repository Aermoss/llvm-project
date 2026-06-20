//===-- ZodiacTargetMachine.cpp - Define TargetMachine for Zodiac ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ZodiacTargetMachine.h"
#include "Zodiac.h"
#include "ZodiacMachineFunctionInfo.h"
#include "ZodiacTargetObjectFile.h"
#include "ZodiacTargetTransformInfo.h"
#include "TargetInfo/ZodiacTargetInfo.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Target/TargetOptions.h"
#include <optional>

using namespace llvm;

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void LLVMInitializeZodiacTarget() {
  RegisterTargetMachine<ZodiacTargetMachine> registered_target(
      getTheZodiacTarget());
  PassRegistry &PR = *PassRegistry::getPassRegistry();
  initializeZodiacAsmPrinterPass(PR);
  initializeZodiacDAGToDAGISelLegacyPass(PR);
}

static Reloc::Model getEffectiveRelocModel(std::optional<Reloc::Model> RM) {
  return RM.value_or(Reloc::PIC_);
}

ZodiacTargetMachine::ZodiacTargetMachine(
    const Target &T, const Triple &TT, StringRef Cpu, StringRef FeatureString,
    const TargetOptions &Options, std::optional<Reloc::Model> RM,
    std::optional<CodeModel::Model> CodeModel, CodeGenOptLevel OptLevel,
    bool JIT)
    : CodeGenTargetMachineImpl(
          T, "E-m:e-p:32:32-i64:64-n32-S64", TT, Cpu, FeatureString, Options,
          getEffectiveRelocModel(RM),
          getEffectiveCodeModel(CodeModel, CodeModel::Medium), OptLevel),
      Subtarget(TT, Cpu, FeatureString, *this, Options, getCodeModel(),
                OptLevel),
      TLOF(new ZodiacTargetObjectFile()) {
  initAsmInfo();

  // Disable verbose assembly output by default — the Zodiac bare-metal
  // assembler doesn't understand comments. Can be re-enabled with
  // -asm-verbose=1.
  this->Options.MCOptions.AsmVerbose = false;

  // Disable .addrsig directive — Zodiac is bare-metal with no linker support.
  this->Options.EmitAddrsig = false;
}

TargetTransformInfo
ZodiacTargetMachine::getTargetTransformInfo(const Function &F) const {
  return TargetTransformInfo(std::make_unique<ZodiacTTIImpl>(this, F));
}

MachineFunctionInfo *ZodiacTargetMachine::createMachineFunctionInfo(
    BumpPtrAllocator &Allocator, const Function &F,
    const TargetSubtargetInfo *STI) const {
  return ZodiacMachineFunctionInfo::create<ZodiacMachineFunctionInfo>(Allocator,
                                                                    F, STI);
}

namespace {
class ZodiacPassConfig : public TargetPassConfig {
public:
  ZodiacPassConfig(ZodiacTargetMachine &TM, PassManagerBase *PassManager)
      : TargetPassConfig(TM, *PassManager) {}

  ZodiacTargetMachine &getZodiacTargetMachine() const {
    return getTM<ZodiacTargetMachine>();
  }

  void addIRPasses() override;
  bool addInstSelector() override;
};
} // namespace

TargetPassConfig *
ZodiacTargetMachine::createPassConfig(PassManagerBase &PassManager) {
  return new ZodiacPassConfig(*this, &PassManager);
}

void ZodiacPassConfig::addIRPasses() {
  addPass(createAtomicExpandLegacyPass());
  TargetPassConfig::addIRPasses();
}

bool ZodiacPassConfig::addInstSelector() {
  addPass(createZodiacISelDag(getZodiacTargetMachine()));
  return false;
}
