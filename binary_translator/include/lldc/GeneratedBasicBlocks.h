#ifndef _GENERATED_BASIC_BLOCKS_H
#define _GENERATED_BASIC_BLOCKS_H

#include <stdint.h>
#include "llvm/IR/BasicBlock.h"

/**
 * This is the class in which the resulting basic blocks from translating one instruction are returned.
 */

class GeneratedBasicBlocks
{
    llvm::BasicBlock * currentBB;
    uint64_t currentPC; //Next instruction pointer in the current BB
    llvm::BasicBlock * nextBB;
    uint64_t nextPC; //Next instruction pointer in the next BB
    llvm::BasicBlock * branchBB;
    uint64_t branchPC; //Next instruction pointer in the branch BB
    llvm::BasicBlock * exceptionBB;
    uint64_t exceptionPC; //Next instruction pointer in the exception BB
    
public:
    GeneratedBasicBlocks() {}
    
    llvm::BasicBlock * getCurrentBB() {return this->currentBB;}
    llvm::BasicBlock * getNextBB() {return this->nextBB;}
    llvm::BasicBlock * getBranchBB() {return this->branchBB;}
    llvm::BasicBlock * getExceptionBB() {return this->exceptionBB;}
    uint64_t getCurrentBBProgramCounter() {return this->currentPC;}
    uint64_t getNextBBProgramCounter() {return this->nextPC;}
    uint64_t getBranchBBProgramCounter() {return this->branchPC;}
    uint64_t getExceptionBBProgramCounter() {return this->exceptionPC;}
    
    void setCurrentBB(uint64_t pc, llvm::BasicBlock * bb) {
        this->currentBB = bb;
        this->currentPC = pc;
    }
    
    void setNextBB(uint64_t pc, llvm::BasicBlock * bb) {
        this->nextBB = bb;
        this->nextPC = pc;
    }
    
    void setBranchBB(uint64_t pc, llvm::BasicBlock * bb) {
        this->branchBB = bb;
        this->branchPC = pc;
    }
    
    void setExceptionBB(uint64_t pc, llvm::BasicBlock * bb) {
        this->exceptionBB = bb;
        this->exceptionPC = pc;
    }
};


#endif /* _GENERATED_BASIC_BLOCKS_H */
