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
#include "TypeBasedDebugInfo.h"

using namespace llvm;
using namespace std;

static cl::opt<bool> Debug("debug-debug-info-manager",
       cl::desc("Print debugging statements for debug info manager"),
       cl::init(false));
static cl::opt<string> TypeList("target-file-name",
       cl::desc("List of types, separated by :"),
       cl::init(""));


TypeBasedDebugInfo::TypeBasedDebugInfo() : ModulePass(ID) {
}


const char* TypeBasedDebugInfo::getPassName() const {
  return "TypeBasedDebugInfo";
}


void TypeBasedDebugInfo::getAnalysisUsage(AnalysisUsage &au) const {
  // We don't modify the program and preserve all the passes
  au.setPreservesAll();
}


void TypeBasedDebugInfo::printDebugInfo(Instruction& instr) {
  /*  
  MDNode *N = instr.getMetadata("dbg");
  DILocation Loc(N);
  unsigned lineNumber = Loc.getLineNumber();
  StringRef fileName = Loc.getFilename();
  StringRef directory = Loc.getDirectory();
  errs() << "\t\t" << directory << "/" << fileName<< " : " << lineNumber << "\n ";
  */
}

set<string>& TypeBasedDebugInfo::split(const string &s, char delim, 
                                        set<string> &elems) {
  stringstream ss(s);
  string item;
  while (getline(ss, item, delim)) {
    elems.insert(item);
  }
  return elems;
}


set<string> TypeBasedDebugInfo::split(const string &s, char delim) {
  set<string> elems;
  split(s, delim, elems);
  return elems;
}


bool TypeBasedDebugInfo::runOnModule(Module& m) {
  for(Module::iterator fi = m.begin(), fe = m.end(); fi != fe; ++fi) {
    if(Debug)
      errs() << "Function: " << fi->getName() << "\n";
    
    for(Function::iterator bi = fi->begin(), be = fi->end(); bi != be; ++bi) {
      for(BasicBlock::iterator ii = bi->begin(), ie = bi->end(); ii != ie; ++ii) { 
        if(isa<Instruction>(*ii)) {
          for(unsigned i = 0; i < ii->getNumOperands() ; ++i){  
            Value* v = ii->getOperand(i);
            Type* t = v->getType();
            errs() << TypeStrings[t->getTypeID()];
            
            MDNode *N = ii->getMetadata("dbg");
            if(N){
              DILocation Loc(N);
              unsigned lineNumber = Loc.getLineNumber();
              StringRef fileName = Loc.getFilename();
              StringRef directory = Loc.getDirectory();
              errs() << "\t\t" << directory << "/" << fileName<< " : " << lineNumber << "\n ";
            }
            
         }
        }
      } 
    }
  }
  return false;
}

char TypeBasedDebugInfo::ID = 0;

static RegisterPass<TypeBasedDebugInfo> X("type-based-debug-info", "Type based debug information extractor",
                                         true /* Only looks at CFG */,
                                         true /* Analysis Pass */);
