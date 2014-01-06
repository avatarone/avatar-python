#include "lldc/BasicBlockCache.h"
#include <sstream>
#include "lldc/PathState.h"
#include "lldc/util.h"

#include "llvm/Support/raw_ostream.h"

using namespace llvm;

BasicBlockCache::BasicBlockCache(LLVMContext& context)
    : context(context) 
{
}

void BasicBlockCache::insert(PathState* state, uint64_t start_pc, uint64_t end_pc, BasicBlock * bb) {
	outs() << "Inserting basic block " << intToHexString(start_pc) << " into cache" << '\n';
    PcToBBMap& pcToBBMap = this->basicBlockCache[state->getFunction()];
    assert(pcToBBMap.find(start_pc) == pcToBBMap.end());
    pcToBBMap.insert(std::make_pair(start_pc, TranslatedBasicBlock(bb, start_pc, end_pc)));
}

TranslatedBasicBlock* BasicBlockCache::find(PathState* state, uint64_t pc) {
    return this->find(state->getFunction(), pc);
}

TranslatedBasicBlock* BasicBlockCache::find(llvm::Function* function, uint64_t pc) {
    BasicBlockCacheMap::iterator itr_outer = this->basicBlockCache.find(function);
    if (itr_outer != this->basicBlockCache.end())
    {
        PcToBBMap& pcToBBMap = itr_outer->second;
        PcToBBMap::iterator itr_inner = pcToBBMap.find(pc);
        if (itr_inner != pcToBBMap.end())
            return &itr_inner->second;
        else
            return NULL;
    }
    else
        return NULL;
}

    

void BasicBlockCache::invalidateBasicBlocks(uint64_t pc) {
    /* TODO */
}
