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

#include <iostream>
#include <fstream>
#include <sstream>

#include "llvm/ADT/Statistic.h"
#include "llvm/IntrinsicInst.h"
#include "llvm/LLVMContext.h"
#include "llvm/DebugInfo.h"

#include "StaticSlicer.h"

using namespace llvm;
using namespace std;


static string removeLeadingWhitespace (string str) {
  str.erase (str.begin(), 
             std::find_if(str.begin(), str.end(), 
             std::bind2nd(std::not_equal_to<char>(), ' ')));
  return str;
}


void StaticSlice::generateSliceReport(Module& module) {
  ofstream logFile;
  logFile.open ("slice.log");
  
  logFile << "\n------------------------" << "\n";
  logFile << "      Static Slice     :" << "\n";
  logFile << "------------------------" << "\n";
  
  string targetDebugLoc = "Source";
  createDebugMetadataString(targetDebugLoc, 
                            debugInfoManager->targetFunction,
                            debugInfoManager->targetInstruction->getMetadata("dbg"));

  string valueStr;
  raw_string_ostream valueOss(valueStr);
  debugInfoManager->targetInstruction->print(valueOss);
  // Remove leading whitespaces
  string instrStr = valueOss.str();
  logFile << removeLeadingWhitespace(instrStr) << "\n" << targetDebugLoc << "\n";
  
  for (Module::iterator fi = module.begin(); fi != module.end(); ++fi) {
    for (src_iterator si = src_begin(fi); si != src_end(fi); ++si) {
      Value* v = const_cast<Value*>(*si);
      string valueStr;
      raw_string_ostream valueOss(valueStr);
      v->print(valueOss);

      assert((valueToDbgMetadata[v].size() > 0) && 
             "we should have had the debug information for this value");
      
      string debugLoc("");
      for (size_t i = 0; i < valueToDbgMetadata[v].size(); ++i) {
        createDebugMetadataString(debugLoc, &*fi, valueToDbgMetadata[v][i]);
      }
      
      string instrStr = valueOss.str();
      logFile << removeLeadingWhitespace(instrStr) << "\n" << debugLoc << "\n";
    }
  }

  logFile.close();  
}


void StaticSlice::createDebugMetadataString(string& str, 
                                            Function* f, 
                                            MDNode* node) {
  if (node) {    
    DILocation loc(node);
    unsigned lineNumber = loc.getLineNumber();
    StringRef fileName = loc.getFilename();
    StringRef directory = loc.getDirectory();
    
    ostringstream ss;
    if (lineNumber > 0 ) {
      ss << lineNumber;
      str += "\t|--> " + directory.str() + "/" + fileName.str() + ": " + ss.str() + "\tF:" + f->getName().str() + "\n";
    }
  }
}


/// Function: isASource()
///
/// Description:
///  This function determines whether the specified value is a source of
///  information (something that has a label independent of its input SSA
///  values)
///
/// Inputs:
///  V - The value to analyze.
///
/// Return value:
///  true  - This value is a source.
///  false - This value is not a source.  Its label is the join of the labels
///          of its input operands.
///
static inline bool isASource (const Value * V) {
  // Call instructions are sources *unless* they are inline assembly.
  if (const CallInst * CI = dyn_cast<CallInst>(V)) {
    if (isa<InlineAsm>(CI->getOperand(0)))
      return false;
    else
      return true;
  }

  if (isa<LoadInst>(V)){
    return true;
  }
  if (isa<Argument>(V)){
    return true;
  }
  if (isa<AllocaInst>(V)){
    return true;
  }
  if (isa<Constant>(V)){
    return true;
  }
  if (isa<GlobalValue>(V)){
    return true;
  }

  return false;
}


///
/// Method: addSource()
///
/// Description:
///  The following value is a source.  Do all of the bookkeeping required.
///
/// Inputs:
///  V - A source that needs to be recorded.
///
void StaticSlice::addSource(const Value * V, const Function * F) {

  // Record the source in the set of sources.
  Sources[F].insert (V);

  // If the source is an argument, record it specially.
  // [BK] Not used for the moment
  if (const Argument * Arg = dyn_cast<Argument>(V))
    Args.insert (Arg);
  return;
}


/// This only works for direct function calls
bool StaticSlice::isFilteredCall (CallInst* callInst) {
  assert (callInst->getCalledFunction() && 
          "Filtering only works for direct function calls");
      
  std::string calleeName = callInst->getCalledFunction()->getName().str();
  if (filteredFunctions.find(calleeName) != filteredFunctions.end()){
    return true;
  }
  return false;
}

