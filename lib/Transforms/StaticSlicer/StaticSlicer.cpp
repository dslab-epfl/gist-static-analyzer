//===- StaticSlicer.cpp - Find information flows within a program ------------ --//
// 
//                          Static Slice Computation Pass
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file implements a static slicer pass
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "staticslicer"

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


void printValue(Value* v) {
  string Str;
  raw_string_ostream oss(Str);
  v->print(oss);
  cerr << "add: " << oss.str() << endl;
}

MDNode* StaticSlice::extractAllocaDebugMetadata (AllocaInst* allocaInst) {
  // We need special treatement to get the debug information for alloca's
  const BasicBlock* parent = allocaInst->getParent();
  // Look in the basic block for llvm.dbg.declare that has the debug information for this alloca
  for (BasicBlock::const_iterator it = parent->begin(); it != parent->end(); ++it) {
    if (const DbgDeclareInst* dbgDeclareInst = dyn_cast<DbgDeclareInst>(&*it)) {        
      Value* value = cast<MDNode>(dbgDeclareInst->getOperand(0))->getOperand(0);
      if(!value)
        return NULL;
      assert(isa<AllocaInst>(value) && "The operand must be an alloca");
      return dbgDeclareInst->getMetadata("dbg");
    }
  }
  assert(false && "Could not extract debug information for alloca!");
}

///
static string removeLeadingWhitespace (string str) {
  str.erase (str.begin(), 
             std::find_if(str.begin(), str.end(), 
             std::bind2nd(std::not_equal_to<char>(), ' ')));
  return str;
}


///
void StaticSlice::generateSliceReport(Module& module) {
  ofstream logFile;
  logFile.open ("slice.log");
  
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
  
  logFile << "\n------------------------" << "\n";
  logFile << ": Static Slice         :" << "\n";
  logFile << ":  call cache size: " << callInstrCache.size() << endl;
  logFile << "------------------------" << "\n";

  string DebugLoc("");
  for (std::vector<WorkItem_t>::iterator it = sources.begin(); it != sources.end(); ++it) {
    Value* v = get<0>(*it);
    const Function* f = get<1>(*it);
    MDNode* node = get<2>(*it);
    
    DILocation loc(node);
    unsigned lineNumber = loc.getLineNumber();
    StringRef fileName = loc.getFilename();
    StringRef directory = loc.getDirectory();
    ostringstream ss;
 
    if (lineNumber > 0 ) {
      ss << lineNumber;
      string dbgString = "\t|--> " + directory.str() + "/" + fileName.str() + ": " + ss.str() + "\tF:" + f->getName().str() + "\n";
      logFile << dbgString;
    }
  }
  logFile.close();  
}


