//===- Zodiac.td - Describe the Zodiac Target Machine ------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Target-independent interfaces which we are implementing
//===----------------------------------------------------------------------===//

include "llvm/Target/Target.td"

//===----------------------------------------------------------------------===//
// Register File, Calling Conv, Instruction Descriptions
//===----------------------------------------------------------------------===//

include "ZodiacSchedule.td"
include "ZodiacRegisterInfo.td"
include "ZodiacCallingConv.td"
include "ZodiacInstrInfo.td"

def ZodiacInstrInfo : InstrInfo;

//===----------------------------------------------------------------------===//
// Zodiac processors supported.
//===----------------------------------------------------------------------===//

def : ProcessorModel<"generic", ZodiacSchedModel, []>;

def ZodiacInstPrinter : AsmWriter {
  string AsmWriterClassName  = "InstPrinter";
  bit isMCAsmWriter = 1;
}

//===----------------------------------------------------------------------===//
// Declare the target which we are implementing
//===----------------------------------------------------------------------===//

def Zodiac : Target {
  // Pull in Instruction Info:
  let InstructionSet = ZodiacInstrInfo;
  let AssemblyWriters = [ZodiacInstPrinter];
}

defm : RemapAllTargetPseudoPointerOperands<GPR>;
