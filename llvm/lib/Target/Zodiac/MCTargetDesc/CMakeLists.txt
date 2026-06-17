add_llvm_component_library(LLVMZodiacDesc
  ZodiacAsmBackend.cpp
  ZodiacELFObjectWriter.cpp
  ZodiacInstPrinter.cpp
  ZodiacMCAsmInfo.cpp
  ZodiacMCCodeEmitter.cpp
  ZodiacMCTargetDesc.cpp

  LINK_COMPONENTS
  ZodiacInfo
  MC
  MCDisassembler
  Support
  TargetParser

  ADD_TO_COMPONENT
  Zodiac
)
