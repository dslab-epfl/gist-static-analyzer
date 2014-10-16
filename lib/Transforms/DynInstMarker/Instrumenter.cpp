//===- Instrumenter.h -------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//

// A pass to instrument potentially racing accesses and pthread functions
//
//===----------------------------------------------------------------------===//

#include <iostream>
#include <sstream>
#include <fstream>
#include <map>
#include <string>

#include "llvm/Type.h"
#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/LLVMContext.h"
#include "llvm/Module.h"
#include "llvm/Instruction.h"
#include "llvm/IntrinsicInst.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/Dominators.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/DebugInfo.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Support/CommandLine.h"

#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/ValueMapper.h"

#include "llvm-c/BitWriter.h"
#include "llvm/Bitcode/ReaderWriter.h"

// TODO: remove when you remove the exit
#include <stdlib.h>

#include "Instrumenter.h"

using namespace llvm;
using namespace std;

namespace {
  cl::opt<bool>
  AnalyzeHB("analyze-hb-array",
            cl::desc("Embed the array of HB relationships that the Hive came up with"),
            cl::init(false));

  cl::opt<bool>
  EmbedHB("embed-hb-array",
          cl::desc("Embed the array of HB relationships that the Hive came up with"),
          cl::init(false));

  cl::opt<bool>
  DebugHB("debug-hb",
          cl::desc("Debug HB inejction results"),
          cl::init(false));
}


void Instrumenter::getAnalysisUsage(AnalysisUsage& au) const {
  au.addRequired<DominatorTree>();
  au.addRequired<PostDominatorTree>();
}

bool Instrumenter::runOnModule(Module& m) {
  /// AnalyzeHB is to be called with a _cord.bc file whereas
  /// EmbedHB is to be called with a _cord_coerced.bc file!
  if(AnalyzeHB || EmbedHB){
    ///  dominator and post dominators from the original module and 
    for(Module::iterator fi = m.begin(), fe = m.end(); fi != fe; ++fi){
      Function& F = *fi;
      
      if (F.size() == 0)
        continue;
      
      dominatorCache[F.getName().str()] = &getAnalysis<DominatorTree>(F);
      pdominatorCache[F.getName().str()] = &getAnalysis<PostDominatorTree>(F);
    }
    /// Get HB edges    
    /// read the to-be created HB indices
    string hbFileName = m.getModuleIdentifier();
    string hbIndicesFileName(hbFileName);
    if(EmbedHB)
      replace(hbIndicesFileName, "_cord_coerced.bc", "-hb-indices.txt");
    else
      replace(hbIndicesFileName, "_cord.bc", "-hb-indices.txt");
    ifstream hbIndicesFile(hbIndicesFileName.c_str(), ifstream::in);  
         
    string hbLine;    
    while(std::getline(hbIndicesFile, hbLine)){
      stringstream lineStream(hbLine);
      string cell;
      vector<int> hbEdge;
      while(std::getline(lineStream,cell,',')){      
        hbEdge.push_back(atoi(cell.c_str()));      
      }
      cerr << endl;
      assert ((hbEdge[0] != hbEdge[1]) && "We don't know how to handle edges with same index");
      hbIndices.push_back(hbEdge);
    }
        
    assert(!hbIndices.empty() && "Probably you did not provide the -hb-indices.txt file");
    hbIndicesFile.close();  
  
    /// Form a map of idx to racing instructions and instructions to idx
    for(Module::const_iterator fi = m.begin(), fe = m.end(); fi != fe; ++fi){
      for(Function::const_iterator bi = fi->begin(), be = fi->end(); bi != be; ++bi){
        for (BasicBlock::const_iterator ii = bi->begin(), ie = bi->end(); ii != ie; ++ii){
          bool advance = true;
          vector<int> toAdd;
          do{
            if(ii->getOpcode() == Instruction::Call) {
              const CallInst* callInst = dyn_cast<CallInst>(&(*ii));            
              assert(callInst && "Call inst pointer is NULL!!!, this shouldn't have happened");
              Function* calledFunc = callInst->getCalledFunction();
              if (calledFunc){
                string calledFuncName = calledFunc->getName().str();                            
                if (calledFuncName.find("beforeRace") != std::string::npos){                
                  assert(dyn_cast<ConstantInt>(callInst->getOperand(0)) && 
                         "We whould have a constant oeprand here");
                  ConstantInt* CI = dyn_cast<ConstantInt>(callInst->getOperand(0));
                  toAdd.push_back(CI->getSExtValue());
                  ++ii;
                } else
                  advance = false;
              } else
                advance = false;
            } else
              advance = false;
          } while(advance);
          
          vector<int>::iterator arrayIt;
          BasicBlock::const_iterator racingInst(&(*ii));
          for(arrayIt = toAdd.begin(); 
              arrayIt != toAdd.end(); ++arrayIt){
            /// Update the maps
            instToIdx[&(*ii)] = *arrayIt;
            idxToInst[*arrayIt] = &(*ii);          
          }
        }
      }
    }
    
    /// Form the instrument cache that RELAY gives to us
    string fileName = m.getModuleIdentifier();
    string indicesFileName(fileName);
    if(EmbedHB)
      replace(indicesFileName, "_cord_coerced.bc", "-hb-indices.txt");
    else
      replace(indicesFileName, "_cord.bc", "-final-indices.txt");
    ifstream file (indicesFileName.c_str());
    string line;  
    int counter = 0;
    while(std::getline(file, line)){
      stringstream lineStream(line);
      string cells[2];
      int indices[2];
      int index = 0;
      while(std::getline(lineStream, cells[index],',')){
        indices[index] = atoi(cells[index].c_str());
        ++index;
      }
      
      counter++;
      if(EmbedHB){
        instrumentCache.push_back(make_pair(idxToInst[indices[0]], idxToInst[indices[1]]));
        if(DebugHB)
          cerr << "pushed for indices: " << indices[0] << " : " << indices[1] << endl;        
      }
      else 
        if (counter%2 == 0){
          instrumentCache.push_back(make_pair(idxToInst[indices[0]], idxToInst[indices[1]]));
          if(DebugHB)
            cerr << "pushed for indices: " << indices[0] << " : " << indices[1] << endl;
        }
    }
    file.close();
    if(AnalyzeHB)
      checkHBElimination(&m);
  }
  cloneNewModule = true;
  Module* newModule = specialCloneModule(&m);

  if(EmbedHB)
    createHBArrayInitializer(newModule);
  
  string errorInfo;
  if (EmbedHB){    
    raw_fd_ostream os("hbinstrumented.bc", errorInfo, raw_fd_ostream::F_Binary);
    WriteBitcodeToFile(newModule, os);    
  } else{
    raw_fd_ostream os("instrumented.bc", errorInfo, raw_fd_ostream::F_Binary);
    WriteBitcodeToFile(newModule, os);
  }
  return false;
}

