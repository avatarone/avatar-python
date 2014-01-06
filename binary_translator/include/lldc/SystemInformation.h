#ifndef _SYSTEM_INFORMATION_H
#define _SYSTEM_INFORMATION_H 

/**
 * This file contains classes for static information about the source system.
 * (i.e. information that needs to be set one time before the decompilation process,
 * and does not need to be changed afterwards)
 */
 
#include "llvm/IR/Type.h"
#include "llvm/ADT/Twine.h"
#include "llvm/IR/DerivedTypes.h"
//#include "llvm/IR/Function.h"
//#include "llvm/IR/Module.h"
//#include "llvm/IR/LLVMContext.h"
//#include "llvm/MC/MCDisassembler.h"
//#include "llvm/Support/TargetRegistry.h"

#include <list>

class ProcessorRegisterDescription
{
    std::string name;
    llvm::Type* type;
    size_t index;
    unsigned regNum;
public:
    ProcessorRegisterDescription(std::string name, llvm::Type* type, size_t index, unsigned regNum) : name(name), type(type), index(index), regNum(regNum) {}
    llvm::Twine getName() const {return llvm::Twine(name);}
    llvm::Type* getType() const {return type;}
    size_t getIndex() const {return index;}
    unsigned getRegNum() const {return regNum;}
} ;

class SystemInformation
{
    private:
        std::list<ProcessorRegisterDescription> registerDescription;    
    public:
        typedef std::list<ProcessorRegisterDescription>::iterator register_iterator; 
        SystemInformation(std::list<ProcessorRegisterDescription>& regDesc) : registerDescription(regDesc) {}
        llvm::StructType* getProcessorStructTy() {
            std::vector<llvm::Type *> elements;
            for (std::list<ProcessorRegisterDescription>::iterator itr = registerDescription.begin();
                 itr != registerDescription.end();
                 itr++)
            {
                elements.push_back(itr->getType());
            }
            
            return llvm::StructType::create(elements, "processor_struct", true);
        }
        register_iterator register_description_begin() {return registerDescription.begin();}
        register_iterator register_description_end() {return registerDescription.end();}
        unsigned getGeneralRegisterBitWidth() {return 32;}
};

#endif /*  _SYSTEM_INFORMATION_H */
