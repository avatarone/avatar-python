#include "lldc/InstructionDecoder.h"
#include "lldc/PathState.h"
#include "lldc/PathsManager.h"
#include "lldc/ArmReverseTranslators.h"
#include "lldc/InstructionTranslationUnitCache.h"

#include "llvm/IR/DerivedTypes.h"
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

#include <sstream>
#include <fstream>

#include "lldc/LLVMArmRegisters.h"
#include "lldc/ReverseTranslatorExceptions.h"
#include "lldc/RecordingMemoryManager.h"
#include "lldc/InstrumentMemoryAccessPass.h"

#include "llvm/Target/TargetOptions.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/JIT.h"
#include "llvm/ExecutionEngine/MCJIT.h"
#include <llvm/Analysis/Verifier.h>



using namespace llvm;

void BinaryTranslator::translateSingleInstruction(UnfinishedInstructionTranslationUnit* unit)        
{
    const MCDisassembler* disassembler = this->config->getDisassembler(unit->getTranslationUnit().getInstructionSet());
    uint64_t size;
    MCDisassembler::DecodeStatus decodeStatus;
    ExtendedMCInst instruction;
    uint64_t pc = unit->getTranslationUnit().getProgramCounter();
    
    decodeStatus = disassembler->getInstruction(instruction, size, *this->memory, pc,
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
            throw DisassemblerException(pc);
        }
        case MCDisassembler::SoftFail:
            
            errs() << "Disassembler WARN: potentially invalid instruction encoding" << '\n';
            //TODO: Store error somewhere
            return;
        case MCDisassembler::Success:
            break;
    }
    
    ReverseInstructionTranslatorFunction instructionTranslator = config->getReverseInstructionTranslator(instruction);
    if (!instructionTranslator) {
        std::stringstream ss;
        ss << "No translator found for opcode " << instruction.getOpcode() << " at address " << intToHexString(pc);
        throw Exception(ss.str());
    }
    
    instructionTranslator(instruction, *config, unit, memory);
    
    delete unit;
}

