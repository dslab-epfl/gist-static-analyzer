namespace llvm{
  class DynInstMarker : public ModulePass {
  public:
      DynInstMarker() : ModulePass(ID){}
    //virtual void getAnalysisUsage(AnalysisUsage& au) const;
      virtual bool runOnModule(Module& m);
      static char ID;
  };
}
