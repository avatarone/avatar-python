#include <stdint.h>
#include <iostream>

#include "lldc/SystemInformation.h"
#include "lldc/BasicBlockCache.h"
#include "lldc/GeneratedBasicBlocks.h"
#include "lldc/ReverseTranslationConfiguration.h"

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

#include "lldc/test_code.h"
#include "lldc/ArrayMemoryObject.h"
#include "lldc/util.h"

#include "lldc/PathState.h"
#include "lldc/PathsManager.h"
#include "lldc/ArmReverseTranslators.h"




#define GET_REGINFO_ENUM
#define GET_INSTRINFO_ENUM
#include "ARMBaseInfo.h"

//This is the enumeration of MC opcodes from the llvm build directory
//#define GET_INSTRINFO_ENUM
//#include "ARMGenInstrInfo.inc"


//#include "ARMGenRegisterInfo.inc"



using namespace llvm;

namespace llvm {
    namespace ARM {
        enum DW_ISA {
            DW_ISA_ARM_thumb = 1,
            DW_ISA_ARM_arm = 2
        };
    }
}


struct Armv5Cpu
{
	uint32_t regs[16];
	uint32_t cpsr;
	uint32_t banked_spsr[3];
	uint32_t banked_lr[4];
	uint32_t banked_sp[4];
	uint32_t fiq_banked_registers[7];
};

enum ArmProcessorMode
{
    ARM_PROCESSOR_MODE_USR,
    ARM_PROCESSOR_MODE_SYS,
    ARM_PROCESSOR_MODE_ABT,
    ARM_PROCESSOR_MODE_SVC,
    ARM_PROCESSOR_MODE_IRQ,
    ARM_PROCESSOR_MODE_FIQ
};

class ArmReverseTranslationContext
{
    uint64_t pc;
    ArmProcessorMode processorMode;
	ARM::DW_ISA instructionSet;
}; 

