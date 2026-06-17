add_llvm_component_library(LLVMZodiacDisassembler
  ZodiacDisassembler.cpp

  LINK_COMPONENTS
  CodeGenTypes
  ZodiacDesc
  ZodiacInfo
  MC
  MCDisassembler
  Support

  ADD_TO_COMPONENT
  Zodiac
  )
