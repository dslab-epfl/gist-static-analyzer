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
#include <llvm/Attributes.h>
#include <llvm/LLVMContext.h>
#include <llvm/DerivedTypes.h>
#include <llvm/GlobalVariable.h>
#include <llvm/Function.h>
#include <llvm/CallingConv.h>
#include <llvm/BasicBlock.h>
#include <llvm/Support/FormattedStream.h>
#include <llvm/Support/MathExtras.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/Assembly/PrintModulePass.h>
#include "IntelPTInstrumentor.h"
#include "llvm/Transforms/Instrumentation.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include <algorithm>
#include <sstream>
#include <iostream>

using namespace llvm;
using namespace std;

static cl::opt<bool> EnableIntelPtPass("enable-intelpt",
                                       cl::desc("File name to start PT tracing"),
                                       cl::init(false));

static cl::opt<string> StartFileName("start-file-name",
                                     cl::desc("File name to start PT tracing"),
                                     cl::init(""));
static cl::opt<string> StartFunction("start-function",
                                     cl::desc("Function to Start PT tracing"),
                                     cl::init(""));
static cl::opt<unsigned> StartLineNumber("start-line-number",
                                         cl::desc("Line number to start PT tracing"),
                                         cl::init(0));
static cl::opt<unsigned> StartIndex("start-index",
                                    cl::desc("Index of the starting instruction with the matching file name and line number"),
                                    cl::init(1));

static cl::opt<string> StopFileName("stop-file-name",
                                    cl::desc("File name to stop PT tracing"),
                                    cl::init(""));
static cl::opt<string> StopFunction("stop-function",
                                    cl::desc("Function to Stop PT tracing"),
                                    cl::init(""));
static cl::opt<unsigned> StopLineNumber("stop-line-number",
                                        cl::desc("Line number to stop PT tracing"),
                                        cl::init(0));
static cl::opt<unsigned> StopIndex("stop-index",
                                   cl::desc("Index of the stopping instruction with the matching file name and line number"),
                                   cl::init(1));

static cl::opt<bool> Debug("debug-debug-info-manager",
                           cl::desc("Print debugging statements for debug info manager"),
                           cl::init(false));


char IntelPTInstrumentor::ID = 0;


INITIALIZE_PASS(IntelPTInstrumentor, "intel-pt-instrument",
                "IntelPTInstrumentor: Do Intel PT instrumentation",
                false, false)


ModulePass* llvm::createIntelPTInstrumentorPass() {
  return new IntelPTInstrumentor();
}


IntelPTInstrumentor::IntelPTInstrumentor() : ModulePass(ID) {
  //StartFileName = "main.c";
  //StartFunction = "main";
  //StartLineNumber = 32;

  //StopFileName = "main.c";
  //StopFunction = "main";
  //StopLineNumber = 34;

  cerr << "enable: " << EnableIntelPtPass << endl;
  /*

  if (StartFileName == "" || StartLineNumber == 0 || StartIndex == 0 ||
      StopFileName == "" || StopLineNumber == 0 || StopLineNumber == 0) {
    errs() << "\nYou need to provide the start/stop file name, function name, and "
           << "the line number!!!" << "\n\n";
              exit(1);
  }
  */
}


const char* IntelPTInstrumentor::getPassName() const {
  return "IntelPTInstrumentor";
}


void IntelPTInstrumentor::getAnalysisUsage(AnalysisUsage &au) const {
  // We don't modify the program and preserve all the passes
  au.setPreservesAll();
}


void IntelPTInstrumentor::printDebugInfo(Instruction& instr) {
  MDNode *N = instr.getMetadata("dbg");
  DILocation Loc(N);
  unsigned lineNumber = Loc.getLineNumber();
  StringRef fileName = Loc.getFilename();
  StringRef directory = Loc.getDirectory();
  errs() << "\t\t" << directory << "/" << fileName<< " : " << lineNumber << "\n ";
}