Module* Instrumenter::specialCloneModule(Module* M){
  ValueToValueMapTy VMap;
  // First off, we need to create the new module.
  Module *New = new Module(M->getModuleIdentifier(), M->getContext());
  New->setDataLayout(M->getDataLayout());
  New->setTargetTriple(M->getTargetTriple());
  New->setModuleInlineAsm(M->getModuleInlineAsm());
  
  /// Initializr CoRD hooks here
  if(cloneNewModule){
    initializeHooks(New);
  }
   
  // Copy all of the dependent libraries over.
  for (Module::lib_iterator I = M->lib_begin(), E = M->lib_end(); I != E; ++I)
    New->addLibrary(*I);

  // Loop over all of the global variables, making corresponding globals in the
  // new module.  Here we add them to the VMap and to the new Module.  We
  // don't worry about attributes or initializers, they will come later.
  //
  for (Module::const_global_iterator I = M->global_begin(), E = M->global_end();
       I != E; ++I) {
    GlobalVariable *GV = new GlobalVariable(*New, 
                                            I->getType()->getElementType(),
                                            I->isConstant(), I->getLinkage(),
                                            (Constant*) 0, I->getName(),
                                            (GlobalVariable*) 0,
                                            I->isThreadLocal(),
                                            I->getType()->getAddressSpace());
    GV->copyAttributesFrom(I);
    VMap[I] = GV;
  }

  // Loop over the functions in the module, making external functions as before
  for (Module::const_iterator I = M->begin(), E = M->end(); I != E; ++I) {
    Function *NF =
      Function::Create(cast<FunctionType>(I->getType()->getElementType()),
                       I->getLinkage(), I->getName(), New);
    NF->copyAttributesFrom(I);
    VMap[I] = NF;
  }

  // Loop over the aliases in the module
  for (Module::const_alias_iterator I = M->alias_begin(), E = M->alias_end();
       I != E; ++I) {
    GlobalAlias *GA = new GlobalAlias(I->getType(), I->getLinkage(),
                                      I->getName(), NULL, New);
    GA->copyAttributesFrom(I);
    VMap[I] = GA;
  }

  // Now that all of the things that global variable initializer can refer to
  // have been created, loop through and copy the global variable referrers
  // over...  We also set the attributes on the global now.
  //
  for (Module::const_global_iterator I = M->global_begin(), E = M->global_end();
       I != E; ++I) {
    GlobalVariable *GV = cast<GlobalVariable>(VMap[I]);
    if (I->hasInitializer())
      GV->setInitializer(MapValue(I->getInitializer(), VMap));
  }

  // Similarly, copy over function bodies now...
  //
  for (Module::const_iterator I = M->begin(), E = M->end(); I != E; ++I) {
    Function *F = cast<Function>(VMap[I]);
    if (!I->isDeclaration()) {
      Function::arg_iterator DestI = F->arg_begin();
      for (Function::const_arg_iterator J = I->arg_begin(); J != I->arg_end();
           ++J) {
        DestI->setName(J->getName());
        VMap[J] = DestI++;
      }

      SmallVector<ReturnInst*, 8> Returns;  // Ignore returns cloned.
      specialCloneFunctionInto(F, I, VMap, /*ModuleLevelChanges=*/true, Returns);
      }
  }

  // And aliases
  for (Module::const_alias_iterator I = M->alias_begin(), E = M->alias_end();
       I != E; ++I) {
    GlobalAlias *GA = cast<GlobalAlias>(VMap[I]);
    if (const Constant *C = I->getAliasee())
      GA->setAliasee(MapValue(C, VMap));
  }

  // And named metadata....
  for (Module::const_named_metadata_iterator I = M->named_metadata_begin(),
         E = M->named_metadata_end(); I != E; ++I) {
    const NamedMDNode &NMD = *I;
    NamedMDNode *NewNMD = New->getOrInsertNamedMetadata(NMD.getName());
    for (unsigned i = 0, e = NMD.getNumOperands(); i != e; ++i)
      NewNMD->addOperand(MapValue(NMD.getOperand(i), VMap));
  }

  insertInitFinalHooks(New);
  return New;
}

