#ifndef _EXTENDED_MC_INST_H
#define _EXTENDED_MC_INST_H

#include "llvm/MC/MCInst.h" 
#include "llvm/IR/BasicBlock.h"

class ExtendedMCInst : public llvm::MCInst {
public:
    uint64_t pc;
    uint64_t size;
    
public:
    ExtendedMCInst() 
    :  pc(0), size(0) 
    {
    }
    
    void setProgramCounter(uint64_t pc) {
        this->pc = pc;
    }
    
    uint64_t getProgramCounter() {
        return pc;
    }
    
    void setSize(uint64_t size) {
        this->size = size;
    }
    
    uint64_t getSize() {
        return size;
    }
};  

#endif /* _EXTENDED_MC_INST_H */
