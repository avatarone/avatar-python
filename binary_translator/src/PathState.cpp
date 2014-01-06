#include "lldc/PathState.h"
#include "lldc/BasicBlockCache.h"
#include "lldc/ReverseTranslationConfiguration.h"
#include "lldc/PathsManager.h"

#include "llvm/Support/raw_ostream.h"

#include <iostream>

using namespace llvm;


/**
 * This class encapsules the translation state of a single basic block.
 * It only exists while a basic block has not yet been completely translated,
 * afterwards the basic block information is managed by the basic block cache.
 */ 
PathState::PathState(llvm::BasicBlock* bb, llvm::Module* mod)
    : mod(mod), start_pc(-1), pc(-1), thumb(true)
{
	bbs.push_back(bb);
}
    
PathState::PathState()
: thumb(true)
{
}
    
void PathState::init(llvm::BasicBlock* bb, 
            llvm::Module* mod, 
            uint64_t start_pc, 
            std::map<unsigned, llvm::Value*>& values,
            bool thumb) {
	this->bbs.erase(this->bbs.begin(), this->bbs.end());
	this->bbs.push_back(bb);
    this->mod = mod;
    this->pc = start_pc;
    this->values = values;
    this->thumb = thumb;
    
    outs() << "Called init on state " << intToHexString(reinterpret_cast<uint64_t>(this)) << ": (thumb = " << (this->thumb ? "true" : "false") << ")" << '\n';
}

llvm::LLVMContext& PathState::getContext() {
    return this->bbs.back()->getContext();
}

llvm::BasicBlock* PathState::getBasicBlock() {
    return this->bbs.back();
}
    
llvm::Module* PathState::getModule() {
    return this->mod;
}

llvm::Function* PathState::getFunction() {
    return this->bbs.back()->getParent();
}

void PathState::setProgramCounter(uint64_t pc) {
    this->pc = pc;
}

uint64_t PathState::getProgramCounter() {
    return this->pc;
}

void PathState::setRegister(unsigned idx, llvm::Value* val) {
    this->values[idx] = val;
}

llvm::Value* PathState::getRegister(unsigned idx) {
    return this->values[idx];
}

void PathState::setCondition(unsigned idx, llvm::Value* val) {
    this->values[idx] = val;
}

llvm::Value* PathState::getCondition(unsigned idx) {
    return this->values[idx];
}

void PathState::instructionFinished(ReverseTranslationConfiguration* config, uint64_t next_pc) {
	llvm::BasicBlock* bb = llvm::BasicBlock::Create(this->getContext(), intToHexString(next_pc), this->getFunction());
	llvm::IRBuilder<> builder(this->getBasicBlock());
	builder.CreateBr(bb);
	config->getBasicBlockTranslationStateManager()->getBasicBlockCache()->insert(this, this->pc, next_pc, this->bbs.back());
	this->bbs.push_back(bb);
	this->pc = next_pc;	
}


/**
    * Call this in the very end when the basic block is finished and all it successor
    * translation states have been generated to do some cleanup.
    */
void PathState::translationFinished(ReverseTranslationConfiguration* config, uint64_t end_pc) {
//	config->getBasicBlockTranslationStateManager()->getBasicBlockCache()->insert(this, start_pc, end_pc, this->bbs.back());
	//TODO: Destroy state
	config->getBasicBlockTranslationStateManager()->getBasicBlockCache()->insert(this, this->pc, end_pc, this->bbs.back());
    config->getBasicBlockTranslationStateManager()->destroyPath(this);
}

bool PathState::isTranslationFinished() {
    return false;
}

PathState::~PathState() {
}

void PathState::setThumb(bool val) {
//    outs() << "Setting thumb mode of state " << intToHexString(reinterpret_cast<uint64_t>(this)) << " to " << (val ? "true" : "false") << '\n';
    this->thumb = val;
}

bool PathState::isThumb() {
//    outs() << "Getting thumb mode of state " << intToHexString(reinterpret_cast<uint64_t>(this)) << ": " << (this->thumb ? "true" : "false") << '\n';
    return this->thumb;
}


