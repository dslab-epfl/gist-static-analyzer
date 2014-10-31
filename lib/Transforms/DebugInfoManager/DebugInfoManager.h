#include <llvm/IR/Function.h>

namespace llvm{
  class DebugInfoManager : public ModulePass {
  public:
    DebugInfoManager();

    const char *getPassName() const override;
    virtual bool runOnModule(Module& m);
    virtual void getAnalysisUsage(AnalysisUsage& au) const override;
    static char ID;
  };
}
