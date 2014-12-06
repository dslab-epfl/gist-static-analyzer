//===- IRPrinter.cpp - -----------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements a fast IR printing pass
// 
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "hello"
#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/Statistic.h"

using namespace llvm;


namespace {
  // Hello2 - The second implementation with getAnalysisUsage implemented.
  struct IRPrinter : public ModulePass {
    static char ID; // Pass identification, replacement for typeid
    IRPrinter() : ModulePass(ID) {}

    virtual bool runOnModule(Module &m) {
      /*
      for (Module::iterator fi = m.begin(), fe = m.end(); fi != fe; ++fi) {
        for (Function::iterator bi = fi->begin(), be = fi->end(); bi != be; ++bi) {
          // TODO: Improve this comparison by getting mangled names from the elf debug information
          if(fi->getName().find(StringRef(FunctionName)) != StringRef::npos)
            for (BasicBlock::iterator ii = bi->begin(), ie = bi->end(); ii != ie; ++ii) { 
              if (MDNode *N = ii->getMetadata("dbg")) {
                DILocation Loc(N);
                unsigned lineNumber = Loc.getLineNumber();
                StringRef fileName = Loc.getFilename();
                if(lineNumber == LineNumber) {
                  // Instead of assuming the offending instruction is a load, we may filter for several
                  // other unlikely instructions like bitcasts and instrinsics
                  if (isa<LoadInst>(*ii)) {
                    StringRef directory = Loc.getDirectory();
                    errs() << "------------------------" << "\n";
                    errs() << "Target LLVM instruction:" << "\n";
                    errs() << "------------------------" << "\n";
                    errs() << "\t" << *ii << "\n\t|--> " << directory << "/" << fileName<< " : " << lineNumber << "\n\n";
                    // trackUseDefChain(*ii);
                    targetInstruction = &(*ii);
                    targetFunction = &(*fi);
                  }
                }
              }
            }
        }
      }
*/    }

    // We don't modify the program, so we preserve all analyses
    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.setPreservesAll();
    }
  };
}

char IRPrinter::ID = 0;
static RegisterPass<IRPrinter>
Y("irprinter", "A fast IRPrinter");
