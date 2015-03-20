#include "llvm/Pass.h"
#include "llvm/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/PassManager.h"
#include "llvm/Instructions.h"
#include "llvm/DebugInfo.h"
#include <llvm/Constants.h>
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

namespace llvm{
    cl::opt<bool> EnableIntelPtPass("enable-intelpt",
                                    cl::desc("File name to start PT tracing"),
                                    cl::init(false));
}

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


char IntelPTInstrumentor::ID = 0;


INITIALIZE_PASS(IntelPTInstrumentor, "intel-pt-instrument",
                "IntelPTInstrumentor: Do Intel PT instrumentation",
                false, false)


ModulePass* llvm::createIntelPTInstrumentorPass() {
  return new IntelPTInstrumentor();
}


IntelPTInstrumentor::IntelPTInstrumentor() : ModulePass(ID),  func_startPt(NULL), func_stopPt(NULL), instrSetup(false) {
  if (EnableIntelPtPass) {
    cerr << "Intel PT instrumentation enabled " << endl;
    cerr << "StartFileName: " << StartFileName << endl;
    cerr << "StartFunction: " << StartFunction << endl;
    cerr << "StartLineNumber: " << StopLineNumber << endl;
    cerr << "StopFileName: " << StopFileName << endl;
    cerr << "StopFunction: " << StopFunction << endl;
    cerr << "StopLineNumber: " << StopLineNumber << endl;
    if (StartFileName == "" || StartFunction == "" || StartLineNumber == 0 ||
        StopFileName == "" || StopFunction == "" || StopLineNumber == 0) {
      errs() << "\nYou need to provide the start/stop file name, function name, and "
             << "the line number!!!" << "\n\n";
                exit(1);
    }
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

void IntelPTInstrumentor::setUpInstrumentation(Module& m) {
  cerr << "setting up instrumentation" << endl;
  // Type Definitions
  PointerType* PointerTy_0 = PointerType::get(IntegerType::get(m.getContext(), 8), 0);
 
  PointerType* PointerTy_1 = PointerType::get(IntegerType::get(m.getContext(), 32), 0);
 
  ArrayType* ArrayTy_2 = ArrayType::get(IntegerType::get(m.getContext(), 8), 9);
 
  PointerType* PointerTy_3 = PointerType::get(ArrayTy_2, 0);
 
  ArrayType* ArrayTy_4 = ArrayType::get(IntegerType::get(m.getContext(), 8), 15);
 
  PointerType* PointerTy_5 = PointerType::get(ArrayTy_4, 0);
 
  std::vector<Type*>FuncTy_6_args;
  FunctionType* FuncTy_6 = FunctionType::get(
                                             /*Result=*/Type::getVoidTy(m.getContext()),
                                             /*Params=*/FuncTy_6_args,
                                             /*isVarArg=*/false);
 
  std::vector<Type*>FuncTy_8_args;
  FuncTy_8_args.push_back(PointerTy_0);
  FunctionType* FuncTy_8 = FunctionType::get(
                                             /*Result=*/IntegerType::get(m.getContext(), 32),
                                             /*Params=*/FuncTy_8_args,
                                             /*isVarArg=*/true);
 
  PointerType* PointerTy_7 = PointerType::get(FuncTy_8, 0);
 
  std::vector<Type*>FuncTy_10_args;
  FuncTy_10_args.push_back(PointerTy_0);
  FuncTy_10_args.push_back(IntegerType::get(m.getContext(), 32));
  FunctionType* FuncTy_10 = FunctionType::get(
                                              /*Result=*/IntegerType::get(m.getContext(), 32),
                                              /*Params=*/FuncTy_10_args,
                                              /*isVarArg=*/true);
 
  PointerType* PointerTy_9 = PointerType::get(FuncTy_10, 0);
 
  std::vector<Type*>FuncTy_12_args;
  FuncTy_12_args.push_back(IntegerType::get(m.getContext(), 32));
  FuncTy_12_args.push_back(IntegerType::get(m.getContext(), 64));
  FunctionType* FuncTy_12 = FunctionType::get(
                                              /*Result=*/IntegerType::get(m.getContext(), 32),
                                              /*Params=*/FuncTy_12_args,
                                              /*isVarArg=*/true);
 
  PointerType* PointerTy_11 = PointerType::get(FuncTy_12, 0);
 
  std::vector<Type*>FuncTy_14_args;
  FuncTy_14_args.push_back(IntegerType::get(m.getContext(), 32));
  FunctionType* FuncTy_14 = FunctionType::get(
                                              /*Result=*/IntegerType::get(m.getContext(), 32),
                                              /*Params=*/FuncTy_14_args,
                                              /*isVarArg=*/false);
 
  PointerType* PointerTy_13 = PointerType::get(FuncTy_14, 0);
 
 
  // Function Declarations
 
  func_startPt = m.getFunction("startPt");
  if (!func_startPt) {
    func_startPt = Function::Create(
                                    /*Type=*/FuncTy_6,
                                    /*Linkage=*/GlobalValue::ExternalLinkage,
                                    /*Name=*/"startPt", &m); 
    func_startPt->setCallingConv(CallingConv::C);
  }
 
  Function* func_printf = m.getFunction("printf");
  if (!func_printf) {
    func_printf = Function::Create(
                                   /*Type=*/FuncTy_8,
                                   /*Linkage=*/GlobalValue::ExternalLinkage,
                                   /*Name=*/"printf", &m); // (external, no body)
    func_printf->setCallingConv(CallingConv::C);
  }
 
  Function* func_open = m.getFunction("open");
  if (!func_open) {
    func_open = Function::Create(
                                 /*Type=*/FuncTy_10,
                                 /*Linkage=*/GlobalValue::ExternalLinkage,
                                 /*Name=*/"open", &m); // (external, no body)
    func_open->setCallingConv(CallingConv::C);
  }
 
  Function* func_ioctl = m.getFunction("ioctl");
  if (!func_ioctl) {
    func_ioctl = Function::Create(
                                  /*Type=*/FuncTy_12,
                                  /*Linkage=*/GlobalValue::ExternalLinkage,
                                  /*Name=*/"ioctl", &m); // (external, no body)
    func_ioctl->setCallingConv(CallingConv::C);
  }
 
  func_stopPt = m.getFunction("stopPt");
  if (!func_stopPt) {
    func_stopPt = Function::Create(
                                   /*Type=*/FuncTy_6,
                                   /*Linkage=*/GlobalValue::ExternalLinkage,
                                   /*Name=*/"stopPt", &m); 
    func_stopPt->setCallingConv(CallingConv::C);
  }
 
  Function* func_close = m.getFunction("close");
  if (!func_close) {
    func_close = Function::Create(
                                  /*Type=*/FuncTy_14,
                                  /*Linkage=*/GlobalValue::ExternalLinkage,
                                  /*Name=*/"close", &m); // (external, no body)
    func_close->setCallingConv(CallingConv::C);
  }
 
  // Global Variable Declarations

 
  GlobalVariable* gvar_int8_pt = new GlobalVariable(/*Module=*/m, 
                                                    /*Type=*/IntegerType::get(m.getContext(), 8),
                                                    /*isConstant=*/false,
                                                    /*Linkage=*/GlobalValue::ExternalLinkage,
                                                    /*Initializer=*/0, // has initializer, specified below
                                                    /*Name=*/"pt");
  gvar_int8_pt->setAlignment(1);
 
  GlobalVariable* gvar_int32_fd = new GlobalVariable(/*Module=*/m, 
                                                     /*Type=*/IntegerType::get(m.getContext(), 32),
                                                     /*isConstant=*/false,
                                                     /*Linkage=*/GlobalValue::ExternalLinkage,
                                                     /*Initializer=*/0, // has initializer, specified below
                                                     /*Name=*/"fd");
  gvar_int32_fd->setAlignment(4);
 
  GlobalVariable* gvar_array__str = new GlobalVariable(/*Module=*/m, 
                                                       /*Type=*/ArrayTy_2,
                                                       /*isConstant=*/true,
                                                       /*Linkage=*/GlobalValue::PrivateLinkage,
                                                       /*Initializer=*/0, // has initializer, specified below
                                                       /*Name=*/".str");
  gvar_array__str->setAlignment(1);
 
  GlobalVariable* gvar_array__str1 = new GlobalVariable(/*Module=*/m, 
                                                        /*Type=*/ArrayTy_4,
                                                        /*isConstant=*/true,
                                                        /*Linkage=*/GlobalValue::PrivateLinkage,
                                                        /*Initializer=*/0, // has initializer, specified below
                                                        /*Name=*/".str1");
  gvar_array__str1->setAlignment(1);
 
  // Constant Definitions
  ConstantInt* const_int8_15 = ConstantInt::get(m.getContext(), APInt(8, StringRef("0"), 10));
  ConstantInt* const_int32_16 = ConstantInt::get(m.getContext(), APInt(32, StringRef("0"), 10));
  Constant *const_array_17 = ConstantDataArray::getString(m.getContext(), "stop Pt!", true);
  Constant *const_array_18 = ConstantDataArray::getString(m.getContext(), "/dev/simple-pt", true);
  std::vector<Constant*> const_ptr_19_indices;
  const_ptr_19_indices.push_back(const_int32_16);
  const_ptr_19_indices.push_back(const_int32_16);
  Constant* const_ptr_19 = ConstantExpr::getGetElementPtr(gvar_array__str, const_ptr_19_indices);
  ConstantInt* const_int8_20 = ConstantInt::get(m.getContext(), APInt(8, StringRef("1"), 10));
  ConstantInt* const_int32_21 = ConstantInt::get(m.getContext(), APInt(32, StringRef("1"), 10));
  std::vector<Constant*> const_ptr_22_indices;
  const_ptr_22_indices.push_back(const_int32_16);
  const_ptr_22_indices.push_back(const_int32_16);
  Constant* const_ptr_22 = ConstantExpr::getGetElementPtr(gvar_array__str1, const_ptr_22_indices);
  ConstantInt* const_int32_23 = ConstantInt::get(m.getContext(), APInt(32, StringRef("524288"), 10));
  ConstantInt* const_int64_24 = ConstantInt::get(m.getContext(), APInt(64, StringRef("9904"), 10));
  ConstantInt* const_int64_25 = ConstantInt::get(m.getContext(), APInt(64, StringRef("9905"), 10));
 
  // Global Variable Definitions
  gvar_int8_pt->setInitializer(const_int8_15);
  gvar_int32_fd->setInitializer(const_int32_16);
  gvar_array__str->setInitializer(const_array_17);
  gvar_array__str1->setInitializer(const_array_18);
 
  // Function Definitions
 
  // Function: startPt (func_startPt)
  {
  
    BasicBlock* label_entry = BasicBlock::Create(m.getContext(), "entry",func_startPt,0);
    BasicBlock* label_if_then = BasicBlock::Create(m.getContext(), "if.then",func_startPt,0);
    BasicBlock* label_if_then2 = BasicBlock::Create(m.getContext(), "if.then2",func_startPt,0);
    BasicBlock* label_if_end = BasicBlock::Create(m.getContext(), "if.end",func_startPt,0);
    BasicBlock* label_if_end5 = BasicBlock::Create(m.getContext(), "if.end5",func_startPt,0);
  
    // Block entry (label_entry)
    CallInst* int32_call = CallInst::Create(func_printf, const_ptr_19, "call", label_entry);
    int32_call->setCallingConv(CallingConv::C);
    int32_call->setTailCall(false);
  
    LoadInst* int8_26 = new LoadInst(gvar_int8_pt, "", false, label_entry);
    int8_26->setAlignment(1);
    ICmpInst* int1_tobool = new ICmpInst(*label_entry, ICmpInst::ICMP_NE, int8_26, const_int8_15, "tobool");
    BranchInst::Create(label_if_end5, label_if_then, int1_tobool, label_entry);
  
    // Block if.then (label_if_then)
    StoreInst* void_28 = new StoreInst(const_int8_20, gvar_int8_pt, false, label_if_then);
    void_28->setAlignment(1);
    LoadInst* int32_29 = new LoadInst(gvar_int32_fd, "", false, label_if_then);
    int32_29->setAlignment(4);
    ICmpInst* int1_tobool1 = new ICmpInst(*label_if_then, ICmpInst::ICMP_NE, int32_29, const_int32_16, "tobool1");
    BranchInst::Create(label_if_end, label_if_then2, int1_tobool1, label_if_then);
  
    // Block if.then2 (label_if_then2)
    StoreInst* void_31 = new StoreInst(const_int32_21, gvar_int32_fd, false, label_if_then2);
    void_31->setAlignment(4);
    std::vector<Value*> int32_call3_params;
    int32_call3_params.push_back(const_ptr_22);
    int32_call3_params.push_back(const_int32_23);
    CallInst* int32_call3 = CallInst::Create(func_open, int32_call3_params, "call3", label_if_then2);
    int32_call3->setCallingConv(CallingConv::C);
    int32_call3->setTailCall(false);
  
    BranchInst::Create(label_if_end, label_if_then2);
  
    // Block if.end (label_if_end)
    LoadInst* int32_33 = new LoadInst(gvar_int32_fd, "", false, label_if_end);
    int32_33->setAlignment(4);
    std::vector<Value*> int32_call4_params;
    int32_call4_params.push_back(int32_33);
    int32_call4_params.push_back(const_int64_24);
    CallInst* int32_call4 = CallInst::Create(func_ioctl, int32_call4_params, "call4", label_if_end);
    int32_call4->setCallingConv(CallingConv::C);
    int32_call4->setTailCall(false);
  
    BranchInst::Create(label_if_end5, label_if_end);
  
    // Block if.end5 (label_if_end5)
    ReturnInst::Create(m.getContext(), label_if_end5);  
 }
 
  // Function: stopPt (func_stopPt)
  {
  
    BasicBlock* label_entry_36 = BasicBlock::Create(m.getContext(), "entry",func_stopPt,0);
    BasicBlock* label_if_then_37 = BasicBlock::Create(m.getContext(), "if.then",func_stopPt,0);
    BasicBlock* label_if_then2_38 = BasicBlock::Create(m.getContext(), "if.then2",func_stopPt,0);
    BasicBlock* label_if_end_39 = BasicBlock::Create(m.getContext(), "if.end",func_stopPt,0);
    BasicBlock* label_if_end6 = BasicBlock::Create(m.getContext(), "if.end6",func_stopPt,0);
  
    // Block entry (label_entry_36)
    CallInst* int32_call_40 = CallInst::Create(func_printf, const_ptr_19, "call", label_entry_36);
    int32_call_40->setCallingConv(CallingConv::C);
    int32_call_40->setTailCall(false);
  
    LoadInst* int8_41 = new LoadInst(gvar_int8_pt, "", false, label_entry_36);
    int8_41->setAlignment(1);
    ICmpInst* int1_tobool_42 = new ICmpInst(*label_entry_36, ICmpInst::ICMP_NE, int8_41, const_int8_15, "tobool");
    BranchInst::Create(label_if_then_37, label_if_end6, int1_tobool_42, label_entry_36);
  
    // Block if.then (label_if_then_37)
    LoadInst* int32_44 = new LoadInst(gvar_int32_fd, "", false, label_if_then_37);
    int32_44->setAlignment(4);
    ICmpInst* int1_tobool1_45 = new ICmpInst(*label_if_then_37, ICmpInst::ICMP_NE, int32_44, const_int32_16, "tobool1");
    BranchInst::Create(label_if_end_39, label_if_then2_38, int1_tobool1_45, label_if_then_37);
  
    // Block if.then2 (label_if_then2_38)
    StoreInst* void_47 = new StoreInst(const_int32_21, gvar_int32_fd, false, label_if_then2_38);
    void_47->setAlignment(4);
    std::vector<Value*> int32_call3_48_params;
    int32_call3_48_params.push_back(const_ptr_22);
    int32_call3_48_params.push_back(const_int32_23);
    CallInst* int32_call3_48 = CallInst::Create(func_open, int32_call3_48_params, "call3", label_if_then2_38);
    int32_call3_48->setCallingConv(CallingConv::C);
    int32_call3_48->setTailCall(false);
  
    BranchInst::Create(label_if_end_39, label_if_then2_38);
  
    // Block if.end (label_if_end_39)
    LoadInst* int32_50 = new LoadInst(gvar_int32_fd, "", false, label_if_end_39);
    int32_50->setAlignment(4);
    std::vector<Value*> int32_call4_51_params;
    int32_call4_51_params.push_back(int32_50);
    int32_call4_51_params.push_back(const_int64_25);
    CallInst* int32_call4_51 = CallInst::Create(func_ioctl, int32_call4_51_params, "call4", label_if_end_39);
    int32_call4_51->setCallingConv(CallingConv::C);
    int32_call4_51->setTailCall(false);
  
    LoadInst* int32_52 = new LoadInst(gvar_int32_fd, "", false, label_if_end_39);
    int32_52->setAlignment(4);
    CallInst* int32_call5 = CallInst::Create(func_close, int32_52, "call5", label_if_end_39);
    int32_call5->setCallingConv(CallingConv::C);
    int32_call5->setTailCall(false);
  
    BranchInst::Create(label_if_end6, label_if_end_39);
  
    // Block if.end6 (label_if_end6)
    ReturnInst::Create(m.getContext(), label_if_end6);
  
  }
}

bool IntelPTInstrumentor::runOnModule(Module& m) {
  if (!EnableIntelPtPass)
    return false;

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
                  if(!instrSetup)
                    setUpInstrumentation(m);
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
  return true;
}
