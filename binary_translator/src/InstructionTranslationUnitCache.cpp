#include "lldc/InstructionTranslationUnitCache.h"
#include "lldc/util.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Value.h"

using namespace llvm;


void InstructionTranslationUnit::addIncoming(const std::map< unsigned, llvm::Value* >& values, llvm::BasicBlock* predecessor)
{
    for (std::map< unsigned, Value* >::const_iterator itr = values.begin();
         itr != values.end();
         itr++)
    {
        assert(phi_nodes.find(itr->first) != phi_nodes.end() && "Found value that is not specified in the value information of the configuration");
        
        phi_nodes[itr->first]->addIncoming(itr->second, predecessor);
    }  
}


bool UnfinishedInstructionTranslationUnit::operator==(const UnfinishedInstructionTranslationUnit& other)
{
    return instruction_translation_unit.getProgramCounter() == other.instruction_translation_unit.getProgramCounter();
}

InstructionTranslationUnit* InstructionTranslationUnitCache::find(Function* function, uint64_t pc) 
{
    std::map< Function*, std::map< uint64_t, InstructionTranslationUnit > >::iterator outer_itr = instruction_translation_unit_cache.find(function);
    if (outer_itr != instruction_translation_unit_cache.end())
    {
        std::map< uint64_t, InstructionTranslationUnit >::iterator inner_itr = outer_itr->second.find(pc);
        if (inner_itr != outer_itr->second.end())
            return &inner_itr->second;
    }
    
    return NULL;
}

InstructionTranslationUnit* InstructionTranslationUnitCache::create(
    ReverseTranslationConfiguration& config, 
    Function* function, 
    uint64_t pc) 
{
    BasicBlock *head_bb = BasicBlock::Create(config.getLLVMContext(), intToHexString(pc), function);
    IRBuilder<> builder(head_bb);
    InstructionTranslationUnit& unit = instruction_translation_unit_cache[function][pc];
    std::map< unsigned, Value* > new_values;
    
    unit.head_bb = head_bb;
    unit.instruction_set = 0;
    unit.original_instruction = 0;
    unit.is_finished = false;
    unit.function = function;
    unit.pc = pc;
    
    for (std::list< ValueInformation >::const_iterator value_info_itr = config.getValueInformation().begin();
         value_info_itr != config.getValueInformation().end();
         value_info_itr++)
    {
        PHINode *phi = builder.CreatePHI(value_info_itr->getType(), 0, value_info_itr->getName());
        unit.phi_nodes[value_info_itr->getId()] = phi;
        new_values[value_info_itr->getId()] = phi;
    }  
    
    
    this->unfinished_instruction_translation_units.push_back(new UnfinishedInstructionTranslationUnit(unit, new_values));
    
    return &unit;
}

bool InstructionTranslationUnitCache::has_more_unfinished_translation_units()
{
    return !this->unfinished_instruction_translation_units.empty();
}

UnfinishedInstructionTranslationUnit* InstructionTranslationUnitCache::get_next_unfinished_translation_unit()
{
    UnfinishedInstructionTranslationUnit* unit = this->unfinished_instruction_translation_units.front();
    this->unfinished_instruction_translation_units.pop_front();
    return unit;
}

UnfinishedInstructionTranslationUnit::UnfinishedInstructionTranslationUnit(InstructionTranslationUnit& unit, const std::map< unsigned, llvm::Value* >& values)
    : instruction_translation_unit(unit), values(values)
{
}
    
/**
 * Get a translated instruction, if it is already translated, or create a new one
 * if no translation has been found.
 */
InstructionTranslationUnit* InstructionTranslationUnitCache::find_create(
    ReverseTranslationConfiguration& config, 
    Function* function, 
    uint64_t pc,
    const std::map< unsigned, Value* >& predecessor_values,
    BasicBlock *predecessor_bb,
    bool terminate_last_block)
{
    InstructionTranslationUnit* unit = this->find(function, pc);
    
    if (!unit)
    {
        unit = this->create(config, function, pc);
    }
    
    unit->addIncoming(predecessor_values, predecessor_bb);
    
    if (terminate_last_block)
    {
        IRBuilder<> builder(predecessor_bb);
        
        builder.CreateBr(unit->getHead());
    }
    
    return unit;
}

InstructionTranslationUnitCache& InstructionTranslationUnitCache::getInstance()
{
    static InstructionTranslationUnitCache instance;
    
    return instance;
}