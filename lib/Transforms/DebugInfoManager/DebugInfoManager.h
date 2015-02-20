
namespace llvm{
  class DebugInfoManager : public ModulePass {
  public:
    DebugInfoManager();

    const char *getPassName() const;
    virtual void getAnalysisUsage(AnalysisUsage& au) const;
    void printDebugInfo(Instruction& instr);

    void trackUseDefChain(Value& value);
    virtual bool runOnModule(Module& m);
    std::vector<Value*> targetOperands;
    std::vector<Instruction*> targetInstructions;
    std::vector<Function*> targetFunctions;

    static char ID;
  };
}
