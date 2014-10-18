#include <iostream>

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/PassManager.h"

#include "DynInstMarker.h"

using namespace llvm;
using namespace std;

namespace llvm {

  cl::opt<bool>
  InstLoads("inst-loads",
	    cl::desc("This will instrument all loads"),
	    cl::init(false));

  cl::opt<bool>
  InstStores("inst-stores",
	     cl::desc("This will instrument all loads"),
	     cl::init(false));

  cl::opt<bool>
  InstFun("inst-func",
	  cl::desc("This will limit the instrumentation to the given function"),
	  cl::init(false));
}

bool DynInstMarker::runOnModule(Module& m) {
  errs() << "Hello: ";
  for(Module::const_iterator fi = m.begin(), fe = m.end(); fi != fe; ++fi){
    errs() << "Size: " << fi->size();
    for(Function::const_iterator bi = fi->begin(), be = fi->end(); bi != be; ++bi){
      errs().write_escaped(bi->getName()) << "\n";
      for (BasicBlock::const_iterator ii = bi->begin(), ie = bi->end(); ii != ie; ++ii){
	
      }
    }
  }
  return true;
}

char DynInstMarker::ID = 0;

//static RegisterPass<DynInstMarker> X("marker", "Marker pass", false, false);

static void registerMyPass(const PassManagerBuilder &,
                           PassManagerBase &PM) {
    PM.add(new DynInstMarker());
}

static RegisterStandardPasses
    RegisterMyPass(PassManagerBuilder::EP_EnabledOnOptLevel0,
                   registerMyPass);
