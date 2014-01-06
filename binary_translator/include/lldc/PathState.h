#ifndef _BASIC_BLOCK_TRANSLATION_STATE_H
#define _BASIC_BLOCK_TRANSLATION_STATE_H

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"

#include "util.h"
#include "ReverseTranslateBasicBlock.h"
#include <list>

class BasicBlockCache;
class ReverseTranslationConfiguration;

// class StackState {
// private:
//     std::list<llvm::Value*> stack;
// public:
//     StackState() {
//     }
//     
//     StackState(StackState& other) {
//         this->stack = other.stack;
//     }
//     
//     void push(llvm::Value* val) {
//         stack.push_back(val);
//     }
//     
//     llvm::Value* pop() {
//         llvm::Value* val = stack.back();
//         stack.pop_back();
//         return val;
//     }
//     
//     void setStackElement(unsigned idx) {
//         return stack[stack.size() - 1 - idx];
//     */}
// };


/**
 * This class encapsules the translation state of a single basic block.
 * It only exists while a basic block has not yet been completely translated,
 * afterwards the basic block information is managed by the basic block cache.
 */ 
class PathState : public ReverseTranslateBasicBlock
{
    friend class PathsManager;
private:
    /** The actual basic block */
	std::list<llvm::BasicBlock*> bbs;
    /** The LLVM Module in which the basic block will be created. */
    llvm::Module* mod;
    /** Program counter at the beginning of the basic block */
    uint64_t start_pc;
    /** The program counter at the current translation point */
    uint64_t pc;
    /** The registers at the current translation point */
    std::map<unsigned, llvm::Value *> values;
    /**The stack's state.*/
//     StackState stack;
    /** TODO: Arm specific, should go somewhere else */
    bool thumb;
    
public:
    PathState(llvm::BasicBlock* bb, llvm::Module* mod);
    PathState();
    
    void init(llvm::BasicBlock* bb, 
              llvm::Module* mod,     
              uint64_t start_pc, 
              std::map<unsigned, llvm::Value*>& values,
              bool thumb = true);
    
    llvm::LLVMContext& getContext();
    
    virtual llvm::BasicBlock* getBasicBlock();
    
    llvm::Module* getModule();
    
    llvm::Function* getFunction();
    
    uint64_t getStartProgramCounter();
    
    void setProgramCounter(uint64_t pc);
    
    uint64_t getProgramCounter();
    
    void setRegister(unsigned idx, llvm::Value* val);
    
    void setThumb(bool val);
    
    bool isThumb();
    
    llvm::Value* getRegister(unsigned idx);
    
    void setCondition(unsigned idx, llvm::Value* val);
    
    llvm::Value* getCondition(unsigned idx);
    
//     StackState& getStackState() {return stack;}
    
    /**
     * Call this in the very end when the basic block is finished and all it successor
     * translation states have been generated to do some cleanup.
     */
    void translationFinished(ReverseTranslationConfiguration* config, uint64_t end_pc);

	void instructionFinished(ReverseTranslationConfiguration* config, uint64_t next_pc);
	
    llvm::Value* getRegisterStructPtr();
    
    void storeRegisters();
    
    void loadRegisters();
    
    virtual bool isTranslationFinished();
    
    virtual ~PathState();
};

#endif /* _BASIC_BLOCK_TRANSLATION_STATE_H */