void Instrumenter::specialCloneFunctionInto(Function *NewFunc, 
                                            const Function *OldFunc,
                                            ValueToValueMapTy &VMap,
                                            bool ModuleLevelChanges,
                                            SmallVectorImpl<ReturnInst*> &Returns,
                                            const char *NameSuffix, 
                                            ClonedCodeInfo *CodeInfo) {
  assert(NameSuffix && "NameSuffix cannot be null!");

#ifndef NDEBUG
  for (Function::const_arg_iterator I = OldFunc->arg_begin(), 
       E = OldFunc->arg_end(); I != E; ++I)
    assert(VMap.count(I) && "No mapping from source argument specified!");
#endif

  // Clone any attributes.
  if (NewFunc->arg_size() == OldFunc->arg_size())
    NewFunc->copyAttributesFrom(OldFunc);
  else {
    //Some arguments were deleted with the VMap. Copy arguments one by one
    for (Function::const_arg_iterator I = OldFunc->arg_begin(), 
           E = OldFunc->arg_end(); I != E; ++I)
      if (Argument* Anew = dyn_cast<Argument>(VMap[I]))
        Anew->addAttr( OldFunc->getAttributes()
                       .getParamAttributes(I->getArgNo() + 1));
    NewFunc->setAttributes(NewFunc->getAttributes()
                           .addAttr(0, OldFunc->getAttributes()
                                     .getRetAttributes()));
    NewFunc->setAttributes(NewFunc->getAttributes()
                           .addAttr(~0, OldFunc->getAttributes()
                                     .getFnAttributes()));

  }

  // Loop over all of the basic blocks in the function, cloning them as
  // appropriate.  Note that we save BE this way in order to handle cloning of
  // recursive functions into themselves.
  //
  for (Function::const_iterator BI = OldFunc->begin(), BE = OldFunc->end();
       BI != BE; ++BI) {
    const BasicBlock &BB = *BI;

    // Create a new basic block and copy instructions into it!
    BasicBlock *CBB = specialCloneBasicBlock(&BB, VMap, NameSuffix, NewFunc, CodeInfo);
    VMap[&BB] = CBB;                       // Add basic block mapping.

    if (ReturnInst *RI = dyn_cast<ReturnInst>(CBB->getTerminator()))
      Returns.push_back(RI);
  }

  // Loop over all of the instructions in the function, fixing up operand
  // references as we go.  This uses VMap to do all the hard work.
  for (Function::iterator BB = cast<BasicBlock>(VMap[OldFunc->begin()]),
         BE = NewFunc->end(); BB != BE; ++BB)
    // Loop over all instructions, fixing each one as we find it...
    for (BasicBlock::iterator II = BB->begin(); II != BB->end(); ++II){
      RemapInstruction(II, VMap,
                       ModuleLevelChanges ? RF_None : RF_NoModuleLevelChanges);
    }
}

