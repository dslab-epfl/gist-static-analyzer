#include <llvm/IR/Function.h>

namespace llvm{
  class DynInstMarker : public ModulePass {
  public:
    DynInstMarker() : ModulePass(ID){}

    const char *getPassName() const override { return "DynInstMarker"; }
    bool initInstrumentation(Module& m);
    void insertInstrumentation(BasicBlock::iterator& ii, 
                               Function::iterator& bi,
                               Module* m);
    virtual bool runOnModule(Module& m);

    static char ID;
  private:
    // The dynamic instrumentation marking function
    // Function* resMarkerFunc;
    StoreInst* resMarkerInst;
    ConstantInt* resCharNewValue;
    GlobalVariable* resChar;
  };
}
