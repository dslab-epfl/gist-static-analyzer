#include "llvm/Pass.h"
#include "llvm/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/PassManager.h"
#include "llvm/Instructions.h"
#include "llvm/DebugInfo.h"
#include <llvm/Constants.h>
#include <llvm/InlineAsm.h>
#include <llvm/Intrinsics.h>

#include <sstream>
#include <iostream>
#include "SrcToLLVM.h"

using namespace llvm;
using namespace std;

static cl::opt<string> TargetFileName("target-file-name",
       cl::desc("The file name that contains the target instruction"),
       cl::init(""));
static cl::opt<string> TargetFunctionName("target-function-name",
       cl::desc("The function where the instruction lies"),
       cl::init(""));
static cl::opt<int> TargetLineNumber("target-line-number",
       cl::desc("The line numbers of the target instructions"),
       cl::init(0));
static cl::opt<bool> Debug("debug-debug-info-manager",
       cl::desc("Print debugging statements for debug info manager"),
       cl::init(false));


SrcToLLVM::SrcToLLVM() : ModulePass(ID) {
  if (TargetLineNumber == 0 || TargetFileName == "" || TargetFunctionName == "") {
    errs() << "\nYou need to provide the file name, function name, and "
           << "the line number to retrieve the LLVM IR instructions and their count!!!" << "\n\n";
              exit(1);
  }
}


const char* SrcToLLVM::getPassName() const {
  return "SrcToLLVM";
}


void SrcToLLVM::getAnalysisUsage(AnalysisUsage &au) const {
  // We don't modify the program and preserve all the passes
  au.setPreservesAll();
}


bool SrcToLLVM::runOnModule(Module& m) {
  int counter = 0;
  for (Module::iterator fi = m.begin(), fe = m.end(); fi != fe; ++fi) {
    if (Debug)
      errs() << "Function: " << fi->getName() << "\n";
    
    for (Function::iterator bi = fi->begin(), be = fi->end(); bi != be; ++bi) {
      // TODO: Improve this comparison by getting mangled names from the elf debug information
      if(fi->getName().find(StringRef(TargetFunctionName)) != StringRef::npos)
        for (BasicBlock::iterator ii = bi->begin(), ie = bi->end(); ii != ie; ++ii) { 
          if (MDNode *N = ii->getMetadata("dbg")) {
            DILocation Loc(N);
            unsigned lineNumber = Loc.getLineNumber();
            StringRef fileName = Loc.getFilename();
            if(lineNumber == TargetLineNumber) {
              // currently only use calls and loads as the potential target instructions
              if ((fileName.find(StringRef(TargetFileName))) != StringRef::npos) {
                counter++;
                errs() << *ii << "\n";
              }
            }
          }
        }
    }
  }
  cerr << "LLVM instruction count: " << counter << endl;
  return false;
}

char SrcToLLVM::ID = 0;

static RegisterPass<SrcToLLVM> X("src-to-llvm", "Src lines to LLVM lines mapper",
                                 true /* Only looks at CFG */,
                                 true /* Analysis Pass */);
