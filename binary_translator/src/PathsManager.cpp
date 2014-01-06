#include "llvm/Support/raw_ostream.h"

#include "lldc/PathsManager.h"

using namespace llvm;
using std::list;
using std::find;

PathsManager::PathsManager(BasicBlockCache* cache)
: cache(cache)
{
}
    
PathState* PathsManager::createPath(PathState* previous, uint64_t pc) {
    return createPath(previous->getContext(),
                                            previous->getModule(),
                                            previous->getFunction(),
                                            pc,
                                            previous->values);
}

PathState* PathsManager::createPath(PathState* previous, 
                                                                 llvm::Function* func, 
                                                                 uint64_t start_pc) 
{
    return createPath(previous->getContext(),
                                            previous->getModule(),
                                            func,
                                            start_pc,
                                            previous->values);
}

PathState* PathsManager::createPath(llvm::LLVMContext& context,
                                                                    llvm::Module* mod, 
                                                                    llvm::Function* func, 
                                                                    uint64_t start_pc, 
                                                                    std::map<unsigned, llvm::Value*>& values) 
{
    outs() << "Creating new path " << intToHexString(start_pc) << " for function " << intToHexString(reinterpret_cast<uint64_t>(func)) << '\n';
    llvm::BasicBlock* bb = llvm::BasicBlock::Create(context, intToHexString(start_pc), func);
    PathState* state;
    if (freeStates.size() > 0) {
        state = freeStates.back();
        freeStates.pop_back();
    } else {
        state = new PathState();
    }
    
    usedStates.push_back(state);
    
    state->init(bb, mod, start_pc, values);
    
    return state;
}

ReverseTranslateBasicBlock* PathsManager::getOrCreateReverseTranslateBasicBlock(PathState* previous, uint64_t pc) {
    outs() << "Getting or creating new path from path " << intToHexString(reinterpret_cast<uint64_t>(previous)) << " at " << intToHexString(pc) << '\n';
    ReverseTranslateBasicBlock* bb = this->getReverseTranslateBasicBlock(previous->getFunction(), pc);
    if (bb)
        return bb;
        
    return createPath(previous, pc);
}

ReverseTranslateBasicBlock* PathsManager::getReverseTranslateBasicBlock(llvm::Function* function, uint64_t pc) {
    TranslatedBasicBlock* bb = this->cache->find(function, pc);
    
    if (bb)
        return bb;

	for (list<PathState*>::const_iterator itr = this->usedStates.begin();
	     itr != this->usedStates.end();
	     itr++) 
	{
		if ((*itr)->getProgramCounter() == pc) {
			assert((*itr)->getFunction() == function && "Function different for searched block and found block");
			return *itr;
		}
	}
    
    return 0;
}

void PathsManager::destroyPath(PathState* state) {
	outs() << "Destroying state " << intToHexString(reinterpret_cast<uint64_t>(state)) << " at pc " << intToHexString(state->getProgramCounter()) << '\n';
	list<PathState*>::iterator itr = find(this->usedStates.begin(), this->usedStates.end(), state);
	if (itr != this->usedStates.end()) {
		this->usedStates.erase(itr);
		this->freeStates.push_back(state);
	}
	else {
		//TODO: Error, state not found
		assert(false && "State to destroy not in used states list");
	}
}

PathState* PathsManager::getUnfinishedPath() {
    if (this->usedStates.empty()) {
		outs() << "No more unfinished states" << '\n';
		return 0;
	}

	PathState* state = this->usedStates.front();
	outs() << "Getting unfinished state " << intToHexString(reinterpret_cast<uint64_t>(state)) << '\n';
    return state;
}