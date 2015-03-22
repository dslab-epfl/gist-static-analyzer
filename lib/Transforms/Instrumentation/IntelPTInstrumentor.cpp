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


IntelPTInstrumentor::IntelPTInstrumentor() : ModulePass(ID),  func_startPt(NULL), func_stopPt(NULL), instrSetup(false), startHandled(false), stopHandled(false) {
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

void IntelPTInstrumentor::setUpInstrumentation(Module* mod) {
  // Type Definitions                                                                                                                                                                                                                                                           
  PointerType* PointerTy_0 = PointerType::get(IntegerType::get(mod->getContext(), 8), 0);

  PointerType* PointerTy_1 = PointerType::get(IntegerType::get(mod->getContext(), 32), 0);

  ArrayType* ArrayTy_2 = ArrayType::get(IntegerType::get(mod->getContext(), 8), 10);

  PointerType* PointerTy_3 = PointerType::get(ArrayTy_2, 0);

  ArrayType* ArrayTy_4 = ArrayType::get(IntegerType::get(mod->getContext(), 8), 9);

  PointerType* PointerTy_5 = PointerType::get(ArrayTy_4, 0);

  ArrayType* ArrayTy_6 = ArrayType::get(IntegerType::get(mod->getContext(), 8), 15);

  PointerType* PointerTy_7 = PointerType::get(ArrayTy_6, 0);

  std::vector<Type*>FuncTy_8_args;
  FunctionType* FuncTy_8 = FunctionType::get(
					     /*Result=*/Type::getVoidTy(mod->getContext()),
					     /*Params=*/FuncTy_8_args,
					     /*isVarArg=*/false);

  std::vector<Type*>FuncTy_10_args;
  FuncTy_10_args.push_back(PointerTy_0);
  FunctionType* FuncTy_10 = FunctionType::get(
					      /*Result=*/IntegerType::get(mod->getContext(), 32),
					      /*Params=*/FuncTy_10_args,
					      /*isVarArg=*/true);

  PointerType* PointerTy_9 = PointerType::get(FuncTy_10, 0);

  std::vector<Type*>FuncTy_12_args;
  FuncTy_12_args.push_back(PointerTy_0);
  FuncTy_12_args.push_back(IntegerType::get(mod->getContext(), 32));
  FunctionType* FuncTy_12 = FunctionType::get(
					      /*Result=*/IntegerType::get(mod->getContext(), 32),
					      /*Params=*/FuncTy_12_args,
					      /*isVarArg=*/true);

  PointerType* PointerTy_11 = PointerType::get(FuncTy_12, 0);

  std::vector<Type*>FuncTy_14_args;
  FuncTy_14_args.push_back(IntegerType::get(mod->getContext(), 32));
  FuncTy_14_args.push_back(IntegerType::get(mod->getContext(), 64));
  FunctionType* FuncTy_14 = FunctionType::get(
					      /*Result=*/IntegerType::get(mod->getContext(), 32),
					      /*Params=*/FuncTy_14_args,
					      /*isVarArg=*/true);

  PointerType* PointerTy_13 = PointerType::get(FuncTy_14, 0);

  std::vector<Type*>FuncTy_16_args;
  FuncTy_16_args.push_back(IntegerType::get(mod->getContext(), 32));
  FunctionType* FuncTy_16 = FunctionType::get(
					      /*Result=*/IntegerType::get(mod->getContext(), 32),
					      /*Params=*/FuncTy_16_args,
					      /*isVarArg=*/false);

  PointerType* PointerTy_15 = PointerType::get(FuncTy_16, 0);


  // Function Declarations                                                                                                                                                                                                                                                      

  Function* func_startPt = mod->getFunction("startPt");
  if (!func_startPt) {
    func_startPt = Function::Create(
				    /*Type=*/FuncTy_8,
				    /*Linkage=*/GlobalValue::ExternalLinkage,
				    /*Name=*/"startPt", mod);
    func_startPt->setCallingConv(CallingConv::C);
  }

  Function* func_printf = mod->getFunction("printf");
  if (!func_printf) {
    func_printf = Function::Create(
				   /*Type=*/FuncTy_10,
				   /*Linkage=*/GlobalValue::ExternalLinkage,
				   /*Name=*/"printf", mod); // (external, no body)                                                                                                                                                                                                                              
    func_printf->setCallingConv(CallingConv::C);
  }
  AttrListPtr func_printf_PAL;
  func_printf->setAttributes(func_printf_PAL);

  Function* func_open = mod->getFunction("open");
  if (!func_open) {
    func_open = Function::Create(
				 /*Type=*/FuncTy_12,
				 /*Linkage=*/GlobalValue::ExternalLinkage,
				 /*Name=*/"open", mod); // (external, no body)                                                                                                                                                                                                                                
    func_open->setCallingConv(CallingConv::C);
  }
  AttrListPtr func_open_PAL;
  func_open->setAttributes(func_open_PAL);

  Function* func_ioctl = mod->getFunction("ioctl");
  if (!func_ioctl) {
    func_ioctl = Function::Create(
				  /*Type=*/FuncTy_14,
				  /*Linkage=*/GlobalValue::ExternalLinkage,
				  /*Name=*/"ioctl", mod); // (external, no body)                                                                                                                                                                                                                               
    func_ioctl->setCallingConv(CallingConv::C);
  }

  Function* func_stopPt = mod->getFunction("stopPt");
  if (!func_stopPt) {
    func_stopPt = Function::Create(
				   /*Type=*/FuncTy_8,
				   /*Linkage=*/GlobalValue::ExternalLinkage,
				   /*Name=*/"stopPt", mod);
    func_stopPt->setCallingConv(CallingConv::C);
  }

  Function* func_close = mod->getFunction("close");
  if (!func_close) {
    func_close = Function::Create(
				  /*Type=*/FuncTy_16,
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

  GlobalVariable* gvar_array__str1 = new GlobalVariable(/*Module=*/*mod,
							/*Type=*/ArrayTy_4,
							/*isConstant=*/true,
							/*Linkage=*/GlobalValue::PrivateLinkage,
							/*Initializer=*/0, // has initializer, specified below                                                                                                                                                                                                                        
							/*Name=*/".str1");
  gvar_array__str1->setAlignment(1);

  GlobalVariable* gvar_array__str2 = new GlobalVariable(/*Module=*/*mod,
							/*Type=*/ArrayTy_6,
							/*isConstant=*/true,
							/*Linkage=*/GlobalValue::PrivateLinkage,
							/*Initializer=*/0, // has initializer, specified below                                                                                                                                                                                                                        
							/*Name=*/".str2");
  gvar_array__str2->setAlignment(1);

  GlobalVariable* gvar_array__str3 = new GlobalVariable(/*Module=*/*mod,
							/*Type=*/ArrayTy_4,
							/*isConstant=*/true,
							/*Linkage=*/GlobalValue::PrivateLinkage,
							/*Initializer=*/0, // has initializer, specified below                                                                                                                                                                                                                        
							/*Name=*/".str3");
  gvar_array__str3->setAlignment(1);

  GlobalVariable* gvar_array__str4 = new GlobalVariable(/*Module=*/*mod,
							/*Type=*/ArrayTy_2,
							/*isConstant=*/true,
							/*Linkage=*/GlobalValue::PrivateLinkage,
							/*Initializer=*/0, // has initializer, specified below                                                                                                                                                                                                                        
							/*Name=*/".str4");
  gvar_array__str4->setAlignment(1);

  // Constant Definitions                                                                                                                                                                                                                                                       
  ConstantInt* const_int8_17 = ConstantInt::get(mod->getContext(), APInt(8, StringRef("0"), 10));
  ConstantInt* const_int32_18 = ConstantInt::get(mod->getContext(), APInt(32, StringRef("0"), 10));
  Constant *const_array_19 = ConstantDataArray::getString(mod->getContext(), "start Pt!", true);
  Constant *const_array_20 = ConstantDataArray::getString(mod->getContext(), "starting", true);
  Constant *const_array_21 = ConstantDataArray::getString(mod->getContext(), "/dev/simple-pt", true);
  Constant *const_array_22 = ConstantDataArray::getString(mod->getContext(), "stop Pt!", true);
  Constant *const_array_23 = ConstantDataArray::getString(mod->getContext(), "stopping!", true);
  std::vector<Constant*> const_ptr_24_indices;
  const_ptr_24_indices.push_back(const_int32_18);
  const_ptr_24_indices.push_back(const_int32_18);
  Constant* const_ptr_24 = ConstantExpr::getGetElementPtr(gvar_array__str, const_ptr_24_indices);
  ConstantInt* const_int8_25 = ConstantInt::get(mod->getContext(), APInt(8, StringRef("1"), 10));
  ConstantInt* const_int32_26 = ConstantInt::get(mod->getContext(), APInt(32, StringRef("1"), 10));
  std::vector<Constant*> const_ptr_27_indices;
  const_ptr_27_indices.push_back(const_int32_18);
  const_ptr_27_indices.push_back(const_int32_18);
  Constant* const_ptr_27 = ConstantExpr::getGetElementPtr(gvar_array__str1, const_ptr_27_indices);
  std::vector<Constant*> const_ptr_28_indices;
  const_ptr_28_indices.push_back(const_int32_18);
  const_ptr_28_indices.push_back(const_int32_18);
  Constant* const_ptr_28 = ConstantExpr::getGetElementPtr(gvar_array__str2, const_ptr_28_indices);
  ConstantInt* const_int32_29 = ConstantInt::get(mod->getContext(), APInt(32, StringRef("524288"), 10));
  ConstantInt* const_int64_30 = ConstantInt::get(mod->getContext(), APInt(64, StringRef("9904"), 10));
  std::vector<Constant*> const_ptr_31_indices;
  const_ptr_31_indices.push_back(const_int32_18);
  const_ptr_31_indices.push_back(const_int32_18);
  Constant* const_ptr_31 = ConstantExpr::getGetElementPtr(gvar_array__str3, const_ptr_31_indices);
  ConstantInt* const_int8_32 = ConstantInt::get(mod->getContext(), APInt(8, StringRef("2"), 10));
  std::vector<Constant*> const_ptr_33_indices;
  const_ptr_33_indices.push_back(const_int32_18);
  const_ptr_33_indices.push_back(const_int32_18);
  Constant* const_ptr_33 = ConstantExpr::getGetElementPtr(gvar_array__str4, const_ptr_33_indices);
  ConstantInt* const_int64_34 = ConstantInt::get(mod->getContext(), APInt(64, StringRef("9905"), 10));

  // Global Variable Definitions                                                                                                                                                                                                                                                
  gvar_int8_mode->setInitializer(const_int8_17);
  gvar_int32_fd->setInitializer(const_int32_18);
  gvar_array__str->setInitializer(const_array_19);
  gvar_array__str1->setInitializer(const_array_20);
  gvar_array__str2->setInitializer(const_array_21);
  gvar_array__str3->setInitializer(const_array_22);
  gvar_array__str4->setInitializer(const_array_23);

  // Function Definitions                                                                                                                                                                                                                                                       

  // Function: startPt (func_startPt)                                                                                                                                                                                                                                           
  {

    BasicBlock* label_entry = BasicBlock::Create(mod->getContext(), "entry",func_startPt,0);
    BasicBlock* label_if_then = BasicBlock::Create(mod->getContext(), "if.then",func_startPt,0);
    BasicBlock* label_if_then2 = BasicBlock::Create(mod->getContext(), "if.then2",func_startPt,0);
    BasicBlock* label_if_end = BasicBlock::Create(mod->getContext(), "if.end",func_startPt,0);
    BasicBlock* label_if_end6 = BasicBlock::Create(mod->getContext(), "if.end6",func_startPt,0);

    // Block entry (label_entry)                                                                                                                                                                                                                                                 
    CallInst* int32_call = CallInst::Create(func_printf, const_ptr_24, "call", label_entry);
    int32_call->setCallingConv(CallingConv::C);
    int32_call->setTailCall(false);
    AttrListPtr int32_call_PAL;
    int32_call->setAttributes(int32_call_PAL);

    LoadInst* int8_35 = new LoadInst(gvar_int8_mode, "", false, label_entry);
    int8_35->setAlignment(1);
    CastInst* int32_conv = new SExtInst(int8_35, IntegerType::get(mod->getContext(), 32), "conv", label_entry);
    ICmpInst* int1_cmp = new ICmpInst(*label_entry, ICmpInst::ICMP_EQ, int32_conv, const_int32_18, "cmp");
    BranchInst::Create(label_if_then, label_if_end6, int1_cmp, label_entry);

    // Block if.then (label_if_then)                                                                                                                                                                                                                                             
    StoreInst* void_37 = new StoreInst(const_int8_25, gvar_int8_mode, false, label_if_then);
    void_37->setAlignment(1);
    LoadInst* int32_38 = new LoadInst(gvar_int32_fd, "", false, label_if_then);
    int32_38->setAlignment(4);
    ICmpInst* int1_tobool = new ICmpInst(*label_if_then, ICmpInst::ICMP_NE, int32_38, const_int32_18, "tobool");
    BranchInst::Create(label_if_end, label_if_then2, int1_tobool, label_if_then);
    // Block if.then2 (label_if_then2)                                                                                                                                                                                                                                           
    StoreInst* void_40 = new StoreInst(const_int32_26, gvar_int32_fd, false, label_if_then2);
    void_40->setAlignment(4);
    CallInst* int32_call3 = CallInst::Create(func_printf, const_ptr_27, "call3", label_if_then2);
    int32_call3->setCallingConv(CallingConv::C);
    int32_call3->setTailCall(false);
    AttrListPtr int32_call3_PAL;
    int32_call3->setAttributes(int32_call3_PAL);

    std::vector<Value*> int32_call4_params;
    int32_call4_params.push_back(const_ptr_28);
    int32_call4_params.push_back(const_int32_29);
    CallInst* int32_call4 = CallInst::Create(func_open, int32_call4_params, "call4", label_if_then2);
    int32_call4->setCallingConv(CallingConv::C);
    int32_call4->setTailCall(false);
    AttrListPtr int32_call4_PAL;
    int32_call4->setAttributes(int32_call4_PAL);

    BranchInst::Create(label_if_end, label_if_then2);

    // Block if.end (label_if_end)                                                                                                                                                                                                                                               
    LoadInst* int32_42 = new LoadInst(gvar_int32_fd, "", false, label_if_end);
    int32_42->setAlignment(4);
    std::vector<Value*> int32_call5_params;
    int32_call5_params.push_back(int32_42);
    int32_call5_params.push_back(const_int64_30);
    CallInst* int32_call5 = CallInst::Create(func_ioctl, int32_call5_params, "call5", label_if_end);
    int32_call5->setCallingConv(CallingConv::C);
    int32_call5->setTailCall(false);

    BranchInst::Create(label_if_end6, label_if_end);

    // Block if.end6 (label_if_end6)                                                                                                                                                                                                                                             
    ReturnInst::Create(mod->getContext(), label_if_end6);

  }

  // Function: stopPt (func_stopPt)                                                                                                                                                                                                                                             
  {

    BasicBlock* label_entry_45 = BasicBlock::Create(mod->getContext(), "entry",func_stopPt,0);
    BasicBlock* label_if_then_46 = BasicBlock::Create(mod->getContext(), "if.then",func_stopPt,0);
    BasicBlock* label_if_then2_47 = BasicBlock::Create(mod->getContext(), "if.then2",func_stopPt,0);
    BasicBlock* label_if_end_48 = BasicBlock::Create(mod->getContext(), "if.end",func_stopPt,0);
    BasicBlock* label_if_end7 = BasicBlock::Create(mod->getContext(), "if.end7",func_stopPt,0);

    // Block entry (label_entry_45)                                                                                                                                                                                                                                              
    CallInst* int32_call_49 = CallInst::Create(func_printf, const_ptr_31, "call", label_entry_45);
    int32_call_49->setCallingConv(CallingConv::C);
    int32_call_49->setTailCall(false);
    AttrListPtr int32_call_49_PAL;
    int32_call_49->setAttributes(int32_call_49_PAL);

    LoadInst* int8_50 = new LoadInst(gvar_int8_mode, "", false, label_entry_45);
    int8_50->setAlignment(1);
    CastInst* int32_conv_51 = new SExtInst(int8_50, IntegerType::get(mod->getContext(), 32), "conv", label_entry_45);
    ICmpInst* int1_cmp_52 = new ICmpInst(*label_entry_45, ICmpInst::ICMP_EQ, int32_conv_51, const_int32_26, "cmp");
    BranchInst::Create(label_if_then_46, label_if_end7, int1_cmp_52, label_entry_45);

    // Block if.then (label_if_then_46)                                                                                                                                                                                                                                          
    StoreInst* void_54 = new StoreInst(const_int8_32, gvar_int8_mode, false, label_if_then_46);
    void_54->setAlignment(1);
    LoadInst* int32_55 = new LoadInst(gvar_int32_fd, "", false, label_if_then_46);
    int32_55->setAlignment(4);
    ICmpInst* int1_tobool_56 = new ICmpInst(*label_if_then_46, ICmpInst::ICMP_NE, int32_55, const_int32_18, "tobool");
    BranchInst::Create(label_if_end_48, label_if_then2_47, int1_tobool_56, label_if_then_46);

    // Block if.then2 (label_if_then2_47)                                                                                                                                                                                                                                        
    StoreInst* void_58 = new StoreInst(const_int32_26, gvar_int32_fd, false, label_if_then2_47);
    void_58->setAlignment(4);
    std::vector<Value*> int32_call3_59_params;
    int32_call3_59_params.push_back(const_ptr_28);
    int32_call3_59_params.push_back(const_int32_29);
    CallInst* int32_call3_59 = CallInst::Create(func_open, int32_call3_59_params, "call3", label_if_then2_47);
    int32_call3_59->setCallingConv(CallingConv::C);
    int32_call3_59->setTailCall(false);
    AttrListPtr int32_call3_59_PAL;
    int32_call3_59->setAttributes(int32_call3_59_PAL);

    BranchInst::Create(label_if_end_48, label_if_then2_47);

    // Block if.end (label_if_end_48)                                                                                                                                                                                                                                            
    CallInst* int32_call4_61 = CallInst::Create(func_printf, const_ptr_33, "call4", label_if_end_48);
    int32_call4_61->setCallingConv(CallingConv::C);
    int32_call4_61->setTailCall(false);
    AttrListPtr int32_call4_61_PAL;
    int32_call4_61->setAttributes(int32_call4_61_PAL);

    LoadInst* int32_62 = new LoadInst(gvar_int32_fd, "", false, label_if_end_48);
    int32_62->setAlignment(4);
    std::vector<Value*> int32_call5_63_params;
    int32_call5_63_params.push_back(int32_62);
    int32_call5_63_params.push_back(const_int64_34);
    CallInst* int32_call5_63 = CallInst::Create(func_ioctl, int32_call5_63_params, "call5", label_if_end_48);
    int32_call5_63->setCallingConv(CallingConv::C);
    int32_call5_63->setTailCall(false);

    LoadInst* int32_64 = new LoadInst(gvar_int32_fd, "", false, label_if_end_48);
    int32_64->setAlignment(4);
    CallInst* int32_call6 = CallInst::Create(func_close, int32_64, "call6", label_if_end_48);
    int32_call6->setCallingConv(CallingConv::C);
    int32_call6->setTailCall(false);
    AttrListPtr int32_call6_PAL;
    int32_call6->setAttributes(int32_call6_PAL);

    BranchInst::Create(label_if_end7, label_if_end_48);

    // Block if.end7 (label_if_end7)                                                                                                                                                                                                                                             
    ReturnInst::Create(mod->getContext(), label_if_end7);

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
                    if(!instrSetup)
                      setUpInstrumentation(&m);
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
      }
      if(fi->getName().find(StringRef(StopFunction)) != StringRef::npos) {
        for (BasicBlock::iterator ii = bi->begin(), ie = bi->end(); ii != ie; ++ii) {
          if(stopHandled) {
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
                    CallInst::Create(func_stopPt, "", ii);
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
