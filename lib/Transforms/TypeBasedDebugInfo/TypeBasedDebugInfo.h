
namespace llvm{
  class TypeBasedDebugInfo : public ModulePass {
  public:
    TypeBasedDebugInfo();

    const char *getPassName() const;
    virtual void getAnalysisUsage(AnalysisUsage& au) const;

    virtual bool runOnModule(Module& m);
    
    static char ID;
  };
}