/// This only works for direct function calls
bool StaticSlice::isSpecialCall(CallInst* callInst) {
  assert (callInst->getCalledFunction() && 
          "Special handling only works for direct function calls");
      
  std::string funcName = callInst->getCalledFunction()->getName().str();
  if (specialFunctions.find(funcName) != specialFunctions.end()){
    return true;
  }
  return false;
}

/// Method: findCallTargets()
///
/// Description:
///  Find the set of functions that can be called by the given call instruction.
///
/// Inputs:
///  CI      - The call instruction to analyze.
///
/// Outputs:
///  Targets - A list of functions that can be called by the call instruction.
///
void StaticSlice::findCallTargets (CallInst * callInst,
                                   vector<const Function *> & Targets,
                                   vector<Value*>& operands) {
  // We do not consider the intrinsics as sources
  if (isa<IntrinsicInst>(callInst))
    return;
      
  // Check to see if the call instruction is a direct call.  If so, then add
  // the target to the set of known targets and return.
  Function * calledFunction = callInst->getCalledFunction();
  if (calledFunction) {
    // Filter out some functions which we do not consider as sources
    if (isFilteredCall(callInst))
      return;

    Targets.push_back (calledFunction);
  } else {
    // This is an indirect function call.  Get the DSNode for the function
    // pointer and then use that to find the set of call targets.
    CallSite CS(callInst);
    const DSCallGraph & CallGraph = dsaPass->getCallGraph();
    Targets.insert (Targets.end(),
                    CallGraph.callee_begin(CS),
                    CallGraph.callee_end(CS));
  
    //
    // Remove targets that do not match the call instruction's argument list.
    removeIncompatibleTargets  (callInst, Targets);
  }
  
  // we have a +1, because the first operand is the function call itself
  for (User::op_iterator it = callInst->op_begin() + 1, 
       end = callInst->op_end(); it != end; ++it) {
    operands.push_back((Value*)&*it);
  }
  return;
}


///
void StaticSlice::handleSpecialCall(CallInst* callInst, 
                                    std::vector<const Function*>& Targets,
                                    vector<Value*>& operands) {
  assert (callInst->getCalledFunction() && 
          "Special handling only works for direct function calls");
  
  string calleeName = callInst->getCalledFunction()->getName().str();
  if (calleeName == "pthread_create") {
    // pthread_create's third operand is the start routine, and the fourth 
    // operand is the argument (instruction) through which a value is passed. 

    Value* v = callInst->getOperand(2);
    assert(isa<Function>(v) && 
           "The third operand of the pthread_create call must be a function");
    const Function* f = dyn_cast<Function>(v);
    Targets.push_back(f);
    
    funcToCallInst[f] = callInst;
    
    if(callInst->getCalledFunction() == f) {
      cerr <<  "!@@@@@@2match" << endl;
    }
      
    
    v = callInst->getOperand(3);
    assert(isa<Instruction>(v) && 
           "The fourth operand of the pthread_create call must be a ");
    operands.push_back(callInst->getOperand(3));
    
  }
}


///
void StaticSlice::extractArgs (Argument * Arg, 
                               vector<const Function*>& Targets,
                               Processed_t& Processed,
                               vector<Value*>& operands,
                               vector<Value*>& actualArgs) {
  // Skip this call site if it does not call the function to which the
  // specified argument belongs.
  Function* calledFunc = Arg->getParent();
  std::set<const Function *> TargetSet;
  std::set<const Function *>::iterator it;
  TargetSet.insert (Targets.begin(), Targets.end());
  it = TargetSet.find (calledFunc);
  if (it == TargetSet.end())
    return;
  else {
    // We need to explicity add special callers
    CallInst* specialCaller = funcToCallInst[*it];
    addSource(specialCaller, specialCaller->getParent()->getParent());
    valueToDbgMetadata[specialCaller].push_back(specialCaller->getMetadata("dbg"));
  }
  
  // Assert that the call and the called function have the same # of arguments.
  assert ((calledFunc->getFunctionType()->getNumParams()) == operands.size()
           && "Number of arguments doesn't match function signature!\n");

  // Walk the argument list of the call instruction and look for actual
  // arguments needing labels.  Add them to our local worklist.
  Function::arg_iterator FormalArg = calledFunc->arg_begin();
  for (unsigned index = 0;
       index < operands.size();
       ++index, ++FormalArg) {

    if (((Argument *)(FormalArg)) == Arg) {
      if (Processed.find (operands[index]) == Processed.end()) {
        actualArgs.push_back (operands[index]);
      }
    }
  }
}


