//===- FindFlows.cpp - Find information flows within a program ------------ --//
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

#define DEBUG_TYPE "giri"

#include "giri/FindFlows.h"

#include "llvm/ADT/Statistic.h"
#include "llvm/IntrinsicInst.h"
#include "llvm/LLVMContext.h"

#include <iostream>

using namespace llvm;

// ID Variable to identify the pass
char FindFlows::ID = 0;

//
// Pass registration
//
static RegisterPass<FindFlows> X ("flows", "Find Information Flows");

// Statistics
//STATISTIC (NullChecks ,    "Poolchecks with NULL pool descriptor");

//
// Function: isASource()
//
// Description:
//  This function determines whether the specified value is a source of
//  information (something that has a label independent of its input SSA
//  values).
//
// Inputs:
//  V - The value to analyze.
//
// Return value:
//  true  - This value is a source.
//  false - This value is not a source.  Its label is the join of the labels
//          of its input operands.
//
static inline bool
isASource (const Value * V) {
  //
  // Call instructions are sources *unless* they are inline assembly.
  //
  if (const CallInst * CI = dyn_cast<CallInst>(V)) {
    if (isa<InlineAsm>(CI->getOperand(0)))
      return false;
    else
      return true;
  }

  if ((isa<LoadInst>(V)) ||
      (isa<Argument>(V)) ||
      (isa<AllocaInst>(V)) ||
      (isa<Constant>(V)) ||
      (isa<GlobalValue>(V))) {
    return true;
  }

  return false;
}

//
// Method: addSource()
//
// Description:
//  The following value is a source.  Do all of the bookkeeping required.
//
// Inputs:
//  V - A source that needs to be recorded.
//
void
FindFlows::addSource (const Value * V, const Function * F) {
  //
  // Record the source in the set of sources.
  //
  Sources[F].insert (V);

  //
  // If the source is an argument, record it specially.
  //
  if (const Argument * Arg = dyn_cast<Argument>(V))
    Args.insert (Arg);
  return;
}

//
// Method: findFlow()
//
// Description:
//  For the given value, find all of the values upon which it depends.  The
//  labels of these values will combine together to form the label of the given
//  value.
//
void
FindFlows::findFlow (Value * Initial, const Function & Fu) {
  // Already processed values
  Processed_t Processed;

  // Worklist
  Worklist_t Worklist;
  Worklist.push_back (std::make_pair(Initial, &Fu));

  while (Worklist.size()) {
    //
    // Pop an item off of the worklist.
    //
    Value * V = Worklist.back().first;
    const Function * F = Worklist.back().second;
    Worklist.pop_back();

    //
    // If the value is a source, add it to the set of sources.  Otherwise, add
    // its operands to the worklist if they have not yet been processed.
    //
    //
    if (isASource (V)) {
      addSource (V, F);

      //
      // Some sources imply that information flow must be traced inside another
      // function.  Needing the label of a call instruction's return value
      // means that we need to know the sources for the called function's
      // return value.  Requiring the label of a function argument means that
      // we'll need the labels of any value passed into the function.
      //
      if (CallInst * CI = dyn_cast<CallInst>(V)) {
        findCallSources (CI, Worklist, Processed);
      } else if (Argument * Arg = dyn_cast<Argument>(V)) {
        findArgSources (Arg, Worklist, Processed);
      }
    } else if (User * U = dyn_cast<User>(V)) {
      // Record any phi nodes located
      if (PHINode * PHI = dyn_cast<PHINode>(V)) PhiNodes.insert (PHI);

      for (unsigned index = 0; index < U->getNumOperands(); ++index) {
        if (Processed.find (U->getOperand(index)) == Processed.end()) {
          Worklist.push_back (std::make_pair(U->getOperand(index), F));
          if (!(isa<Constant>(U->getOperand(index)))) Processed.insert (U->getOperand(index));
        }
      }
    }
  }

  return;
}

