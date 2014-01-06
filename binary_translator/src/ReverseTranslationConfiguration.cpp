#include "lldc/ReverseTranslationConfiguration.h"
#include "lldc/PathState.h"
#include "lldc/LLVMArmRegisters.h"

#include "llvm/Support/raw_ostream.h"

using namespace llvm;

ReverseTranslationConfiguration::ReverseTranslationConfiguration(
        llvm::LLVMContext& llvmContext, 
        const llvm::MCDisassembler* thumbDisassembler, 
        const llvm::MCDisassembler* armDisassembler,
        const llvm::Target* target,
        llvm::StructType* instrumentation_struct_ty, 
        SystemInformation* sys_info, 
        PathsManager* mgr)
    : llvmContext(llvmContext), 
      thumbDisassembler(thumbDisassembler), 
      armDisassembler(armDisassembler), target(target), 
      instrumentation_struct_ty(instrumentation_struct_ty),
      sys_info(sys_info), 
      basicBlockTranslationStateManager(mgr)
    {
    }
    
const llvm::MCDisassembler* ReverseTranslationConfiguration::getDisassembler(unsigned instruction_set) {
    assert(instruction_set == 0);
    
    return this->thumbDisassembler;
}

llvm::LLVMContext& ReverseTranslationConfiguration::getLLVMContext() {
    return this->llvmContext;
}

const llvm::Target* ReverseTranslationConfiguration::getTarget(PathState* state) {
    return this->target;
}

SystemInformation* ReverseTranslationConfiguration::getSystemInformation() {
    return this->sys_info;
}

llvm::StructType* ReverseTranslationConfiguration::getInstrumentationStructTy() {
    return this->instrumentation_struct_ty;
}

ReverseInstructionTranslatorFunction ReverseTranslationConfiguration::getReverseInstructionTranslator(ExtendedMCInst& instruction) {
    std::map<unsigned, ReverseInstructionTranslatorFunction>::const_iterator itr = this->instruction_translators.find(instruction.getOpcode());
    
    if (itr == this->instruction_translators.end())
        throw ReverseTranslatorUknownOpcodeException(instruction);
    
    return itr->second;
}

std::map<unsigned, ReverseInstructionTranslatorFunction>& ReverseTranslationConfiguration::getInstructionTranslators() {
    return this->instruction_translators;
}

PathsManager* ReverseTranslationConfiguration::getBasicBlockTranslationStateManager() {
    return basicBlockTranslationStateManager;
}

FunctionCache& ReverseTranslationConfiguration::getFunctionCache() {
    return functionCache;
}

const std::list<ValueInformation>& ReverseTranslationConfiguration::getValueInformation()
{
    static const ValueInformation static_value_info[] = {
        ValueInformation("r0.", ARM::R0, Type::getInt32Ty(this->getLLVMContext())),
        ValueInformation("r1.", ARM::R1, Type::getInt32Ty(this->getLLVMContext())),
        ValueInformation("r2.", ARM::R2, Type::getInt32Ty(this->getLLVMContext())),
        ValueInformation("r3.", ARM::R3, Type::getInt32Ty(this->getLLVMContext())),
        ValueInformation("r4.", ARM::R4, Type::getInt32Ty(this->getLLVMContext())),
        ValueInformation("r5.", ARM::R5, Type::getInt32Ty(this->getLLVMContext())),
        ValueInformation("r6.", ARM::R6, Type::getInt32Ty(this->getLLVMContext())),
        ValueInformation("r7.", ARM::R7, Type::getInt32Ty(this->getLLVMContext())),
        ValueInformation("r8.", ARM::R8, Type::getInt32Ty(this->getLLVMContext())),
        ValueInformation("r9.", ARM::R9, Type::getInt32Ty(this->getLLVMContext())),
        ValueInformation("r10.", ARM::R10, Type::getInt32Ty(this->getLLVMContext())),
        ValueInformation("r11.", ARM::R11, Type::getInt32Ty(this->getLLVMContext())),
        ValueInformation("r12.", ARM::R12, Type::getInt32Ty(this->getLLVMContext())),
        ValueInformation("sp.", ARM::SP, Type::getInt32Ty(this->getLLVMContext())),
        ValueInformation("lr.", ARM::LR, Type::getInt32Ty(this->getLLVMContext())),
        ValueInformation("cpsr.", ARM::CPSR, Type::getInt32Ty(this->getLLVMContext())),
        ValueInformation("flag_z.", ARM::FLAG_Z, Type::getInt1Ty(this->getLLVMContext())),
        ValueInformation("flag_n.", ARM::FLAG_N, Type::getInt1Ty(this->getLLVMContext())),
        ValueInformation("flag_c.", ARM::FLAG_C, Type::getInt1Ty(this->getLLVMContext())),
        ValueInformation("flag_v.", ARM::FLAG_V, Type::getInt1Ty(this->getLLVMContext()))};
        
    //TODO: This is still ARM specific, make it configurable
    static std::list< ValueInformation > value_information(static_value_info, static_value_info + (sizeof(static_value_info) / sizeof(ValueInformation)));
    
    return value_information;
}
