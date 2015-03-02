#include<set>

namespace llvm{
  class SrcToLLVM : public ModulePass {
  public:
    SrcToLLVM();

    const char *getPassName() const;
    virtual void getAnalysisUsage(AnalysisUsage& au) const;

    virtual bool runOnModule(Module& m);

    static char ID;
  };
}
