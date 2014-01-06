#ifndef _REVERSE_TRANSLATE_BASIC_BLOCK_H
#define _REVERSE_TRANSLATE_BASIC_BLOCK_H

#include "llvm/IR/BasicBlock.h"

class ReverseTranslateBasicBlock {
public:
    virtual bool isTranslationFinished() = 0;
    virtual llvm::BasicBlock* getBasicBlock() = 0;
    virtual ~ReverseTranslateBasicBlock() {
    }
};

#endif /* _REVERSE_TRANSLATE_BASIC_BLOCK_H */
