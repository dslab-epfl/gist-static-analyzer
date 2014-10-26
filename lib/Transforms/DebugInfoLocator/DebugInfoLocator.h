#include <llvm/IR/Function.h>

namespace llvm{
  class DebugInfoLocator : public ModulePass {
  public:
    DebugInfoLocator() : ModulePass(ID){}

    const char *getPassName() const override { return "DebugInfoLocator"; }
    virtual bool runOnModule(Module& m);

    static char ID;
  };
}
