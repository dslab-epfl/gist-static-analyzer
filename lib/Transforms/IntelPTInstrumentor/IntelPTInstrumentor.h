#include<set>

namespace llvm{
  class IntelPTInstrumentor : public ModulePass {
  public:
    IntelPTInstrumentor();

    const char *getPassName() const;
    virtual void getAnalysisUsage(AnalysisUsage& au) const;
    void printDebugInfo(Instruction& instr);

    virtual bool runOnModule(Module& m);
    std::vector<Value*> targetOperands;
    std::vector<Instruction*> targetInstructions;
    std::vector<Function*> targetFunctions;

    std::set<std::string>& split(const std::string &s, char delim, std::set<std::string> &elems);
    std::set<std::string> split(const std::string &s, char delim);

    static char ID;
  };
}