BasicBlock* Instrumenter::specialCloneBasicBlock(const BasicBlock *BB,
                                                 ValueToValueMapTy &VMap,
                                                 const Twine &NameSuffix, Function *F,
                                                 ClonedCodeInfo *CodeInfo) {
  BasicBlock *NewBB = BasicBlock::Create(BB->getContext(), "", F);
  if (BB->hasName()) NewBB->setName(BB->getName()+NameSuffix);

  bool hasCalls = false, hasDynamicAllocas = false, hasStaticAllocas = false;

  // Loop over all instructions, and copy them over.
  for (BasicBlock::const_iterator II = BB->begin(), IE = BB->end();
       II != IE; ++II) {
    Instruction *NewInst = II->clone();
      
    if (II->hasName())
      NewInst->setName(II->getName()+NameSuffix);
    NewBB->getInstList().push_back(NewInst);
    VMap[II] = NewInst;                // Add instruction map to value.
    
    hasCalls |= (isa<CallInst>(II) && !isa<DbgInfoIntrinsic>(II));
    if (const AllocaInst *AI = dyn_cast<AllocaInst>(II)) {
      if (isa<ConstantInt>(AI->getArraySize()))
        hasStaticAllocas = true;
      else
        hasDynamicAllocas = true;
    }

    if(cloneNewModule && F->getName().str() == "main")
      cloneNewModule = false;    
  } 

  if (CodeInfo) {
    CodeInfo->ContainsCalls          |= hasCalls;
    CodeInfo->ContainsDynamicAllocas |= hasDynamicAllocas;
    CodeInfo->ContainsDynamicAllocas |= hasStaticAllocas && 
      BB != &BB->getParent()->getEntryBlock();
  }

  return NewBB;
}

void Instrumenter::initializeHooks(Module* module){
  /// Initialization hook
  llvm::FunctionType *tyInit = 
    FunctionType::get(Type::getVoidTy(getGlobalContext()), false);

  initRace = Function::Create(tyInit, 
                              GlobalVariable::ExternalLinkage, 
                              "_Z8initRacev",
                              module);  

  /// Finalization hook
  llvm::FunctionType *tyFin = 
    FunctionType::get(Type::getVoidTy(getGlobalContext()), false);
  
  finalizeRace = Function::Create(tyFin, 
                                  GlobalVariable::ExternalLinkage, 
                                  "_Z12finalizeRacev",
                                  module);  
}

void Instrumenter::insertInitFinalHooks(Module* m){
  // add the initializer function
  assert(initRace && "initRace is NULL!");
  Function* main = m->getFunction("main");
  assert(main && "main is NULL!");
  BasicBlock& firstBB = main->front();
  Instruction& firstI = firstBB.front();
  CallInst::Create(initRace, "", &firstI);

  // add the finalizer function
  assert(finalizeRace && "finalizeRace is NULL!");
  BasicBlock& lastBB = main->back();
  Instruction& lastI = lastBB.back();
  CallInst::Create(finalizeRace, "", &lastI);
}

