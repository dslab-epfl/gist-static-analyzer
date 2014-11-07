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

//#include "giri/config.h"

#include "llvm/Constant.h"
#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/Type.h"

#include "/home/res/projects/res/projects/poolalloc/include/poolalloc/PoolAllocate.h"

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
removeIncompatibleTargets (const CallInst * CI,
                            std::vector<const Function *> & Targets) {
  //
  // If the call is a direct call, do not look for incompatibility.
  //
  if (CI->getCalledFunction()) return;

  //
  // Remove any function from the set of targets that has the wrong number of
  // arguments.  Find all the functions to remove first and record them in a
  // container so that we don't invalidate any iterators.
  //
  std::vector<const Function *>::iterator FI = Targets.begin();
  std::vector<const Function *>::iterator FE = Targets.begin();
  while (FI != FE) {
    const Function * F = *FI;
    if ((F->getFunctionType()->getNumParams()) != (CI->getNumOperands() - 1)) {
      if (FI == Targets.begin()) {
        Targets.erase (Targets.begin());
        FI = Targets.begin();
      } else {
        Targets.erase (FI--);
      }
    } else {
      ++F;
    }
  }

  return;
}

//
// Module Pass: FindFlows
//
// Description:
//  This pass analyzes a function and determines the sources of information
//  that may need checks or label propagation through the heap.
//
struct FindFlows : public ModulePass {
  public:
    //////////////////////////////////////////////////////////////////////////
    // LLVM Pass Variables and Methods 
    //////////////////////////////////////////////////////////////////////////

    static char ID;
    FindFlows () : ModulePass (ID) { }
    virtual bool runOnModule (Module & M);

    const char *getPassName() const {
      return "Find Flows";
    }

    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      // We need DSA information for finding indirect function call targets
      AU.addRequired<EQTDDataStructures>();

      // This pass is an analysis pass, so it does not modify anything
      AU.setPreservesAll();
    };

    virtual void releaseMemory () {
      Sources.clear();
    }

    //////////////////////////////////////////////////////////////////////////
    // Public type definitions
    //////////////////////////////////////////////////////////////////////////
    typedef std::set<const Value *> SourceSet;
    typedef std::map<const Function *, SourceSet > SourceMap;
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

    bool returnNeedsLabel (const ReturnInst & RI) {
      return (Returns.find (&RI) != Returns.end());
    }

    bool argNeedsLabel (const Argument * Arg) {
      return (Args.find (Arg) != Args.end());
    }

  private:
    // Private typedefs
    typedef std::vector<std::pair<Value *, const Function * > > Worklist_t;
    typedef std::set<const Value *> Processed_t;

    // Private methods
    void findSources (Function & F);
    void findCallSources (CallInst * CI, Worklist_t & Wl, Processed_t & P);
    void findArgSources (Argument * Arg, Worklist_t & Wl, Processed_t & P);
    void findFlow (Value * V, const Function & F);
    void addSource (const Value * V, const Function * F);
    void findCallTargets (CallInst * CI, std::vector<const Function *> & Tgts);

    // Map from values needing labels to sources from which those labels derive
    SourceMap Sources;

    // Set of phi nodes that will need special processing
    std::set<const PHINode *> PhiNodes;

    // Set of return instructions that require labels
    std::set<const ReturnInst *> Returns;

    // Set of function arguments that require labels
    std::set<const Argument *> Args;

    // Worklist of return instructions to process
    std::map<Function *, std::set<Argument *> > ArgWorklist;

    // Passes used by this pass
    EQTDDataStructures * dsaPass;
};

#endif

