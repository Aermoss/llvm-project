//===-- ZodiacELFObjectWriter.cpp - Zodiac ELF Writer -----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/ZodiacBaseInfo.h"
#include "MCTargetDesc/ZodiacFixupKinds.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

namespace {

class ZodiacELFObjectWriter : public MCELFObjectTargetWriter {
public:
  explicit ZodiacELFObjectWriter(uint8_t OSABI);

  ~ZodiacELFObjectWriter() override = default;

protected:
  unsigned getRelocType(const MCFixup &, const MCValue &,
                        bool IsPCRel) const override;
  bool needsRelocateWithSymbol(const MCValue &, unsigned Type) const override;
};

} // end anonymous namespace

ZodiacELFObjectWriter::ZodiacELFObjectWriter(uint8_t OSABI)
    : MCELFObjectTargetWriter(/*Is64Bit_=*/false, OSABI, ELF::EM_ZODIAC,
                              /*HasRelocationAddend_=*/true) {}

unsigned ZodiacELFObjectWriter::getRelocType(const MCFixup &Fixup,
                                            const MCValue &, bool) const {
  unsigned Type;
  unsigned Kind = static_cast<unsigned>(Fixup.getKind());
  switch (Kind) {
  case Zodiac::FIXUP_ZODIAC_21:
    Type = ELF::R_ZODIAC_21;
    break;
  case Zodiac::FIXUP_ZODIAC_21_F:
    Type = ELF::R_ZODIAC_21_F;
    break;
  case Zodiac::FIXUP_ZODIAC_25:
    Type = ELF::R_ZODIAC_25;
    break;
  case Zodiac::FIXUP_ZODIAC_32:
  case FK_Data_4:
    Type = ELF::R_ZODIAC_32;
    break;
  case Zodiac::FIXUP_ZODIAC_HI16:
    Type = ELF::R_ZODIAC_HI16;
    break;
  case Zodiac::FIXUP_ZODIAC_LO16:
    Type = ELF::R_ZODIAC_LO16;
    break;
  case Zodiac::FIXUP_ZODIAC_NONE:
    Type = ELF::R_ZODIAC_NONE;
    break;

  default:
    llvm_unreachable("Invalid fixup kind!");
  }
  return Type;
}

bool ZodiacELFObjectWriter::needsRelocateWithSymbol(const MCValue &,
                                                   unsigned Type) const {
  switch (Type) {
  case ELF::R_ZODIAC_21:
  case ELF::R_ZODIAC_21_F:
  case ELF::R_ZODIAC_25:
  case ELF::R_ZODIAC_32:
  case ELF::R_ZODIAC_HI16:
    return true;
  default:
    return false;
  }
}

std::unique_ptr<MCObjectTargetWriter>
llvm::createZodiacELFObjectWriter(uint8_t OSABI) {
  return std::make_unique<ZodiacELFObjectWriter>(OSABI);
}