BinaryTranslator::BinaryTranslator(std::string architecture, MemoryObject* memory, std::list<ProcessorRegisterDescription>& registerDescription, uint64_t entry_pc, StringRef module_name) 
    : memory(memory),
      is_debug(false)
{
    assert((architecture == "arm" || architecture == "thumb") && "Only ARM ond thumb architecture currently implemented");
    
    LLVMContext& context = getGlobalContext();
    
    //Configurable variables
    std::string arch("thumb");
    std::string arm_arch("arm");
    std::string cpu("");
    std::string cpuFeatures("");
    Triple theTriple(Triple::normalize(""));

    // Initialize targets and assembly printers/parsers.
    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllDisassemblers();

    std::string error;
    thumbTarget = TargetRegistry::lookupTarget(arch, theTriple, error);
    if (!thumbTarget) {
        throw Exception(error);
    }
    thumbInstrInfo = thumbTarget->createMCInstrInfo();
    assert(thumbInstrInfo && "Unable to create mcInstrInfo");
    thumbRegInfo = thumbTarget->createMCRegInfo(theTriple.getTriple());
    assert(thumbRegInfo && "Unable to create mcRegInfo");
    thumbSubtarget = thumbTarget->createMCSubtargetInfo(theTriple.getTriple(), cpu, cpuFeatures);
    assert(thumbSubtarget && "Unable to create mcSubtargetInfo");
    thumbAsmInfo = thumbTarget->createMCAsmInfo(*thumbRegInfo, theTriple.getTriple());
    assert(thumbAsmInfo && "Unable to create mcAsmInfo");
    thumbDisassembler = thumbTarget->createMCDisassembler(*thumbSubtarget);
    assert(thumbDisassembler && "Unable to create disassembler");
    
    const Target * armTarget = TargetRegistry::lookupTarget(arm_arch, theTriple, error);
    if (!armTarget) {
        throw Exception(error);
    }

    armInstrInfo = armTarget->createMCInstrInfo();
    assert(armInstrInfo && "Unable to create mcInstrInfo");
    armRegInfo = armTarget->createMCRegInfo(theTriple.getTriple());
    assert(armRegInfo && "Unable to create mcRegInfo");
    armSubtarget = armTarget->createMCSubtargetInfo(theTriple.getTriple(), cpu, cpuFeatures);
    assert(armSubtarget && "Unable to create mcSubtargetInfo");
    armAsmInfo = armTarget->createMCAsmInfo(*armRegInfo, theTriple.getTriple());
    assert(armAsmInfo && "Unable to create mcAsmInfo");
    armDisassembler = armTarget->createMCDisassembler(*armSubtarget);
    assert(armDisassembler && "Unable to create disassembler");

//    thumbInstPrinter(thumbTarget->createMCInstPrinter(outputAsmVariant, *thumbAsmInfo, *thumbInstrInfo, *thumbRegInfo, *thumbSubtarget));
    
    SystemInformation *sysInfo = new SystemInformation(registerDescription);
    PathsManager *mgr = new PathsManager(new BasicBlockCache(getGlobalContext()));
    
    //TODO: Instrumentation struct should not be defined here ...
    SmallVector<Type*, 2> read_handler_args;
    read_handler_args.push_back(PointerType::get(Type::getInt8Ty(context), 0));
    read_handler_args.push_back(Type::getInt32Ty(context));
    SmallVector<Type*, 3> write_handler_args;
    write_handler_args.push_back(PointerType::get(Type::getInt8Ty(context), 0));
    write_handler_args.push_back(Type::getInt32Ty(context));
    write_handler_args.push_back(Type::getInt32Ty(context));
    SmallVector<Type*, 2> instrumentation_funcs;
    instrumentation_funcs.push_back(PointerType::get(FunctionType::get(Type::getInt32Ty(context), read_handler_args, false), 0));
    instrumentation_funcs.push_back(PointerType::get(FunctionType::get(Type::getVoidTy(context), write_handler_args, false), 0));
//    instrumentation_funcs.push_back(FunctionType::get(Type::getInt32Ty(context), read_handler_args, false));
//    instrumentation_funcs.push_back(FunctionType::get(Type::getVoidTy(context), write_handler_args, false));
    StructType* instrumentation_struct_type = StructType::create(instrumentation_funcs, "instrumentation_funcs", true);
    
    config = new ReverseTranslationConfiguration(context, thumbDisassembler, armDisassembler, thumbTarget, instrumentation_struct_type, sysInfo, mgr) ;
    //TODO: Set thumb mode
    getArmReverseTranslators(config->getInstructionTranslators());
    
    this->module = new llvm::Module(module_name, context);
    this->entry_pc = entry_pc;
    genCodeEntry(entry_pc);
}


/**
 * Reads the CPSR register and extracts the condition flags from it.
 */
static void decode_cpsr_to_flags(BasicBlock* bb, std::map< unsigned, Value* >& values)
{
    assert(values.find(ARM::CPSR) != values.end() && "No CPSR register in values");
    IRBuilder<> builder(bb);
    
    Value* cpsr = values[ARM::CPSR];
    values.insert(std::make_pair(ARM::FLAG_Z, builder.CreateICmpNE(
        builder.CreateAnd(builder.CreateLShr(
            cpsr, builder.getInt32(30)), builder.getInt32(1)), builder.getInt32(0), "flag_z.")));
    values.insert(std::make_pair(ARM::FLAG_N, builder.CreateICmpNE(
        builder.CreateAnd(builder.CreateLShr(
            cpsr, builder.getInt32(31)), builder.getInt32(1)), builder.getInt32(0), "flag_n.")));
    values.insert(std::make_pair(ARM::FLAG_C, builder.CreateICmpNE(
        builder.CreateAnd(builder.CreateLShr(
            cpsr, builder.getInt32(29)), builder.getInt32(1)), builder.getInt32(0), "flag_c.")));
    values.insert(std::make_pair(ARM::FLAG_V, builder.CreateICmpNE(
        builder.CreateAnd(builder.CreateLShr(
            cpsr, builder.getInt32(28)), builder.getInt32(1)), builder.getInt32(0), "flag_v.")));
}

