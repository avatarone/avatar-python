#include "lldc/c_interface.h"

#include <cstring>
#include <cassert>

#include <string>
#include <iostream>

#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/ADT/OwningPtr.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCInstPrinter.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCObjectFileInfo.h"
#include "llvm/MC/MCParser/AsmLexer.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSectionMachO.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCDisassembler.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCTargetAsmParser.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileUtilities.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/ToolOutputFile.h"
#include  "llvm/Support/MemoryObject.h"

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"

#include "llvm/PassManager.h"
#include "llvm/Assembly/PrintModulePass.h"

#include "lldc/ProxyMemoryObject.h"
#include "lldc/SystemInformation.h"
#include "lldc/BasicBlockCache.h"
#include "lldc/GeneratedBasicBlocks.h"
#include "lldc/ReverseTranslationConfiguration.h"
#include "lldc/util.h"
#include "lldc/PathState.h"
#include "lldc/PathsManager.h"
#include "lldc/ArmReverseTranslators.h"
#include "lldc/InstructionDecoder.h"

#define GET_REGINFO_ENUM
#define GET_INSTRINFO_ENUM
#include "ARMBaseInfo.h"


using namespace llvm;

int instrument_memory_access(const char * architecture,
                             uint64_t entry_point,
                             ProgramCounterRange * pc_ranges,
                             uint64_t generated_code_address,
                             ReadCodeMemoryFunc get_code_callback,
                             void *opaque,
                             GeneratedCode * generated_code,
                             DictionaryElement * opts)
{
    std::map<std::string, std::string> cxx_opts;

    for (; opts != NULL && opts->key != NULL; opts++)
    {
        cxx_opts.insert(std::make_pair(opts->key, opts->value));
    }
    //TODO: Address to link to not taken into account ... seems to be a bit more tricky, requires changes of the RuntimeDyld ...
    //TODO: opts not taken into account, should pass processor type etc through this
    std::list<ProcessorRegisterDescription> registerDescription;
    registerDescription.push_back(ProcessorRegisterDescription("R0.", Type::getInt32Ty(getGlobalContext()), 0, ARM::R0));
    registerDescription.push_back(ProcessorRegisterDescription("R1.", Type::getInt32Ty(getGlobalContext()), 1, ARM::R1));
    registerDescription.push_back(ProcessorRegisterDescription("R2.", Type::getInt32Ty(getGlobalContext()), 2, ARM::R2));
    registerDescription.push_back(ProcessorRegisterDescription("R3.", Type::getInt32Ty(getGlobalContext()), 3, ARM::R3));
    registerDescription.push_back(ProcessorRegisterDescription("R4.", Type::getInt32Ty(getGlobalContext()), 4, ARM::R4));
    registerDescription.push_back(ProcessorRegisterDescription("R5.", Type::getInt32Ty(getGlobalContext()), 5, ARM::R5));
    registerDescription.push_back(ProcessorRegisterDescription("R6.", Type::getInt32Ty(getGlobalContext()), 6, ARM::R6));
    registerDescription.push_back(ProcessorRegisterDescription("R7.", Type::getInt32Ty(getGlobalContext()), 7, ARM::R7));
    registerDescription.push_back(ProcessorRegisterDescription("R8.", Type::getInt32Ty(getGlobalContext()), 8, ARM::R8));
    registerDescription.push_back(ProcessorRegisterDescription("R9.", Type::getInt32Ty(getGlobalContext()), 9, ARM::R9));
    registerDescription.push_back(ProcessorRegisterDescription("R10.", Type::getInt32Ty(getGlobalContext()), 10, ARM::R10));
    registerDescription.push_back(ProcessorRegisterDescription("R11.", Type::getInt32Ty(getGlobalContext()), 11, ARM::R11));
    registerDescription.push_back(ProcessorRegisterDescription("R12.", Type::getInt32Ty(getGlobalContext()), 12, ARM::R12));
    registerDescription.push_back(ProcessorRegisterDescription("SP.", Type::getInt32Ty(getGlobalContext()), 13, ARM::SP));
    registerDescription.push_back(ProcessorRegisterDescription("LR.", Type::getInt32Ty(getGlobalContext()), 14, ARM::LR));
//    registerDescription.push_back(ProcessorRegisterDescription("PC.", Type::getInt32Ty(getGlobalContext()), 15, ARM::PC));
    registerDescription.push_back(ProcessorRegisterDescription("CPSR.", Type::getInt32Ty(getGlobalContext()), 15, ARM::CPSR));
    
    BinaryTranslator instructionDecoder(architecture, new ProxyMemoryObject(get_code_callback, opaque), registerDescription, entry_point);
    
    if (cxx_opts.find("debug") != cxx_opts.end())
        instructionDecoder.setDebug(true);

    std::list<std::pair<uint64_t, uint64_t> > list_pc_ranges;
    for (unsigned i = 0; pc_ranges[i].end != 0; i++) {
        list_pc_ranges.push_back(std::make_pair(pc_ranges[i].start, pc_ranges[i].end));
    }
    Ranges cxx_pc_ranges(list_pc_ranges);
    instructionDecoder.translate(cxx_pc_ranges);

    if (cxx_opts.find("print_ir") != cxx_opts.end())
        instructionDecoder.dumpIR();

        
//    instructionDecoder.dumpIR();
    if (cxx_opts.find("instrument_memory_access") != cxx_opts.end())
        instructionDecoder.instrumentMemoryAccess();
//    outs() << "TADAAAAAAA: ========================================================================================" << '\n';
//    instructionDecoder.dumpIR();
    CompiledCode* compiled_code = instructionDecoder.compile();
    
    if (generated_code)
    {
        generated_code->code = (char *) malloc(compiled_code->getSize());
        memcpy(generated_code->code, compiled_code->getData(), compiled_code->getSize());
        generated_code->size = compiled_code->getSize();
        generated_code->address = compiled_code->getAddress();
    }
//     ReverseTranslateBasicBlock* state = instructionDecoder.genCodeEntry(entry_point);
// //    ReverseTranslateBasicBlock* state = genEntryFunctionCall(config, 0x650);
//     std::list<ReverseTranslateBasicBlock*> states;
//     states.push_back(state);
//     std::list<ReverseTranslateBasicBlock*> doneStates;
//     
//     NextInstructions nextInstructions;
//     try
//     {
//         instructionDecoder.translate(entry_point, pc_ranges);
// //        translateAll(&ram, config, static_cast<PathState*>(state));
//     }
//     catch(ReverseTranslatorUknownOpcodeException ex) {
//         outs() << "Unknown opcode at address " << intToHexString(ex.getInstruction().getProgramCounter()) << "!!!!!!!" << '\n';
// //         ex.getInstruction().dump_pretty(outs(), thumbAsmInfo.get(), thumbInstPrinter.get());
// //             outs() << '\n';
//     }
//     catch (...) {
//         outs() << "Other exception" << '\n';
//     }
//   
    
 //   reverseTranslationContext.currentBasicBlock->dump();

    
    

    return 0;
}
                             
                             

    