void Instrumenter::checkHBElimination(Module* m){
  ofstream file;
  string fileName = m->getModuleIdentifier();
  string indicesFileName(fileName);
  replace(indicesFileName, ".bc", "-elimination-results.txt");
  file.open(indicesFileName.c_str());

  cerr << "POST ANALYSIS OF RACES" << endl << endl;

  /// TODO: this scheme is less resilient to changing binaries, but that is quite a future work
  
  /// Now for every HB edge, we should see how many other races it ends up eliminating
  /// TODO: group the HB edges that are introduced into per
  /// basic block chunks and run the analysis based on this
  std::vector<std::vector<int> >::iterator hbIt;
  int index = 0;
  for(hbIt = hbIndices.begin(); hbIt != hbIndices.end(); ++hbIt){
    InstrumentCache::const_iterator instIt;    
    // cerr << "HB index: " << index << endl;
    // cerr << "(*hbIt)[0]: " << (*hbIt)[0] << endl;
    // cerr << "(*hbIt)[1]: " << (*hbIt)[1] << endl;
    index++;
    const Instruction* firstHBVertex = idxToInst[(*hbIt)[0]];
    const Instruction* secondHBVertex = idxToInst[(*hbIt)[1]];
    
    assert(firstHBVertex && secondHBVertex && 
           "idxToInst map should not be empty!");
    
    Function* firstVertexFunc = const_cast<Function*>(firstHBVertex->getParent()->getParent());
    Function* secondVertexFunc = const_cast<Function*>(secondHBVertex->getParent()->getParent());
    eliminatorEdges.push_back(make_pair(firstHBVertex, secondHBVertex));

    assert(firstVertexFunc && secondVertexFunc && 
           "The vertex instructions should have a parent function!");
    assert(dominatorCache.find(firstVertexFunc->getName().str()) != dominatorCache.end() &&
           "Could not find the first vertex function in the dominator cache!");
    assert(pdominatorCache.find(secondVertexFunc->getName().str()) != pdominatorCache.end() &&
           "Could not find the second vertex function in the pdominator cache!");

    DominatorTree* dtFirstVertex = dominatorCache[firstVertexFunc->getName().str()];
    PostDominatorTree* dtSecondVertex  = pdominatorCache[secondVertexFunc->getName().str()];
    // DominatorTree &dtFirstVertex = getAnalysis<DominatorTree>(*firstVertexFunc);
    // PostDominatorTree &dtSecondVertex = getAnalysis<PostDominatorTree>(*secondVertexFunc);
    /// look at all the racing instructions

    for(instIt = instrumentCache.begin();
        instIt != instrumentCache.end(); ++instIt){
      if(DebugHB){
        llvm::errs() << "firstHBVertex :" << *firstHBVertex;
        cerr << endl;
        llvm::errs() << "secondHBVertex :" << *secondHBVertex;
        cerr << endl;
        
        llvm::errs() << "instIt->first :" << *instIt->first;
        cerr << endl;
        llvm::errs() << "instIt->second :" << *instIt->second;
        cerr << endl;
      }
      /// Skip if the HB edge happens to be the potential race as well            
      if((instIt->first == firstHBVertex && instIt->second == secondHBVertex) ||
         (instIt->first == secondHBVertex && instIt->second == firstHBVertex) )
        continue;

      Function* firstAccessFunc = const_cast<Function*>(instIt->first->getParent()->getParent());
      Function* secondAccessFunc = const_cast<Function*>(instIt->second->getParent()->getParent());
      
      /// Skip if either hbIt->first or hbIt->second can reach itself 
      /// TODO: reconsider this if this ends up being too restrictive

      /// Giant hack
      BasicBlock::const_iterator firstHBVertexSucc(firstHBVertex);
      ++firstHBVertexSucc;
      if(isReachable(&*firstHBVertexSucc, firstHBVertex)){
        if(DebugHB)
          cerr << "HB " << (*hbIt)[0] << " might be in a loop" << endl;
        continue;
      }

      /// Giant hack
      BasicBlock::const_iterator secondHBVertexSucc(secondHBVertex);
      ++secondHBVertexSucc;
      if(isReachable(&*secondHBVertexSucc, secondHBVertex)){
        if(DebugHB)
          cerr << "HB " << (*hbIt)[0] << " might be in a loop" << endl;
        continue;
      }
      
      /// Otherwise we check the following:  
      /// instIt->first is a dominator of firstHBVertex and instIt->first is not reachable from firstHBVertex
      /// instIt->second is a post-dominator of secondHBVertex and secondHBVertex is not reachable from instIt->second
      /// If this holds, then HB edge eliminates the potential race (instIt->first, instIt->second)     
      /// TODO: plugin a interproceural dominator analysis in here.

      /// For dominator analysis to be meaningful, check that instIt->first, firstHBVertex annd
      /// instIt->second, secondHBVertex are in the same function
      if(firstVertexFunc == firstAccessFunc && secondVertexFunc == secondAccessFunc){
        if(dtFirstVertex->dominates(instIt->first, firstHBVertex)){
          if(!isReachable(firstHBVertex, instIt->first)){
            if(dtSecondVertex->dominates(secondHBVertex->getParent(), 
                                        instIt->second->getParent())){
              if(!isReachable(instIt->second, secondHBVertex)){                

                file << (*hbIt)[0] << ",";
                file << (*hbIt)[1] << " --> ";
                file << instToIdx[instIt->first] << ",";
                file << instToIdx[instIt->second] << endl;
                
                cerr << "HB Edge: " << endl;
                llvm::errs() << "   " << *firstHBVertex << ", idx:" << (*hbIt)[0];
                cerr << endl << "    --> "  << endl;
                llvm::errs() << "   " << *secondHBVertex << ", idx:" << (*hbIt)[1];
                cerr << endl;
                cerr << "Eliminates the potential race between:" << endl;              
                llvm::errs() << "   " << *instIt->first << ", idx:" << instToIdx[instIt->first];
                cerr << endl << "     | "  << endl;
                llvm::errs() << "   " << *instIt->second << ", idx:" << instToIdx[instIt->second];
                cerr << endl << endl;      

              } else {
                if(DebugHB)
                  cerr << "One can reach from the second access to the second HB edge!" << endl;
              }
            } else{
              if(DebugHB)
                cerr << "Second HB edge does not post dominate the second access" << endl;
            }
          } else{
            if(DebugHB)
              cerr << "One can reach from the first HB edge to the first access" << endl;
          }
        } else{
          if(DebugHB)
            cerr << "First access does not dominate the first HB edge" << endl;
        }
      }
      
      if(firstVertexFunc == secondAccessFunc && secondVertexFunc == firstAccessFunc){
        if(dtFirstVertex->dominates(instIt->second, firstHBVertex)){
          if(!isReachable(firstHBVertex, instIt->second)){
            if(dtSecondVertex->dominates(secondHBVertex->getParent(),
                                        instIt->first->getParent())){
              if(!isReachable(instIt->first, secondHBVertex)){

                file << (*hbIt)[0] << ",";
                file << (*hbIt)[1] << " --> ";
                file << instToIdx[instIt->second] << ",";
                file << instToIdx[instIt->first] << endl;

                cerr << "#HB Edge: " << endl;
                llvm::errs() << "   " << *firstHBVertex;
                cerr << endl << "    --> "  << endl;
                llvm::errs() << "   " << *secondHBVertex;
                cerr << endl;
                cerr << "#Eliminates the potential race between:" << endl;              
                llvm::errs() << "   " << *instIt->second;
                cerr << endl << "     | "  << endl;
                llvm::errs() << "   " << *instIt->first;
                cerr << endl << endl;                            

              } else{
                if(DebugHB)
                  cerr << "One can reach from the first access to the second HB edge!" << endl;
              }
            } else{
              if(DebugHB)
                cerr << "Second HB edge does not post dominate the first access!" << endl;
            }
          } else {
            if(DebugHB)
              cerr << "One can reach from first HB edge to the second access" << endl;
          }
        } else{
          if(DebugHB)
            cerr << "Second access does not dominate first HB edge" << endl;
        }
      }
    }    
  }  

  file.close();
}

