add_llvm_loadable_module( LLVMTypeBasedDebugInfo
  TypeBasedDebugInfo.cpp
  )

if (TARGET intrinsics_gen)
	add_dependencies(LLVMTypeBasedDebugInfo intrinsics_gen)
endif()
