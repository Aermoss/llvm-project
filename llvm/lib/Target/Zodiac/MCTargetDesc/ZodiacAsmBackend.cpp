//===-- ZodiacAsmBackend.cpp - Zodiac Assembler Backend ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ZodiacFixupKinds.h"
#include "MCTargetDesc/ZodiacMCTargetDesc.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

static unsigned adjustFixupValue(unsigned Kind, uint64_t Value) {
  switch (Kind) {
  case FK_Data_1:
  case FK_Data_2:
  case FK_Data_4:
  case FK_Data_8:
    return Value;
  case Zodiac::FIXUP_ZODIAC_LO11:
    return Value & 0x7FF;
  case Zodiac::FIXUP_ZODIAC_HI21:
    return (Value >> 11) & 0x1FFFFF;
  case Zodiac::FIXUP_ZODIAC_IMM16:
    return Value & 0xFFFF;
  case Zodiac::FIXUP_ZODIAC_IMM21:
    return Value & 0x1FFFFF;
  case Zodiac::FIXUP_ZODIAC_IMM26:
    return Value & 0x3FFFFFF;
  case Zodiac::FIXUP_ZODIAC_32:
    return Value;
  default:
    llvm_unreachable("Unknown fixup kind!");
  }
}

namespace {
class ZodiacAsmBackend : public MCAsmBackend {
  Triple::OSType OSType;

public:
  ZodiacAsmBackend(const Target &T, Triple::OSType OST)
      : MCAsmBackend(llvm::endianness::big), OSType(OST) {}

  void applyFixup(const MCFragment &, const MCFixup &, const MCValue &Target,
                  uint8_t *Data, uint64_t Value, bool IsResolved) override;

  std::unique_ptr<MCObjectTargetWriter>
  createObjectTargetWriter() const override;

  MCFixupKindInfo getFixupKindInfo(MCFixupKind Kind) const override;

  bool writeNopData(raw_ostream &OS, uint64_t Count,
                    const MCSubtargetInfo *STI) const override;
};

bool ZodiacAsmBackend::writeNopData(raw_ostream &OS, uint64_t Count,
                                   const MCSubtargetInfo *STI) const {
  if ((Count % 4) != 0)
    return false;

  for (uint64_t i = 0; i < Count; i += 4)
    OS.write("\0\0\0\0", 4);

  return true;
}

void ZodiacAsmBackend::applyFixup(const MCFragment &F, const MCFixup &Fixup,
                                 const MCValue &Target, uint8_t *Data,
                                 uint64_t Value, bool IsResolved) {
  if (!IsResolved)
    Asm->getWriter().recordRelocation(F, Fixup, Target, Value);

  MCFixupKind Kind = Fixup.getKind();
  Value = adjustFixupValue(static_cast<unsigned>(Kind), Value);
  if (!Value)
    return; // This value doesn't change the encoding

  unsigned NumBytes = (getFixupKindInfo(Kind).TargetSize + 7) / 8;
  unsigned FullSize = 4;

  uint64_t CurVal = 0;

  for (unsigned i = 0; i != NumBytes; ++i) {
    unsigned Idx = (FullSize - 1 - i);
    CurVal |= static_cast<uint64_t>(static_cast<uint8_t>(Data[Idx])) << (i * 8);
  }

  uint64_t Mask =
      (static_cast<uint64_t>(-1) >> (64 - getFixupKindInfo(Kind).TargetSize));
  CurVal |= Value & Mask;

  for (unsigned i = 0; i != NumBytes; ++i) {
    unsigned Idx = (FullSize - 1 - i);
    Data[Idx] = static_cast<uint8_t>((CurVal >> (i * 8)) & 0xff);
  }
}

std::unique_ptr<MCObjectTargetWriter>
ZodiacAsmBackend::createObjectTargetWriter() const {
  return createZodiacELFObjectWriter(MCELFObjectTargetWriter::getOSABI(OSType));
}

MCFixupKindInfo ZodiacAsmBackend::getFixupKindInfo(MCFixupKind Kind) const {
  static const MCFixupKindInfo Infos[Zodiac::NumTargetFixupKinds] = {
      // name, offset, bits, flags
      {"FIXUP_ZODIAC_LO11", 16, 16, 0},
      {"FIXUP_ZODIAC_HI21", 11, 21, 0},
      {"FIXUP_ZODIAC_IMM16", 16, 16, 0},
      {"FIXUP_ZODIAC_IMM21", 11, 21, 0},
      {"FIXUP_ZODIAC_IMM26", 6, 26, 0},
      {"FIXUP_ZODIAC_32", 0, 32, 0}};

  if (Kind < FirstTargetFixupKind)
    return MCAsmBackend::getFixupKindInfo(Kind);

  assert(unsigned(Kind - FirstTargetFixupKind) < Zodiac::NumTargetFixupKinds &&
         "Invalid kind!");
  return Infos[Kind - FirstTargetFixupKind];
}

} // namespace

MCAsmBackend *llvm::createZodiacAsmBackend(const Target &T,
                                          const MCSubtargetInfo &STI,
                                          const MCRegisterInfo & /*MRI*/,
                                          const MCTargetOptions & /*Options*/) {
  const Triple &TT = STI.getTargetTriple();
  if (!TT.isOSBinFormatELF())
    llvm_unreachable("OS not supported");

  return new ZodiacAsmBackend(T, TT.getOS());
}
