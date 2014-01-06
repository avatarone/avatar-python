#ifndef _INSTRUMENT_MEMORY_ACCESS_PASS_H
#define _INSTRUMENT_MEMORY_ACCESS_PASS_H

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"

struct InstrumentMemoryAccessPass : public llvm::BasicBlockPass
{
private:
    llvm::Value* read_handler;
    llvm::Value* write_handler;
public:
    InstrumentMemoryAccessPass(char& pid, llvm::Value* read_handler, llvm::Value* write_handler);
    virtual bool doInitialization(llvm::Function &F);
    virtual bool runOnBasicBlock(llvm::BasicBlock &BB);
    virtual bool doFinalization(llvm::Function &F);
    virtual void getAnalysisUsage(llvm::AnalysisUsage &Info) const;
    virtual const char * getPassName() const;
};

#endif /* _INSTRUMENT_MEMORY_ACCESS_PASS_H */