static void encode_flags_to_cpsr(LLVMContext& context, BasicBlock* bb, std::map< unsigned, Value* >& values)
{
    assert(values.find(ARM::CPSR) != values.end() && "No CPSR register in values");
    assert(values.find(ARM::FLAG_Z) != values.end() && "No Z flag in values");
    assert(values.find(ARM::FLAG_N) != values.end() && "No N flag in values");
    assert(values.find(ARM::FLAG_C) != values.end() && "No C flag in values");
    assert(values.find(ARM::FLAG_V) != values.end() && "No V flag in values");
    IRBuilder<> builder(bb);
    
    Value* flags = builder.CreateOr(
        builder.CreateOr(
            builder.CreateShl(
                builder.CreateIntCast(values[ARM::FLAG_Z], Type::getInt32Ty(context), false), 
                builder.getInt32(30)),
            builder.CreateShl(
                builder.CreateIntCast(values[ARM::FLAG_N], Type::getInt32Ty(context), false), 
                builder.getInt32(31))),
        builder.CreateOr(
            builder.CreateShl(
                builder.CreateIntCast(values[ARM::FLAG_C], Type::getInt32Ty(context), false), 
                builder.getInt32(29)),
            builder.CreateShl(
                builder.CreateIntCast(values[ARM::FLAG_V], Type::getInt32Ty(context), false), 
                builder.getInt32(28))));
    
    
    values[ARM::CPSR] = builder.CreateOr(builder.CreateAnd(values[ARM::CPSR], builder.getInt32(0x0FFFFFFF)), flags);
}
    
    
    
    
    

//TODO: Pass generated wrapper function to the outside world
void BinaryTranslator::genCodeEntry(uint64_t pc)
{ 
    StructType * processorStructType = this->config->getSystemInformation()->getProcessorStructTy();
    StructType * instrumentationStructType = this->config->getInstrumentationStructTy();
    PointerType * processorStructPtrType = PointerType::get(processorStructType, 0);
    PointerType * instrumentationStructPtrType = PointerType::get(instrumentationStructType, 0);
    
    //TODO: Generate random name for function
    wrapper_function = cast<Function>(module->getOrInsertFunction("wrapper",
                                         (Type *) Type::getVoidTy(this->config->getLLVMContext()),
                                         processorStructPtrType,
                                         instrumentationStructPtrType,
                                         NULL));
                                         
    //Build entry block
    BasicBlock * entry_bb = BasicBlock::Create(this->config->getLLVMContext(), "entry", wrapper_function);
//    PathState state(entry, module);
    std::map< unsigned, Value* > entry_values;
    IRBuilder<> builder(entry_bb);
    
    Function::arg_iterator args = wrapper_function->arg_begin();
    Value* processor_registers = args++; processor_registers->setName("processor_registers");
    Value* instrumentation_functions = args++; instrumentation_functions->setName("instrumentation_functions");
    
    instrumentationReadHandler = builder.CreateLoad(builder.CreateConstGEP2_32(instrumentation_functions, 0, 0), "read_handler_func");
    instrumentationWriteHandler = builder.CreateLoad(builder.CreateConstGEP2_32(instrumentation_functions, 0, 1), "write_handler_func");
    
    for (SystemInformation::register_iterator itr = this->config->getSystemInformation()->register_description_begin();
         itr != this->config->getSystemInformation()->register_description_end();
         itr++)
    {
//        outs() << "Loading register " << itr->getName() << " with regnum " << itr->getRegNum() << " idx " << itr->getIndex() << '\n';
        //TODO: This should not be int32, but whatever the cpu's register width is
        Value* elementPtr = builder.CreateConstGEP2_32(processor_registers, 0, itr->getIndex(), itr->getName());
        LoadInst* value = builder.CreateLoad(elementPtr, itr->getName());
        
        entry_values.insert(std::make_pair(itr->getRegNum(), value));
//        state.setRegister(itr->getRegNum(), value);
    }
    
    decode_cpsr_to_flags(entry_bb, entry_values);
    
    InstructionTranslationUnitCache::getInstance().find_create(*this->config, wrapper_function, pc, entry_values, entry_bb);                          
    //TODO: It would be nice to insert a check that the processor's T flag corresponds to the instruction set that we use to decompile the code ...
    //The translated function would need a mechanism to report errors to the caller.
    //TODO: Check that the PC from the entry structure corresponds to the PC we expect
}