/// Method: findArgSources()
///
/// Description:
///  Find the label sources for every actual parameter in the worklist of formal
///  parameters (i.e., arguments) that need labels.
///
/// Inputs:
///  Arg       - The argument for which the actual parameters must be labeled.
///  Processed - The set of LLVM values which have already been discovered as
///              part of an information flow requiring labels.
///
/// Outputs:
///  Worklist  - This set is modified to contain the actual parameters that need
///              to be processed when back-tracking an information flow.
///  Processed - This set is updated to hold any new values that were added to
///              the worklist.  This will prevent them from being added multiple
///              times.
///
void StaticSlice::findArgSources (Argument* Arg,
                                  Worklist_t& Worklist,
                                  Processed_t& Processed) {
  // Iterate over all functions in the program looking for call instructions.
  // When we find a call instruction, we will check to see if the function
  // has arguments that need labels.  If it does, we'll find the labels of
  // all the actual parameters.
  Module * M = Arg->getParent()->getParent();
  for (Module::iterator F = M->begin(); F != M->end(); ++F) {
    // Set of actual arguments needing labels
    vector<Value*> actualArgs;

    // Scan the function looking for call instructions.
    for (Function::iterator BB = F->begin(); BB != F->end(); ++BB) {
      for (BasicBlock::iterator II = BB->begin(); II != BB->end(); ++II) {
        if (CallInst* callInst = dyn_cast<CallInst>(II)) {
          // Ignore inline assembly code.
          if (isa<InlineAsm>(callInst->getOperand(0)))
            continue;
          
          // Find the set of functions called by this call instruction.
          vector <const Function*> Targets;
          vector<Value*> operands;
          // Some functions such as pthread_create require sepcial handling
          if (isSpecialCall(callInst)) {
            handleSpecialCall(callInst, Targets, operands);
          } else {
            findCallTargets (callInst, Targets, operands);
          }
          if (!operands.empty()) {
            assert(Targets.size() == 1 && "2 calls to the same function in same BB, examine");
            extractArgs(Arg, Targets, Processed, operands, actualArgs);
          }
        }
      }
    }

    // Finally, find the sources for all the actual arguments needing labels.
    vector<Value*>::iterator i;
    for (i = actualArgs.begin(); i != actualArgs.end(); ++i) {
      Value* V = *i;
      Worklist.push_back (make_pair (V, F));
    }

    // Add the new items to process to the processed list.
    for (unsigned index = 0; index < actualArgs.size(); ++index) {
      if (!(isa<Constant>(actualArgs[index])))
        Processed.insert (actualArgs[index]);
    }
  }

  return;
}


/// Method: findCallSources()
///
/// Description:
///  For the given call instruction, find the functions that it calls and
///  add to the worklist those values that contribute to the called functions'
///  return values.
///
/// Inputs:
///  CI        - The call instruction whose return value requires a label.
///  Processed - The set of LLVM values that have already been identified as
///              part of an information flow.
///
/// Outputs:
///  Worklist -  The return instructions that determine the value of the call
///              instruction are added to the worklist.
///  Processed - Items added to the worklist are also added to the Processed
///              container to ensure that they are only identified once for
///              information flow purposes.
///
void StaticSlice::findCallSources (CallInst* CI,
                                   Worklist_t& Worklist,
                                   Processed_t& Processed) {
  // Find the function called by this call instruction
  vector<const Function *> Targets;
  vector<Value*> operands;
  valueToDbgMetadata[CI].push_back(CI->getMetadata("dbg"));
  findCallTargets (CI, Targets, operands);

  // Process each potential function call target
  const Type* VoidType = Type::getVoidTy(getGlobalContext());
  while (Targets.size()) {
    // Set of return instructions needing labels discovered
    vector<ReturnInst*> NewReturns;

    // Process one of the functions from the list of potential call targets
    Function * F = const_cast<Function *>((Targets.back()));
    Targets.pop_back ();

    // Ensure that the function's return value is not void
    assert ((F->getReturnType() != VoidType) && "Want void function label!\n");

    // Add any return values in the called function to the list of return
    // instructions to process.  Note that we may add them multiple times, but
    // this is okay since Returns is a set that does not allow duplicate
    // entries.
    for (Function::iterator BB = F->begin(); BB != F->end(); ++BB)
      if (ReturnInst* RI = dyn_cast<ReturnInst>(BB->getTerminator())) {
        NewReturns.push_back (RI);
      }

    // Record the returns that require labels.
    // [BK] this may not be needed for our purposes
    Returns.insert (NewReturns.begin(), NewReturns.end());

    // Finally, add any return instructions that have not already been
    // processed to the worklist
    std::vector<ReturnInst*>::iterator ri;
    for (ri = NewReturns.begin(); ri != NewReturns.end(); ++ri) {
      ReturnInst* RI = *ri;

      valueToDbgMetadata[RI].push_back(RI->getMetadata("dbg"));

      if (Processed.find (RI) == Processed.end()) {
        Worklist.push_back (std::make_pair(RI, F));
        Processed.insert (RI);
      }
    }
  }

  return;
}

