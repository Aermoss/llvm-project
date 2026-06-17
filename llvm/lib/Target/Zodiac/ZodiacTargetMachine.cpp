//===-- ZodiacTargetMachine.cpp - Define TargetMachine for Zodiac ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Implements the info about Zodiac target spec.
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
  // Register the target.
  RegisterTargetMachine<ZodiacTargetMachine> registered_target(
      getTheZodiacTarget());
  PassRegistry &PR = *PassRegistry::getPassRegistry();
  initializeZodiacAsmPrinterPass(PR);
  initializeZodiacDAGToDAGISelLegacyPass(PR);
  initializeZodiacMemAluCombinerPass(PR);
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
          T, TT.computeDataLayout(), TT, Cpu, FeatureString, Options,
          getEffectiveRelocModel(RM),
          getEffectiveCodeModel(CodeModel, CodeModel::Medium), OptLevel),
      Subtarget(TT, Cpu, FeatureString, *this, Options, getCodeModel(),
                OptLevel),
      TLOF(new ZodiacTargetObjectFile()) {
  initAsmInfo();
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
// Zodiac Code Generator Pass Configuration Options.
class ZodiacPassConfig : public TargetPassConfig {
public:
  ZodiacPassConfig(ZodiacTargetMachine &TM, PassManagerBase *PassManager)
      : TargetPassConfig(TM, *PassManager) {}

  ZodiacTargetMachine &getZodiacTargetMachine() const {
    return getTM<ZodiacTargetMachine>();
  }

  void addIRPasses() override;
  bool addInstSelector() override;
  void addPreSched2() override;
  void addPreEmitPass() override;
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

// Install an instruction selector pass.
bool ZodiacPassConfig::addInstSelector() {
  addPass(createZodiacISelDag(getZodiacTargetMachine()));
  return false;
}

// Implemented by targets that want to run passes immediately before
// machine code is emitted.
void ZodiacPassConfig::addPreEmitPass() {
  addPass(createZodiacDelaySlotFillerPass(getZodiacTargetMachine()));
}

// Run passes after prolog-epilog insertion and before the second instruction
// scheduling pass.
void ZodiacPassConfig::addPreSched2() {
  addPass(createZodiacMemAluCombinerPass());
}
