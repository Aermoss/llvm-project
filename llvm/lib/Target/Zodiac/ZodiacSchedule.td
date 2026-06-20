//===- ZodiacSchedule.td - Zodiac Scheduling Definitions ----*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Zodiac is a single-issue, in-order, multi-cycle CPU with no delay slots.
//
//===----------------------------------------------------------------------===//

def ZodiacSchedModel : SchedMachineModel {
  let IssueWidth = 1;
  let MicroOpBufferSize = 0; // in-order
  let CompleteModel = 0;
  let PostRAScheduler = 0;
}

// Instruction itinerary classes
def IIC_ALU : InstrItinClass;

// SchedWrite definitions
def WriteALU : SchedWrite;
