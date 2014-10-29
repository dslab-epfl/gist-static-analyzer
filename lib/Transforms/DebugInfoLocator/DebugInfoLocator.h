#include <llvm/IR/Function.h>

namespace llvm{
  class DebugInfoLocator : public ModulePass {
  public:
    DebugInfoLocator();

    const char *getPassName() const override;
    virtual bool runOnModule(Module& m);
    virtual void getAnalysisUsage(AnalysisUsage& au) const override;
    static char ID;
  };
}