set<string>& IntelPTInstrumentor::split(const string &s, char delim, 
                                        set<string> &elems) {
  stringstream ss(s);
  string item;
  while (getline(ss, item, delim)) {
    elems.insert(item);
  }
  return elems;
}


set<string> IntelPTInstrumentor::split(const string &s, char delim) {
  set<string> elems;
  split(s, delim, elems);
  return elems;
}


bool IntelPTInstrumentor::runOnModule(Module& m) {
  // Type Definitions                                                                                                                  
  vector<Type*>FuncTy_0_args;
  FunctionType* FuncTy_0 = FunctionType::get(
                                             /*Result=*/Type::getVoidTy(m.getContext()),
                                             /*Params=*/FuncTy_0_args,
                                             /*isVarArg=*/false);


  // Function Declarations
  Function* func_startPt = m.getFunction("startPt");
  if (!func_startPt) {
    func_startPt = Function::Create(
                                    /*Type=*/FuncTy_0,
                                    /*Linkage=*/GlobalValue::ExternalLinkage,
                                    /*Name=*/"startPt", &m);
    func_startPt->setCallingConv(CallingConv::C);
  }

  Function* func_stopPt = m.getFunction("stopPt");
  if (!func_stopPt) {
    func_stopPt = Function::Create(
                                   /*Type=*/FuncTy_0,
                                   /*Linkage=*/GlobalValue::ExternalLinkage,
                                   /*Name=*/"stopPt", &m);
    func_stopPt->setCallingConv(CallingConv::C);
  }

  unsigned startIndex = 1;
  unsigned stopIndex = 1;

  for (Module::iterator fi = m.begin(), fe = m.end(); fi != fe; ++fi) {    
    for (Function::iterator bi = fi->begin(), be = fi->end(); bi != be; ++bi) {
      if(fi->getName().find(StringRef(StartFunction)) != StringRef::npos) {
        for (BasicBlock::iterator ii = bi->begin(), ie = bi->end(); ii != ie; ++ii) { 
          if (MDNode *N = ii->getMetadata("dbg")) {
            DILocation Loc(N);
            unsigned lineNumber = Loc.getLineNumber();
            if(lineNumber == StartLineNumber) {
              StringRef fileName = Loc.getFilename();
              if ((fileName.find(StringRef(StartFileName))) != StringRef::npos) {
                if(startIndex == StartIndex) {
                  cerr << "match start " << endl;
                  // We need a start at each predecessor of the basic block that contains this instruction
                  BasicBlock* parent = ii->getParent();
                  pred_iterator it;
                  pred_iterator et;
                  int predCount = 0;
                  for (it = pred_begin(parent), et = pred_end(parent); it != et; ++it, ++predCount) {
                    CallInst::Create(func_startPt, "", (*it)->getFirstInsertionPt());
                  }
                  // If there are no predecessors, insert the instrumentation to this block
                  if (predCount == 0) {
                    cerr << "No predecessors" << endl;
                    CallInst::Create(func_startPt, "", ii);
                  }
                  startIndex = 1;
                }
              }
            }             
          }
        }
      }
      if(fi->getName().find(StringRef(StopFunction)) != StringRef::npos) {
        for (BasicBlock::iterator ii = bi->begin(), ie = bi->end(); ii != ie; ++ii) {
          if (MDNode *N = ii->getMetadata("dbg")) {
            DILocation Loc(N);
            unsigned lineNumber = Loc.getLineNumber();            
            if(lineNumber == StopLineNumber) {
              StringRef fileName = Loc.getFilename();
              if ((fileName.find(StringRef(StopFileName))) != StringRef::npos) {
                if(stopIndex == StopIndex) {
                  cerr << "match stop " << endl;
                  stopIndex = 1;
                  CallInst::Create(func_stopPt, "", ii);
                }            
              }              
            }
          }
        }
      }      
    }
  }
  return false;
}
