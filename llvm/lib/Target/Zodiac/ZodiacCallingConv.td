//===- ZodiacCallingConv.td - Calling Conventions Zodiac ----*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This describes the calling conventions for the Zodiac architecture.
//
// Register usage:
//   x0       = zero (hardwired)
//   x1-x4    = arguments / return values (caller-saved)
//   x5-x11   = temporaries (caller-saved)
//   x12-x27  = saved registers (callee-saved)
//   x28      = temporary (caller-saved)
//   x29      = frame pointer (callee-saved)
//   x30      = return address / link register (callee-saved)
//   x31      = stack pointer (reserved)
//
//===----------------------------------------------------------------------===//

// Zodiac 32-bit C Calling Convention.
def CC_Zodiac32 : CallingConv<[
  // Promote i8/i16 args to i32
  CCIfType<[i8, i16], CCPromoteToType<i32>>,

  // Pass i32 arguments in registers x1-x4
  CCIfType<[i32], CCAssignToReg<[X1, X2, X3, X4]>>,

  // Otherwise they are assigned to the stack in 4-byte aligned units.
  CCAssignToStack<4, 4>
]>;

// Zodiac 32-bit C return-value convention.
def RetCC_Zodiac32 : CallingConv<[
  CCIfType<[i32], CCAssignToReg<[X1]>>
]>;

// Callee-saved registers.
def CSR : CalleeSavedRegs<(add X12, X13, X14, X15, X16, X17,
                               X18, X19, X20, X21, X22, X23,
                               X24, X25, X26, X27, X29, X30)>;

