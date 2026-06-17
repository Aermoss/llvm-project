//===-- ZodiacSelectionDAGInfo.h - Zodiac SelectionDAG Info -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the Zodiac subclass for TargetSelectionDAGInfo.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ZODIAC_ZODIACSELECTIONDAGINFO_H
#define LLVM_LIB_TARGET_ZODIAC_ZODIACSELECTIONDAGINFO_H

#include "llvm/CodeGen/SelectionDAGTargetInfo.h"
#include "llvm/Target/TargetMachine.h"

#define GET_SDNODE_ENUM
#include "ZodiacGenSDNodeInfo.inc"

namespace llvm {

class ZodiacSelectionDAGInfo : public SelectionDAGGenTargetInfo {
public:
  ZodiacSelectionDAGInfo();

  SDValue EmitTargetCodeForMemcpy(SelectionDAG &DAG, const SDLoc &dl,
                                  SDValue Chain, SDValue Dst, SDValue Src,
                                  SDValue Size, Align DstAlign, Align SrcAlign,
                                  bool isVolatile, bool AlwaysInline,
                                  MachinePointerInfo DstPtrInfo,
                                  MachinePointerInfo SrcPtrInfo) const override;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_ZODIAC_ZODIACSELECTIONDAGINFO_H
