#ifndef _BASIC_BLOCK_CACHE_H
#define _BASIC_BLOCK_CACHE_H

#include <map>
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"

#include "ReverseTranslateBasicBlock.h"

class PathState;

class TranslatedBasicBlock : public ReverseTranslateBasicBlock {
private:
    llvm::BasicBlock* bb;
    uint64_t pc_start;
    uint64_t pc_end;
    
public:

    TranslatedBasicBlock(llvm::BasicBlock* bb, uint64_t pc_start, uint64_t pc_end)
    : bb(bb), pc_start(pc_start), pc_end(pc_end)
    {
    }
    
    virtual llvm::BasicBlock* getBasicBlock() {
        return this->bb;
    }
    
    uint64_t getStartProgramCounter() {
        return this->pc_start;
    }
    
    uint64_t getEndProgramCounter() {
        return this->pc_end;
    }
    
    virtual bool isTranslationFinished() {
        return true;
    }
    
    virtual ~TranslatedBasicBlock() {
    }
    
};

/**
 * This is an interface for the basic block cache.
 * TODO: Currently the implementation is here, should be moved out
 * into its own subclass.
 */

class BasicBlockCache {
    typedef std::map<uint64_t, TranslatedBasicBlock> PcToBBMap;
    typedef std::map<llvm::Function *, PcToBBMap> BasicBlockCacheMap;
    
    BasicBlockCacheMap basicBlockCache;
    llvm::LLVMContext& context;
    
public:
    BasicBlockCache(llvm::LLVMContext& context);
    
    /**
     * Insert a new basic block in the cache.
     */
    void insert(PathState* state, uint64_t pc_start, uint64_t pc_end, llvm::BasicBlock * bb);
    
    /**
     * Find a basic block from its function and entry program counter. 
     */
    TranslatedBasicBlock * find(PathState* state, uint64_t pc);
    
     TranslatedBasicBlock * find(llvm::Function* function, uint64_t pc);
    
    /**
     * Has to be called whenever data is written to (code) memory.
     */
    void invalidateBasicBlocks(uint64_t pc);
};

#endif /* _BASIC_BLOCK_CACHE_H */
