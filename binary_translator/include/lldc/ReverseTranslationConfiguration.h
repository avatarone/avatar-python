#ifndef _REVERSE_TRANSLATION_CONFIGURATION
#define _REVERSE_TRANSLATION_CONFIGURATION

#include "llvm/Support/MemoryObject.h"
#include "llvm/MC/MCDisassembler.h"
#include "llvm/Support/TargetRegistry.h"

#include "SystemInformation.h"
#include "ExtendedMCInst.h"
#include "NextInstructions.h"
#include "ReverseTranslatorExceptions.h"
#include "FunctionCache.h"

class ReverseTranslationConfiguration;
class ReverseTranslationState;
class PathsManager;
class PathState;
class UnfinishedInstructionTranslationUnit;

typedef void (*ReverseInstructionTranslatorFunction)(ExtendedMCInst& instruction, ReverseTranslationConfiguration& config, UnfinishedInstructionTranslationUnit* unit, llvm::MemoryObject* memory);

class ValueInformation
{
    std::string name;
    unsigned id;
    llvm::Type* type;
public:
    ValueInformation(std::string name, unsigned id, llvm::Type* type)
        : name(name), id(id), type(type) 
    {
    }
    
    const std::string& getName() const {return name;}
    unsigned getId() const {return id;}
    llvm::Type* getType() const {return type;}
};

class ReverseTranslationConfiguration
{
    llvm::LLVMContext& llvmContext; //< Stored here just for our convenience
    const llvm::MCDisassembler* thumbDisassembler;
    const llvm::MCDisassembler* armDisassembler;
    const llvm::Target* target;
    llvm::StructType* instrumentation_struct_ty;
    SystemInformation* sys_info;
    std::map<unsigned, ReverseInstructionTranslatorFunction> instruction_translators;
    PathsManager* basicBlockTranslationStateManager;
    FunctionCache functionCache;
public:
    ReverseTranslationConfiguration(llvm::LLVMContext& llvmContext, const llvm::MCDisassembler* thumbDisassembler, 
                                    const llvm::MCDisassembler* armDisassembler,
                                    const llvm::Target* target,
                                    llvm::StructType* instrumentation_struct_ty, SystemInformation* sys_info, 
                                    PathsManager* mgr);
    
    const llvm::MCDisassembler* getDisassembler(unsigned instruction_set);
    
    llvm::LLVMContext& getLLVMContext();
    
    const llvm::Target* getTarget(PathState* state);
    
    SystemInformation* getSystemInformation();
    
    llvm::StructType* getInstrumentationStructTy();
    
    ReverseInstructionTranslatorFunction getReverseInstructionTranslator(ExtendedMCInst& instruction);
    
    std::map<unsigned, ReverseInstructionTranslatorFunction>& getInstructionTranslators();
    
    PathsManager* getBasicBlockTranslationStateManager();
    
    FunctionCache& getFunctionCache();
    
    const std::list<ValueInformation>& getValueInformation();
};

#endif /* _REVERSE_TRANSLATION_CONFIGURATION */