void translateInstruction(llvm::MemoryObject* memory, ReverseTranslationConfiguration* config, PathState* state, NextInstructions& next)        
{
    assert(next.getNumNextInstructions() == 0 && "NextInstructions object already contains data");
    
    const MCDisassembler* disassembler = config->getDisassembler(state);
    uint64_t size;
    MCDisassembler::DecodeStatus decodeStatus;
    ExtendedMCInst instruction;
	uint64_t pc = state->getProgramCounter();
    
    decodeStatus = disassembler->getInstruction(instruction, size, *memory, pc,
                                  /*REMOVE*/ nulls(), nulls());
    instruction.setProgramCounter(pc); 
    instruction.setSize(size);

    
    switch (decodeStatus) {
        case MCDisassembler::Fail: {
            uint32_t opcode = 0;
            memory->readByte(pc, reinterpret_cast<uint8_t *>(&opcode));
            memory->readByte(pc + 1, reinterpret_cast<uint8_t *>(&opcode) + 1);
            memory->readByte(pc + 2, reinterpret_cast<uint8_t *>(&opcode) + 2);
            memory->readByte(pc + 3, reinterpret_cast<uint8_t *>(&opcode) + 3);
            
            if ((opcode & 0xFFFFFFF0) == 0xE12FFF10) {
                //Actually BX, wrongly translated
                instruction.clear();
                instruction.setOpcode(ARM::BX);
                instruction.addOperand(MCOperand::CreateReg(ARM::LR));
                break;
            }
            errs() << "Disassembler FAIL: invalid instruction encoding" << '\n';
            //TODO: Store error somewhere
            return;
        }
        case MCDisassembler::SoftFail:
            std::cerr << "Disassembler WARN: potentially invalid instruction encoding" << std::endl;
            //TODO: Store error somewhere
            return;
        case MCDisassembler::Success:
            break;
    }
    
    ReverseInstructionTranslatorFunction instructionTranslator = config->getReverseInstructionTranslator(instruction);
    if (!instructionTranslator) {
	// 	OwningPtr<MCInstrInfo> mcInstrInfo(config->getTarget(state)->createMCInstrInfo());
	// 	assert(mcInstrInfo && "Unable to create mcInstrInfo");
	// 	OwningPtr<MCRegisterInfo> mcRegInfo(config->getTarget(state)->createMCRegInfo(config->getTarget(state)->getTriple()));
	// 	assert(mcRegInfo && "Unable to create mcRegInfo");
	// 	OwningPtr<MCSubtargetInfo>
	// 	    mcSubtargetInfo(theTarget->createMCSubtargetInfo(config->getTarget(state).getTriple(), cpu, cpuFeatures));
	// 	assert(mcSubtargetInfo && "Unable to create mcSubtargetInfo");
	// 	OwningPtr<MCAsmInfo> mcAsmInfo(theTarget->createMCAsmInfo(*mcRegInfo, config->getTarget(state).getTriple()));
	// 	assert(mcAsmInfo && "Unable to create mcAsmInfo");
	// 
	// 	OwningPtr<const MCDisassembler> disassembler(theTarget->createMCDisassembler(*mcSubtargetInfo));
	// 	assert(disassembler && "Unable to create disassembler");
	// //	OwningPtr<MCAsmBackend> mcAsmBackend(theTarget->createMCAsmBackend(theTriple.getTriple(), cpu));
	// //	assert(mcAsmBackend && "Unable to create mcAsmBackend");
	// //	OWningPtr<MCStreamer> mcStreamer(theTarget->createAsmStreamer(Ctx, FOS, /*asmverbose*/true,
	// //	                                           /*useLoc*/ true,
	// //											                                              UseCFI,
	// //																						                                             /*useDwarfDirectory*/ true,
	// //																																	                                            IP, CE, MAB, ShowInst)
	// 	OwningPtr<MCInstPrinter> mcInstPrinter(theTarget->createMCInstPrinter(outputAsmVariant, *mcAsmInfo, *mcInstrInfo, *mcRegInfo, *mcSubtargetInfo));
	// 	instruction.dump_pretty(outs(), mcAsmInfo.get(), mcInstPrinter.get());
        outs() << "No translator found for opcode " << instruction.getOpcode() << " at address " << intToHexString(state->getProgramCounter()) << '\n';
        throw("Unknown Opcode");
    }
    instructionTranslator(instruction, config, state, memory, next);
	
    if (next.endsBasicBlock()) {
		outs() << "Instruction at " << intToHexString(pc) << " in state " << intToHexString(reinterpret_cast<uint64_t>(state)) << " ends basic block" << '\n';
		state->translationFinished(config, pc + size);
	}
	else {
		outs() << "Instruction at " << intToHexString(pc) << " in state " << intToHexString(reinterpret_cast<uint64_t>(state)) << " finished" << '\n';
		state->instructionFinished(config, pc + size);
	}
}

void translateBasicBlock(llvm::MemoryObject* memory, ReverseTranslationConfiguration* config, PathState* state, NextInstructions& next) {
    do {
        next.clear();
        translateInstruction(memory, config, state, next);
//        outs() << "Translating next instruction ..." << '\n';

    } while (!next.endsBasicBlock());
}

void translateAll(llvm::MemoryObject* memory, ReverseTranslationConfiguration* config, PathState* state) {
    NextInstructions next;
    while (state) {
        translateBasicBlock(memory, config, state, next);
        state = config->getBasicBlockTranslationStateManager()->getUnfinishedPath();
    }
}

//I do not use the description from LLVM because you might want to use
//less registers than the processor actually has ... to be fixed, this
//could also be derived from the feature string

