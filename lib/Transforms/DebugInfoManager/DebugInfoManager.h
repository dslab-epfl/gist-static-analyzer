//#include <llvm/Function.h>

namespace llvm{
  class DebugInfoManager : public ModulePass {
  public:
    DebugInfoManager();

    const char *getPassName() const;
    virtual void getAnalysisUsage(AnalysisUsage& au) const;
    void printDebugInfo(Instruction& instr);

    void trackUseDefChain(Value& value);
    virtual bool runOnModule(Module& m);
    Instruction* targetInstruction;
    Function* targetFunction;

    static char ID;
  };
}
