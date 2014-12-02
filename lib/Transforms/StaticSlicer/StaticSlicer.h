//===- FindFlows.h - Find information flows within a program -----------------//
// 
//                          The Information Flow Compiler
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file implements a pass that finds information flows within a program.
// To be more precise, it identifies where information flow checks are needed
// and where information escapes into memory and then finds the source of that
// information.
//
//===----------------------------------------------------------------------===//

#ifndef _CIF_FINDFLOWS_H_
#define _CIF_FINDFLOWS_H_

#include "llvm/Constant.h"
#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/Type.h"

#include "../../../projects/poolalloc/include/poolalloc/PoolAllocate.h"
#include "../DebugInfoManager/DebugInfoManager.h"

#include <map>
#include <set>
#include <vector>

using namespace llvm;

//
// Function: removeIncompatibleTargets()
//
// Description:
//  If the call is an indirect call, remove all targets from the list that have
//  an incompatible number of arguments.
//
// Inputs:
//  CI      - The call instruction.
//  Targets - The set of function call targets previously computed for this
//            call instruction.
//
// Outputs:
//  Targets - The same set as was input with mismatching targets removed.
//
// Note:
//  This function is here because it is needed by several classes.
//
static void inline
removeIncompatibleTargets (const CallInst* CI,
                           std::vector<const Function *> & Targets) {
  // If the call is a direct call, do not look for incompatibility.
  // [BK] We shouldn't be getting here with a direct function call
  if (CI->getCalledFunction()) 
    return;

  // Remove any function from the set of targets that has the wrong number of
  // arguments.  Find all the functions to remove first and record them in a
  // container so that we don't invalidate any iterators.
  std::vector<const Function*>::iterator it = Targets.begin();
  
  while(it != Targets.end()) {
    if((*it)->getFunctionType()->getNumParams() != (CI->getNumOperands() - 1))
      it = Targets.erase(it);
    else 
      ++it;
  }

  return;
}

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
    StaticSlice () : ModulePass (ID), dsaPass(NULL), debugInfoManager(NULL) {
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
      Sources.clear();
    }

    //////////////////////////////////////////////////////////////////////////
    // Public type definitions
    //////////////////////////////////////////////////////////////////////////
    typedef std::set<const Value*> SourceSet;
    typedef std::map<const Function*, SourceSet > SourceMap;
    typedef SourceSet::iterator src_iterator;

    //////////////////////////////////////////////////////////////////////////
    // Public, class specific methods
    //////////////////////////////////////////////////////////////////////////
    src_iterator src_begin(const Function * F) {
      return Sources[F].begin();
    }

    src_iterator src_end(const Function * F) {
      return Sources[F].end();
    }

    const std::set<const PHINode *> & getPHINodes (void) {
      return PhiNodes;
    }

  private:
    // Private typedefs
    typedef std::vector<std::pair<Value *, const Function * > > Worklist_t;
    typedef std::set<const Value *> Processed_t;

    // Private methods
    void findSources (Function & F);
    void findCallSources (CallInst * CI, Worklist_t & Wl, Processed_t & P);
    void findArgSources  (Argument * Arg, Worklist_t & Wl, Processed_t & P);
    void findFlow  (Value * V, const Function & F);
    bool isASource (Worklist_t& Worklist, Processed_t& Processed, const Value * v, const Function * F);
    void addSource (const Value * V, const Function * F);
    
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
                                   Function* f,
                                   MDNode* node);
    
    void cacheCallInstructions(Module& module);
    
    MDNode* extractAllocaDebugMetadata (AllocaInst* allocaInst);
    
    // Map from values needing labels to sources from which those labels derive
    SourceMap Sources;

    // Set of phi nodes that will need special processing
    std::set<const PHINode *> PhiNodes;

    // Passes used by this pass
    EQTDDataStructures* dsaPass;
    DebugInfoManager* debugInfoManager;
    
    // For all the source values we save, try to keep as accurate debug information as possible
    // The debug information for a given value may be 
    std::map<Value*, std::set<MDNode*> > valueToDbgMetadata;
    
    std::set<std::string> filteredFunctions;
    std::set<std::string> specialFunctions;
    
    std::map<const Function*, CallInst*> funcToCallInst;
    
    typedef std::map<Function*, std::vector<CallInst*> > CallInstrCache_t;
    CallInstrCache_t callInstrCache;
    
    typedef std::map<CallInst*, std::pair<std::vector<const Function*>, std::vector<Value*> > > CallTargetsCache_t;
    CallTargetsCache_t callTargetsCache;
    
    std::ofstream debugLogFile;
     
};

#endif
