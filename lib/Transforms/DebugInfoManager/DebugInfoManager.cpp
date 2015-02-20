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

#include "DebugInfoManager.h"

using namespace llvm;
using namespace std;

static cl::opt<string> TargetFileName("target-file-name",
       cl::desc("The file name that contains the target instruction"),
       cl::init(""));
static cl::opt<string> TargetFunctionName("target-function-name",
       cl::desc("The function where the instruction lies"),
       cl::init(""));
static cl::opt<unsigned> TargetLineNumber("target-line-number",
       cl::desc("The line number of the target instruction"),
       cl::init(0));
static cl::opt<bool> Debug("debug-debug-info-manager",
       cl::desc("Print debugging statements for debug info manager"),
       cl::init(false));
static cl::opt<bool> FindMultBBCode("find-mult-bb",
       cl::desc("Find and count the lines of source code that have multi-basic-block LLVM implementations"),
       cl::init(false));


DebugInfoManager::DebugInfoManager() : ModulePass(ID) {
  if (FindMultBBCode)
    ;
  else if (TargetLineNumber == 0 || TargetFileName == "" || TargetFunctionName == "") {
    errs() << "\nYou need to provide the file name, function name, and "
           << "the line number to retrieve the LLVM IR instruction!!!" << "\n\n";
              exit(1);
  }
}


const char* DebugInfoManager::getPassName() const {
  return "DebugInfoLocator";
}


void DebugInfoManager::getAnalysisUsage(AnalysisUsage &au) const {
  // We don't modify the program and preserve all the passes
  au.setPreservesAll();
}


void DebugInfoManager::printDebugInfo(Instruction& instr) {
  MDNode *N = instr.getMetadata("dbg");
  DILocation Loc(N);
  unsigned lineNumber = Loc.getLineNumber();
  StringRef fileName = Loc.getFilename();
  StringRef directory = Loc.getDirectory();
  errs() << "\t\t" << directory << "/" << fileName<< " : " << lineNumber << "\n ";
}


void DebugInfoManager::trackUseDefChain(Value& value){
  string Str;
  if(LoadInst* loadInst = dyn_cast<LoadInst>(&value)) {
    Value *v = loadInst->getOperand(0);
    raw_string_ostream oss(Str);
    v->print(oss);
    errs() << oss.str() << "\n";
    printDebugInfo(*loadInst);
    if(isa<Instruction>(v))
      trackUseDefChain(*v);
    else
      assert(false && "Handle the non-instruction case");
  } else if (BitCastInst* bitCastInst = dyn_cast<BitCastInst>(&value)) {
    Value *v = bitCastInst->getOperand(0);
    raw_string_ostream oss(Str);
    v->print(oss);
    errs() << oss.str() << "\n";
    printDebugInfo(*bitCastInst);
  } else if (GetElementPtrInst* geptr = dyn_cast<GetElementPtrInst>(&value)) {
    geptr->getPointerOperand();
    //printDebugInfo(*geptr);
  }
}


// TODO: We should cache the results once they are computed for a given binary.
bool DebugInfoManager::runOnModule(Module& m) {
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
                if (isa<LoadInst>(*ii)) {
                  targetInstructions.push_back(&(*ii));
                  targetFunctions.push_back(&(*fi));
                  targetOperands.push_back(ii->getOperand(0));
                } else if (isa<CallInst>(&(*ii))) {
                  targetInstructions.push_back(&(*ii));
                  targetFunctions.push_back(&(*fi));
                  targetOperands.push_back(ii->getOperand(0));
                }
                /* 
                if (!done) {
                  if (isa<LoadInst>(*ii)) {
                    StringRef directory = Loc.getDirectory();
                    errs() << "------------------------" << "\n";
                    errs() << "Target LLVM instruction:" << "\n";
                    errs() << "------------------------" << "\n";
                    errs() << "\t" << *ii << "\n\t|--> " << directory << "/" << fileName<< " : " << lineNumber << "\n\n";
                    targetInstruction = &(*ii);
                    targetOperand = targetInstruction->getOperand(0); // For a load, we are interested in the first operand
                    targetFunction = &(*fi);
                    // Special case handlings
                    if (fileName == "urlglob.c") {
                      // done = true;
                    }
                  } else if (CallInst* CI = dyn_cast<CallInst>(&(*ii))) {
                    // We admit a call instruction in the case of mod_mem_cache.c
                    if (fileName == "mod_mem_cache.c") {
                      done = true;
                      StringRef directory = Loc.getDirectory();
                      targetInstruction = &(*ii);
                      assert (CI->getCalledFunction()->getName().str() == "free" && 
                              "We only know the case of free being here at this moment (for double free)");
                      targetOperand = targetInstruction->getOperand(0);
                      targetFunction = &(*fi);                      
                    }                    
                  } else {
                    ;
                    StringRef directory = Loc.getDirectory();
                    errs() << "@@@\t" << *ii << "\n\t|--> " << directory << "/" << fileName<< " : " << lineNumber << "\n\n";
                  }
                }
                */
              }
            }
          }
        }
     }
   }
  return false;
}

char DebugInfoManager::ID = 0;

static RegisterPass<DebugInfoManager> X("debug-info-manager", "Debug information locator",
                                         true /* Only looks at CFG */,
                                         true /* Analysis Pass */);
