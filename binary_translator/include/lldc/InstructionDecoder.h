#ifndef __INSTRUCTION_DECODER_H
#define __INSTRUCTION_DECODER_H

#include "lldc/ReverseTranslationConfiguration.h"
#include "lldc/ReverseTranslateBasicBlock.h"
#include "lldc/SystemInformation.h"
#include "lldc/BasicBlockCache.h"

#include "llvm/Support/MemoryObject.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h" 

#include <list>

class UnfinishedInstructionTranslationUnit;

class Ranges
{
    std::list<std::pair<uint64_t, uint64_t> > ranges;
    
public:
    Ranges(std::list<std::pair<uint64_t, uint64_t> >& ranges) : ranges(ranges) {}
    
    bool is_in_range(uint64_t val) {
        for (std::list<std::pair<uint64_t, uint64_t> >::iterator itr = ranges.begin();
             itr != ranges.end();
             itr++)
        {
            if (val >= itr->first && val < itr->second)
                return true;
        }
        
        return false;
    }
};

class CompiledCode
{
    uint8_t* data;
    unsigned size;
    uint64_t address;
public:
    CompiledCode(uint64_t address, uint8_t * data, unsigned size)
        : data(data), size(size), address(address)
    {
        this->data = new uint8_t[size];
        memcpy(this->data, data, size);
    }
    
    unsigned getSize() {return size;}
    uint8_t* getData() {return data;}
    uint64_t getAddress() {return address;}
    virtual ~CompiledCode() 
    {
        if (data)
            delete[] data;
    }
};
                 

class BinaryTranslator {
    private:
        ReverseTranslationConfiguration* config;
        llvm::MemoryObject* memory;
        llvm::MCInstrInfo* thumbInstrInfo;
        llvm::MCRegisterInfo* thumbRegInfo;
        llvm::MCSubtargetInfo* thumbSubtarget;
        llvm::MCAsmInfo* thumbAsmInfo;
        const llvm::MCDisassembler* thumbDisassembler;
        llvm::MCInstrInfo* armInstrInfo;
        llvm::MCRegisterInfo* armRegInfo;
        llvm::MCSubtargetInfo* armSubtarget;
        llvm::MCAsmInfo* armAsmInfo;
        const llvm::MCDisassembler* armDisassembler;
        llvm::MCInstPrinter* thumbInstPrinter;
        SystemInformation* sysInfo;
//        BasicBlockCache> basicBlockCache;
//        PathsManager* mgr;
        llvm::Module* module;
        llvm::Function* wrapper_function;
        const llvm::Target * thumbTarget;
        llvm::Value* instrumentationReadHandler;
        llvm::Value* instrumentationWriteHandler;
        
        void genCodeEntry(uint64_t pc);
        uint64_t entry_pc;
        bool is_debug;
    public:
        BinaryTranslator(std::string architecture, llvm::MemoryObject* memory, std::list<ProcessorRegisterDescription>& registerDescription, uint64_t entry_pc, llvm::StringRef module_name = "dummy_module");
        virtual ~BinaryTranslator() throw();
        void translate(Ranges& ranges);
        void translateSingleInstruction(UnfinishedInstructionTranslationUnit* unit);
        void genCodeExit(UnfinishedInstructionTranslationUnit* unit);
        void dumpIR(llvm::raw_ostream* out = NULL);
        CompiledCode* compile();
        void instrumentMemoryAccess();
        void setDebug(bool debug) {this->is_debug = debug;}
};

#endif /*  __INSTRUCTION_DECODER_H */
