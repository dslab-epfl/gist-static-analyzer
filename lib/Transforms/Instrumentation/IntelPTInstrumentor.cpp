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
  cerr << "enable: " << EnableIntelPtPass << endl;
  if (StartFileName == "" || StartFunction == "" || StartLineNumber == 0 ||
      StopFileName == "" || StopFunction == "" || StopLineNumber == 0) {
    errs() << "\nYou need to provide the start/stop file name, function name, and "
           << "the line number!!!" << "\n\n";
              exit(1);
  }
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
  PointerType* PointerTy_0 = PointerType::get(IntegerType::get(m.getContext(), 8), 0);
  ArrayType* ArrayTy_1 = ArrayType::get(IntegerType::get(m.getContext(), 8), 15);

  std::vector<Type*>FuncTy_4_args;
  FunctionType* FuncTy_4 = FunctionType::get(
                                             /*Result=*/Type::getVoidTy(m.getContext()),
                                             /*Params=*/FuncTy_4_args,
                                             /*isVarArg=*/false);

  std::vector<Type*>FuncTy_6_args;
  FuncTy_6_args.push_back(PointerTy_0);
  FuncTy_6_args.push_back(IntegerType::get(m.getContext(), 32));
  FunctionType* FuncTy_6 = FunctionType::get(
                                             /*Result=*/IntegerType::get(m.getContext(), 32),
                                             /*Params=*/FuncTy_6_args,
                                             /*isVarArg=*/true);


  std::vector<Type*>FuncTy_8_args;
  FuncTy_8_args.push_back(IntegerType::get(m.getContext(), 32));
  FuncTy_8_args.push_back(IntegerType::get(m.getContext(), 64));
  FunctionType* FuncTy_8 = FunctionType::get(
                                             /*Result=*/IntegerType::get(m.getContext(), 32),
                                             /*Params=*/FuncTy_8_args,
                                             /*isVarArg=*/true);


  // Function Declarations                                                                                                             

  Function* func_startPt = m.getFunction("startPt");
  if (!func_startPt) {
    func_startPt = Function::Create(
                                    /*Type=*/FuncTy_4,
                                    /*Linkage=*/GlobalValue::ExternalLinkage,
                                    /*Name=*/"startPt", &m);
    func_startPt->setCallingConv(CallingConv::C);
  }
  /*
  AttrListPtr func_startPt_PAL;
  {
    SmallVector<AttributeWithIndex, 4> Attrs;
    AttributeWithIndex PAWI;
    PAWI.Index = 4294967295U; PAWI.Attrs = Attributes::AttrVal::None  | Attributes::AttrVal::NoUnwind | Attributes::AttrVal::UWTable;
    Attrs.push_back(PAWI);
    func_startPt_PAL = AttrListPtr::get(Attrs);

  }
  func_startPt->setAttributes(func_startPt_PAL);
  */
  Function* func_open = m.getFunction("open");
  if (!func_open) {
    func_open = Function::Create(
                                 /*Type=*/FuncTy_6,
                                 /*Linkage=*/GlobalValue::ExternalLinkage,
                                 /*Name=*/"open", &m); // (external, no body)                                                                                       
    func_open->setCallingConv(CallingConv::C);
  }
  /*
  AttrListPtr func_open_PAL;
  func_open->setAttributes(func_open_PAL);
  */
  Function* func_ioctl = m.getFunction("ioctl");
  if (!func_ioctl) {
    func_ioctl = Function::Create(
                                  /*Type=*/FuncTy_8,
                                  /*Linkage=*/GlobalValue::ExternalLinkage,
                                  /*Name=*/"ioctl", &m); // (external, no body)                                                                                      
    func_ioctl->setCallingConv(CallingConv::C);
  }
  /*
  AttrListPtr func_ioctl_PAL;
  {
    SmallVector<AttributeWithIndex, 4> Attrs;
    AttributeWithIndex PAWI;
    PAWI.Index = 4294967295U; PAWI.Attrs = Attribute::None  | Attribute::NoUnwind;
    Attrs.push_back(PAWI);
    func_ioctl_PAL = AttrListPtr::get(Attrs);

  }
  func_ioctl->setAttributes(func_ioctl_PAL);
  */
  Function* func_stopPt = m.getFunction("stopPt");
  if (!func_stopPt) {
    func_stopPt = Function::Create(
                                   /*Type=*/FuncTy_4,
                                   /*Linkage=*/GlobalValue::ExternalLinkage,
                                   /*Name=*/"stopPt", &m);
    func_stopPt->setCallingConv(CallingConv::C);
  }
  /*
  AttrListPtr func_stopPt_PAL;
  {
    SmallVector<AttributeWithIndex, 4> Attrs;
    AttributeWithIndex PAWI;
    PAWI.Index = 4294967295U; PAWI.Attrs = Attribute::None  | Attribute::NoUnwind | Attribute::UWTable;
    Attrs.push_back(PAWI);
    func_stopPt_PAL = AttrListPtr::get(Attrs);

  }
  func_stopPt->setAttributes(func_stopPt_PAL);
  */
  // Global Variable Declarations                                                                                                      


  GlobalVariable* gvar_int8_pt = new GlobalVariable(/*Module=*/m,
                                                    /*Type=*/IntegerType::get(m.getContext(), 8),
                                                    /*isConstant=*/false,
                                                    /*Linkage=*/GlobalValue::ExternalLinkage,
                                                    /*Initializer=*/0, // has initializer, specified below                                                                               
                                                    /*Name=*/"pt");
  gvar_int8_pt->setAlignment(1);

  GlobalVariable* gvar_array__str = new GlobalVariable(/*Module=*/m,
                                                       /*Type=*/ArrayTy_1,
                                                       /*isConstant=*/true,
                                                       /*Linkage=*/GlobalValue::PrivateLinkage,
                                                       /*Initializer=*/0, // has initializer, specified below                                                                               
                                                       /*Name=*/".str");
  gvar_array__str->setAlignment(1);

  GlobalVariable* gvar_int32_fd = new GlobalVariable(/*Module=*/m,
                                                     /*Type=*/IntegerType::get(m.getContext(), 32),
                                                     /*isConstant=*/false,
                                                     /*Linkage=*/GlobalValue::CommonLinkage,
                                                     /*Initializer=*/0, // has initializer, specified below                                                                               
                                                     /*Name=*/"fd");
  gvar_int32_fd->setAlignment(4);

  // Constant Definitions                                                                                                              
  ConstantInt* const_int8_9 = ConstantInt::get(m.getContext(), APInt(8, StringRef("0"), 10));
  Constant *const_array_10 = ConstantDataArray::getString(m.getContext(), "/dev/simple-pt", true);
  ConstantInt* const_int32_11 = ConstantInt::get(m.getContext(), APInt(32, StringRef("0"), 10));
  ConstantInt* const_int8_12 = ConstantInt::get(m.getContext(), APInt(8, StringRef("1"), 10));
  std::vector<Constant*> const_ptr_13_indices;
  const_ptr_13_indices.push_back(const_int32_11);
  const_ptr_13_indices.push_back(const_int32_11);
  Constant* const_ptr_13 = ConstantExpr::getGetElementPtr(gvar_array__str, const_ptr_13_indices);
  ConstantInt* const_int32_14 = ConstantInt::get(m.getContext(), APInt(32, StringRef("524288"), 10));
  ConstantInt* const_int64_15 = ConstantInt::get(m.getContext(), APInt(64, StringRef("9904"), 10));
  ConstantInt* const_int64_16 = ConstantInt::get(m.getContext(), APInt(64, StringRef("9905"), 10));

  // Global Variable Definitions                                                                                                       
  gvar_int8_pt->setInitializer(const_int8_9);
  gvar_array__str->setInitializer(const_array_10);
  gvar_int32_fd->setInitializer(const_int32_11);

  // Function Definitions                                                                                                              

  // Function: startPt (func_startPt)                                                                                                  
  {

    BasicBlock* label_entry = BasicBlock::Create(m.getContext(), "entry",func_startPt,0);
    BasicBlock* label_if_then = BasicBlock::Create(m.getContext(), "if.then",func_startPt,0);
    BasicBlock* label_if_end = BasicBlock::Create(m.getContext(), "if.end",func_startPt,0);

    // Block entry (label_entry)                                                                                                        
    LoadInst* int8_17 = new LoadInst(gvar_int8_pt, "", false, label_entry);
    int8_17->setAlignment(1);
    ICmpInst* int1_tobool = new ICmpInst(*label_entry, ICmpInst::ICMP_NE, int8_17, const_int8_9, "tobool");
    BranchInst::Create(label_if_end, label_if_then, int1_tobool, label_entry);

    // Block if.then (label_if_then)                                                                                                    
    StoreInst* void_19 = new StoreInst(const_int8_12, gvar_int8_pt, false, label_if_then);
    void_19->setAlignment(1);
    std::vector<Value*> int32_call_params;
    int32_call_params.push_back(const_ptr_13);
    int32_call_params.push_back(const_int32_14);
    CallInst* int32_call = CallInst::Create(func_open, int32_call_params, "call", label_if_then);
    int32_call->setCallingConv(CallingConv::C);
    int32_call->setTailCall(false);
    AttrListPtr int32_call_PAL;
    int32_call->setAttributes(int32_call_PAL);

    StoreInst* void_20 = new StoreInst(int32_call, gvar_int32_fd, false, label_if_then);
    void_20->setAlignment(4);
    LoadInst* int32_21 = new LoadInst(gvar_int32_fd, "", false, label_if_then);
    int32_21->setAlignment(4);
    std::vector<Value*> int32_call1_params;
    int32_call1_params.push_back(int32_21);
    int32_call1_params.push_back(const_int64_15);
    CallInst* int32_call1 = CallInst::Create(func_ioctl, int32_call1_params, "call1", label_if_then);
    int32_call1->setCallingConv(CallingConv::C);
    int32_call1->setTailCall(false);
    /*
    AttrListPtr int32_call1_PAL;
    {
      SmallVector<AttributeWithIndex, 4> Attrs;
      AttributeWithIndex PAWI;
      PAWI.Index = 4294967295U; PAWI.Attrs = Attribute::None  | Attribute::NoUnwind;
      Attrs.push_back(PAWI);
      int32_call1_PAL = AttrListPtr::get(Attrs);

    }
    int32_call1->setAttributes(int32_call1_PAL);
    */
    BranchInst::Create(label_if_end, label_if_then);

    // Block if.end (label_if_end)                                                                                                      
    ReturnInst::Create(m.getContext(), label_if_end);
  }

  // Function: stopPt (func_stopPt)                                                                                                    
  {

    BasicBlock* label_entry_24 = BasicBlock::Create(m.getContext(), "entry",func_stopPt,0);
    BasicBlock* label_if_then_25 = BasicBlock::Create(m.getContext(), "if.then",func_stopPt,0);
    BasicBlock* label_if_end_26 = BasicBlock::Create(m.getContext(), "if.end",func_stopPt,0);

    // Block entry (label_entry_24)                                                                                                     
    LoadInst* int8_27 = new LoadInst(gvar_int8_pt, "", false, label_entry_24);
    int8_27->setAlignment(1);
    ICmpInst* int1_tobool_28 = new ICmpInst(*label_entry_24, ICmpInst::ICMP_NE, int8_27, const_int8_9, "tobool");
    BranchInst::Create(label_if_then_25, label_if_end_26, int1_tobool_28, label_entry_24);

    // Block if.then (label_if_then_25)                                                                                                 
    LoadInst* int32_30 = new LoadInst(gvar_int32_fd, "", false, label_if_then_25);
    int32_30->setAlignment(4);
    std::vector<Value*> int32_call_31_params;
    int32_call_31_params.push_back(int32_30);
    int32_call_31_params.push_back(const_int64_16);
    CallInst* int32_call_31 = CallInst::Create(func_ioctl, int32_call_31_params, "call", label_if_then_25);
    int32_call_31->setCallingConv(CallingConv::C);
    int32_call_31->setTailCall(false);
    /*
    AttrListPtr int32_call_31_PAL;
    {
      SmallVector<AttributeWithIndex, 4> Attrs;
      AttributeWithIndex PAWI;
      PAWI.Index = 4294967295U; PAWI.Attrs = Attribute::None  | Attribute::NoUnwind;
      Attrs.push_back(PAWI);
      int32_call_31_PAL = AttrListPtr::get(Attrs);

    }
    int32_call_31->setAttributes(int32_call_31_PAL);
    */
    BranchInst::Create(label_if_end_26, label_if_then_25);

    // Block if.end (label_if_end_26)                                                                                                   
    ReturnInst::Create(m.getContext(), label_if_end_26);

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
