#ifndef _PATHS_MANAGER_H
#define _PATHS_MANAGER_H

#include <list>
#include <map>

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Value.h"

#include "lldc/BasicBlockCache.h"
#include "lldc/PathState.h"
#include "lldc/ReverseTranslateBasicBlock.h"


class PathsManager {
    std::list<PathState*> freeStates;
    std::list<PathState*> usedStates;
    BasicBlockCache* cache;
public:
    PathsManager(BasicBlockCache* cache);

	BasicBlockCache* getBasicBlockCache() {
		return this->cache;
	}

	PathState* getUnfinishedPath();
        
    PathState* createPath(PathState* previous, uint64_t pc);
    
    PathState* createPath(PathState* previous, 
                                                                 llvm::Function* func, 
                                                                 uint64_t start_pc) ;
    
    PathState* createPath(llvm::LLVMContext& context,
                                                                        llvm::Module* mod, 
                                                                        llvm::Function* func, 
                                                                        uint64_t start_pc, 
                                                                        std::map<unsigned, llvm::Value*>& values) ;
    
    ReverseTranslateBasicBlock* getOrCreateReverseTranslateBasicBlock(PathState* previous, uint64_t pc);
    ReverseTranslateBasicBlock* getReverseTranslateBasicBlock(llvm::Function* function, uint64_t pc);
	void destroyPath(PathState* state);
        
};

#endif /* _PATHS_MANAGER_H */