//TODO maybe change function callee into pc ...
static ReverseTranslateBasicBlock* genArmFunctionCall(ReverseTranslationConfiguration* config, PathState* state, uint64_t function_address)
{
    IRBuilder<> builder(state->getBasicBlock());
    
    //TODO: Check if function already exists 
    
    Function* call_function = cast<Function>(state->getModule()->getOrInsertFunction(intToHexString(function_address),
                                         (Type *) Type::getInt32Ty(config->getLLVMContext()),
                                         Type::getInt32Ty(config->getLLVMContext()) /* R0 */,
                                         Type::getInt32Ty(config->getLLVMContext()) /* R1 */,
                                         Type::getInt32Ty(config->getLLVMContext()) /* R2 */,
                                         Type::getInt32Ty(config->getLLVMContext()) /* R3 */,
                                         Type::getInt32Ty(config->getLLVMContext()) /* R4 */,
                                         Type::getInt32Ty(config->getLLVMContext()) /* R5 */,
                                         Type::getInt32Ty(config->getLLVMContext()) /* R6 */,
                                         Type::getInt32Ty(config->getLLVMContext()) /* R7 */,
                                         Type::getInt32Ty(config->getLLVMContext()) /* R8 */,
                                         Type::getInt32Ty(config->getLLVMContext()) /* R9 */,
                                         Type::getInt32Ty(config->getLLVMContext()) /* R10 */,
                                         Type::getInt32Ty(config->getLLVMContext()) /* R11 */,
                                         Type::getInt32Ty(config->getLLVMContext()) /* R12 */,
                                         Type::getInt32Ty(config->getLLVMContext()) /* SP */,
                                         /* TODO: Processor mode should be passed */                                                                      
                                         NULL));
    
    ReverseTranslateBasicBlock* newState = config->getBasicBlockTranslationStateManager()->
                            getOrCreateReverseTranslateBasicBlock(state, function_address);

    if (!newState->isTranslationFinished())
    {
        PathState* trans_state = static_cast<PathState*>(newState);
        
        Function::arg_iterator args = call_function->arg_begin();
        args->setName("R0"); trans_state->setRegister(ARM::R0, args);
        args++; args->setName("R1"); trans_state->setRegister(ARM::R1, args);
        args++; args->setName("R2"); trans_state->setRegister(ARM::R2, args);
        args++; args->setName("R3"); trans_state->setRegister(ARM::R3, args);
        args++; args->setName("R4"); trans_state->setRegister(ARM::R4, args);
        args++; args->setName("R5"); trans_state->setRegister(ARM::R5, args);
        args++; args->setName("R6"); trans_state->setRegister(ARM::R6, args);
        args++; args->setName("R7"); trans_state->setRegister(ARM::R7, args);
        args++; args->setName("R8"); trans_state->setRegister(ARM::R8, args);
        args++; args->setName("R9"); trans_state->setRegister(ARM::R9, args);
        args++; args->setName("R10"); trans_state->setRegister(ARM::R10, args);
        args++; args->setName("R11"); trans_state->setRegister(ARM::R11, args);
        args++; args->setName("R12"); trans_state->setRegister(ARM::R12, args);
        args++; args->setName("SP"); trans_state->setRegister(ARM::SP, args);
    }

    SmallVector<Value*, 14> params;
    params.push_back(state->getRegister(ARM::R0));
    params.push_back(state->getRegister(ARM::R1));
    params.push_back(state->getRegister(ARM::R2));
    params.push_back(state->getRegister(ARM::R3));
    params.push_back(state->getRegister(ARM::R4));
    params.push_back(state->getRegister(ARM::R5));
    params.push_back(state->getRegister(ARM::R6));
    params.push_back(state->getRegister(ARM::R7));
    params.push_back(state->getRegister(ARM::R8));
    params.push_back(state->getRegister(ARM::R9));
    params.push_back(state->getRegister(ARM::R10));
    params.push_back(state->getRegister(ARM::R11));
    params.push_back(state->getRegister(ARM::R12));
    params.push_back(state->getRegister(ARM::SP));

    Value* returnValue = builder.CreateCall(call_function, params, "R0.");
    state->setRegister(ARM::R0, returnValue);
 
    return newState;
}

