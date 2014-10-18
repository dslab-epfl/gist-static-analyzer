namespace llvm{
  class DynInstMarker : public ModulePass {
  public:
      DynInstMarker() : ModulePass(ID){}
    //virtual void getAnalysisUsage(AnalysisUsage& au) const;
      virtual bool runOnModule(Module& m);
      const char *getPassName() const override { return "DynInstMarker"; }
      static char ID;
  };
}
