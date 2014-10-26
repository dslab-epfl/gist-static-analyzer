#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/PassManager.h"
#include "llvm/IR/Instructions.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/InlineAsm.h>

#include "DebugInfoLocator.h"

using namespace llvm;
using namespace std;

namespace llvm {
  cl::opt<string>
  FileName("file-name",
	    cl::desc("The file name that contains the target instruction"),
	    cl::init(""));
  cl::opt<string>
  FunctionName("function-name",
	       cl::desc("The function where the instruction lies"),
	       cl::init(""));
  cl::opt<string>
  LineNumber("line-number",
	     cl::desc("the line number of the target instruction"),
	     cl::init(""));
}

bool DebugInfoLocator::runOnModule(Module& m) {
  for (Module::iterator fi = m.begin(), fe = m.end(); fi != fe; ++fi) {
    errs().write_escaped(fi->getName()) << "\n";
    for (Function::iterator bi = fi->begin(), be = fi->end(); bi != be; ++bi) {
      //             !fi->getName().equals(StringRef("_Z13resMarkerFuncv")))
      for (BasicBlock::iterator ii = bi->begin(), ie = bi->end(); ii != ie; ++ii) {
	;
      }
    }
  }
  return true;
}

char DebugInfoLocator::ID = 0;

static void registerMyPass(const PassManagerBuilder &,
                           PassManagerBase &PM) {
    PM.add(new DebugInfoLocator());
}

static RegisterStandardPasses
    RegisterMyPass(PassManagerBuilder::EP_EnabledOnOptLevel0,
                   registerMyPass);
