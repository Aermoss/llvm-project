add_llvm_component_library(LLVMZodiacInfo
  ZodiacTargetInfo.cpp

  LINK_COMPONENTS
  MC
  Support

  ADD_TO_COMPONENT
  Zodiac
  )