bool Instrumenter::isReachable(const Instruction* source, 
                               const Instruction* dest){  
  
  if(source->getParent() == dest->getParent()){
    if(globalInstructionOrder[dest] > globalInstructionOrder[source]){
      cerr << "yay\n";
      return true;
    }
  }

  BasicBlock::const_iterator itSource(source);
  BasicBlock::const_iterator itDest(dest);
  reachable = false;
  stmtCache.clear();
  // cerr << "Starting reachability check " << endl;
  traverseStatement(*(itSource->getParent()->getParent()), 
                    itSource, itDest);

  /*
  if(reachable){
    llvm::errs() << *dest << "\nIS REACHABLE FROM\n" << *source;
    cerr << endl << endl;
  }else{
    llvm::errs() << *dest << "\nIS NOT REACHABLE FROM\n" << *source;
    cerr << endl << endl;
  }
  */
  /*
  cerr << "Source index: " << globalInstructionOrder[source]<< endl;
  cerr << "Dest index: " << globalInstructionOrder[dest] << endl;
  */
  return reachable;
}

void Instrumenter::traverseFunction(const Function& f, 
                                    const BasicBlock::const_iterator& itDest){
  if(reachable)
    return;
  if (f.size() == 0)
    return;
  
  Function::const_iterator firstBB = f.begin();
  BasicBlock::const_iterator firstInstr = firstBB->begin();

  return traverseStatement(f, firstInstr, itDest);
}

