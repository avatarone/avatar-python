#ifndef _INSTRUCTION_TRANSLATION_UNIT_CACHE_H
#define _INSTRUCTION_TRANSLATION_UNIT_CACHE_H

#include <map>
#include "llvm/IR/Instructions.h"
#include "llvm/MC/MCInst.h"

#include "lldc/ReverseTranslationConfiguration.h"

/**
 * A translated instruction consists of at least one basic block, but can be composed of several
 * (in case of conditional assembler instructions).
 * Each instruction starts with a phi node for every register and flag.
 * The entry basic block of the instruction is labelled with the pc value as hex string, all further
 * blocks are labelled with the pc hex string plus an underscore and some number.
 * A translated instruction can only belong to one function (this might pose a problem if you start
 * translating in the middle of a function that is recursive).
 */
class InstructionTranslationUnit
{
    friend class InstructionTranslationUnitCache;
    
    std::map< unsigned, llvm::PHINode* > phi_nodes;
    uint64_t pc;
    llvm::BasicBlock* head_bb;
    bool is_finished;
    unsigned instruction_set;
    llvm::MCInst* original_instruction;
    llvm::Function* function;
    
public:
    InstructionTranslationUnit()
        : pc(0), head_bb(0), is_finished(false), instruction_set(0), original_instruction(0), function(0)
    {
    }
    
    void addIncoming(const std::map< unsigned, llvm::Value* >& values, llvm::BasicBlock* predecessor);
    
    uint64_t getProgramCounter() {return pc;}
    llvm::BasicBlock* getHead() {return head_bb;}
    unsigned getInstructionSet() {return instruction_set;}
    
    llvm::Function* getFunction() {return function;}
};

class UnfinishedInstructionTranslationUnit
{
    InstructionTranslationUnit& instruction_translation_unit;
    std::map< unsigned, llvm::Value* > values;
    llvm::BasicBlock* tail_bb;
public:
    UnfinishedInstructionTranslationUnit(InstructionTranslationUnit& unit, const std::map< unsigned, llvm::Value* >& values);
    void translation_finished(void);
    bool operator==(const UnfinishedInstructionTranslationUnit& other);
    InstructionTranslationUnit& getTranslationUnit() {return instruction_translation_unit;}
    std::map< unsigned, llvm::Value* >& getValues() {return values;}
    llvm::Value* getValue(unsigned id) {return values[id];}
    void setValue(unsigned id, llvm::Value* val) {values[id] = val;}
};

class InstructionTranslationUnitCache
{
    std::map< llvm::Function*, std::map< uint64_t, InstructionTranslationUnit > > instruction_translation_unit_cache;
    std::list< UnfinishedInstructionTranslationUnit* > unfinished_instruction_translation_units;
    
private:
    InstructionTranslationUnit* find(llvm::Function* function, uint64_t pc);
    InstructionTranslationUnit* create(ReverseTranslationConfiguration&, llvm::Function*, uint64_t);
    
public:
    InstructionTranslationUnit* find_create(ReverseTranslationConfiguration& config, llvm::Function* function, uint64_t pc, const std::map< unsigned, llvm::Value* >& predecessor_values, llvm::BasicBlock *predecessor_bb, bool terminate_last_block = true);
    UnfinishedInstructionTranslationUnit* get_next_unfinished_translation_unit();
    bool has_more_unfinished_translation_units();
    static InstructionTranslationUnitCache& getInstance();
};

#endif /* _INSTRUCTION_TRANSLATION_UNIT_CACHE_H */