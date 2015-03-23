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


IntelPTInstrumentor::IntelPTInstrumentor() : ModulePass(ID),  func_startPt(NULL), func_stopPt(NULL), startHandled(false), stopHandled(false) {
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

void IntelPTInstrumentor::justPT(Module* mod) {

 // Type Definitions
 PointerType* PointerTy_0 = PointerType::get(IntegerType::get(mod->getContext(), 8), 0);
 
 PointerType* PointerTy_1 = PointerType::get(IntegerType::get(mod->getContext(), 32), 0);
 
 ArrayType* ArrayTy_2 = ArrayType::get(IntegerType::get(mod->getContext(), 8), 15);
 
 PointerType* PointerTy_3 = PointerType::get(ArrayTy_2, 0);
 
 std::vector<Type*>FuncTy_4_args;
 FunctionType* FuncTy_4 = FunctionType::get(
  /*Result=*/Type::getVoidTy(mod->getContext()),
  /*Params=*/FuncTy_4_args,
  /*isVarArg=*/false);
 
 std::vector<Type*>FuncTy_6_args;
 FuncTy_6_args.push_back(PointerTy_0);
 FuncTy_6_args.push_back(IntegerType::get(mod->getContext(), 32));
 FunctionType* FuncTy_6 = FunctionType::get(
  /*Result=*/IntegerType::get(mod->getContext(), 32),
  /*Params=*/FuncTy_6_args,
  /*isVarArg=*/true);
 
 PointerType* PointerTy_5 = PointerType::get(FuncTy_6, 0);
 
 std::vector<Type*>FuncTy_8_args;
 FuncTy_8_args.push_back(IntegerType::get(mod->getContext(), 32));
 FuncTy_8_args.push_back(IntegerType::get(mod->getContext(), 64));
 FunctionType* FuncTy_8 = FunctionType::get(
  /*Result=*/IntegerType::get(mod->getContext(), 32),
  /*Params=*/FuncTy_8_args,
  /*isVarArg=*/true);
 
 PointerType* PointerTy_7 = PointerType::get(FuncTy_8, 0);
 
 std::vector<Type*>FuncTy_10_args;
 FuncTy_10_args.push_back(IntegerType::get(mod->getContext(), 32));
 FunctionType* FuncTy_10 = FunctionType::get(
  /*Result=*/IntegerType::get(mod->getContext(), 32),
  /*Params=*/FuncTy_10_args,
  /*isVarArg=*/false);
 
 PointerType* PointerTy_9 = PointerType::get(FuncTy_10, 0);
 
 
 // Function Declarations
 
 func_startPt = mod->getFunction("startPt");
 if (!func_startPt) {
 func_startPt = Function::Create(
  /*Type=*/FuncTy_4,
  /*Linkage=*/GlobalValue::ExternalLinkage,
  /*Name=*/"startPt", mod); 
 func_startPt->setCallingConv(CallingConv::C);
 }
 AttrListPtr func_startPt_PAL;
 {
  SmallVector<AttributeWithIndex, 4> Attrs;
  AttributeWithIndex PAWI;
  PAWI.Index = 4294967295U;
 {
    AttrBuilder B;
    B.addAttribute(Attributes::NoUnwind);
    B.addAttribute(Attributes::UWTable);
    PAWI.Attrs = Attributes::get(mod->getContext(), B);
 }
  Attrs.push_back(PAWI);
  func_startPt_PAL = AttrListPtr::get(mod->getContext(), Attrs);
  
 }
 func_startPt->setAttributes(func_startPt_PAL);
 
 Function* func_open = mod->getFunction("open");
 if (!func_open) {
 func_open = Function::Create(
  /*Type=*/FuncTy_6,
  /*Linkage=*/GlobalValue::ExternalLinkage,
  /*Name=*/"open", mod); // (external, no body)
 func_open->setCallingConv(CallingConv::C);
 }
 AttrListPtr func_open_PAL;
 func_open->setAttributes(func_open_PAL);
 
 Function* func_ioctl = mod->getFunction("ioctl");
 if (!func_ioctl) {
 func_ioctl = Function::Create(
  /*Type=*/FuncTy_8,
  /*Linkage=*/GlobalValue::ExternalLinkage,
  /*Name=*/"ioctl", mod); // (external, no body)
 func_ioctl->setCallingConv(CallingConv::C);
 }
 AttrListPtr func_ioctl_PAL;
 {
  SmallVector<AttributeWithIndex, 4> Attrs;
  AttributeWithIndex PAWI;
  PAWI.Index = 4294967295U;
 {
    AttrBuilder B;
    B.addAttribute(Attributes::NoUnwind);
    PAWI.Attrs = Attributes::get(mod->getContext(), B);
 }
  Attrs.push_back(PAWI);
  func_ioctl_PAL = AttrListPtr::get(mod->getContext(), Attrs);
  
 }
 func_ioctl->setAttributes(func_ioctl_PAL);
 
 func_stopPt = mod->getFunction("stopPt");
 if (!func_stopPt) {
 func_stopPt = Function::Create(
  /*Type=*/FuncTy_4,
  /*Linkage=*/GlobalValue::ExternalLinkage,
  /*Name=*/"stopPt", mod); 
 func_stopPt->setCallingConv(CallingConv::C);
 }
 AttrListPtr func_stopPt_PAL;
 {
  SmallVector<AttributeWithIndex, 4> Attrs;
  AttributeWithIndex PAWI;
  PAWI.Index = 4294967295U;
 {
    AttrBuilder B;
    B.addAttribute(Attributes::NoUnwind);
    B.addAttribute(Attributes::UWTable);
    PAWI.Attrs = Attributes::get(mod->getContext(), B);
 }
  Attrs.push_back(PAWI);
  func_stopPt_PAL = AttrListPtr::get(mod->getContext(), Attrs);
  
 }
 func_stopPt->setAttributes(func_stopPt_PAL);
 
 Function* func_close = mod->getFunction("close");
 if (!func_close) {
 func_close = Function::Create(
  /*Type=*/FuncTy_10,
  /*Linkage=*/GlobalValue::ExternalLinkage,
  /*Name=*/"close", mod); // (external, no body)
 func_close->setCallingConv(CallingConv::C);
 }
 AttrListPtr func_close_PAL;
 func_close->setAttributes(func_close_PAL);
 
 // Global Variable Declarations

 
 GlobalVariable* gvar_int8_mode = new GlobalVariable(/*Module=*/*mod, 
 /*Type=*/IntegerType::get(mod->getContext(), 8),
 /*isConstant=*/false,
 /*Linkage=*/GlobalValue::ExternalLinkage,
 /*Initializer=*/0, // has initializer, specified below
 /*Name=*/"mode");
 gvar_int8_mode->setAlignment(1);
 
 GlobalVariable* gvar_int32_fd = new GlobalVariable(/*Module=*/*mod, 
 /*Type=*/IntegerType::get(mod->getContext(), 32),
 /*isConstant=*/false,
 /*Linkage=*/GlobalValue::ExternalLinkage,
 /*Initializer=*/0, // has initializer, specified below
 /*Name=*/"fd");
 gvar_int32_fd->setAlignment(4);
 
 GlobalVariable* gvar_array__str = new GlobalVariable(/*Module=*/*mod, 
 /*Type=*/ArrayTy_2,
 /*isConstant=*/true,
 /*Linkage=*/GlobalValue::PrivateLinkage,
 /*Initializer=*/0, // has initializer, specified below
 /*Name=*/".str");
 gvar_array__str->setAlignment(1);
 
 // Constant Definitions
 ConstantInt* const_int8_11 = ConstantInt::get(mod->getContext(), APInt(8, StringRef("0"), 10));
 ConstantInt* const_int32_12 = ConstantInt::get(mod->getContext(), APInt(32, StringRef("0"), 10));
 Constant *const_array_13 = ConstantDataArray::getString(mod->getContext(), "/dev/simple-pt", true);
 ConstantInt* const_int8_14 = ConstantInt::get(mod->getContext(), APInt(8, StringRef("1"), 10));
 std::vector<Constant*> const_ptr_15_indices;
 const_ptr_15_indices.push_back(const_int32_12);
 const_ptr_15_indices.push_back(const_int32_12);
 Constant* const_ptr_15 = ConstantExpr::getGetElementPtr(gvar_array__str, const_ptr_15_indices);
 ConstantInt* const_int32_16 = ConstantInt::get(mod->getContext(), APInt(32, StringRef("524288"), 10));
 ConstantInt* const_int64_17 = ConstantInt::get(mod->getContext(), APInt(64, StringRef("9904"), 10));
 ConstantInt* const_int32_18 = ConstantInt::get(mod->getContext(), APInt(32, StringRef("1"), 10));
 ConstantInt* const_int8_19 = ConstantInt::get(mod->getContext(), APInt(8, StringRef("2"), 10));
 ConstantInt* const_int64_20 = ConstantInt::get(mod->getContext(), APInt(64, StringRef("9905"), 10));
 
 // Global Variable Definitions
 gvar_int8_mode->setInitializer(const_int8_11);
 gvar_int32_fd->setInitializer(const_int32_12);
 gvar_array__str->setInitializer(const_array_13);
 
 // Function Definitions
 
 // Function: startPt (func_startPt)
 {
  
  BasicBlock* label_entry = BasicBlock::Create(mod->getContext(), "entry",func_startPt,0);
  BasicBlock* label_if_then = BasicBlock::Create(mod->getContext(), "if.then",func_startPt,0);
  BasicBlock* label_if_then2 = BasicBlock::Create(mod->getContext(), "if.then2",func_startPt,0);
  BasicBlock* label_if_end = BasicBlock::Create(mod->getContext(), "if.end",func_startPt,0);
  BasicBlock* label_if_end4 = BasicBlock::Create(mod->getContext(), "if.end4",func_startPt,0);
  
  // Block entry (label_entry)
  LoadInst* int8_21 = new LoadInst(gvar_int8_mode, "", false, label_entry);
  int8_21->setAlignment(1);
  CastInst* int32_conv = new SExtInst(int8_21, IntegerType::get(mod->getContext(), 32), "conv", label_entry);
  ICmpInst* int1_cmp = new ICmpInst(*label_entry, ICmpInst::ICMP_EQ, int32_conv, const_int32_12, "cmp");
  BranchInst::Create(label_if_then, label_if_end4, int1_cmp, label_entry);
  
  // Block if.then (label_if_then)
  StoreInst* void_23 = new StoreInst(const_int8_14, gvar_int8_mode, false, label_if_then);
  void_23->setAlignment(1);
  LoadInst* int32_24 = new LoadInst(gvar_int32_fd, "", false, label_if_then);
  int32_24->setAlignment(4);
  ICmpInst* int1_tobool = new ICmpInst(*label_if_then, ICmpInst::ICMP_NE, int32_24, const_int32_12, "tobool");
  BranchInst::Create(label_if_end, label_if_then2, int1_tobool, label_if_then);
  
  // Block if.then2 (label_if_then2)
  std::vector<Value*> int32_call_params;
  int32_call_params.push_back(const_ptr_15);
  int32_call_params.push_back(const_int32_16);
  CallInst* int32_call = CallInst::Create(func_open, int32_call_params, "call", label_if_then2);
  int32_call->setCallingConv(CallingConv::C);
  int32_call->setTailCall(false);
  AttrListPtr int32_call_PAL;
  int32_call->setAttributes(int32_call_PAL);
  
  StoreInst* void_26 = new StoreInst(int32_call, gvar_int32_fd, false, label_if_then2);
  void_26->setAlignment(4);
  BranchInst::Create(label_if_end, label_if_then2);
  
  // Block if.end (label_if_end)
  LoadInst* int32_28 = new LoadInst(gvar_int32_fd, "", false, label_if_end);
  int32_28->setAlignment(4);
  std::vector<Value*> int32_call3_params;
  int32_call3_params.push_back(int32_28);
  int32_call3_params.push_back(const_int64_17);
  CallInst* int32_call3 = CallInst::Create(func_ioctl, int32_call3_params, "call3", label_if_end);
  int32_call3->setCallingConv(CallingConv::C);
  int32_call3->setTailCall(false);
  AttrListPtr int32_call3_PAL;
  {
   SmallVector<AttributeWithIndex, 4> Attrs;
   AttributeWithIndex PAWI;
   PAWI.Index = 4294967295U;
 {
    AttrBuilder B;
    B.addAttribute(Attributes::NoUnwind);
    PAWI.Attrs = Attributes::get(mod->getContext(), B);
 }
   Attrs.push_back(PAWI);
   int32_call3_PAL = AttrListPtr::get(mod->getContext(), Attrs);
   
  }
  int32_call3->setAttributes(int32_call3_PAL);
  
  BranchInst::Create(label_if_end4, label_if_end);
  
  // Block if.end4 (label_if_end4)
  ReturnInst::Create(mod->getContext(), label_if_end4);
  
 }
 
 // Function: stopPt (func_stopPt)
 {
  
  BasicBlock* label_entry_31 = BasicBlock::Create(mod->getContext(), "entry",func_stopPt,0);
  BasicBlock* label_if_then_32 = BasicBlock::Create(mod->getContext(), "if.then",func_stopPt,0);
  BasicBlock* label_if_end_33 = BasicBlock::Create(mod->getContext(), "if.end",func_stopPt,0);
  
  // Block entry (label_entry_31)
  LoadInst* int8_34 = new LoadInst(gvar_int8_mode, "", false, label_entry_31);
  int8_34->setAlignment(1);
  CastInst* int32_conv_35 = new SExtInst(int8_34, IntegerType::get(mod->getContext(), 32), "conv", label_entry_31);
  ICmpInst* int1_cmp_36 = new ICmpInst(*label_entry_31, ICmpInst::ICMP_EQ, int32_conv_35, const_int32_18, "cmp");
  BranchInst::Create(label_if_then_32, label_if_end_33, int1_cmp_36, label_entry_31);
  
  // Block if.then (label_if_then_32)
  StoreInst* void_38 = new StoreInst(const_int8_19, gvar_int8_mode, false, label_if_then_32);
  void_38->setAlignment(1);
  LoadInst* int32_39 = new LoadInst(gvar_int32_fd, "", false, label_if_then_32);
  int32_39->setAlignment(4);
  std::vector<Value*> int32_call_40_params;
  int32_call_40_params.push_back(int32_39);
  int32_call_40_params.push_back(const_int64_20);
  CallInst* int32_call_40 = CallInst::Create(func_ioctl, int32_call_40_params, "call", label_if_then_32);
  int32_call_40->setCallingConv(CallingConv::C);
  int32_call_40->setTailCall(false);
  AttrListPtr int32_call_40_PAL;
  {
   SmallVector<AttributeWithIndex, 4> Attrs;
   AttributeWithIndex PAWI;
   PAWI.Index = 4294967295U;
 {
    AttrBuilder B;
    B.addAttribute(Attributes::NoUnwind);
    PAWI.Attrs = Attributes::get(mod->getContext(), B);
 }
   Attrs.push_back(PAWI);
   int32_call_40_PAL = AttrListPtr::get(mod->getContext(), Attrs);
   
  }
  int32_call_40->setAttributes(int32_call_40_PAL);
  
  LoadInst* int32_41 = new LoadInst(gvar_int32_fd, "", false, label_if_then_32);
  int32_41->setAlignment(4);
  CallInst* int32_call2 = CallInst::Create(func_close, int32_41, "call2", label_if_then_32);
  int32_call2->setCallingConv(CallingConv::C);
  int32_call2->setTailCall(false);
  AttrListPtr int32_call2_PAL;
  int32_call2->setAttributes(int32_call2_PAL);
  
  BranchInst::Create(label_if_end_33, label_if_then_32);
  
  // Block if.end (label_if_end_33)
  ReturnInst::Create(mod->getContext(), label_if_end_33);
  
 } 
}

void IntelPTInstrumentor::justPrint(Module* mod) {
 // Type Definitions
 PointerType* PointerTy_0 = PointerType::get(IntegerType::get(mod->getContext(), 8), 0); 
 PointerType* PointerTy_1 = PointerType::get(IntegerType::get(mod->getContext(), 32), 0); 
 ArrayType* ArrayTy_2 = ArrayType::get(IntegerType::get(mod->getContext(), 8), 11); 
 PointerType* PointerTy_3 = PointerType::get(ArrayTy_2, 0); 
 ArrayType* ArrayTy_4 = ArrayType::get(IntegerType::get(mod->getContext(), 8), 10); 
 PointerType* PointerTy_5 = PointerType::get(ArrayTy_4, 0);
 
 std::vector<Type*>FuncTy_6_args;
 FunctionType* FuncTy_6 = FunctionType::get(
  Type::getVoidTy(mod->getContext()),
  FuncTy_6_args,
  false);
 
 std::vector<Type*>FuncTy_8_args;
 FuncTy_8_args.push_back(PointerTy_0);
 FunctionType* FuncTy_8 = FunctionType::get(
  IntegerType::get(mod->getContext(), 32),
  FuncTy_8_args,
  true);
 
 PointerType* PointerTy_7 = PointerType::get(FuncTy_8, 0);
 
 std::vector<Type*>FuncTy_9_args;
 FunctionType* FuncTy_9 = FunctionType::get(
  IntegerType::get(mod->getContext(), 32),
  FuncTy_9_args,
  false);
 
 PointerType* PointerTy_10 = PointerType::get(FuncTy_6, 0);
 
 // Function Declarations
 func_startPt = mod->getFunction("startPt");
 if (!func_startPt) {
 func_startPt = Function::Create(
  FuncTy_6,
  GlobalValue::ExternalLinkage,
  "startPt", mod); 
 func_startPt->setCallingConv(CallingConv::C);
 }
 AttrListPtr func_startPt_PAL;
 {
  SmallVector<AttributeWithIndex, 4> Attrs;
  AttributeWithIndex PAWI;
  PAWI.Index = 4294967295U;
 {
    AttrBuilder B;
    B.addAttribute(Attributes::NoUnwind);
    B.addAttribute(Attributes::UWTable);
    PAWI.Attrs = Attributes::get(mod->getContext(), B);
 }
  Attrs.push_back(PAWI);
  func_startPt_PAL = AttrListPtr::get(mod->getContext(), Attrs);
  
 }
 func_startPt->setAttributes(func_startPt_PAL);
 
 Function* func_printf = mod->getFunction("printf");
 if (!func_printf) {
 func_printf = Function::Create(
  FuncTy_8,
  GlobalValue::ExternalLinkage,
  "printf", mod); // (external, no body)
 func_printf->setCallingConv(CallingConv::C);
 }
 AttrListPtr func_printf_PAL;
 func_printf->setAttributes(func_printf_PAL);
 
 func_stopPt = mod->getFunction("stopPt");
 if (!func_stopPt) {
 func_stopPt = Function::Create(
  FuncTy_6,
  GlobalValue::ExternalLinkage,
  "stopPt", mod); 
 func_stopPt->setCallingConv(CallingConv::C);
 }
 AttrListPtr func_stopPt_PAL;
 {
  SmallVector<AttributeWithIndex, 4> Attrs;
  AttributeWithIndex PAWI;
  PAWI.Index = 4294967295U;
 {
    AttrBuilder B;
    B.addAttribute(Attributes::NoUnwind);
    B.addAttribute(Attributes::UWTable);
    PAWI.Attrs = Attributes::get(mod->getContext(), B);
 }
  Attrs.push_back(PAWI);
  func_stopPt_PAL = AttrListPtr::get(mod->getContext(), Attrs);
  
 }
 func_stopPt->setAttributes(func_stopPt_PAL);


 // Global Variable Declarations 
 GlobalVariable* gvar_int8_mode = new GlobalVariable(/*Module=*/*mod, 
 IntegerType::get(mod->getContext(), 8),
 false,
 GlobalValue::ExternalLinkage,
 0, // has initializer, specified below
 "mode");
 gvar_int8_mode->setAlignment(1);
 
 GlobalVariable* gvar_int32_fd = new GlobalVariable(/*Module=*/*mod, 
 IntegerType::get(mod->getContext(), 32),
 false,
 GlobalValue::ExternalLinkage,
 0, // has initializer, specified below
 "fd");
 gvar_int32_fd->setAlignment(4);
 
 GlobalVariable* gvar_array__str = new GlobalVariable(/*Module=*/*mod, 
 ArrayTy_2,
 true,
 GlobalValue::PrivateLinkage,
 0, // has initializer, specified below
 ".str");
 gvar_array__str->setAlignment(1);
 
 GlobalVariable* gvar_array__str1 = new GlobalVariable(/*Module=*/*mod, 
 ArrayTy_4,
 true,
 GlobalValue::PrivateLinkage,
 0, // has initializer, specified below
 ".str1");
 gvar_array__str1->setAlignment(1);
 
 // Constant Definitions
 ConstantInt* const_int8_11 = ConstantInt::get(mod->getContext(), APInt(8, StringRef("0"), 10));
 ConstantInt* const_int32_12 = ConstantInt::get(mod->getContext(), APInt(32, StringRef("0"), 10));
 Constant *const_array_13 = ConstantDataArray::getString(mod->getContext(), "start Pt!\x0A", true);
 Constant *const_array_14 = ConstantDataArray::getString(mod->getContext(), "stop Pt!\x0A", true);
 std::vector<Constant*> const_ptr_15_indices;
 const_ptr_15_indices.push_back(const_int32_12);
 const_ptr_15_indices.push_back(const_int32_12);
 Constant* const_ptr_15 = ConstantExpr::getGetElementPtr(gvar_array__str, const_ptr_15_indices);
 ConstantInt* const_int8_16 = ConstantInt::get(mod->getContext(), APInt(8, StringRef("1"), 10));
 ConstantInt* const_int32_17 = ConstantInt::get(mod->getContext(), APInt(32, StringRef("1"), 10));
 std::vector<Constant*> const_ptr_18_indices;
 const_ptr_18_indices.push_back(const_int32_12);
 const_ptr_18_indices.push_back(const_int32_12);
 Constant* const_ptr_18 = ConstantExpr::getGetElementPtr(gvar_array__str1, const_ptr_18_indices);
 ConstantInt* const_int8_19 = ConstantInt::get(mod->getContext(), APInt(8, StringRef("2"), 10));
 
 // Global Variable Definitions
 gvar_int8_mode->setInitializer(const_int8_11);
 gvar_int32_fd->setInitializer(const_int32_12);
 gvar_array__str->setInitializer(const_array_13);
 gvar_array__str1->setInitializer(const_array_14);
 
 // Function Definitions
 
 // Function: startPt (func_startPt)
 {
  
  BasicBlock* label_entry = BasicBlock::Create(mod->getContext(), "entry",func_startPt,0);
  BasicBlock* label_if_then = BasicBlock::Create(mod->getContext(), "if.then",func_startPt,0);
  BasicBlock* label_if_then2 = BasicBlock::Create(mod->getContext(), "if.then2",func_startPt,0);
  BasicBlock* label_if_end = BasicBlock::Create(mod->getContext(), "if.end",func_startPt,0);
  BasicBlock* label_if_end3 = BasicBlock::Create(mod->getContext(), "if.end3",func_startPt,0);
  
  // Block entry (label_entry)
  LoadInst* int8_20 = new LoadInst(gvar_int8_mode, "", false, label_entry);
  int8_20->setAlignment(1);
  CastInst* int32_conv = new SExtInst(int8_20, IntegerType::get(mod->getContext(), 32), "conv", label_entry);
  ICmpInst* int1_cmp = new ICmpInst(*label_entry, ICmpInst::ICMP_EQ, int32_conv, const_int32_12, "cmp");
  BranchInst::Create(label_if_then, label_if_end3, int1_cmp, label_entry);
  
  // Block if.then (label_if_then)
  CallInst* int32_call = CallInst::Create(func_printf, const_ptr_15, "call", label_if_then);
  int32_call->setCallingConv(CallingConv::C);
  int32_call->setTailCall(false);
  AttrListPtr int32_call_PAL;
  int32_call->setAttributes(int32_call_PAL);
  
  StoreInst* void_22 = new StoreInst(const_int8_16, gvar_int8_mode, false, label_if_then);
  void_22->setAlignment(1);
  LoadInst* int32_23 = new LoadInst(gvar_int32_fd, "", false, label_if_then);
  int32_23->setAlignment(4);
  ICmpInst* int1_tobool = new ICmpInst(*label_if_then, ICmpInst::ICMP_NE, int32_23, const_int32_12, "tobool");
  BranchInst::Create(label_if_end, label_if_then2, int1_tobool, label_if_then);
  
  // Block if.then2 (label_if_then2)
  StoreInst* void_25 = new StoreInst(const_int32_17, gvar_int32_fd, false, label_if_then2);
  void_25->setAlignment(4);
  BranchInst::Create(label_if_end, label_if_then2);
  
  // Block if.end (label_if_end)
  BranchInst::Create(label_if_end3, label_if_end);
  
  // Block if.end3 (label_if_end3)
  ReturnInst::Create(mod->getContext(), label_if_end3);
  
 }
 
 // Function: stopPt (func_stopPt)
 {
  
  BasicBlock* label_entry_29 = BasicBlock::Create(mod->getContext(), "entry",func_stopPt,0);
  BasicBlock* label_if_then_30 = BasicBlock::Create(mod->getContext(), "if.then",func_stopPt,0);
  BasicBlock* label_if_end_31 = BasicBlock::Create(mod->getContext(), "if.end",func_stopPt,0);
  
  // Block entry (label_entry_29)
  LoadInst* int8_32 = new LoadInst(gvar_int8_mode, "", false, label_entry_29);
  int8_32->setAlignment(1);
  CastInst* int32_conv_33 = new SExtInst(int8_32, IntegerType::get(mod->getContext(), 32), "conv", label_entry_29);
  ICmpInst* int1_cmp_34 = new ICmpInst(*label_entry_29, ICmpInst::ICMP_EQ, int32_conv_33, const_int32_17, "cmp");
  BranchInst::Create(label_if_then_30, label_if_end_31, int1_cmp_34, label_entry_29);
  
  // Block if.then (label_if_then_30)
  CallInst* int32_call_36 = CallInst::Create(func_printf, const_ptr_18, "call", label_if_then_30);
  int32_call_36->setCallingConv(CallingConv::C);
  int32_call_36->setTailCall(false);
  AttrListPtr int32_call_36_PAL;
  int32_call_36->setAttributes(int32_call_36_PAL);
  
  StoreInst* void_37 = new StoreInst(const_int8_19, gvar_int8_mode, false, label_if_then_30);
  void_37->setAlignment(1);
  BranchInst::Create(label_if_end_31, label_if_then_30);
  
  // Block if.end (label_if_end_31)
  ReturnInst::Create(mod->getContext(), label_if_end_31);
  
 }
}


void IntelPTInstrumentor::setUpInstrumentation(Module* mod) {
  //justPrint(mod);
  justPT(mod);
}

bool IntelPTInstrumentor::runOnModule(Module& m) {
  if (!EnableIntelPtPass)
    return false;

  setUpInstrumentation(&m);

  unsigned startIndex = 1;
  unsigned stopIndex = 1;

  for (Module::iterator fi = m.begin(), fe = m.end(); fi != fe; ++fi) {    
    for (Function::iterator bi = fi->begin(), be = fi->end(); bi != be; ++bi) {
      startHandled = false;
      stopHandled = false;
      if(fi->getName().find(StringRef(StartFunction)) != StringRef::npos) {
        for (BasicBlock::iterator ii = bi->begin(), ie = bi->end(); ii != ie; ++ii) { 
          if(!startHandled) {
            if (MDNode *N = ii->getMetadata("dbg")) {
              DILocation Loc(N);
              unsigned lineNumber = Loc.getLineNumber();
              if(lineNumber == StartLineNumber) {
                StringRef fileName = Loc.getFilename();
                if ((fileName.find(StringRef(StartFileName))) != StringRef::npos) {
                  if(startIndex == StartIndex) {
                    cerr << "match start " << endl;
                    startHandled = true;
                    // We need a start at each predecessor of the basic block that contains this instruction
                    BasicBlock* parent = ii->getParent();
                    pred_iterator it;
                    pred_iterator et;
                    int predCount = 0;
                    for (it = pred_begin(parent), et = pred_end(parent); it != et; ++it, ++predCount) {
                      CallInst* ci = CallInst::Create(func_startPt, "", (*it)->getFirstInsertionPt());
		      ci->setCallingConv(CallingConv::C);
		      ci->setTailCall(false);

                    }
                    // If there are no predecessors, insert the instrumentation to this block
                    if (predCount == 0) {
                      cerr << "No predecessors" << endl;		      
                      CallInst* ci = CallInst::Create(func_startPt, "", parent->getFirstInsertionPt());
		      ci->setCallingConv(CallingConv::C);
		      ci->setTailCall(false);
                    }
                    startIndex = 1;
                  }
                }
              }             
            }
          }
        }
      }
      if(fi->getName().find(StringRef(StopFunction)) != StringRef::npos) {
        for (BasicBlock::iterator ii = bi->begin(), ie = bi->end(); ii != ie; ++ii) {
          if(!stopHandled) {
            if (MDNode *N = ii->getMetadata("dbg")) {
              DILocation Loc(N);
              unsigned lineNumber = Loc.getLineNumber();            
              if(lineNumber == StopLineNumber) {
                StringRef fileName = Loc.getFilename();
                if ((fileName.find(StringRef(StopFileName))) != StringRef::npos) {
                  if(stopIndex == StopIndex) {
                    cerr << "match stop " << endl;
                    stopHandled = true;
                    stopIndex = 1;
                    CallInst* ci = CallInst::Create(func_stopPt, "", ii->getParent()->getFirstInsertionPt());
		    ci->setCallingConv(CallingConv::C);
		    ci->setTailCall(false);		      
                  }            
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
