add_llvm_loadable_module( LLVMDebugInfoManager
  DebugInfoManager.cpp
  )

if (TARGET intrinsics_gen)
	add_dependencies(LLVMDebugInfoManager intrinsics_gen)
endif()