void BinaryTranslator::genCodeExit(UnfinishedInstructionTranslationUnit* unit)
{ 
//    StructType * processorStructType = this->config->getSystemInformation()->getProcessorStructTy();
//    StructType * instrumentationStructType = this->config->getInstrumentationStructTy();
//    PointerType * processorStructPtrType = PointerType::get(processorStructType, 0);
//    PointerType * instrumentationStructPtrType = PointerType::get(instrumentationStructType, 0);
    
    IRBuilder<> builder(unit->getTranslationUnit().getHead());
    
    encode_flags_to_cpsr(this->config->getLLVMContext(), unit->getTranslationUnit().getHead(), unit->getValues());

    Function::arg_iterator args = wrapper_function->arg_begin();
    Value* processor_registers = args++; processor_registers->setName("processor_registers");
    Value* instrumentation_functions = args++; instrumentation_functions->setName("instrumentation_functions");
    
    for (SystemInformation::register_iterator itr = this->config->getSystemInformation()->register_description_begin();
         itr != this->config->getSystemInformation()->register_description_end();
         itr++)
    {
        //TODO: This should not be int32, but whatever the cpu's register width is
        Value* elementPtr = builder.CreateConstGEP2_32(processor_registers, 0, itr->getIndex(), itr->getName());
        builder.CreateStore(unit->getValues()[itr->getRegNum()], elementPtr);
    }
    
    //TODO: Write exit PC to structure
    
    builder.CreateRetVoid();
}

void BinaryTranslator::translate(Ranges& pc_ranges)
{ 
    assert(pc_ranges.is_in_range(this->entry_pc) && "Entry PC is not in PC ranges");
    
    

    try
    {
        while (InstructionTranslationUnitCache::getInstance().has_more_unfinished_translation_units()) {
            UnfinishedInstructionTranslationUnit* unit = InstructionTranslationUnitCache::getInstance().get_next_unfinished_translation_unit();
            
            if (!pc_ranges.is_in_range(unit->getTranslationUnit().getProgramCounter()))
            {
                genCodeExit(unit);
            }
            else
            {
                translateSingleInstruction(unit);
            }
        }
    }
    catch(ReverseTranslatorUknownOpcodeException ex) {
        Triple theTriple(Triple::normalize(""));
        unsigned outputAsmVariant = 0;
        MCAsmInfo* thumbAsmInfo = thumbTarget->createMCAsmInfo(*thumbRegInfo, theTriple.getTriple());
        MCInstPrinter* thumbInstPrinter = thumbTarget->createMCInstPrinter(outputAsmVariant, *thumbAsmInfo, *thumbInstrInfo, *thumbRegInfo, *thumbSubtarget);
        outs() << "Unknown opcode at address " << intToHexString(ex.getInstruction().getProgramCounter()) << "!!!!!!!" << '\n';
        ex.getInstruction().dump_pretty(outs(), thumbAsmInfo, thumbInstPrinter);
             outs() << '\n';
        delete thumbInstPrinter;
        delete thumbAsmInfo;
             
    }
    catch (...) {
        outs() << "Other exception" << '\n';
    }
}

