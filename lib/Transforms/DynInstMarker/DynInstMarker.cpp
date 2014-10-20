#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/PassManager.h"
#include "llvm/IR/Instructions.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/InlineAsm.h>

#include "DynInstMarker.h"

using namespace llvm;
using namespace std;

namespace llvm {
  cl::opt<bool>
  InstLoads("inst-loads",
	    cl::desc("This will instrument all loads"),
	    cl::init(false));
  cl::opt<bool>
  InstStores("inst-stores",
	     cl::desc("This will instrument all loads"),
	     cl::init(false));
  cl::opt<bool>
  InstFun("inst-func",
	  cl::desc("This will limit the instrumentation to the given function"),
	  cl::init(false));
}

bool DynInstMarker::initInstrumentation(Module& m) {
  /*
  // Instrumentation function declaration
  resMarkerFunc = m.getFunction("_Z13resMarkerFuncv");
  assert(!resMarkerFunc && "resMarkerFunc already exists in the module!");
  if (!resMarkerFunc) {
    std::vector<Type*>resMarkerFuncTypeArgs;
    FunctionType* resMarkerFuncType = FunctionType::get(
          Type::getVoidTy(m.getContext()),
          resMarkerFuncTypeArgs,
          false);
    resMarkerFunc = Function::Create(resMarkerFuncType,
                                     GlobalValue::ExternalLinkage,
                                     "_Z13resMarkerFuncv", &m);
    resMarkerFunc->setCallingConv(CallingConv::C);
    AttributeSet resMarkerFuncAS;
    SmallVector<AttributeSet, 4> Attrs;
    AttributeSet PAS;
    AttrBuilder B;
    B.addAttribute(Attribute::NoUnwind);
    B.addAttribute(Attribute::StackProtect);
    B.addAttribute(Attribute::UWTable);
    PAS = AttributeSet::get(m.getContext(), ~0U, B);
    Attrs.push_back(PAS);
    resMarkerFuncAS = AttributeSet::get(m.getContext(), Attrs);

    resMarkerFunc->setAttributes(resMarkerFuncAS);
  } else
    return false;

  // Global variable to disable the optimization of the instrumentation function
  GlobalVariable* resChar = new GlobalVariable(*&m,
                               IntegerType::get(m.getContext(), 8),
                               false, GlobalValue::ExternalLinkage,
                               0, "resChar");
  resChar->setAlignment(1);
  ConstantInt* resCharInitValue = ConstantInt::get(m.getContext(),
                                                   APInt(8, StringRef("0"), 10));
  resChar->setInitializer(resCharInitValue);

  // Instrumentation function definition
  BasicBlock* label_entry = BasicBlock::Create(m.getContext(), "entry",resMarkerFunc,0);
  // Block entry
  LoadInst* loadInst = new LoadInst(resChar, "",
                                    true, label_entry);
  loadInst->setAlignment(1);
  ConstantInt* incrementValue = ConstantInt::get(m.getContext(),
                                                 APInt(8, StringRef("1"), 10));
  BinaryOperator* incrementOperator = BinaryOperator::Create(Instruction::Add,
                                                             loadInst, incrementValue,
                                                             "inc", label_entry);
  StoreInst* storeInst = new StoreInst(incrementOperator,
                                       resChar, true, label_entry);
  storeInst->setAlignment(1);
  ReturnInst::Create(m.getContext(), label_entry);
  */
  /*
  // Global variable to disable the optimization of the instrumentation function
  resChar = new GlobalVariable(*&m,
                               IntegerType::get(m.getContext(), 8),
                               false, GlobalValue::ExternalLinkage,
                               0, "resChar");
  resChar->setAlignment(1);
  ConstantInt* resCharInitValue = ConstantInt::get(m.getContext(),
                                                   APInt(8, StringRef("0"), 10));
  resChar->setInitializer(resCharInitValue);
  
  // 
  resCharNewValue = ConstantInt::get(m.getContext(),
                                                   APInt(8, StringRef("9"), 10));
  */
  return true;
}

void DynInstMarker::insertInstrumentation(BasicBlock::iterator& ii, 
                                          Function::iterator& bi,
                                            Module* mod) {
  std::vector<Type*>FuncTy_4_args;
  FunctionType* FuncTy_4 = FunctionType::get(
   /*Result=*/Type::getVoidTy(mod->getContext()),
   /*Params=*/FuncTy_4_args,
   /*isVarArg=*/false);
  
  InlineAsm* ptr_10 = InlineAsm::get(FuncTy_4, "nop", "~{dirflag},~{fpsr},~{flags}",true);
   CallInst* void_9 = CallInst::Create(ptr_10, "", (&(*ii)));
   void_9->setCallingConv(CallingConv::C);
   void_9->setTailCall(false);
   AttributeSet void_9_PAL;
   {
    SmallVector<AttributeSet, 4> Attrs;
    AttributeSet PAS;
     {
      AttrBuilder B;
      B.addAttribute(Attribute::NoUnwind);
      PAS = AttributeSet::get(mod->getContext(), ~0U, B);
     }
 
    Attrs.push_back(PAS);
    void_9_PAL = AttributeSet::get(mod->getContext(), Attrs);
 
   }
   void_9->setAttributes(void_9_PAL);
}

bool DynInstMarker::runOnModule(Module& m) {
  bool retVal = initInstrumentation(m);
  errs() << retVal << "\n";
  for (Module::iterator fi = m.begin(), fe = m.end(); fi != fe; ++fi) {
    errs().write_escaped(fi->getName()) << "\n";
    for (Function::iterator bi = fi->begin(), be = fi->end(); bi != be; ++bi) {
      for (BasicBlock::iterator ii = bi->begin(), ie = bi->end(); ii != ie; ++ii) {
        if (isa<LoadInst>(*ii) && 
            !fi->getName().equals(StringRef("_Z13resMarkerFuncv"))) { // TODO: optimize this
          insertInstrumentation(ii, bi, &m);
        }
      }
    }
  }
  return retVal;
}

char DynInstMarker::ID = 0;

//static RegisterPass<DynInstMarker> X("marker", "Marker pass", false, false);

static void registerMyPass(const PassManagerBuilder &,
                           PassManagerBase &PM) {
    PM.add(new DynInstMarker());
}

static RegisterStandardPasses
    RegisterMyPass(PassManagerBuilder::EP_EnabledOnOptLevel0,
                   registerMyPass);
