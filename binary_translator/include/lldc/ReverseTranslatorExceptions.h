#ifndef _REVERSE_TRANSLATOR_EXCEPTIONS_H
#define _REVERSE_TRANSLATOR_EXCEPTIONS_H

class Exception : public std::exception
{
    std::string message;
public:
    Exception(std::string message) : message(message) {}
    virtual const char * what() const throw() {return message.c_str();}
    virtual ~Exception() throw() {}
};

class DisassemblerException : public Exception
{
    uint64_t pc;
    
public:
    DisassemblerException(uint64_t pc) : Exception("Disassembler exception"), pc(pc) {}
    uint64_t getProgramCounter() {return pc;}
    virtual ~DisassemblerException() throw() {}
};

class LLVMException : public Exception
{
public:
    LLVMException(std::string description) : Exception(description) {}
    virtual ~LLVMException() throw() {}
};

class ReverseTranslatorUknownOpcodeException : public Exception {
private:
    ExtendedMCInst instruction;

public:
    ReverseTranslatorUknownOpcodeException(ExtendedMCInst& instruction)
    : Exception("Encountered unknown upcode upon translation"), instruction(instruction)
    {
    }
   
    ExtendedMCInst& getInstruction() {
        return this->instruction;
    }
    virtual ~ReverseTranslatorUknownOpcodeException() throw() {}
};

#endif /* _REVERSE_TRANSLATOR_EXCEPTIONS_H */
