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
static cl::opt<bool> Debug("debug-debug-info-manager",
       cl::desc("Print debugging statements for debug info manager"),
       cl::init(false));
static cl::opt<bool> FindMultBBCode("find-mult-bb",
       cl::desc("Find and count the lines of source code that have multi-basic-block LLVM implementations"),
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
  errs() << "  " << directory << "/" << fileName<< " : " << lineNumber << "\n ";
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
    printDebugInfo(*bitCastInst);
    errs() << oss.str() << "\n";
  } else if (GetElementPtrInst* gEPtr = dyn_cast<GetElementPtrInst>(&value)) {
    printDebugInfo(*gEPtr);
  }
}

// TODO: We should cache the results once they are computed for a given binary.
bool DebugInfoManager::runOnModule(Module& m) {
  for (Module::iterator fi = m.begin(), fe = m.end(); fi != fe; ++fi) {
    if (Debug)
      errs() << "Function: " << fi->getName() << "\n";
    for (Function::iterator bi = fi->begin(), be = fi->end(); bi != be; ++bi) {
      // TODO: Improve this comparison by getting mangled names from the elf debug information
      if(fi->getName().find(StringRef(FunctionName)) != StringRef::npos)
        for (BasicBlock::iterator ii = bi->begin(), ie = bi->end(); ii != ie; ++ii) { 
          if (MDNode *N = ii->getMetadata("dbg")) {
            DILocation Loc(N);
            unsigned lineNumber = Loc.getLineNumber();
            StringRef fileName = Loc.getFilename();
            if(lineNumber == LineNumber) {
              // Instead of assuming the offending instruction is a load, we may filter for several
              // other unlikely instructions like bitcasts and instrinsics
              if (isa<LoadInst>(*ii)) {
                StringRef directory = Loc.getDirectory();
                errs() << "\n### Target LLVM instrcution:" << "\n";
                errs() << "  " << directory << "/" << fileName<< " : " << lineNumber << *ii << "\n ";
                trackUseDefChain(*ii);
              }
            }
          }
        }
     }
   }
  return true;
}

char DebugInfoManager::ID = 0;

static RegisterPass<DebugInfoManager> X("debug-info-manager", "Debug information locator",
                                         true /* Only looks at CFG */,
                                         true /* Analysis Pass */);
