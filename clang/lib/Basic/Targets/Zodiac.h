//===--- Zodiac.h - Declare Zodiac target feature support -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares Zodiac TargetInfo objects.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_BASIC_TARGETS_ZODIAC_H
#define LLVM_CLANG_LIB_BASIC_TARGETS_ZODIAC_H

#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/TargetOptions.h"
#include "llvm/Support/Compiler.h"
#include "llvm/TargetParser/Triple.h"

namespace clang {
namespace targets {

class LLVM_LIBRARY_VISIBILITY ZodiacTargetInfo : public TargetInfo {
  static const char *const GCCRegNames[];

public:
  ZodiacTargetInfo(const llvm::Triple &Triple, const TargetOptions &)
      : TargetInfo(Triple) {
    TLSSupported = false;
    IntWidth = 32;
    IntAlign = 32;
    LongWidth = 32;
    LongLongWidth = 64;
    LongAlign = 32;
    LongLongAlign = 64;
    FloatWidth = 32;
    FloatAlign = 32;
    DoubleWidth = LongDoubleWidth = 64;
    DoubleAlign = LongDoubleAlign = 64;
    PointerWidth = 32;
    PointerAlign = 32;
    SuitableAlign = 32;
    SizeType = UnsignedInt;
    IntMaxType = SignedLongLong;
    IntPtrType = SignedInt;
    PtrDiffType = SignedInt;
    SigAtomicType = SignedInt;
    resetDataLayout("E-m:e-p:32:32-i64:64-n32-S64");
  }
  void getTargetDefines(const LangOptions &Opts,
                        MacroBuilder &Builder) const override;

  llvm::SmallVector<Builtin::InfosShard> getTargetBuiltins() const override {
    return {};
  }

  bool allowsLargerPreferedTypeAlignment() const override { return false; }

  bool hasFeature(StringRef Feature) const override {
    return Feature == "zodiac";
  }

  ArrayRef<const char *> getGCCRegNames() const override;

  ArrayRef<TargetInfo::GCCRegAlias> getGCCRegAliases() const override {
    return {};
  }

  bool validateAsmConstraint(const char *&Name,
                             TargetInfo::ConstraintInfo &info) const override {
    return false;
  }

  std::string_view getClobbers() const override {
    return "";
  }

  BuiltinVaListKind getBuiltinVaListKind() const override {
    return TargetInfo::CharPtrBuiltinVaList;
  }
};

} // namespace targets
} // namespace clang
#endif // LLVM_CLANG_LIB_BASIC_TARGETS_ZODIAC_H