//
// Method: findSources()
//
// Description:
//  For every store and external function call in the specified function, find
//  all the instructions that generate a label (i.e., is a source of
//  information)  for the value(s) being stored into memory.
//
// Inputs:
//  F - The function to analyze.
//
void
FindFlows::findSources (Function & F) {
  //
  // Iterate over all instructions in the program and process those that
  // need the information flow of their inputs.
  //
  for (Function::iterator BB = F.begin(); BB != F.end(); ++BB) {
    for (BasicBlock::iterator II = BB->begin(); II != BB->end(); ++II) {
      //
      // Store instructions need to know their label so that they can attach
      // this information to the memory object to which they write.
      //
      if (StoreInst * SI = dyn_cast<StoreInst>(II)) {
        findFlow (SI->getOperand(0), F);
        continue;
      }

      //
      // Certain intrinsic functions need the labels of their inputs.
      //
      if (MemSetInst * MSI = dyn_cast<MemSetInst>(II)) {
        findFlow (MSI->getValue(), F);
        continue;
      }

      //
      // Calls to certain external library functions also need the labels of
      // their inputs.
      //
      if (CallInst * CI = dyn_cast<CallInst>(II)) {
        if (Function * CalledFunc = CI->getCalledFunction()) {
          std::string name = CalledFunc->getName().str();
          if (name == "memset") {
            findFlow (CI->getOperand(3), F);
          }
        }
      }
    }
  }

  return;
}

//
// Method: findCallTargets()
//
// Description:
//  Find the set of functions that can be called by the given call instruction.
//
// Inputs:
//  CI      - The call instruction to analyze.
//
// Outputs:
//  Targets - A list of functions that can be called by the call instruction.
//
void
FindFlows::findCallTargets (CallInst * CI,
                            std::vector<const Function *> & Targets) {
  //
  // Check to see if the call instruction is a direct call.  If so, then add
  // the target to the set of known targets and return.
  //
  Function * CalledFunc = CI->getCalledFunction();
  if (CalledFunc) {
    Targets.push_back (CalledFunc);
    return;
  }

  //
  // This is an indirect function call.  Get the DSNode for the function
  // pointer and then use that to find the set of call targets.
  //
  CallSite CS(CI);
  const DSCallGraph & CallGraph = dsaPass->getCallGraph();
  Targets.insert (Targets.end(),
                  CallGraph.callee_begin(CS),
                  CallGraph.callee_end(CS));

  //
  // Remove targets that do not match the call instruction's argument list.
  //
  removeIncompatibleTargets  (CI, Targets);
  return;
}

//
// Method: findCallSources()
//
// Description:
//  For the given call instruction, find the functions that it calls and
//  add to the worklist those values that contribute to the called functions'
//  return values.
//
// Inputs:
//  CI        - The call instruction whose return value requires a label.
//  Processed - The set of LLVM values that have already been identified as
//              part of an information flow.
//
// Outputs:
//  Worklist -  The return instructions that determine the value of the call
//              instruction are added to the worklist.
//  Processed - Items added to the worklist are also added to the Processed
//              container to ensure that they are only identified once for
//              information flow purposes.
//
void
FindFlows::findCallSources (CallInst * CI,
                            Worklist_t & Worklist,
                            Processed_t & Processed) {
  //
  // Find the function called by this call instruction.
  //
  std::vector<const Function *> Targets;
  findCallTargets (CI, Targets);

  //
  // Process each potential function call target.
  //
  const Type * VoidType = Type::getVoidTy(getGlobalContext());
  while (Targets.size()) {
    // Set of return instructions needing labels discovered
    std::vector<ReturnInst *> NewReturns;

    //
    // Process one of the functions from the list of potential call targets.
    //
    Function * F = const_cast<Function *>((Targets.back()));
    Targets.pop_back ();

    //
    // Ensure that the function's return value is not void.
    //
    assert ((F->getReturnType() != VoidType) && "Want void function label!\n");

    //
    // Add any return values in the called function to the list of return
    // instructions to process.  Note that we may add them multiple times, but
    // this is okay since Returns is a set that does not allow duplicate
    // entries.
    //
    for (Function::iterator BB = F->begin(); BB != F->end(); ++BB)
      if (ReturnInst * RI = dyn_cast<ReturnInst>(BB->getTerminator()))
        NewReturns.push_back (RI);

    //
    // Record the returns that require labels.
    //
    Returns.insert (NewReturns.begin(), NewReturns.end());

    //
    // Finally, add any return instructions that have not already been
    // processed to the worklist.
    //
    std::vector<ReturnInst *>::iterator ri;
    for (ri = NewReturns.begin(); ri != NewReturns.end(); ++ri) {
      ReturnInst * RI = *ri;
      if (Processed.find (RI) == Processed.end()) {
        Worklist.push_back (std::make_pair(RI, F));
        Processed.insert (RI);
      }
    }
  }

  return;
}

