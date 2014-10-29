#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/PassManager.h"
#include "llvm/IR/Instructions.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/InlineAsm.h>

#include "llvm/IR/DebugInfo.h"
#include "DebugInfoLocator.h"

using namespace llvm;
using namespace std;

static  cl::opt<string> FileName("file-name",
                                 cl::desc("The file name that contains the target instruction"),
                                 cl::init(""));
static  cl::opt<string> FunctionName("function-name",
                                     cl::desc("The function where the instruction lies"),
                                     cl::init(""));
static  cl::opt<string> LineNumber("line-number",
                                   cl::desc("the line number of the target instruction"),
                                   cl::init(""));

DebugInfoLocator::DebugInfoLocator() : ModulePass(ID){
  if (strlen(FileName.ValueStr) == 0 )
    errs() << "No no" << "\n";
  //errs() << FileName;
  
}

// TODO: we should cache the results once they are computed for a given binary.
bool DebugInfoLocator::runOnModule(Module& m) {
  for (Module::iterator fi = m.begin(), fe = m.end(); fi != fe; ++fi) {
    errs().write_escaped(fi->getName()) << "\n";
    for (Function::iterator bi = fi->begin(), be = fi->end(); bi != be; ++bi) {
      if(FunctionName.hasArgStr() && fi->getName().find(StringRef(FunctionName)) != StringRef::npos)
        for (BasicBlock::iterator ii = bi->begin(), ie = bi->end(); ii != ie; ++ii) { 
          if (MDNode *N = ii->getMetadata("dbg")) {
            DILocation Loc(N);
            unsigned Line = Loc.getLineNumber();
            StringRef File = Loc.getFilename();
            StringRef Dir = Loc.getDirectory();
      	    errs() << Dir << "/" << File.str() << " : " << Line << *ii << "\n ";
          }
        }
     }
   }
  return true;
}
                                                                                                                          
void DebugInfoLocator::getAnalysisUsage(AnalysisUsage &au) const {
  // We don't modify the program and preserve all the passes
  au.setPreservesAll();
}

const char* DebugInfoLocator::getPassName() const { 
  return "DebugInfoLocator"; 
}

char DebugInfoLocator::ID = 0;

static RegisterPass<DebugInfoLocator> X("debug-info-locator", "Debug information locator",
                             true /* Only looks at CFG */,
                             true /* Analysis Pass */);
