//===- StaticSlicer.h - Find information flows within a program -----------------//

#ifndef _STATIC_SLICER_
#define _STATIC_SLICER_

#include "llvm/Constant.h"
#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/Type.h"

#include "../../../projects/poolalloc/include/poolalloc/PoolAllocate.h"
#include "../DebugInfoManager/DebugInfoManager.h"

#include <map>
#include <set>
#include <vector>
#include <fstream>
#include <tuple>

using namespace llvm;

static cl::opt<std::string> GftFile("gft-file",
       cl::desc("The file name that contains Intel PT function trace"),
       cl::init(""));

static cl::opt<std::string> BBFile("bb-file",
       cl::desc("The file name that contains Intel PT basic block trace"),
       cl::init(""));

// Module Pass: StaticSlice
//
// Description:
//  This pass analyzes a function and determines the sources of information
//  that may need checks or label propagation through the heap.
//
struct StaticSlice : public ModulePass {
  public:
    //////////////////////////////////////////////////////////////////////////
    // LLVM Pass Variables and Methods 
    //////////////////////////////////////////////////////////////////////////

    static char ID;
    StaticSlice () : ModulePass (ID), dsaPass(NULL), debugInfoManager(NULL) , gftTraceGiven(true), bbTraceGiven(true){
      // Filter the below set of functions from potential  
      // call sites for the arguments we are tracking
      filteredFunctions.insert("fwrite");
      filteredFunctions.insert("remove");
      filteredFunctions.insert("fprintf");
      filteredFunctions.insert("chown");
      filteredFunctions.insert("chmod");
      filteredFunctions.insert("open64");
      filteredFunctions.insert("__fxstat64");
      filteredFunctions.insert("sleep");
      filteredFunctions.insert("strncpy");
      filteredFunctions.insert("strtol");   
      filteredFunctions.insert("strlen");      
      filteredFunctions.insert("signal");
      filteredFunctions.insert("fclose");
      filteredFunctions.insert("strncasecmp");
      filteredFunctions.insert("exit");
      filteredFunctions.insert("fgetc");
      filteredFunctions.insert("ungetc");
      filteredFunctions.insert("ferror");
      filteredFunctions.insert("utime");
      filteredFunctions.insert("gettimeofday");
      filteredFunctions.insert("read");
      filteredFunctions.insert("close");
      filteredFunctions.insert("fopen64");
      filteredFunctions.insert("sysconf");
      filteredFunctions.insert("__xstat64");
      filteredFunctions.insert("lseek64");
      filteredFunctions.insert("usleep");
      filteredFunctions.insert("write");
      
      // Some functions like pthread_create require special 
      // care when tracing call chains, as they are not explicitly
      // called, but the runtime calls them
      specialFunctions.insert("pthread_create");

      std::ifstream bbFile(StringRef(BBFile).str().c_str());
      if(!bbFile.good())
        bbTraceGiven = false;
      else {
        std::string bbFileLine;
        std::string currentFile = "";
        while (std::getline(bbFile, bbFileLine))
          if(bbFileLine.at(0) == '/') {
            currentFile = bbFileLine; 
          } else {
            fileToLines[currentFile].insert(std::stoi(bbFileLine));
          }
      }
      
      std::ifstream gftFile(StringRef(GftFile).str().c_str());
      if(!gftFile.good())
        gftTraceGiven = false;
      else {  
        std::string funcName;
        while (std::getline(gftFile, funcName)) {
          ptFunctionSet.insert(funcName);
        }
      }
    }
    virtual bool runOnModule (Module& M);

    const char *getPassName() const {
      return "Static Slice";
    }

    virtual void getAnalysisUsage(AnalysisUsage& AU) const {
      // We need DSA information for finding indirect function call targets
      AU.addRequired<EQTDDataStructures>();
      AU.addRequired<DebugInfoManager>();
      // This pass is an analysis pass, so it does not modify anything
      AU.setPreservesAll();
    };

    virtual void releaseMemory () {
      sources.clear();  
    }

    //////////////////////////////////////////////////////////////////////////
    // Public type definitions
    //////////////////////////////////////////////////////////////////////////   
    //typedef std::set<const Value*> SourceSet;
    //typedef std::map<const Function*, SourceSet > SourceMap;
    //typedef SourceSet::iterator src_iterator;
    typedef std::vector<const Function*>::iterator fun_iterator;
    
    //////////////////////////////////////////////////////////////////////////
    // Public, class specific methods
    //////////////////////////////////////////////////////////////////////////
    const std::set<const PHINode *> & getPHINodes (void) {
      return PhiNodes;
    }

  private:
    // Private typedefs
    typedef std::tuple<Value *, const Function*, MDNode*> WorkItem_t;
    typedef std::vector<WorkItem_t> Worklist_t;
    typedef std::set<const Value *> Processed_t;
    
    // Private methods
    void findSources ();
    void findCallSources (CallInst * CI, Worklist_t & Wl, Processed_t & P);
    void findArgSources  (Argument * Arg, Worklist_t & Wl, Processed_t & P);
    void findFlow  ();
    bool isASource (Worklist_t& Worklist, Processed_t& Processed, const Value * v, const Function * F);
    void addSource (WorkItem_t item);
    
    void findCallTargets   (CallInst * callInst, std::vector<const Function*> & Targets, 
                            std::vector<Value*>& operands);
    void handleSpecialCall (CallInst* callInst, std::vector<const Function*>& Targets, 
                            std::vector<Value*>& operands);
    
    bool isFilteredCall (CallInst* callInst);
    bool isSpecialCall  (CallInst* callInst);
    
    void extractArgs (CallInst* callInst,
                      Argument * Arg,   
                      Processed_t& Processed,
                      std::vector<Value*>& actualArgs);
    
    void generateSliceReport(Module& module);
    
    void createDebugMetadataString(std::string& str, 
                                   const Function* f,
                                   MDNode* node);
    
    void cacheCallInstructions(Module& module);
    
    MDNode* extractAllocaDebugMetadata (AllocaInst* allocaInst);
    
    void removeIncompatibleTargets (const CallInst* CI,
                               std::vector<const Function *> & Targets);
    
    bool isInPTTrace(std::string str);
    bool isInBBTrace(std::string func, int line);
    // Set of phi nodes that will need special processing
    std::set<const PHINode *> PhiNodes;

    // Passes used by this pass
    EQTDDataStructures* dsaPass;
    DebugInfoManager* debugInfoManager;
    bool gftTraceGiven;
    bool bbTraceGiven;
    
    std::set<std::string> filteredFunctions;
    std::set<std::string> specialFunctions;
    
    typedef std::map<Function*, std::vector<CallInst*> > CallInstrCache_t;
    CallInstrCache_t callInstrCache;
    
    typedef std::map<CallInst*, std::pair<std::vector<const Function*>, std::vector<Value*> > > CallTargetsCache_t;
    CallTargetsCache_t callTargetsCache;
    
    std::ofstream debugLogFile;
     
    std::set<std::string> ptFunctionSet;
    
    std::vector<WorkItem_t> sources;
    
    std::map<std::string, std::set<int> > fileToLines;
};
#endif