//TODO: Pass generated wrapper function to the outside world
ReverseTranslateBasicBlock* genEntryFunctionCall(ReverseTranslationConfiguration* config, uint64_t pc)
{ 
    StructType * processorStructType = config->getSystemInformation()->getProcessorStructTy();
    StructType * instrumentationStructType = config->getInstrumentationStructTy();
    PointerType * processorStructPtrType = PointerType::get(processorStructType, 0);
    PointerType * instrumentationStructPtrType = PointerType::get(instrumentationStructType, 0);
    llvm::Module* module = new llvm::Module("mymodule", config->getLLVMContext());
    
    //TODO: Generate random name for function
    Function* wrapper_function = cast<Function>(module->getOrInsertFunction("wrapper",
                                         (Type *) Type::getVoidTy(config->getLLVMContext()),
                                         processorStructPtrType,
                                         instrumentationStructPtrType,
                                         NULL));
                                         
    //Build entry block
    BasicBlock * entry = BasicBlock::Create(config->getLLVMContext(), "entry", wrapper_function);
    PathState state(entry, module);
    IRBuilder<> builder(entry);
    
    Function::arg_iterator args = wrapper_function->arg_begin();
    Value* processor_registers = args++; processor_registers->setName("processor_registers");
    Value* instrumentation_functions = args++; instrumentation_functions->setName("instrumentation_functions");
    
    for (SystemInformation::register_iterator itr = config->getSystemInformation()->register_description_begin();
         itr != config->getSystemInformation()->register_description_end();
         itr++)
    {
//        outs() << "Getting register " << itr->getName() << "\n";
        
        //TODO: This should not be int32, but whatever the cpu's register width is
        Value* elementPtr = builder.CreateConstGEP2_32(processor_registers, 0, itr->getIndex(), itr->getName());
        LoadInst* value = builder.CreateLoad(elementPtr, itr->getName());
        
        state.setRegister(itr->getRegNum(), value);
    }
    
    ReverseTranslateBasicBlock* newState = genArmFunctionCall(config, &state, pc);
    
    for (SystemInformation::register_iterator itr = config->getSystemInformation()->register_description_begin();
         itr != config->getSystemInformation()->register_description_end();
         itr++)
    {
        Value* elementPtr = builder.CreateConstGEP2_32(processor_registers, 0, itr->getIndex(), itr->getName());
        builder.CreateStore(state.getRegister(itr->getRegNum()), elementPtr);
    }

    return newState;
}
    
    