//
// Method: findArgSources()
//
// Description:
//  Find the label sources for every actual parameter in the worklist of formal
//  parameters (i.e., arguments) that need labels.
//
// Inputs:
//  Arg       - The argument for which the actual parameters must be labeled.
//  Processed - The set of LLVM values which have already been discovered as
//              part of an information flow requiring labels.
//
// Outputs:
//  Worklist  - This set is modified to contain the actual parameters that need
//              to be processed when back-tracking an information flow.
//  Processed - This set is updated to hold any new values that were added to
//              the worklist.  This will prevent them from being added multiple
//              times.
//
void
FindFlows::findArgSources (Argument * Arg,
                           Worklist_t & Worklist,
                           Processed_t & Processed) {
  //
  // Iterate over all functions in the program looking for call instructions.
  // When we find a call instruction, we will check to see if the function
  // has arguments that need labels.  If it does, we'll find the labels of
  // all the actual parameters.
  //
  Module * M = Arg->getParent()->getParent();
  for (Module::iterator F = M->begin(); F != M->end(); ++F) {
    // Set of actual arguments needing labels
    std::vector<Value *> ActualArgs;

    //
    // Scan the function looking for call instructions.
    //
    for (Function::iterator BB = F->begin(); BB != F->end(); ++BB) {
      for (BasicBlock::iterator II = BB->begin(); II != BB->end(); ++II) {
        if (CallInst * CI = dyn_cast<CallInst>(II)) {
          //
          // Ignore inline assembly code.
          //
          if (isa<InlineAsm>(CI->getOperand(0))) continue;

          //
          // Find the set of functions called by this call instruction.
          //
          std::vector <const Function *> Targets;
          findCallTargets (CI, Targets);

          //
          // Skip this call site if it does not call the function to which the
          // specified argument belongs.
          //
          Function * CalledFunc = Arg->getParent();
          std::set<const Function *> TargetSet;
          TargetSet.insert (Targets.begin(), Targets.end());
          if ((TargetSet.find (CalledFunc)) == (TargetSet.end())) continue;

          //
          // Assert that the call and the called function have the same number
          // of arguments.
          //
          assert ((CalledFunc->getFunctionType()->getNumParams()) ==
                  (CI->getNumOperands() - 1) &&
                  "Number of arguments doesn't match function signature!\n");

          //
          // Walk the argument list of the call instruction and look for actual
          // arguments needing labels.  Add them to our local worklist.
          //
          Function::arg_iterator FormalArg = CalledFunc->arg_begin();
          for (unsigned index = 1;
               index < CI->getNumOperands();
               ++index, ++FormalArg) {
            if (((Argument *)(FormalArg)) == Arg) {
              if (Processed.find (CI->getOperand(index)) == Processed.end()) {
                ActualArgs.push_back (CI->getOperand(index));
              }
            }
          }
        }
      }
    }

    //
    // Finally, find the sources for all the actual arguments needing labels.
    //
    std::vector<Value *>::iterator i;
    for (i = ActualArgs.begin(); i != ActualArgs.end(); ++i) {
      Value * V = *i;
      Worklist.push_back (std::make_pair (V, F));
    }

    //
    // Add the new items to process to the processed list.
    //
    for (unsigned index = 0; index < ActualArgs.size(); ++index) {
      if (!(isa<Constant>(ActualArgs[index])))
        Processed.insert (ActualArgs[index]);
    }
  }

  return;
}

//
// Method: runOnModule()
//
// Description:
//  Entry point for this LLVM pass.  Find statements that require the
//  label of a piece of information and then find the sources of that
//  information.
//
// Inputs:
//  M - The module to analyze.
//
// Return value:
//  false - The module was not modified.
//
bool
FindFlows::runOnModule (Module & M) {
  //
  // Get prerequisite passes.
  //
  dsaPass = &getAnalysis<EQTDDataStructures>();

  //
  // Begin by finding the sources of all labels for store instructions.
  //
  for (Module::iterator F = M.begin(); F != M.end(); ++F) {
    findSources (*F);
  }

  //
  // This is an analysis pass, so always return false.
  //
  return false;
}

