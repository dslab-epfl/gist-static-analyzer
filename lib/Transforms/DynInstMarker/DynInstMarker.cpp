#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"

#include "DynInstMarker.h"

using namespace llvm;
using namespace std;

namespace {
  cl::opt<bool>
  InstAllMovs("inst-all-movs",
	      cl::desc("Embed the array of HB relationships that the Hive came up with"),
	      cl::init(false));
}

bool Instrumenter::runOnModule(Module& m) {
  for(Module::const_iterator fi = m.begin(), fe = m.end(); fi != fe; ++fi){
    for(Function::const_iterator bi = fi->begin(), be = fi->end(); bi != be; ++bi){
      for (BasicBlock::const_iterator ii = bi->begin(), ie = bi->end(); ii != ie; ++ii){
	
      }
    }
  }
}
