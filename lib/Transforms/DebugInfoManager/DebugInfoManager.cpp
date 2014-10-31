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
#include "DebugInfoManager.h"

using namespace llvm;
using namespace std;

static cl::opt<string> FileName("file-name",
                                cl::desc("The file name that contains the target instruction"),
                                cl::init(""));
static cl::opt<string> FunctionName("function-name",
                                    cl::desc("The function where the instruction lies"),
                                    cl::init(""));
static cl::opt<unsigned> LineNumber("line-number",
                                cl::desc("The line number of the target instruction"),
                                cl::init(0));
static cl::opt<bool> FindMultBBCode("find-mult-bb",
                                      cl::desc("Find and count the lines of source code that have \
                                                multi-basic-block LLVM implementations"),
                                      cl::init(false));

DebugInfoManager::DebugInfoManager() : ModulePass(ID){
  if (FindMultBBCode)
    ;
  else if (LineNumber == 0 || FileName == "" || FunctionName == "") {
    errs() << "\nYou need to provide the file name, function name, and "
           << "the line number to retrieve the LLVM IR instruction!!!" << "\n\n";
              exit(1);
  }
}

// TODO: We should cache the results once they are computed for a given binary.
bool DebugInfoManager::runOnModule(Module& m) {
  for (Module::iterator fi = m.begin(), fe = m.end(); fi != fe; ++fi) {
    //errs().write_escaped(fi->getName()) << "\n";
    for (Function::iterator bi = fi->begin(), be = fi->end(); bi != be; ++bi) {
      // TODO: Improve this comparison by getting mangled names from the elf debug information
      if(fi->getName().find(StringRef(FunctionName)) != StringRef::npos)
        for (BasicBlock::iterator ii = bi->begin(), ie = bi->end(); ii != ie; ++ii) { 
          if (MDNode *N = ii->getMetadata("dbg")) {
            DILocation Loc(N);
            unsigned lineNumber = Loc.getLineNumber();
            StringRef fileName = Loc.getFilename();
            if(lineNumber == LineNumber) {
              StringRef directory = Loc.getDirectory();
              errs() << directory << "/" << fileName<< " : " << lineNumber << *ii << "\n ";
            }
          }
        }
     }
   }
  return true;
}
                                                                                                                          
void DebugInfoManager::getAnalysisUsage(AnalysisUsage &au) const {
  // We don't modify the program and preserve all the passes
  au.setPreservesAll();
}

const char* DebugInfoManager::getPassName() const {
  return "DebugInfoLocator"; 
}

char DebugInfoManager::ID = 0;

static RegisterPass<DebugInfoManager> X("debug-info-manager", "Debug information locator",
                                         true /* Only looks at CFG */,
                                         true /* Analysis Pass */);