void Instrumenter::traverseStatement(const Function& f,
                                     const BasicBlock::const_iterator& inst,
                                     const BasicBlock::const_iterator& itDest){
  /*
  llvm::errs() << *inst;
  cerr << endl;
  */
  if(reachable)
    return;

  if(stmtCache.find(&(*itDest)) != stmtCache.end()){    
    reachable = true;
    return;
  }
  
  if(stmtCache.find(&(*inst)) != stmtCache.end())
    return;
    
  stmtCache.insert(&(*inst));
  
  if(inst->getOpcode() == Instruction::Call) {
    const CallInst* callInst = dyn_cast<CallInst>(&(*inst));
    assert(callInst && "Call inst pointer is NULL!!!, this shouldn't have happened");
    Function* calledFunc = callInst->getCalledFunction();
    if (calledFunc)
      traverseFunction(*calledFunc, itDest);
  }
  
  BasicBlock* bb = const_cast<BasicBlock*>(inst->getParent());
  
  if(&(bb->back()) == &(*inst)){
    /// last instruction
    for(succ_iterator child = succ_begin(bb), end = succ_end(bb);
        child != end; ++child){
      BasicBlock::const_iterator firstInstr = child->begin();
      traverseStatement(f, firstInstr, itDest);
    }
  } else{
    BasicBlock::const_iterator nextInst = inst;
    ++nextInst;
    traverseStatement(f, nextInst, itDest);
  }
}