///
/// Method: findFlow()
///
/// Description:
///  For the given value, find all of the values upon which it depends.  The
///  labels of these values will combine together to form the label of the given
///  value.
///
void StaticSlice::findFlow (Value * Initial, const Function & Fu) {
  // Already processed values
  Processed_t Processed;

  // Worklist
  Worklist_t Worklist;
  Worklist.push_back (std::make_pair(Initial, &Fu));

  while (Worklist.size()) {
    //
    // Pop an item off of the worklist.
    //
    Value * v = Worklist.back().first;
    const Function * f = Worklist.back().second;
    Worklist.pop_back();

    // If the value is a source, add it to the set of sources.  Otherwise, add
    // its operands to the worklist if they have not yet been processed. 
    if (isASource (v)) {
      addSource (v, f);

      // Some sources imply that information flow must be traced inside another
      // function.  Needing the label of a call instruction's return value
      // means that we need to know the sources for the called function's
      // return value.  Requiring the label of a function argument means that
      // we'll need the labels of any value passed into the function.
      if (CallInst * CI = dyn_cast<CallInst>(v)) {
        findCallSources (CI, Worklist, Processed);
      } else if (Argument * Arg = dyn_cast<Argument>(v)) {
        findArgSources (Arg, Worklist, Processed);
      }
    } else if (User * user = dyn_cast<User>(v)) {
      assert (isa<Instruction>(user) && 
              "user is not an instruction, will lose debug information");
      Instruction* instr = dyn_cast<Instruction>(user);
      
      // Record any phi nodes located
      if (PHINode * phiNode = dyn_cast<PHINode>(v))
        PhiNodes.insert (phiNode);

      for (unsigned index = 0; index < user->getNumOperands(); ++index) {
        Value* operand = user->getOperand(index); 
        if (Processed.find (operand) == Processed.end()) {
          Worklist.push_back (std::make_pair(operand, f));

          if (PHINode* PHI = dyn_cast<PHINode>(v))
            valueToDbgMetadata[operand].push_back(PHI->getIncomingBlock(index)->getTerminator()->getMetadata("dbg"));
          else if (!isa<CallInst>(operand)) {
            valueToDbgMetadata[operand].push_back(instr->getMetadata("dbg"));
          }
          if (!(isa<Constant>(operand)))
            Processed.insert (operand);
        }
      }
    }
  }

  return;
}

///
/// Method: findSources()
///
/// Description:
///  For every store and external function call in the specified function, find
///  all the instructions that generate a label (i.e., is a source of
///  information)  for the value(s) being stored into memory.
///
/// Inputs:
///  F - The function to analyze.
///
void StaticSlice::findSources (Function & F) {
  // Retrieve the target instruction from the debug info manager
  // and process those the information flow of its inputs.
  assert (debugInfoManager->targetInstruction && "Target instruction cannot be NULL");
  if (LoadInst * instr = dyn_cast<LoadInst>(debugInfoManager->targetInstruction)) {
    std::string Str;
    raw_string_ostream oss(Str);
    instr->getOperand(0)->print(oss);
    errs() << "------------------------" << "\n";
    errs() << "     Target Operand    :" << "\n";
    errs() << "------------------------" << "\n";
    errs() << oss.str() << "\n";
    valueToDbgMetadata[instr->getOperand(0)].push_back(instr->getMetadata("dbg"));
    findFlow (instr->getOperand(0), F); 
  }
  else
    assert(false && "Target instruction is not a load! Handle this case");

  return;
}

/// Method: runOnModule()
///
/// Description:
///  Entry point for this LLVM pass.  Find statements that require the
///  label of a piece of information and then find the sources of that
///  information.
///
/// Inputs:
///  M - The module to analyze.
///
/// Return value:
///  false - The module was not modified.
///
bool StaticSlice::runOnModule (Module& module) {
  // Get prerequisite passes.
  dsaPass = &getAnalysis<EQTDDataStructures>();
  debugInfoManager = &getAnalysis<DebugInfoManager>();

  // Finding the sources of all labels of the target instruction we get from the coredump.
  assert(debugInfoManager->targetFunction && "Target function cannot be NULL");
  findSources (*(debugInfoManager->targetFunction));

  generateSliceReport(module);

  // This is an analysis pass, so always return false.
  return false;
}

/// ID Variable to identify the pass
char StaticSlice::ID = 0;

/// Pass registration
static RegisterPass<StaticSlice> X ("slice", "Compute a static slice given an instruction");
