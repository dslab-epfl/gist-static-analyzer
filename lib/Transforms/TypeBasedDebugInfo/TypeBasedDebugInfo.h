#include <set>
#include <map>

/*
VoidTyID = 0,    ///<  0: type with no size
HalfTyID,        ///<  1: 16-bit floating point type
FloatTyID,       ///<  2: 32-bit floating point type
DoubleTyID,      ///<  3: 64-bit floating point type
X86_FP80TyID,    ///<  4: 80-bit floating point type (X87)
FP128TyID,       ///<  5: 128-bit floating point type (112-bit mantissa)
PPC_FP128TyID,   ///<  6: 128-bit floating point type (two 64-bits, PowerPC)
LabelTyID,       ///<  7: Labels
MetadataTyID,    ///<  8: Metadata
X86_MMXTyID,     ///<  9: MMX vectors (64 bits, X86 specific)

// Derived types... see DerivedTypes.h file.
// Make sure FirstDerivedTyID stays up to date!
IntegerTyID,     ///< 10: Arbitrary bit width integers
FunctionTyID,    ///< 11: Functions
StructTyID,      ///< 12: Structures
ArrayTyID,       ///< 13: Arrays
PointerTyID,     ///< 14: Pointers
VectorTyID,      ///< 15: SIMD 'packed' format, or other vector type
*/

namespace llvm{
  static const char * TypeStrings[] = { "Void", "Float_16", "Float_32", "Float_64", "Float_80", 
                                        "Float_128_112", "Float_128_64", "Label", "Metadata",  "MMX_Vector", 
                                        "Integer", "Function", "Struct", "Array", "Pointer", "Vector"};

  class TypeBasedDebugInfo : public ModulePass {
  public:
    TypeBasedDebugInfo();

    const char *getPassName() const;
    virtual void getAnalysisUsage(AnalysisUsage& au) const;
    void printDebugInfo(Instruction& instr);

    virtual bool runOnModule(Module& m);

    std::set<std::string>& split(const std::string &s, char delim, std::set<std::string> &elems);
    std::set<std::string> split(const std::string &s, char delim);
    std::map<std::string, std::vector<MDNode*>> typeToDebugInfo;
    
    static char ID;
  };
}