void StaticSlice::createDebugMetadataString(string& str, 
                                            const Function* f,
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
bool StaticSlice::isASource (Worklist_t& Worklist, Processed_t& Processed, 
                             const Value * v, const Function * F) { 
  // Call instructions are sources *unless* they are inline assembly.
  if (const CallInst * callInst = dyn_cast<CallInst>(v)) {
    if (isa<InlineAsm>(callInst->getOperand(0)))
      return false;
    else {  
      return true;
    }
  }
  
  // If there is a use of the value v in a store instruction as the destination operand,
  // it means someone is storing to this value, we want to also track where the source
  // of that store is coming from
  Value::const_use_iterator it;
  for (it = v->use_begin(); it != v->use_end() ; ++it) {
    const User* u = *it;
    if (const StoreInst* storeInst = dyn_cast<StoreInst>(u)){      
      if(v == storeInst->getOperand(1)) {   
        if(Processed.find (storeInst->getOperand(0)) == Processed.end()) {
          MDNode* node = NULL;
          if(AllocaInst* allocaInst = dyn_cast<AllocaInst>(storeInst->getOperand(1))) {
            node = extractAllocaDebugMetadata(allocaInst);
          }
          else {
            node = storeInst->getMetadata("dbg");
          }
          Worklist.push_back(WorkItem_t(storeInst->getOperand(0), F, node));
          Processed.insert(storeInst->getOperand(0));
        }
      }
    }
  }
  
  if (const LoadInst* loadInst = dyn_cast<LoadInst>(v)){
    // Don't stop if it is a load, add the operand to the worklist, this is because
    // even we reached a load from memory, we would like to trace where that value 
    // might be coming from
    if(Processed.find (loadInst->getOperand(0)) == Processed.end()) {
      MDNode* node = NULL;
      if(!isa<AllocaInst>(loadInst->getOperand(0))) {
        node = loadInst->getMetadata("dbg");
      }
      Worklist.push_back (WorkItem_t(loadInst->getOperand(0), F, node));
      Processed.insert(loadInst->getOperand(0));
    }
    return true;
  }

  if (isa<Argument>(v)){
    return true;
  }
  
  if (isa<AllocaInst>(v)){
    //dbgMetadata.push_back(make_pair(allocaInst, extractAllocaDebugMetadata(allocaInst)));
    return true;
  }
  if (isa<Constant>(v)){
    if (isa<Function>(v))
      return false;
    else
      return true;
  }
  if (isa<GlobalValue>(v)){
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
void StaticSlice::addSource(WorkItem_t item) {
  sources.push_back(item);
  return;
}

/// This only works for direct function calls
bool StaticSlice::isFilteredCall (CallInst* callInst) {
  Function* calledFunction = callInst->getCalledFunction();
  
  if(calledFunction) {
    string calleeName = calledFunction->getName().str();
    if (filteredFunctions.find(calleeName) != filteredFunctions.end()){
      return true;
    } 
  }
  return false;
}

/// This only works for direct function calls
bool StaticSlice::isSpecialCall(CallInst* callInst) {
  Function* calledFunction = callInst->getCalledFunction();
  
  if(calledFunction) {
    string funcName = calledFunction->getName().str();
    if (specialFunctions.find(funcName) != specialFunctions.end()){
      return true;
    }
  }
  return false;
}


bool StaticSlice::isInPTTrace(string str) {
  if(ptTraceGiven)
    return ptFunctionSet.find(str) != ptFunctionSet.end();
  else {
    return true;
    
  } 
}


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
void StaticSlice::removeIncompatibleTargets (const CallInst* CI,
                                             vector<const Function *> & Targets) {
  // If the call is a direct call, do not look for incompatibility.
  //  if (CI->getCalledFunction()) 
  //    return;

  // Remove any function from the set of targets that has the wrong number of
  // arguments.  Find all the functions to remove first and record them in a
  // container so that we don't invalidate any iterators.
  std::vector<const Function*>::iterator it = Targets.begin();
  
  while(it != Targets.end()) {
    if((*it)->getFunctionType()->getNumParams() != (CI->getNumOperands() - 1))
      it = Targets.erase(it);
    else if(!isInPTTrace((*it)->getName().str())) {
      it = Targets.erase(it);
      cerr << "removing:" << (*it)->getName().str() << endl;
    }
    else {
      ++it;
    }
  }

  return;
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
    Targets.push_back (calledFunction);
  } else {
    // This is an indirect function call.  Get the DSNode for the function
    // pointer and then use that to find the set of call targets.
    CallSite CS(callInst);
    const DSCallGraph & CallGraph = dsaPass->getCallGraph();
    Targets.insert (Targets.end(),
                    CallGraph.callee_begin(CS),
                    CallGraph.callee_end(CS));
  }

  removeIncompatibleTargets (callInst, Targets);
  
  for (unsigned index = 0; index < callInst->getNumOperands(); ++index) {
    Value* operand = callInst->getOperand(index);
    operands.push_back(operand);
  }
  return;
}


///
void StaticSlice::handleSpecialCall(CallInst* callInst, 
                                    vector<const Function*>& Targets,
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
    Function* f = dyn_cast<Function>(v);
    Targets.push_back(f);
    
    v = callInst->getOperand(3);
    assert(isa<Instruction>(v) && 
           "The fourth operand of the pthread_create call must be an instruction ");
    operands.push_back(callInst->getOperand(3));
    operands.push_back(f);
  }
}


///
void StaticSlice::extractArgs (CallInst* callInst,
                               Argument * Arg, 
                               Processed_t& Processed,
                               vector<Value*>& actualArgs) {
  // Skip this call site if it does not call the function to which the
  // specified argument belongs.
  Function* calledFunc = NULL;
  set<const Function *> TargetSet;
  set<const Function *>::iterator it;
  
  vector<const Function*> Targets; 
  vector<Value*> operands; 
  
  calledFunc = Arg->getParent();
  Targets = callTargetsCache[callInst].first;
  operands = callTargetsCache[callInst].second;
  
  
  TargetSet.insert (Targets.begin(), Targets.end());
  it = TargetSet.find (calledFunc);
  if (it == TargetSet.end())
    return;
  
  assert ((calledFunc->getFunctionType()->getNumParams() == operands.size() - 1) 
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
    vector<CallInst*>::iterator it;
    for (it = callInstrCache[&*F].begin(); it != callInstrCache[&*F].end(); ++it) {          
      vector<Value*> actualArgs;
      
      // Find the set of functions called by this call instruction.
      // Some functions such as pthread_create require sepcial handling
      extractArgs(*it, Arg, Processed, actualArgs);

      if(!actualArgs.empty())
        addSource(WorkItem_t(*it, &*F, (*it)->getMetadata("dbg")));
    
      // Finally, find the sources for all the actual arguments needing labels.
      vector<Value*>::iterator i;
      for (i = actualArgs.begin(); i != actualArgs.end(); ++i) {
        MDNode* node = NULL;
        if (Instruction* instr = dyn_cast<Instruction>(*i)) {
          node = instr->getMetadata("dbg");
        }
        ;
        Worklist.push_back (WorkItem_t(*i, F, node));
      } 
    
      // Add the new items to process to the processed list.
      for (unsigned index = 0; index < actualArgs.size(); ++index) {
        if (!(isa<Constant>(actualArgs[index])))
          Processed.insert (actualArgs[index]);
      }    
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
  //dbgMetadata.push_back(make_pair(CI, CI->getMetadata("dbg")));
  // TODO: cache should be handling this, remove 
  
  Targets = callTargetsCache[CI].first;
  operands = callTargetsCache[CI].second;
  
  // Process each potential function call target
  const Type* VoidType = Type::getVoidTy(getGlobalContext());
  while (Targets.size()) {
    // Set of return instructions needing labels discovered
    vector<ReturnInst*> NewReturns;

    // Process one of the functions from the list of potential call targets
    Function * F = const_cast<Function *>((Targets.back()));
    Targets.pop_back ();

    //if(F->getReturnType() == VoidType)
    //  return;
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

    // Finally, add any return instructions that have not already been
    // processed to the worklist
    vector<ReturnInst*>::iterator ri;
    for (ri = NewReturns.begin(); ri != NewReturns.end(); ++ri) {
      ReturnInst* RI = *ri;
      
      if (Processed.find (RI) == Processed.end()) {
        Worklist.push_back (WorkItem_t(RI, F, RI->getMetadata("dbg")));
        //addSource(RI, RI->getParent()->getParent());
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
void StaticSlice::findFlow (Value * Initial, const Function & Fu, MDNode* node) {
  // Already processed values
  Processed_t Processed;

  // Worklist
  Worklist_t Worklist;
  Worklist.push_back (WorkItem_t(Initial, &Fu, node));

  while (Worklist.size()) {
    // Pop an item off of the worklist.
    WorkItem_t item = Worklist.back();
    Value * v = get<0>(item);
    const Function * f = get<1>(item);
    MDNode* node = get<2>(item); 
    Worklist.pop_back();

    // If the value is a source, add it to the set of sources.  Otherwise, add
    // its operands to the worklist if they have not yet been processed. 
    if (isASource (Worklist, Processed, v, f)) {
      addSource (WorkItem_t(v, f, node));

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
      Instruction* instr = dyn_cast<Instruction>(user);
      
      // Record any phi nodes located
      if (PHINode * phiNode = dyn_cast<PHINode>(v))
        PhiNodes.insert (phiNode);

      for (unsigned index = 0; index < user->getNumOperands(); ++index) {
        Value* operand = user->getOperand(index); 
        if (Processed.find (operand) == Processed.end()) {        
          MDNode* n = NULL;
          if (PHINode* PHI = dyn_cast<PHINode>(v)) {
            n = PHI->getIncomingBlock(index)->getTerminator()->getMetadata("dbg");
          } else if (!isa<CallInst>(operand)) {
            assert (isa<Instruction>(user) && 
                    "user is not an instruction, will lose debug information");
            n = instr->getMetadata("dbg");
          }
          Worklist.push_back(WorkItem_t(operand, f, n));
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
    errs() << "------------------------" << "\n";
    errs() << "     Target Operand    :" << "\n";
    errs() << "------------------------" << "\n";
    errs() << *instr << "\n";
    findFlow (instr->getOperand(0), F, instr->getMetadata("dbg")); 
  }
  else
    assert(false && "Target instruction is not a load! Handle this case");

  return;
}


void StaticSlice::cacheCallInstructions(Module& module) {
  for (Module::iterator F = module.begin(); F != module.end(); ++F) {
    // Scan the function looking for call instructions.
    for (Function::iterator BB = F->begin(); BB != F->end(); ++BB) {
      for (BasicBlock::iterator II = BB->begin(); II != BB->end(); ++II) {
        if (CallInst* callInst = dyn_cast<CallInst>(II)) {
          // Ignore inline assembly code.
          if (isa<InlineAsm>(callInst->getOperand(0)))
            continue;
          
          // Filter out some functions which we do not consider as sources
          if (isFilteredCall(callInst))
            continue;
          
          vector<const Function*> targets;
          vector<Value*> operands;
          
          if (isSpecialCall(callInst))
            handleSpecialCall(callInst, targets, operands);
          else
            findCallTargets(callInst, targets, operands);
                     
          callTargetsCache[callInst] = make_pair(targets, operands); 
          
          callInstrCache[&*F].push_back(callInst);
        }
      }
    }
  }
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

  // Cache all call instructions as this will speed up finding the sources of arguments 
  cacheCallInstructions(module);
  
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
