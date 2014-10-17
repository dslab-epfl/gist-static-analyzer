

namespace llvm{
  class DynInsMarker : public ModulePass {
  public:
      DynInsMarker() : ModulePass(ID){}
      virtual void getAnalysisUsage(AnalysisUsage& au) const;
      virtual bool runOnModule(Module& m);
      static char ID;
  };
}
