//===- ZodiacRegisterInfo.td - Zodiac Register defs ----------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//  Declarations that describe the Zodiac register file
//===----------------------------------------------------------------------===//

// Registers are identified with 5-bit ID numbers.
class ZodiacReg<bits<5> num, string n, list<string> altNames = []>
    : Register<n, altNames> {
  field bits<5> Num;
  let Num = num;
  let Namespace = "Zodiac";
}

// Integer registers x0-x31
// x0 is hardwired to zero.
foreach i = 0-31 in {
  def X#i : ZodiacReg<i, "x"#i>, DwarfRegNum<[i]>;
}

// Register classes.
//
// Convention:
//   x0       = zero (hardwired)
//   x1-x4    = arguments / return values (caller-saved)
//   x5-x11   = temporaries (caller-saved)
//   x12-x27  = saved registers (callee-saved)
//   x28      = temporary (caller-saved)
//   x29      = frame pointer (callee-saved)
//   x30      = return address / link register (callee-saved)
//   x31      = stack pointer
def GPR : RegisterClass<"Zodiac", [i32], 32,
    (add
     // Argument / return value registers (caller-saved, allocated first)
     X1, X2, X3, X4,
     // Temporaries (caller-saved)
     X5, X6, X7, X8, X9, X10, X11, X28,
     // Callee-saved registers
     X12, X13, X14, X15, X16, X17,
     X18, X19, X20, X21, X22, X23, X24, X25, X26, X27,
     // Frame pointer (callee-saved)
     X29,
     // Return address (callee-saved)
     X30,
     // Stack pointer (reserved)
     X31,
     // Zero register (never allocated)
     X0
    )>;
