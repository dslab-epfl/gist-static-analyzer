add_llvm_loadable_module( LLVMIntelPTInstrumentor
  IntelPTInstrumentor.cpp
  )

if (TARGET intrinsics_gen)
	add_dependencies(LLVMIntelPTInstrumentor intrinsics_gen)
endif()
