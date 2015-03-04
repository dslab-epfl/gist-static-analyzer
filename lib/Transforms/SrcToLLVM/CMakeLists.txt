add_llvm_loadable_module( LLVMSrcToLLVM
  SrcToLLVM.cpp
  )

if (TARGET intrinsics_gen)
        add_dependencies(LLVMSrcToLLVM intrinsics_gen)
endif()