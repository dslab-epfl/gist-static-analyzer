#include<set>

namespace llvm{
  class IntelPTInstrumentor : public ModulePass {
  public:
    IntelPTInstrumentor();
    const char *getPassName() const;
    virtual void getAnalysisUsage(AnalysisUsage& au) const;
    void printDebugInfo(Instruction& instr);
    virtual bool runOnModule(Module& m);
    void setUpInstrumentation(Module& m);

    std::vector<Value*> targetOperands;
    std::vector<Instruction*> targetInstructions;
    std::vector<Function*> targetFunctions;

    Function* func_startPt;
    Function* func_stopPt;
    bool instrSetup;
    bool startHandled;
    bool stopHandled;
    static char ID;
  };
}
