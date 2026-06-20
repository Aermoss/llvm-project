add_llvm_component_group(Zodiac)

set(LLVM_TARGET_DEFINITIONS Zodiac.td)

tablegen(LLVM ZodiacGenAsmMatcher.inc -gen-asm-matcher)
tablegen(LLVM ZodiacGenAsmWriter.inc -gen-asm-writer)
tablegen(LLVM ZodiacGenCallingConv.inc -gen-callingconv)
tablegen(LLVM ZodiacGenDAGISel.inc -gen-dag-isel)
tablegen(LLVM ZodiacGenDisassemblerTables.inc -gen-disassembler)
tablegen(LLVM ZodiacGenInstrInfo.inc -gen-instr-info)
tablegen(LLVM ZodiacGenMCCodeEmitter.inc -gen-emitter)
tablegen(LLVM ZodiacGenRegisterInfo.inc -gen-register-info)
tablegen(LLVM ZodiacGenSDNodeInfo.inc -gen-sd-node-info)
tablegen(LLVM ZodiacGenSubtargetInfo.inc -gen-subtarget)

add_public_tablegen_target(ZodiacCommonTableGen)

add_llvm_target(ZodiacCodeGen
  ZodiacAsmPrinter.cpp
  ZodiacFrameLowering.cpp
  ZodiacInstrInfo.cpp
  ZodiacISelDAGToDAG.cpp
  ZodiacISelLowering.cpp
  ZodiacMachineFunctionInfo.cpp
  ZodiacMCInstLower.cpp
  ZodiacRegisterInfo.cpp
  ZodiacSelectionDAGInfo.cpp
  ZodiacSubtarget.cpp
  ZodiacTargetMachine.cpp
  ZodiacTargetObjectFile.cpp

  LINK_COMPONENTS
  Analysis
  AsmPrinter
  CodeGen
  CodeGenTypes
  Core
  ZodiacAsmParser
  ZodiacDesc
  ZodiacInfo
  MC
  SelectionDAG
  Support
  Target
  TargetParser
  TransformUtils

  ADD_TO_COMPONENT
  Zodiac
  )

add_subdirectory(AsmParser)
add_subdirectory(Disassembler)
add_subdirectory(MCTargetDesc)
add_subdirectory(TargetInfo)
