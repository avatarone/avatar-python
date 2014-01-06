#include "lldc/FunctionCache.h"
#include "lldc/ReverseTranslationConfiguration.h"
#include "lldc/util.h"
#include "lldc/PathState.h"
#include "lldc/PathsManager.h"

using namespace llvm;

//TODO: Remove this include and put registers to pass in the configuration
#define GET_REGINFO_ENUM
#define GET_INSTRINFO_ENUM
#include "ARMBaseInfo.h"


void FunctionCache::insertFunction(uint64_t pc, Function* function) {
    this->cache.insert(std::make_pair(pc, function));
}

Function* FunctionCache::getOrCreateFunction(ReverseTranslationConfiguration* config, Module* module, uint64_t function_address)
{
    std::map<uint64_t, Function*>::const_iterator itr = this->cache.find(function_address);
    
    if (itr != this->cache.end())
        return itr->second;
    
    Function* func = cast<Function>(module->getOrInsertFunction(intToHexString(function_address),
                                        (Type *) Type::getInt32Ty(config->getLLVMContext()),
                                        Type::getInt32Ty(config->getLLVMContext()) /* R0 */,
                                        Type::getInt32Ty(config->getLLVMContext()) /* R1 */,
                                        Type::getInt32Ty(config->getLLVMContext()) /* R2 */,
                                        Type::getInt32Ty(config->getLLVMContext()) /* R3 */,
                                        Type::getInt32Ty(config->getLLVMContext()) /* R4 */,
                                        Type::getInt32Ty(config->getLLVMContext()) /* R5 */,
                                        Type::getInt32Ty(config->getLLVMContext()) /* R6 */,
                                        Type::getInt32Ty(config->getLLVMContext()) /* R7 */,
                                        Type::getInt32Ty(config->getLLVMContext()) /* R8 */,
                                        Type::getInt32Ty(config->getLLVMContext()) /* R9 */,
                                        Type::getInt32Ty(config->getLLVMContext()) /* R10 */,
                                        Type::getInt32Ty(config->getLLVMContext()) /* R11 */,
                                        Type::getInt32Ty(config->getLLVMContext()) /* R12 */,
                                        Type::getInt32Ty(config->getLLVMContext()) /* SP */,
                                        Type::getInt32Ty(config->getLLVMContext()) /* LR */,
                                        /* TODO: Processor mode should be passed */                                                                      
                                        NULL));
    
    Function::arg_iterator args = func->arg_begin();

    std::map<unsigned, Value*> values;
    
    args->setName("R0"); values.insert(std::make_pair<unsigned, Value*>(ARM::R0, args));
    args++; args->setName("R1"); values.insert(std::make_pair<unsigned, Value*>(ARM::R1, args));
    args++; args->setName("R2"); values.insert(std::make_pair<unsigned, Value*>(ARM::R2, args));
    args++; args->setName("R3"); values.insert(std::make_pair<unsigned, Value*>(ARM::R3, args));
    args++; args->setName("R4"); values.insert(std::make_pair<unsigned, Value*>(ARM::R4, args));
    args++; args->setName("R5"); values.insert(std::make_pair<unsigned, Value*>(ARM::R5, args));
    args++; args->setName("R6"); values.insert(std::make_pair<unsigned, Value*>(ARM::R6, args));
    args++; args->setName("R7"); values.insert(std::make_pair<unsigned, Value*>(ARM::R7, args));
    args++; args->setName("R8"); values.insert(std::make_pair<unsigned, Value*>(ARM::R8, args));
    args++; args->setName("R9"); values.insert(std::make_pair<unsigned, Value*>(ARM::R9, args));
    args++; args->setName("R10"); values.insert(std::make_pair<unsigned, Value*>(ARM::R10, args));
    args++; args->setName("R11"); values.insert(std::make_pair<unsigned, Value*>(ARM::R11, args));
    args++; args->setName("R12"); values.insert(std::make_pair<unsigned, Value*>(ARM::R12, args));
    args++; args->setName("SP"); values.insert(std::make_pair<unsigned, Value*>(ARM::SP, args));
    args++; args->setName("LR"); values.insert(std::make_pair<unsigned, Value*>(ARM::LR, args));
    
    config->getBasicBlockTranslationStateManager()->
        createPath(config->getLLVMContext(),
                                     module,
                                     func,
                                     function_address,
                                     values);
    this->cache.insert(std::make_pair(function_address, func));
    return func;
}

