#ifndef _FUNCTION_CACHE_H
#define _FUNCTION_CACHE_H

#include <map>
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"

class ReverseTranslationConfiguration;

class FunctionCache {
private:
    std::map<uint64_t, llvm::Function*> cache;

public:
    void insertFunction(uint64_t pc, llvm::Function* function);

    llvm::Function* getOrCreateFunction(ReverseTranslationConfiguration* config, llvm::Module* module, uint64_t function_address);
};
    
#endif /* _FUNCTION_CACHE_H */
