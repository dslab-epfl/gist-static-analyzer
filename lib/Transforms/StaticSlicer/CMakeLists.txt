add_llvm_loadable_module( LLVMStaticSlicer
  StaticSlicer.cpp
  )

if (TARGET intrinsics_gen)
  # Check if we are building as part of an LLVM build
  add_dependencies(LLVMStaticSlicer intrinsics_gen)
endif()