// int main(int argc, char ** argv)
// {
// 
// 	//Configurable variables
// 	std::string architecture("thumb");
//     std::string arm_architecture("arm");
// 	std::string cpu("");
// 	std::string cpuFeatures("");
// 	unsigned outputAsmVariant = 0;
// 	Triple theTriple(Triple::normalize(""));
//     const size_t MEMORY_SIZE = 100 * 1024 * 1024;
// 
// 	//Program variables
// 	ArrayMemoryObject ram(0, MEMORY_SIZE);
// 
//     //Populate memory
//     for (size_t i = 0; i < memory_data_entries; i++)
//     {
//         if (memory_data[i].address + memory_data[i].length < MEMORY_SIZE)
//             memcpy(&ram.getArray()[memory_data[i].address], memory_data[i].code, memory_data[i].length);
//     }
// 
//     // Initialize targets and assembly printers/parsers.
//     llvm::InitializeAllTargetInfos();
//     llvm::InitializeAllTargetMCs();
// //    llvm::InitializeAllAsmParsers();
//     llvm::InitializeAllDisassemblers();
// 
// 	std::string error;
// 	const Target * thumbTarget = TargetRegistry::lookupTarget(architecture, theTriple, error);
// 	if (!thumbTarget) {
// 		std::cerr << "ERROR: " << error << std::endl;
// 		return 1;
// 	}
// 	OwningPtr<MCInstrInfo> thumbInstrInfo(thumbTarget->createMCInstrInfo());
// 	assert(thumbInstrInfo && "Unable to create mcInstrInfo");
// 	OwningPtr<MCRegisterInfo> thumbRegInfo(thumbTarget->createMCRegInfo(theTriple.getTriple()));
// 	assert(thumbRegInfo && "Unable to create mcRegInfo");
// 	OwningPtr<MCSubtargetInfo>
// 	    thumbSubtarget(thumbTarget->createMCSubtargetInfo(theTriple.getTriple(), cpu, cpuFeatures));
// 	assert(thumbSubtarget && "Unable to create mcSubtargetInfo");
// 	OwningPtr<MCAsmInfo> thumbAsmInfo(thumbTarget->createMCAsmInfo(*thumbRegInfo, theTriple.getTriple()));
// 	assert(thumbAsmInfo && "Unable to create mcAsmInfo");
// 	OwningPtr<const MCDisassembler> thumbDisassembler(thumbTarget->createMCDisassembler(*thumbSubtarget));
// 	assert(thumbDisassembler && "Unable to create disassembler");
// 	
// 	const Target * armTarget = TargetRegistry::lookupTarget(arm_architecture, theTriple, error);
// 	if (!armTarget) {
// 		std::cerr << "ERROR: " << error << std::endl;
// 		return 1;
// 	}
// 
// 	OwningPtr<MCInstrInfo> armInstrInfo(armTarget->createMCInstrInfo());
// 	assert(armInstrInfo && "Unable to create mcInstrInfo");
// 	OwningPtr<MCRegisterInfo> armRegInfo(armTarget->createMCRegInfo(theTriple.getTriple()));
// 	assert(armRegInfo && "Unable to create mcRegInfo");
// 	OwningPtr<MCSubtargetInfo>
// 	    armSubtarget(armTarget->createMCSubtargetInfo(theTriple.getTriple(), cpu, cpuFeatures));
// 	assert(armSubtarget && "Unable to create mcSubtargetInfo");
// 	OwningPtr<MCAsmInfo> armAsmInfo(armTarget->createMCAsmInfo(*armRegInfo, theTriple.getTriple()));
// 	assert(armAsmInfo && "Unable to create mcAsmInfo");
// 	OwningPtr<const MCDisassembler> armDisassembler(armTarget->createMCDisassembler(*armSubtarget));
// 	assert(armDisassembler && "Unable to create disassembler");
// //	OwningPtr<MCAsmBackend> mcAsmBackend(theTarget->createMCAsmBackend(theTriple.getTriple(), cpu));
// //	assert(mcAsmBackend && "Unable to create mcAsmBackend");
// //	OWningPtr<MCStreamer> mcStreamer(theTarget->createAsmStreamer(Ctx, FOS, /*asmverbose*/true,
// //	                                           /*useLoc*/ true,
// //											                                              UseCFI,
// //																						                                             /*useDwarfDirectory*/ true,
// //																																	                                            IP, CE, MAB, ShowInst)
// 	OwningPtr<MCInstPrinter> thumbInstPrinter(thumbTarget->createMCInstPrinter(outputAsmVariant, *thumbAsmInfo, *thumbInstrInfo, *thumbRegInfo, *thumbSubtarget));
// 	MCDisassembler::DecodeStatus decodeStatus;
// //	MCInst instruction;
// 	uint64_t size;
// 	uint64_t index = 0x102884;
//     
//     for (int i = 0; i < 30; i++)
//     {
//         MCInst instruction;
// 	decodeStatus = thumbDisassembler->getInstruction(instruction, size, ram, index,
// 	                              /*REMOVE*/ nulls(), nulls());
// 
// 	switch (decodeStatus) {
// 		case MCDisassembler::Fail:
// 			std::cerr << "Disassembler FAIL: invalid instruction encoding" << std::endl;
// 			break;
// 		case MCDisassembler::SoftFail:
// 			std::cerr << "Disassembler WARN: potentially invalid instruction encoding" << std::endl;
// 			break;
// 		case MCDisassembler::Success:
// 			instruction.dump_pretty(outs(), thumbAsmInfo.get(), thumbInstPrinter.get());
// 			outs() << '\n';
// 			break;
// 	}
// 	
// 	index += size;
//     }
//     
//     std::vector<ProcessorRegisterDescription> registerDescription;
//     registerDescription.push_back(ProcessorRegisterDescription("R0.", Type::getInt32Ty(getGlobalContext()), 0, ARM::R0));
//     registerDescription.push_back(ProcessorRegisterDescription("R1.", Type::getInt32Ty(getGlobalContext()), 1, ARM::R1));
//     registerDescription.push_back(ProcessorRegisterDescription("R2.", Type::getInt32Ty(getGlobalContext()), 2, ARM::R2));
//     registerDescription.push_back(ProcessorRegisterDescription("R3.", Type::getInt32Ty(getGlobalContext()), 3, ARM::R3));
//     registerDescription.push_back(ProcessorRegisterDescription("R4.", Type::getInt32Ty(getGlobalContext()), 4, ARM::R4));
//     registerDescription.push_back(ProcessorRegisterDescription("R5.", Type::getInt32Ty(getGlobalContext()), 5, ARM::R5));
//     registerDescription.push_back(ProcessorRegisterDescription("R6.", Type::getInt32Ty(getGlobalContext()), 6, ARM::R6));
//     registerDescription.push_back(ProcessorRegisterDescription("R7.", Type::getInt32Ty(getGlobalContext()), 7, ARM::R7));
//     registerDescription.push_back(ProcessorRegisterDescription("R8.", Type::getInt32Ty(getGlobalContext()), 8, ARM::R8));
//     registerDescription.push_back(ProcessorRegisterDescription("R9.", Type::getInt32Ty(getGlobalContext()), 9, ARM::R9));
//     registerDescription.push_back(ProcessorRegisterDescription("R10.", Type::getInt32Ty(getGlobalContext()), 10, ARM::R10));
//     registerDescription.push_back(ProcessorRegisterDescription("R11.", Type::getInt32Ty(getGlobalContext()), 11, ARM::R11));
//     registerDescription.push_back(ProcessorRegisterDescription("R12.", Type::getInt32Ty(getGlobalContext()), 12, ARM::R12));
//     registerDescription.push_back(ProcessorRegisterDescription("SP.", Type::getInt32Ty(getGlobalContext()), 13, ARM::SP));
//     registerDescription.push_back(ProcessorRegisterDescription("LR.", Type::getInt32Ty(getGlobalContext()), 14, ARM::LR));
//     registerDescription.push_back(ProcessorRegisterDescription("PC.", Type::getInt32Ty(getGlobalContext()), 15, ARM::PC));
//     registerDescription.push_back(ProcessorRegisterDescription("CPSR.", Type::getInt32Ty(getGlobalContext()), 16, ARM::CPSR));
//     SystemInformation* sysInfo = new SystemInformation(registerDescription);
//     BasicBlockCache* basicBlockCache = new BasicBlockCache(getGlobalContext());
//     PathsManager* mgr = new PathsManager(basicBlockCache);
//     
//     ReverseTranslationConfiguration* config  = new ReverseTranslationConfiguration(getGlobalContext(), thumbDisassembler.get(), armDisassembler.get(), thumbTarget, StructType::create(getGlobalContext()), sysInfo, mgr);
//     //TODO: Set thumb mode
//     getArmReverseTranslators(config->getInstructionTranslators());
//     
//     outs() << "=========================\n";
//     
//     ReverseTranslateBasicBlock* state = genEntryFunctionCall(config, 0x650);
//     std::list<ReverseTranslateBasicBlock*> states;
//     states.push_back(state);
//     std::list<ReverseTranslateBasicBlock*> doneStates;
//     
//     NextInstructions nextInstructions;
//     try
//     {
//         translateAll(&ram, config, static_cast<PathState*>(state));
// //         while (!states.empty())
// //         {
// //             state = states.back();
// //             states.pop_back();
// //             doneStates.push_back(state);
// //             translateBasicBlock(&ram, config, static_cast<PathState*>(state), nextInstructions);
// //             
// //             for (unsigned i = 0; i < nextInstructions.getNumNextInstructions(); i++) {
// //                 if (!nextInstructions.getBasicBlock(i)->isTranslationFinished() &&
// //                     (nextInstructions.getFlowType(i) == NextInstructions::FLOW_NORMAL || 
// //                      nextInstructions.getFlowType(i) == NextInstructions::FLOW_BRANCH ||
// //                      nextInstructions.getFlowType(i) == NextInstructions::FLOW_CONDITIONAL_BRANCH))  
// //                 {
// //                     if (std::find(doneStates.begin(), doneStates.end(), nextInstructions.getBasicBlock(i))==doneStates.end())
// //                         states.push_back(nextInstructions.getBasicBlock(i));
// //                 }
// //             }
// //         }
//     }
//     catch(ReverseTranslatorUknownOpcodeException ex) {
//         outs() << "Unknown opcode at address " << intToHexString(ex.getInstruction().getProgramCounter()) << "!!!!!!!" << '\n';
//         ex.getInstruction().dump_pretty(outs(), thumbAsmInfo.get(), thumbInstPrinter.get());
//             outs() << '\n';
//     }
//     catch (...) {
//         outs() << "Other exception" << '\n';
//     }
//   
//     PassManager PM;
//     PM.add(createPrintModulePass(&outs()));
//     PM.run(*static_cast<PathState*>(state)->getModule());
//     
//  //   reverseTranslationContext.currentBasicBlock->dump();
// 
//     
//     
// 
//     return 0;
// }