void BinaryTranslator::dumpIR(raw_ostream* out)
{
    PassManager PM;
    
    if (out == NULL)
        out = &outs();
    PM.add(createPrintModulePass(out));
    PM.run(*this->module);
}

CompiledCode* BinaryTranslator::compile()
{
    std::string errorDescription;
    TargetOptions targetOptions;
    
    InitializeAllTargets(); 
    InitializeAllTargetMCs();
    InitializeAllAsmPrinters();
    
    verifyModule(*module, PrintMessageAction);
  
    targetOptions.PrintMachineCode = 0;

    if (this->is_debug)
        targetOptions.PrintMachineCode = 1;
    //  targetOptions.PositionIndependentExecutable = 1; //TODO: PIC not yet supported by MCJIT
    targetOptions.PositionIndependentExecutable = 0;
  
  
    RecordingMemoryManager* memoryManager = new RecordingMemoryManager();
    ExecutionEngine* engine = EngineBuilder(module)
        .setEngineKind(EngineKind::JIT)
        .setUseMCJIT(true)
        .setMCJITMemoryManager(memoryManager)
        .setErrorStr(&errorDescription)
        .setCodeModel(CodeModel::Small)
//        .setOptLevel(CodeGenOpt::Default)
         .setOptLevel(CodeGenOpt::Aggressive)
//        .setRelocationModel(Reloc::PIC_) //TODO: PIC not yet supported by MCJIT
        .setRelocationModel(Reloc::Static)
        .setTargetOptions(targetOptions)
        .setMArch("thumb")
        .setMCPU("arm966e-s")
        .create();
    if (!engine)
        throw LLVMException(errorDescription);
        
    engine->DisableLazyCompilation(true);
  
  //MyObjectCache objectCache;
  //engine->setObjectCache(&objectCache);
    
  

    //Touch all functions to trigger compilation
    for (Module::iterator I = module->begin(), E = module->end(); I != E; ++I) 
    {
        Function *Fn = &*I;
        if (!Fn->isDeclaration())
            engine->getPointerToFunction(Fn);
    }

    //We assume that our code compiles to a single code section
    if (memoryManager->code_begin() + 1 != memoryManager->code_end())
        throw Exception("Expected exactly one code section after compilation");
    if (memoryManager->data_begin() != memoryManager->data_end())
        throw Exception("Expected no data section after compilation");
    
    
    //TODO: This is an ugly hack to get the size of the generated code ...
    RecordingMemoryManager::const_code_iterator code_section = memoryManager->code_begin();
    uint64_t code_size = 0;
    
    for (const uint32_t * ptr = reinterpret_cast<const uint32_t *>(static_cast<const uint8_t *>(code_section->first.base()) + code_section->first.size() - 4); 
           ptr > static_cast<const uint32_t *>(code_section->first.base()); 
           ptr--)
    {
        if (*ptr != 0) {
            code_size = reinterpret_cast<uint64_t>(ptr) - reinterpret_cast<uint64_t>(code_section->first.base()) + 4;
            break;
        }
    }
      
//    outs() << "Code section: start = " << intToHexString(reinterpret_cast<uint64_t>(code_section->first.base())) << ", size = " << intToHexString(code_size) << '\n';
//    std::ofstream outfile("code.bin", std::ios::binary | std::ios::out);
//    outfile.write(reinterpret_cast<const char *>(code_section->first.base()), code_size);
//    outfile.close();
    
    CompiledCode* compiled_code = new CompiledCode(0, reinterpret_cast<uint8_t *>(code_section->first.base()), code_size);
    
    delete engine;
//    delete memoryManager;
    return compiled_code;
}

void BinaryTranslator::instrumentMemoryAccess()
{
    char c;
    PassManager PM;
    
    PM.add(new InstrumentMemoryAccessPass(c, instrumentationReadHandler, instrumentationWriteHandler));
    PM.run(*this->module);
}

BinaryTranslator::~BinaryTranslator() throw()
{
    if (config)
        delete config;
        
}