void Instrumenter::createHBArrayInitializer(Module* m){
  /// CORD initialization function
  std::vector<Type*> initCordArgs;
  FunctionType* initCordType = 
    FunctionType::get(/*Result=*/Type::getVoidTy(m->getContext()),
                      /*Params=*/initCordArgs,
                      /*isVarArg=*/false);
  
  Function* func__Z8initCordv = m->getFunction("_Z8initCordv");
  if (!func__Z8initCordv) {
    func__Z8initCordv = Function::Create(/*Type=*/initCordType,
                                         /*Linkage=*/GlobalValue::ExternalLinkage,
                                         /*Name=*/"_Z8initCordv", m);
    func__Z8initCordv->setCallingConv(CallingConv::C);
  }
  AttrListPtr func__Z8initCordv_PAL;
  {
    SmallVector<AttributeWithIndex, 4> Attrs;
    AttributeWithIndex PAWI;
    PAWI.Index = 4294967295U; PAWI.Attrs = Attribute::None  | Attribute::NoUnwind | Attribute::UWTable;
    Attrs.push_back(PAWI);
    func__Z8initCordv_PAL = AttrListPtr::get(Attrs);

  }
  func__Z8initCordv->setAttributes(func__Z8initCordv_PAL);
  /// HB edges
  GlobalVariable* gbHBIndicesSize = 
    new GlobalVariable(/*Module=*/*m, 
                       /*Type=*/IntegerType::get(m->getContext(), 32),
                       /*isConstant=*/false,
                       /*Linkage=*/GlobalValue::ExternalLinkage,
                       /*Initializer=*/0, // has initializer, specified below
                       /*Name=*/"hbIndicesSize");
  gbHBIndicesSize->setAlignment(4);
  gbHBIndicesSize->setInitializer(ConstantInt::get(Type::getInt32Ty(m->getContext()), 
                                                            hbIndices.size()));

  ArrayType* ArrayTy_1 = ArrayType::get(IntegerType::get(m->getContext(), 32), 2);
  ArrayType* ArrayTy_0 = ArrayType::get(ArrayTy_1, hbIndices.size());
  GlobalVariable* ghbIndicesArray = 
    new GlobalVariable(/*Module=*/*m, 
                       /*Type=*/ArrayTy_0,
                       /*isConstant=*/false,
                       /*Linkage=*/GlobalValue::ExternalLinkage,
                       /*Initializer=*/0, // has initializer, specified below
                       /*Name=*/"hbIndicesArray");
  ghbIndicesArray->setAlignment(16);

  /// Constants:   
  ///  1. All 0 initialization field fo the HB edges array
  ConstantAggregateZero* hbIndicesArrayAllZeros = ConstantAggregateZero::get(ArrayTy_0);
  /// A zero index for get elemenent pointer's first indexing
  ConstantInt* zero = ConstantInt::get(Type::getInt32Ty(m->getContext()), 0);
  ///  2.  2nd array indices for the racing accesses
  ConstantInt* first = ConstantInt::get(Type::getInt32Ty(m->getContext()), 0);
  ConstantInt* second = ConstantInt::get(Type::getInt32Ty(m->getContext()), 1);
  /// Array is initialized to all 0s since it is a global variable
  ghbIndicesArray->setInitializer(hbIndicesArrayAllZeros);   

  /// Build the body of the function
  BasicBlock* label_entry = BasicBlock::Create(m->getContext(), "entry",func__Z8initCordv,0);
  /// Create the array initializer code
  for(unsigned i=0; i<hbIndices.size(); ++i){    std::vector<Constant*> const_ptr_first_store_indices;
    cerr << "hbIndices[i][0]: " << hbIndices[i][0] << endl;
    cerr << "hbIndices[i][1]: " << hbIndices[i][1] << endl;
    std::vector<Constant*> const_ptr_second_store_indices;
    /// insert the first index of getelementptr
    const_ptr_first_store_indices.push_back(zero);
    const_ptr_second_store_indices.push_back(zero);
    /// now insert the current index of the array
    ConstantInt* currentHBIndex = ConstantInt::get(Type::getInt32Ty(m->getContext()), i);
    const_ptr_first_store_indices.push_back(currentHBIndex);
    const_ptr_second_store_indices.push_back(currentHBIndex);
    /// finally insert the actual index where we will write the HB vertices
    const_ptr_first_store_indices.push_back(first);
    const_ptr_second_store_indices.push_back(second);
    /// Form the pointers for insertion point
    Constant* firstPtr = 
      ConstantExpr::getGetElementPtr(ghbIndicesArray, const_ptr_first_store_indices);
    Constant* secondPtr = 
      ConstantExpr::getGetElementPtr(ghbIndicesArray, const_ptr_second_store_indices);    
    /// So what to write
    ConstantInt* firstVertex = ConstantInt::get(Type::getInt32Ty(m->getContext()), hbIndices[i][0]);
    ConstantInt* secondVertex = ConstantInt::get(Type::getInt32Ty(m->getContext()), hbIndices[i][1]);
    /// Write it finally!
    StoreInst* firstHBVertexStore = new StoreInst(firstVertex, firstPtr, false, label_entry);
    firstHBVertexStore->setAlignment(4);
    StoreInst* secondHBVertexStore = new StoreInst(secondVertex, secondPtr, false, label_entry);
    secondHBVertexStore->setAlignment(4);
  }

  ReturnInst::Create(m->getContext(), label_entry);

  /// Insert the init cord function to the beginning of the module
  Function* main = m->getFunction("main");
 
  BasicBlock& firstBB = main->front();
  Instruction& firstI = firstBB.front();
  CallInst::Create(func__Z8initCordv, "", &firstI);
}

bool Instrumenter::replace(string& str, 
                           const string& from, 
                           const string& to) {
  size_t start_pos = str.find(from);
  if(start_pos == std::string::npos)
    return false;
  str.replace(start_pos, from.length(), to);
  return true;
}

char Instrumenter::ID = 0;
static RegisterPass<Instrumenter>
Y("instrument", "The instrumenter");
