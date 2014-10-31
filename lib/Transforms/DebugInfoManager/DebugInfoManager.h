#include <llvm/IR/Function.h>

namespace llvm{
  class DebugInfoManager : public ModulePass {
  public:
    DebugInfoManager();

    const char *getPassName() const override;
    virtual void getAnalysisUsage(AnalysisUsage& au) const override;
    void printDebugInfo(Instruction& instr);

    void trackUseDefChain(Value& value);
    virtual bool runOnModule(Module& m);

    static char ID;
  };
}
