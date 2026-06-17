include_directories( ${CMAKE_CURRENT_BINARY_DIR}/.. ${CMAKE_CURRENT_SOURCE_DIR}/.. )

add_llvm_component_library(LLVMZodiacAsmParser
  ZodiacAsmParser.cpp

  LINK_COMPONENTS
  CodeGenTypes
  ZodiacDesc
  ZodiacInfo
  MC
  MCParser
  Support

  ADD_TO_COMPONENT
  Zodiac
  )

add_dependencies( LLVMZodiacAsmParser ZodiacCommonTableGen )
