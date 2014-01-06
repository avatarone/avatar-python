#include "lldc/ArmReverseTranslators.h"
#include "lldc/LLVMArmRegisters.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "lldc/ExtendedMCInst.h"
#include "lldc/NextInstructions.h"
#include "lldc/PathState.h"
#include "lldc/PathsManager.h"
#include "lldc/ReverseTranslatorExceptions.h"
#include "lldc/InstructionDecoder.h"
#include "ARMAddressingModes.h"

#include "lldc/InstructionTranslationUnitCache.h"

#include <iostream>

using namespace llvm;

//TODO: This stuff should go in the header

//TODO: This stuff is duplicate, put it in a good position and header
void genArmFunctionCall(ReverseTranslationConfiguration* config, PathState* state, uint64_t function_address, NextInstructions& next, bool is_thumb = true)
{
    IRBuilder<> builder(state->getBasicBlock());
    
    //TODO: Check if function already exists 
    
    Function* callee_function = config->getFunctionCache().getOrCreateFunction(config, state->getModule(), function_address);
    assert(callee_function && "Function creation failed");
    ReverseTranslateBasicBlock* callee_state = config->getBasicBlockTranslationStateManager()->getReverseTranslateBasicBlock(callee_function, function_address);
    
    assert(callee_state && "Could not find state for function");
    if (!callee_state->isTranslationFinished()) {
        static_cast<PathState*>(callee_state)->setThumb(is_thumb);
    }
        

    SmallVector<Value*, 15> params;
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
    params.push_back(builder.getInt32(0xcafebabe)); /* TODO: Push a kind of magic value that can be tracked */
    
    //FIXME: Reenable the next isntruction thing, just disabled for debugging
    next.addNextInstruction(NextInstructions::FLOW_CALL, function_address, callee_state);
    
    Value* returnValue = builder.CreateCall(callee_function, params, "R0.");
    state->setRegister(ARM::R0, returnValue);
//    builder.CreateBr(after_call_state->getBasicBlock()); //TODO: This is an ugly hack, but we need to terminate the bb - LLVM doesn't know that we do not do a proper return
    //The return will be done once we are in IR and can do a reverse analysis from "jump to return BB" -> RET
}
   
static void translate_tPUSH(ExtendedMCInst& instr, ReverseTranslationConfiguration& config, UnfinishedInstructionTranslationUnit* unit, MemoryObject* memory) {
    assert(instr.getNumOperands() > 2 && "There has to be at least one register that is pushed");
    //TODO: What about the condition?
    //TODO: using virtual registers and keeping a stack of them during compile time
    //      would be much more efficient for optimization, but then the conformity to the
    //      original program is less ... maybe allow certain options by switch
    BasicBlock *bb = unit->getTranslationUnit().getHead();
    IRBuilder<> builder(bb);  
    
    Value* reg_sp = unit->getValue(ARM::SP);   
    //FIXME: Use correct register sizes, not fixed ones
    Value* stack_ptr = builder.CreateIntToPtr(reg_sp, PointerType::get(Type::getInt32Ty(config.getLLVMContext()), 0));
    
    
    for (unsigned cur_op = 2; cur_op < instr.getNumOperands(); cur_op++) {
        MCOperand& op = instr.getOperand(cur_op);
        assert(op.isReg() && "push operand has to be a register");
        
        //Get SP, decrease and write back
        stack_ptr = builder.CreateConstGEP1_32(stack_ptr, -1, "SP.");
        Value* reg_cur = unit->getValue(op.getReg());
//         state->getStackState().push(reg_cur);
        
         //TODO: What to do with a label?
         if (reg_cur->getType() == Type::getLabelTy(config.getLLVMContext())) {
             outs() << "Need to push label type to stack, what to do?" << '\n';
         }
         else {
             Instruction* ir_instr = builder.CreateStore(reg_cur, stack_ptr);
             ir_instr->setDebugLoc(DebugLoc::get(instr.getProgramCounter(), 0, 0));
             ir_instr->setMetadata("bla", MDNode::get(config.getLLVMContext(), MDString::get(config.getLLVMContext(), "this is a test")));
         }
    }
    
    unit->setValue(ARM::SP, builder.CreatePtrToInt(stack_ptr, Type::getInt32Ty(config.getLLVMContext())));
    //Touch next block to translate
    InstructionTranslationUnitCache::getInstance().find_create(config, unit->getTranslationUnit().getFunction(), instr.getProgramCounter() + instr.getSize(), unit->getValues(), bb);
}

static void translate_tLDRpciASM(ExtendedMCInst& inst, ReverseTranslationConfiguration& config, UnfinishedInstructionTranslationUnit* unit, MemoryObject* memory) {
    assert(inst.getOperand(2).isImm() && inst.getOperand(2).getImm() == ARMCC::AL && "Cond needs to be AL");
    assert(inst.getOperand(3).isReg() && inst.getOperand(3).getReg() == 0 && "Last operand needs to be reg 0");
    BasicBlock *bb = unit->getTranslationUnit().getHead();
    IRBuilder<> builder(bb);  

    unsigned offset = inst.getOperand(1).getImm();
    //TODO: mind endianness here 
    uint64_t val = 0;
    memory->readBytes(((inst.getProgramCounter() + 4) & 0xFFFFFFFC) + offset, 4 /* TODO: Get size from register size */, reinterpret_cast<uint8_t *>(&val));
    Value* dest_reg_value = builder.getInt32( /* TODO: Should be reg type */val);
    
    //TODO: Mark old register contents dead
    unit->setValue(inst.getOperand(0).getReg(), dest_reg_value);
    InstructionTranslationUnitCache::getInstance().find_create(config, unit->getTranslationUnit().getFunction(), inst.getProgramCounter() + inst.getSize(), unit->getValues(), bb);
}

static void translate_tLDRHi(ExtendedMCInst& inst, ReverseTranslationConfiguration& config, UnfinishedInstructionTranslationUnit* unit, MemoryObject* memory) 
{
    assert(inst.getOperand(3).isImm() && inst.getOperand(3).getImm() == ARMCC::AL && "Cond needs to be AL");
    assert(inst.getOperand(4).isReg() && inst.getOperand(4).getReg() == 0 && "Last operand needs to be reg 0");
    BasicBlock *bb = unit->getTranslationUnit().getHead();
    IRBuilder<> builder(bb); 

    Value* rn = unit->getValue(inst.getOperand(1).getReg());
    Value* offset = builder.getInt32(inst.getOperand(2).getImm()); //TODO: Data type register size dependent
    Value* base = builder.CreateIntToPtr(rn, PointerType::get(Type::getInt16Ty(config.getLLVMContext()), 0));
    Value* addr = builder.CreateGEP(base, offset);
    LoadInst* load = builder.CreateLoad(addr);
    load->setMetadata("translated_memory_access", MDNode::get(config.getLLVMContext(), ArrayRef< Value* >()));
    Value* val = builder.CreateIntCast(load, Type::getInt32Ty(config.getLLVMContext()), false);
    unit->setValue(inst.getOperand(0).getReg(), val);

    InstructionTranslationUnitCache::getInstance().find_create(config, unit->getTranslationUnit().getFunction(), inst.getProgramCounter() + inst.getSize(), unit->getValues(), bb);
}

static void translate_tCMPr(ExtendedMCInst& inst, ReverseTranslationConfiguration& config, UnfinishedInstructionTranslationUnit* unit, MemoryObject* memory) {
    assert(inst.getOperand(2).isImm() && inst.getOperand(2).getImm() == ARMCC::AL && "Cond needs to be AL");
    assert(inst.getOperand(3).isReg() && inst.getOperand(3).getReg() == 0 && "Last operand needs to be reg 0");
    BasicBlock *bb = unit->getTranslationUnit().getHead();
    IRBuilder<> builder(bb);  
    
    Value* lhs = unit->getValue(inst.getOperand(0).getReg());
    Value* rhs = unit->getValue(inst.getOperand(1).getReg());
    
    unit->setValue(ARM::FLAG_Z, builder.CreateICmpEQ(lhs, rhs, "flag_z."));
    unit->setValue(ARM::FLAG_C, builder.CreateICmpUGE(lhs, rhs, "flag_c."));
    unit->setValue(ARM::FLAG_N, builder.CreateICmpNE(builder.CreateLShr(builder.CreateSub(lhs, rhs, "tmp."), builder.getInt32(31)), builder.getInt32(0), "flag_n."));
    //VS //see http://teaching.idallen.com/dat2343/10f/notes/040_overflow.txt
    //TODO: make overflow calculation more efficient   
    Value* lhs_sign = builder.CreateICmpNE(builder.CreateLShr(lhs, builder.getInt32(31)), builder.getInt32(0));
    Value* rhs_sign = builder.CreateICmpNE(builder.CreateLShr(rhs, builder.getInt32(31)), builder.getInt32(0));
    Value* result_sign = builder.CreateICmpNE(builder.CreateLShr(builder.CreateSub(lhs, rhs), builder.getInt32(31)), builder.getInt32(0));
    Value* V = builder.CreateAnd(
		builder.CreateXor(lhs_sign, rhs_sign),
		builder.CreateXor(lhs_sign, result_sign), "flag_v.");
    unit->setValue(ARM::FLAG_V, V);
    InstructionTranslationUnitCache::getInstance().find_create(config, unit->getTranslationUnit().getFunction(), inst.getProgramCounter() + inst.getSize(), unit->getValues(), bb);
}

static void translate_tCMPi8(ExtendedMCInst& inst, ReverseTranslationConfiguration& config, UnfinishedInstructionTranslationUnit* unit, MemoryObject* memory) {
    assert(inst.getOperand(2).isImm() && inst.getOperand(2).getImm() == ARMCC::AL && "Cond needs to be AL");
    assert(inst.getOperand(3).isReg() && inst.getOperand(3).getReg() == 0 && "Last operand needs to be reg 0");
    BasicBlock *bb = unit->getTranslationUnit().getHead();
    IRBuilder<> builder(bb);

    Value* lhs = unit->getValue(inst.getOperand(0).getReg());
    Value* rhs = builder.getInt32(inst.getOperand(1).getImm());

    unit->setValue(ARM::FLAG_Z, builder.CreateICmpEQ(lhs, rhs, "flag_z."));
    unit->setValue(ARM::FLAG_C, builder.CreateICmpUGE(lhs, rhs, "flag_c."));
    unit->setValue(ARM::FLAG_N, builder.CreateICmpNE(builder.CreateLShr(builder.CreateSub(lhs, rhs, "tmp."), builder.getInt32(31)), builder.getInt32(0), "flag_n."));
    //VS //see http://teaching.idallen.com/dat2343/10f/notes/040_overflow.txt
    //TODO: make overflow calculation more efficient
    Value* lhs_sign = builder.CreateICmpNE(builder.CreateLShr(lhs, builder.getInt32(31)), builder.getInt32(0));
    Value* rhs_sign = builder.CreateICmpNE(builder.CreateLShr(rhs, builder.getInt32(31)), builder.getInt32(0));
    Value* result_sign = builder.CreateICmpNE(builder.CreateLShr(builder.CreateSub(lhs, rhs), builder.getInt32(31)), builder.getInt32(0));
    Value* V = builder.CreateAnd(
        builder.CreateXor(lhs_sign, rhs_sign),
        builder.CreateXor(lhs_sign, result_sign), "flag_v.");
    unit->setValue(ARM::FLAG_V, V);
    InstructionTranslationUnitCache::getInstance().find_create(config, unit->getTranslationUnit().getFunction(), inst.getProgramCounter() + inst.getSize(), unit->getValues(), bb);
}

static Value* getCondition(BasicBlock* bb, const std::map< unsigned, Value*>& values, unsigned cond)
{
    assert(values.find(ARM::FLAG_Z) != values.end() && "Z flag is needed for comparison");
    assert(values.find(ARM::FLAG_N) != values.end() && "N flag is needed for comparison");
    assert(values.find(ARM::FLAG_C) != values.end() && "C flag is needed for comparison");
    assert(values.find(ARM::FLAG_V) != values.end() && "V flag is needed for comparison");
	IRBuilder<> builder(bb);
    
	Value* Z = values.find(ARM::FLAG_Z)->second;
	Value* N = values.find(ARM::FLAG_N)->second;
	Value* C = values.find(ARM::FLAG_C)->second;
	Value* V = values.find(ARM::FLAG_V)->second;
	
	switch(cond) {
		case ARMCC::EQ:
			return Z;
		case ARMCC::NE:
			return builder.CreateNot(Z, "cond_ne.");
		case ARMCC::HS:
			return C;
		case ARMCC::LO:
			return builder.CreateNot(C, "cond_cc.");
		case ARMCC::MI:
			return N;
		case ARMCC::PL:
			return builder.CreateNot(N, "cond_pl.");
		case ARMCC::VS:
			return V;
		case ARMCC::VC:
			return builder.CreateNot(V, "cond_vc.");
		case ARMCC::HI:
			return builder.CreateAnd(
				builder.CreateNot(Z),
				C, "cond_hi.");
		case ARMCC::LS:
			return builder.CreateOr(
				builder.CreateNot(C),
				Z, "cond_ls.");
		case ARMCC::GE:
			return builder.CreateICmpEQ(
				N,
				V, "cond_ge.");
		case ARMCC::LT:
			return builder.CreateICmpNE(
				N,
				V, "cond_lt.");
		case ARMCC::GT:
			return builder.CreateAnd(
				builder.CreateICmpEQ(
					N,
					V),
				builder.CreateNot(Z), "cond_gt.");
		case ARMCC::LE:
			return builder.CreateOr(
				builder.CreateICmpNE(
					N,
					V),
				Z, "cond_gt.");
		default:
			throw "Unexpected ARM condition code";
	}
}

static void translate_tBcc(ExtendedMCInst& inst, ReverseTranslationConfiguration& config, UnfinishedInstructionTranslationUnit* unit, MemoryObject* memory) {
    assert(inst.getOperand(2).isReg() && inst.getOperand(2).getReg() == ARM::CPSR && "register 2 needs to be CPSR");
    BasicBlock *bb = unit->getTranslationUnit().getHead();
    IRBuilder<> builder(bb);  
    
    uint64_t offset = inst.getOperand(0).getImm();
    unsigned condition = inst.getOperand(1).getImm();
    uint64_t cond_false_next_pc = inst.getProgramCounter() + inst.getSize();
    uint64_t cond_true_next_pc = inst.getProgramCounter() + 4 + offset;
    InstructionTranslationUnit* cond_false_unit = InstructionTranslationUnitCache::getInstance().find_create(config, unit->getTranslationUnit().getFunction(), cond_false_next_pc, unit->getValues(), bb, false);
    InstructionTranslationUnit* cond_true_unit = InstructionTranslationUnitCache::getInstance().find_create(config, unit->getTranslationUnit().getFunction(), cond_true_next_pc, unit->getValues(), bb, false);
    builder.CreateCondBr(getCondition(bb, unit->getValues(), condition), 
                         cond_true_unit->getHead(),
                         cond_false_unit->getHead());
}

static void translate_tB(ExtendedMCInst& inst, ReverseTranslationConfiguration& config, UnfinishedInstructionTranslationUnit* unit, MemoryObject* memory) {
    assert(inst.getOperand(1).isImm() && inst.getOperand(1).getImm() == ARMCC::AL && "Cond needs to be AL");
    assert(inst.getOperand(2).isReg() && inst.getOperand(2).getReg() == 0 && "Last operand needs to be reg 0");
    BasicBlock *bb = unit->getTranslationUnit().getHead();
    IRBuilder<> builder(bb);

    uint64_t offset = inst.getOperand(0).getImm();
    uint64_t next_pc = inst.getProgramCounter() + 4 + offset;
    InstructionTranslationUnit* next_unit = InstructionTranslationUnitCache::getInstance().find_create(config, unit->getTranslationUnit().getFunction(), next_pc, unit->getValues(), bb, false);
    builder.CreateBr(next_unit->getHead());
}
// 
// static void translate_tMOVSr(ExtendedMCInst& instruction, ReverseTranslationConfiguration* config, UnfinishedInstructionTranslationUnit* unit, MemoryObject* memory, NextInstructions& next) {
//     assert(instruction.getNumOperands() >= 2 && "needs at least two operands");
//     assert(instruction.getOperand(0).isReg() && "Dest needs to be register");
//     assert(instruction.getOperand(1).isReg() && "Src needs to be register");
//     
// //    std::cout << "Inserting into basic block " << state->getBasicBlock()->getName() << '\n';
//     
//     state->setRegister(instruction.getOperand(0).getReg(), state->getRegister(instruction.getOperand(1).getReg()));
//     next.addNextInstruction(NextInstructions::FLOW_NORMAL, instruction.getProgramCounter() + instruction.getSize(), state);
// }
// 
static void translate_tADDi8(ExtendedMCInst& inst, ReverseTranslationConfiguration& config, UnfinishedInstructionTranslationUnit* unit, MemoryObject* memory) {
    assert(inst.getOperand(4).isImm() && inst.getOperand(4).getImm() == ARMCC::AL && "Cond needs to be AL");
    assert(inst.getOperand(5).isReg() && inst.getOperand(5).getReg() == 0 && "Last operand needs to be reg 0");
    assert(inst.getOperand(1).isReg() && inst.getOperand(1).getReg() == ARM::CPSR && "register 1 needs to be CPSR");
    BasicBlock *bb = unit->getTranslationUnit().getHead();
    IRBuilder<> builder(bb);  
    
    Value* rn = unit->getValue(inst.getOperand(2).getReg());
    Constant* imm = builder.getInt32(inst.getOperand(3).getImm());
    Value* rd = builder.CreateAdd(rn, imm, "add.");

	unit->setValue(inst.getOperand(0).getReg(), rd);
    unit->setValue(ARM::FLAG_Z, builder.CreateICmpEQ(rd, builder.getInt32(0), "flag_z."));
    unit->setValue(ARM::FLAG_N, builder.CreateICmpNE(builder.CreateLShr(rd, builder.getInt32(31)), builder.getInt32(0), "flag_n."));
    unit->setValue(ARM::FLAG_C, builder.CreateOr(
        builder.CreateICmpUGT(imm, rn),
        builder.CreateICmpULT(rd, rn), "flag_c."));
    Value* lhs_sign = builder.CreateICmpNE(builder.CreateLShr(rn, builder.getInt32(31)), builder.getInt32(0));
    Value* rhs_sign = builder.CreateICmpNE(builder.CreateLShr(imm, builder.getInt32(31)), builder.getInt32(0));
    Value* result_sign = builder.CreateICmpNE(builder.CreateLShr(rd, builder.getInt32(31)), builder.getInt32(0));
    unit->setValue(ARM::FLAG_V, builder.CreateAnd(
        builder.CreateNot(builder.CreateXor(lhs_sign, rhs_sign)), 
        builder.CreateXor(lhs_sign, result_sign), "flag_v."));
    InstructionTranslationUnitCache::getInstance().find_create(config, unit->getTranslationUnit().getFunction(), inst.getProgramCounter() + inst.getSize(), unit->getValues(), bb);
}
     
static void translate_tMOVi8(ExtendedMCInst& inst, ReverseTranslationConfiguration& config, UnfinishedInstructionTranslationUnit* unit, MemoryObject* memory) {
    assert(inst.getOperand(3).isImm() && inst.getOperand(3).getImm() == ARMCC::AL && "Cond needs to be AL");
    assert(inst.getOperand(4).isReg() && inst.getOperand(4).getReg() == 0 && "Last operand needs to be reg 0");
    assert(inst.getOperand(1).isReg() && inst.getOperand(1).getReg() == ARM::CPSR && "register 1 needs to be CPSR");
    BasicBlock *bb = unit->getTranslationUnit().getHead();
    IRBuilder<> builder(bb);  
    
    Constant* imm = builder.getInt32(inst.getOperand(2).getImm());
	unit->setValue(inst.getOperand(0).getReg(), imm);

    unit->setValue(ARM::FLAG_Z, builder.CreateICmpEQ(imm, builder.getInt32(0), "flag_z."));
    unit->setValue(ARM::FLAG_N, builder.CreateICmpNE(builder.CreateLShr(imm, builder.getInt32(31)), builder.getInt32(0), "flag_n."));
    InstructionTranslationUnitCache::getInstance().find_create(config, unit->getTranslationUnit().getFunction(), inst.getProgramCounter() + inst.getSize(), unit->getValues(), bb);
}

// static void translate_tSTRspi(ExtendedMCInst& inst, ReverseTranslationConfiguration& config, UnfinishedInstructionTranslationUnit* unit, MemoryObject* memory) {
//     assert(instruction.getNumOperands() >= 3 && "needs at least three operands");
//     assert(instruction.getOperand(0).isReg() && "src needs to be register");
//     assert(instruction.getOperand(1).isReg() && "SP needs to be register"); 
// 	assert(instruction.getOperand(2).isImm() && "immediate needs to be immediate"); 
//     IRBuilder<> builder(state->getBasicBlock());
//     
// 	Value* rd = state->getRegister(instruction.getOperand(0).getReg());
// 	Value* sp = state->getRegister(instruction.getOperand(1).getReg());
//     uint32_t imm = instruction.getOperand(2).getImm();
// //    state->setStackElement(imm, rd);
// 	Value* address = builder.CreateConstGEP1_32(builder.CreateIntToPtr(sp, PointerType::get(Type::getInt32Ty(state->getContext()), 0)), imm, "str_addr.");
// 	builder.CreateStore(rd, address);
//     next.addNextInstruction(NextInstructions::FLOW_NORMAL, instruction.getProgramCounter() + instruction.getSize(), state);
// }

static void translate_tSTRHi(ExtendedMCInst& inst, ReverseTranslationConfiguration& config, UnfinishedInstructionTranslationUnit* unit, MemoryObject* memory) {
    assert(inst.getOperand(3).isImm() && inst.getOperand(3).getImm() == ARMCC::AL && "Cond needs to be AL");
    assert(inst.getOperand(4).isReg() && inst.getOperand(4).getReg() == 0 && "Last operand needs to be reg 0");
    BasicBlock *bb = unit->getTranslationUnit().getHead();
    IRBuilder<> builder(bb);
    
    Value* rn = unit->getValue(inst.getOperand(1).getReg());
	Value* base = builder.CreateIntToPtr(rn, PointerType::get(Type::getInt16Ty(config.getLLVMContext()), 0));
	Value* rd = unit->getValue(inst.getOperand(0).getReg());
	Value* val = builder.CreateIntCast(rd, Type::getInt16Ty(config.getLLVMContext()), true, "strh_val.");
    uint32_t imm = inst.getOperand(2).getImm(); //TODO: Data type register size dependent
	Value* addr = builder.CreateConstGEP1_32(base, imm, "strh_addr.");
	StoreInst* store = builder.CreateStore(val, addr);
    store->setMetadata("translated_memory_access", MDNode::get(config.getLLVMContext(), ArrayRef< Value* >()));
    
    InstructionTranslationUnitCache::getInstance().find_create(config, unit->getTranslationUnit().getFunction(), inst.getProgramCounter() + inst.getSize(), unit->getValues(), bb);
}

static void translate_tLSLri(ExtendedMCInst& inst, ReverseTranslationConfiguration& config, UnfinishedInstructionTranslationUnit* unit, MemoryObject* memory) 
{
    assert(inst.getOperand(4).isImm() && inst.getOperand(4).getImm() == ARMCC::AL && "Cond needs to be AL");
    assert(inst.getOperand(5).isReg() && inst.getOperand(5).getReg() == 0 && "Last operand needs to be reg 0");
    assert(inst.getOperand(1).isReg() && inst.getOperand(1).getReg() == ARM::CPSR && "register 1 needs to be CPSR");
    BasicBlock *bb = unit->getTranslationUnit().getHead();
    IRBuilder<> builder(bb);  
    
    Value* rm = unit->getValue(inst.getOperand(2).getReg());
    uint64_t imm = inst.getOperand(3).getImm();
    Value* rd = builder.CreateShl(rm, imm);
    Value* C = builder.CreateICmpNE(builder.CreateAnd(
        builder.CreateLShr(rd, 32 - inst.getOperand(3).getImm()),
        builder.getInt32(1)), builder.getInt32(0), "flag_c.");
    unit->setValue(inst.getOperand(0).getReg(), rd);
    unit->setValue(ARM::FLAG_Z, builder.CreateICmpEQ(rd, builder.getInt32(0), "flag_z."));
    unit->setValue(ARM::FLAG_N, builder.CreateICmpNE(builder.CreateLShr(rd, builder.getInt32(31)), builder.getInt32(0), "flag_n."));
    unit->setValue(ARM::FLAG_C, C);
    InstructionTranslationUnitCache::getInstance().find_create(config, unit->getTranslationUnit().getFunction(), inst.getProgramCounter() + inst.getSize(), unit->getValues(), bb);
}

static void translate_tORR(ExtendedMCInst& inst, ReverseTranslationConfiguration& config, UnfinishedInstructionTranslationUnit* unit, MemoryObject* memory) {
    assert(inst.getOperand(4).isImm() && inst.getOperand(4).getImm() == ARMCC::AL && "Cond needs to be AL");
    assert(inst.getOperand(5).isReg() && inst.getOperand(5).getReg() == 0 && "Last operand needs to be reg 0");
    assert(inst.getOperand(1).isReg() && inst.getOperand(1).getReg() == ARM::CPSR && "register 1 needs to be CPSR");
    BasicBlock *bb = unit->getTranslationUnit().getHead();
    IRBuilder<> builder(bb);  
    
    Value* rd = unit->getValue(inst.getOperand(2).getReg());
	Value* rm = unit->getValue(inst.getOperand(3).getReg());
	
	Value* result = builder.CreateOr(rd, rm, "orr_result.");
	unit->setValue(inst.getOperand(0).getReg(), result);
	unit->setValue(ARM::FLAG_Z, builder.CreateICmpEQ(result, builder.getInt32(0), "flag_z."));
	unit->setValue(ARM::FLAG_N, builder.CreateICmpNE(builder.CreateLShr(result, builder.getInt32(31)), builder.getInt32(0), "flag_n."));
    InstructionTranslationUnitCache::getInstance().find_create(config, unit->getTranslationUnit().getFunction(), inst.getProgramCounter() + inst.getSize(), unit->getValues(), bb);
}

static void translate_tBIC(ExtendedMCInst& inst, ReverseTranslationConfiguration& config, UnfinishedInstructionTranslationUnit* unit, MemoryObject* memory) 
{
    assert(inst.getOperand(4).isImm() && inst.getOperand(4).getImm() == ARMCC::AL && "Cond needs to be AL");
    assert(inst.getOperand(5).isReg() && inst.getOperand(5).getReg() == 0 && "Last operand needs to be reg 0");
    assert(inst.getOperand(1).isReg() && inst.getOperand(1).getReg() == ARM::CPSR && "register 1 needs to be CPSR");
    BasicBlock *bb = unit->getTranslationUnit().getHead();
    IRBuilder<> builder(bb);  
    
    Value* rd = unit->getValue(inst.getOperand(2).getReg());
    Value* rm = unit->getValue(inst.getOperand(3).getReg());
    Value* result = builder.CreateAnd(rd, builder.CreateNot(rm), "bic.");
    unit->setValue(inst.getOperand(0).getReg(), result);
    unit->setValue(ARM::FLAG_Z, builder.CreateICmpEQ(result, builder.getInt32(0), "flag_z."));
    unit->setValue(ARM::FLAG_N, builder.CreateICmpNE(builder.CreateLShr(result, builder.getInt32(31)), builder.getInt32(0), "flag_n."));
    InstructionTranslationUnitCache::getInstance().find_create(config, unit->getTranslationUnit().getFunction(), inst.getProgramCounter() + inst.getSize(), unit->getValues(), bb);
}

// static void translate_tLDRspi(ExtendedMCInst& instruction, ReverseTranslationConfiguration* config, UnfinishedInstructionTranslationUnit* unit, MemoryObject* memory, NextInstructions& next) {
//     assert(instruction.getNumOperands() >= 3 && "needs at least three operands");
//     assert(instruction.getOperand(0).isReg() && "Dst needs to be register");
//     assert(instruction.getOperand(1).isReg() && "SP needs to be register");
//     assert(instruction.getOperand(2).isImm() && "Offset needs to be immediate");
//     IRBuilder<> builder(state->getBasicBlock()); 
//     
//     Value* sp = state->getRegister(instruction.getOperand(1).getReg());
//     uint32_t imm = instruction.getOperand(2).getImm(); //TODO: Data type register size dependent
//     Value* base = builder.CreateIntToPtr(sp, PointerType::get(Type::getInt32Ty(state->getContext()), 0), "tLDRspi_base.");
//     
//     Value* addr = builder.CreateConstGEP1_32(base, imm, "tLDRspi_addr.");
//     Value* rd = builder.CreateLoad(addr);
//     
//     state->setRegister(instruction.getOperand(0).getReg(), rd);
//     next.addNextInstruction(NextInstructions::FLOW_NORMAL, instruction.getProgramCounter() + instruction.getSize(), state);
// }
// 
// static void translate_tBL(ExtendedMCInst& instruction, ReverseTranslationConfiguration* config, UnfinishedInstructionTranslationUnit* unit, MemoryObject* memory, NextInstructions& next) {
//     assert(instruction.getNumOperands() >= 3 && "needs at least three operands");
//     assert(instruction.getOperand(2).isImm() && "Offset needs to be immediate");
//     
//      genArmFunctionCall(config, 
//                         state,
//                         state->getProgramCounter() + 4 + instruction.getOperand(2).getImm() /* Function addr */, 
//                         next);
//      
//      outs() << "Created BL: Edge " << intToHexString(state->getProgramCounter()) << " -> " << intToHexString(state->getProgramCounter() + 4 + instruction.getOperand(2).getImm()) << '\n';
//      
//      next.addNextInstruction(NextInstructions::FLOW_NORMAL, instruction.getProgramCounter() + instruction.getSize(), state); 
// //     
// //     Value* sp = state->getRegister(instruction.getOperand(1).getReg());
// //     uint32_t imm = instruction.getOperand(2).getImm(); //TODO: Data type register size dependent
// //     Value* base = builder.CreateIntToPtr(sp, PointerType::get(Type::getInt32Ty(state->getContext()), 0), "tLDRspi_base.");
// //     
// //     Value* addr = builder.CreateConstGEP1_32(base, imm, "tLDRspi_addr.");
// //     Value* rd = builder.CreateLoad(addr);
// //     
// //     state->setRegister(instruction.getOperand(0).getReg(), rd);
// //     next.addNextInstruction(NextInstructions::FLOW_NORMAL, instruction.getProgramCounter() + instruction.getSize(), state);
// }
// 
// static void translate_tCMPi8(ExtendedMCInst& instruction, ReverseTranslationConfiguration* config, UnfinishedInstructionTranslationUnit* unit, MemoryObject* memory, NextInstructions& next) {
//     assert(instruction.getNumOperands() >= 3 && "needs at least two operands");
//     assert(instruction.getOperand(0).isReg() && "Dest needs to be register");
//     assert(instruction.getOperand(1).isImm() && "Src needs to be immediate"); 
//     IRBuilder<> builder(state->getBasicBlock());
//     
//     Value* lhs = state->getRegister(instruction.getOperand(0).getReg());
//     Value* rhs = builder.getInt32(instruction.getOperand(1).getImm());
//     
//     state->setCondition(ARM::FLAG_Z, builder.CreateICmpEQ(lhs, rhs, "flag_z."));
//     state->setCondition(ARM::FLAG_C, builder.CreateICmpUGE(lhs, rhs, "flag_c."));
//     state->setCondition(ARM::FLAG_N, builder.CreateLShr(builder.CreateSub(lhs, rhs, "cmp_tmp."), builder.getInt32(31), "flag_n."));
//     //VS //see http://teaching.idallen.com/dat2343/10f/notes/040_overflow.txt
//     //TODO: make overflow calculation more efficient   
//     Value* lhs_sign = builder.CreateLShr(lhs, builder.getInt32(31), "tmp_lhs_sign.");
//     Value* rhs_sign = builder.CreateLShr(rhs, builder.getInt32(31), "tmp_rhs_sign.");
//     Value* result_sign = builder.CreateLShr(builder.CreateSub(lhs, rhs, "tmp_sub_result."), builder.getInt32(31), "tmp_result_sign.");
//     Value* V = builder.CreateAnd(
//         builder.CreateXor(lhs_sign, rhs_sign),
//         builder.CreateXor(lhs_sign, result_sign), "flag_v.");
//     state->setCondition(ARM::FLAG_V, V);
//     next.addNextInstruction(NextInstructions::FLOW_NORMAL, instruction.getProgramCounter() + instruction.getSize(), state);
// }
// 
static void translate_tSTRi(ExtendedMCInst& inst, ReverseTranslationConfiguration& config, UnfinishedInstructionTranslationUnit* unit, MemoryObject* memory) 
{
    assert(inst.getOperand(3).isImm() && inst.getOperand(3).getImm() == ARMCC::AL && "Cond needs to be AL");
    assert(inst.getOperand(4).isReg() && inst.getOperand(4).getReg() == 0 && "Last operand needs to be reg 0");
    BasicBlock *bb = unit->getTranslationUnit().getHead();
    IRBuilder<> builder(bb);  
    
    Value* rn = unit->getValue(inst.getOperand(1).getReg());
    Value* rd = unit->getValue(inst.getOperand(0).getReg());
    Value* imm = builder.getInt32(inst.getOperand(2).getImm()); 
    Value* base = builder.CreateIntToPtr(rn, PointerType::get(Type::getInt32Ty(config.getLLVMContext()), 0));
    Value* addr = builder.CreateGEP(base, imm);
    Instruction* store = builder.CreateStore(rd, addr);
    store->setMetadata("translated_memory_access", MDNode::get(config.getLLVMContext(), ArrayRef< Value* >()));
    InstructionTranslationUnitCache::getInstance().find_create(config, unit->getTranslationUnit().getFunction(), inst.getProgramCounter() + inst.getSize(), unit->getValues(), bb);
}
// 
// 
// static void translate_tPOP(ExtendedMCInst& instr, ReverseTranslationConfiguration* config, UnfinishedInstructionTranslationUnit* unit, MemoryObject* memory, NextInstructions& next) {
//     assert(instr.getNumOperands() > 2 && "There has to be at least one register that is pushed");
//     assert(instr.getOperand(0).isImm() && instr.getOperand(0).getImm() == 14 && "Cond must be AL");
//     assert(instr.getOperand(1).isReg() && instr.getOperand(1).getReg() == 0 && "Op1 must be delimiter");
//     //TODO: What about the condition?
//     //TODO: using virtual registers and keeping a stack of them during compile time
//     //      would be much more efficient for optimization, but then the conformity to the
//     //      original program is less ... maybe allow certain options by switch
//     IRBuilder<> builder(state->getBasicBlock());  
//     bool isReturn = false;
//     
//     Value* reg_sp = state->getRegister(ARM::SP);   
//     //FIXME: Use correct register sizes, not fixed ones
//     Value* stack_ptr = builder.CreateIntToPtr(reg_sp, PointerType::get(Type::getInt32Ty(config->getLLVMContext()), 0));
//     
//     
//     for (unsigned cur_op = 2; cur_op < instr.getNumOperands(); cur_op++) {
//         MCOperand& op = instr.getOperand(cur_op);
//         assert(op.isReg() && "push operand has to be a register");
//         
//         if (op.getReg() == ARM::PC)
//             isReturn = true;
//         else {
//             //Get SP, decrease and write back
//             Value* val = builder.CreateLoad(stack_ptr);
//             state->setRegister(op.getReg(), val);
//     //         state->getStackState().push(reg_cur);
//         }
//         
//         stack_ptr = builder.CreateConstGEP1_32(stack_ptr, 1, "SP.");
//     }
//     
//     state->setRegister(ARM::SP, builder.CreatePtrToInt(stack_ptr, Type::getInt32Ty(state->getContext())));
//     
//     if (isReturn) {
//         outs() << "Found return at " << intToHexString(instr.getProgramCounter()) << '\n';
//         builder.CreateRet(state->getRegister(ARM::R0));
//         next.addNextInstruction(NextInstructions::FLOW_RETURN, 0, 0);
// //        state->translationFinished(config, instr.getProgramCounter() + instr.getSize());
//     }
//     else { 
// //     state->setRegister(ARM::SP, builder.CreatePtrToInt(reg_sp, Type::getInt32Ty(state->getContext())));
//         next.addNextInstruction(NextInstructions::FLOW_NORMAL, instr.getProgramCounter() + instr.getSize(), state);
//     }
// }
// 
// static void translate_tMVN(ExtendedMCInst& instruction, ReverseTranslationConfiguration* config, UnfinishedInstructionTranslationUnit* unit, MemoryObject* memory, NextInstructions& next) {
//     assert(instruction.getNumOperands() >= 3 && "needs at least three operands");
//     assert(instruction.getOperand(1).isReg() && instruction.getOperand(1).getReg() == 3 && "Op1 needs to be CPSR");
//     assert(instruction.getOperand(3).isImm() && instruction.getOperand(3).getImm() == 14 && "Cond needs to be AL");
//     assert(instruction.getOperand(0).isReg() && "Dest needs to be register");
// //    assert(instruction.getOperand(2).isReg() && "Src needs to be register");
//     assert(instruction.getOperand(2).isReg() && "Src needs to be register");
//     IRBuilder<> builder(state->getBasicBlock()); 
//     
// 	Value* rm = state->getRegister(instruction.getOperand(2).getReg());
// 	
// 	Value* rd = builder.CreateNot(rm, "mvn.");
// 	Value* Z = rm;
// 	Value* N = builder.CreateLShr(rd, builder.getInt32(31));
// 	state->setRegister(instruction.getOperand(0).getReg(), rd);
// 	state->setCondition(ARM::FLAG_Z, Z);
// 	state->setCondition(ARM::FLAG_N, N);
//     next.addNextInstruction(NextInstructions::FLOW_NORMAL, instruction.getProgramCounter() + instruction.getSize(), state);
// }
// 
// static void translate_tSUBrr(ExtendedMCInst& instruction, ReverseTranslationConfiguration* config, UnfinishedInstructionTranslationUnit* unit, MemoryObject* memory, NextInstructions& next) {
//     assert(instruction.getNumOperands() >= 3 && "needs at least two operands");
//     assert(instruction.getOperand(0).isReg() && "Dest needs to be register");
//     assert(instruction.getOperand(1).isReg() && instruction.getOperand(1).getReg() == 3 && "Op1 needs to be CPSR");
//     assert(instruction.getOperand(4).isImm() && instruction.getOperand(4).getImm() == 14 && "Cond needs to be AL");
//     assert(instruction.getOperand(2).isReg() && "Src needs to be register"); 
//     assert(instruction.getOperand(3).isReg() && "Src needs to be register"); 
//     IRBuilder<> builder(state->getBasicBlock());
//     
//     Value* lhs = state->getRegister(instruction.getOperand(2).getReg());
//     Value* rhs = state->getRegister(instruction.getOperand(3).getReg());
//     Value* result = builder.CreateSub(lhs, rhs, "sub.");
//     
//     
//     state->setRegister(instruction.getOperand(0).getReg(), result);
//     state->setCondition(ARM::FLAG_Z, builder.CreateICmpEQ(lhs, rhs, "flag_z."));
//     state->setCondition(ARM::FLAG_C, builder.CreateICmpUGE(lhs, rhs, "flag_c."));
//     state->setCondition(ARM::FLAG_N, builder.CreateLShr(result, builder.getInt32(31), "flag_n."));
//     //VS //see http://teaching.idallen.com/dat2343/10f/notes/040_overflow.txt
//     //TODO: make overflow calculation more efficient   
//     Value* lhs_sign = builder.CreateLShr(lhs, builder.getInt32(31), "tmp_lhs_sign.");
//     Value* rhs_sign = builder.CreateLShr(rhs, builder.getInt32(31), "tmp_rhs_sign.");
//     Value* result_sign = builder.CreateLShr(result, builder.getInt32(31), "tmp_result_sign.");
//     Value* V = builder.CreateAnd(
// 		builder.CreateXor(lhs_sign, rhs_sign),
// 		builder.CreateXor(lhs_sign, result_sign), "flag_v.");
//     state->setCondition(ARM::FLAG_V, V);
//     next.addNextInstruction(NextInstructions::FLOW_NORMAL, instruction.getProgramCounter() + instruction.getSize(), state);
// }

static void translate_tSUBi8(ExtendedMCInst& inst, ReverseTranslationConfiguration& config, UnfinishedInstructionTranslationUnit* unit, MemoryObject* memory)
{
    assert(inst.getOperand(1).isReg() && inst.getOperand(1).getReg() == 3 && "Op1 needs to be CPSR");
    assert(inst.getOperand(4).isImm() && inst.getOperand(4).getImm() == ARMCC::AL && "Cond needs to be AL");
    assert(inst.getOperand(5).isReg() && inst.getOperand(5).getReg() == 0 && "Last operand needs to be reg 0");
    BasicBlock *bb = unit->getTranslationUnit().getHead();
    IRBuilder<> builder(bb);

    Value* lhs = unit->getValue(inst.getOperand(2).getReg());
    Value* rhs = builder.getInt32(inst.getOperand(3).getImm());
    Value* result = builder.CreateSub(lhs, rhs, "sub.");


    unit->setValue(inst.getOperand(0).getReg(), result);
    unit->setValue(ARM::FLAG_Z, builder.CreateICmpEQ(lhs, rhs, "flag_z."));
    unit->setValue(ARM::FLAG_C, builder.CreateICmpUGE(lhs, rhs, "flag_c."));
    unit->setValue(ARM::FLAG_N, builder.CreateICmpNE(builder.CreateLShr(result, builder.getInt32(31)), builder.getInt32(0), "flag_n."));
     //VS //see http://teaching.idallen.com/dat2343/10f/notes/040_overflow.txt
     //TODO: make overflow calculation more efficient
     Value* lhs_sign = builder.CreateICmpNE(builder.CreateLShr(lhs, builder.getInt32(31)), builder.getInt32(0), "tmp_lhs_sign.");
     Value* rhs_sign = builder.CreateICmpNE(builder.CreateLShr(rhs, builder.getInt32(31)), builder.getInt32(0), "tmp_rhs_sign.");
     Value* result_sign = builder.CreateICmpNE(builder.CreateLShr(result, builder.getInt32(31)), builder.getInt32(0), "tmp_result_sign.");
     Value* V = builder.CreateAnd(
      builder.CreateXor(lhs_sign, rhs_sign),
      builder.CreateXor(lhs_sign, result_sign), "flag_v.");
     unit->setValue(ARM::FLAG_V, V);
     InstructionTranslationUnitCache::getInstance().find_create(config, unit->getTranslationUnit().getFunction(), inst.getProgramCounter() + inst.getSize(), unit->getValues(), bb);
 }
// 
// static void translate_tBLXi(ExtendedMCInst& instruction, ReverseTranslationConfiguration* config, UnfinishedInstructionTranslationUnit* unit, MemoryObject* memory, NextInstructions& next) {
//     assert(instruction.getNumOperands() >= 4 && "needs at least two operands");
//     assert(instruction.getOperand(2).isImm() && "Offset needs to be register");
//     assert(instruction.getOperand(3).isReg() && instruction.getOperand(3).getReg() == 3 && "Op1 needs to be CPSR");
//     assert(instruction.getOperand(0).isImm() && instruction.getOperand(0).getImm() == 14 && "Cond needs to be AL"); 
//     IRBuilder<> builder(state->getBasicBlock());
//     
//     uint32_t offset = instruction.getOperand(2).getImm();
//     
//     genArmFunctionCall(config, state, (instruction.getProgramCounter() + offset + 4) & 0xFFFFFFFC, next, false);
//     next.addNextInstruction(NextInstructions::FLOW_NORMAL, instruction.getProgramCounter() + instruction.getSize(), state);
//     
//     outs() << "Created BLX: Edge " << intToHexString(state->getProgramCounter()) << " -> " << intToHexString((state->getProgramCounter() + 4 + instruction.getOperand(2).getImm()) & 0xFFFFFFFC) << '\n';
// }
// 
static void translate_tLDRi(ExtendedMCInst& inst, ReverseTranslationConfiguration& config, UnfinishedInstructionTranslationUnit* unit, MemoryObject* memory) 
{   
    assert(inst.getOperand(3).isImm() && inst.getOperand(3).getImm() == ARMCC::AL && "Cond needs to be AL");
    assert(inst.getOperand(4).isReg() && inst.getOperand(4).getReg() == 0 && "Last operand needs to be reg 0");
    BasicBlock *bb = unit->getTranslationUnit().getHead();
    IRBuilder<> builder(bb);  
    
    Value* rn = unit->getValue(inst.getOperand(1).getReg());
    Value* imm = builder.getInt32(inst.getOperand(2).getImm()); 
    Value* base = builder.CreateIntToPtr(rn, PointerType::get(Type::getInt32Ty(config.getLLVMContext()), 0));
    Value* addr = builder.CreateGEP(base, imm);
    LoadInst* load = builder.CreateLoad(addr);
    load->setMetadata("translated_memory_access", MDNode::get(config.getLLVMContext(), ArrayRef< Value* >()));
    Value* rd = builder.CreateIntCast(load, Type::getInt32Ty(config.getLLVMContext()), false);

    unit->setValue(inst.getOperand(0).getReg(), rd);
    InstructionTranslationUnitCache::getInstance().find_create(config, unit->getTranslationUnit().getFunction(), inst.getProgramCounter() + inst.getSize(), unit->getValues(), bb);
}   
// 
// static Value* translate_SUB(UnfinishedInstructionTranslationUnit* unit, IRBuilder<>& builder, Value* lhs, Value* rhs, bool set_flags) {
//     Value* result = builder.CreateSub(lhs, rhs, "sub.");
// 
//     if (set_flags) {
//         state->setCondition(ARM::FLAG_Z, builder.CreateICmpEQ(lhs, rhs, "flag_z."));
//         state->setCondition(ARM::FLAG_C, builder.CreateICmpUGE(lhs, rhs, "flag_c."));
//         state->setCondition(ARM::FLAG_N, builder.CreateLShr(result, builder.getInt32(31), "flag_n."));
//     //VS //see http://teaching.idallen.com/dat2343/10f/notes/040_overflow.txt
//     //TODO: make overflow calculation more efficient   
//         Value* lhs_sign = builder.CreateLShr(lhs, builder.getInt32(31), "tmp_lhs_sign.");
//         Value* rhs_sign = builder.CreateLShr(rhs, builder.getInt32(31), "tmp_rhs_sign.");
//         Value* result_sign = builder.CreateLShr(result, builder.getInt32(31), "tmp_result_sign.");
//         Value* V = builder.CreateAnd(
//     		builder.CreateXor(lhs_sign, rhs_sign),
//     		builder.CreateXor(lhs_sign, result_sign), "flag_v.");
//         state->setCondition(ARM::FLAG_V, V);
//     }
//     
//     return result;
// }
// 
// static void translate_SUBrr(ExtendedMCInst& instruction, ReverseTranslationConfiguration* config, UnfinishedInstructionTranslationUnit* unit, MemoryObject* memory, NextInstructions& next) {
//     assert(instruction.getOperand(3).isImm() && instruction.getOperand(3).getImm() == 14 && "Cond needs to be AL");
//     assert(instruction.getOperand(4).isReg() && instruction.getOperand(4).getReg() == 0 && "Op4 needs to be reg 0");
//     IRBuilder<> builder(state->getBasicBlock());
//     
//     Value* result = translate_SUB(
//             state, 
//             builder, 
//             state->getRegister(instruction.getOperand(1).getReg()),
//             state->getRegister(instruction.getOperand(2).getReg()),
//             instruction.getOperand(5).isReg() && instruction.getOperand(5).getReg() == 3);
//             
//     state->setRegister(instruction.getOperand(0).getReg(), result);
//     next.addNextInstruction(NextInstructions::FLOW_NORMAL, instruction.getProgramCounter() + instruction.getSize(), state);
// }
// 
// static void translate_BX(ExtendedMCInst& instruction, ReverseTranslationConfiguration* config, UnfinishedInstructionTranslationUnit* unit, MemoryObject* memory, NextInstructions& next) {
//     assert(instruction.getOperand(0).isReg() && instruction.getOperand(0).getReg() == ARM::LR && "Only implemented for LR");
//     IRBuilder<> builder(state->getBasicBlock());
//     
//     builder.CreateRet(state->getRegister(ARM::R0));
//     next.addNextInstruction(NextInstructions::FLOW_RETURN, 0, 0);
// }
// 
// static void translate_SBCrr(ExtendedMCInst& instruction, ReverseTranslationConfiguration* config, UnfinishedInstructionTranslationUnit* unit, MemoryObject* memory, NextInstructions& next) {
//     assert(instruction.getOperand(3).isImm() && instruction.getOperand(3).getImm() == 14 && "Cond needs to be AL");
//     assert(instruction.getOperand(4).isReg() && instruction.getOperand(4).getReg() == 0 && "Op4 needs to be reg 0");
//     assert(state->getCondition(ARM::FLAG_C));
//     IRBuilder<> builder(state->getBasicBlock());
//     
//     Value* rhs = state->getRegister(instruction.getOperand(2).getReg());
//     Value* C = builder.CreateIntCast(builder.CreateNot(state->getCondition(ARM::FLAG_C)), Type::getInt32Ty(state->getContext()), false);
//     
//     Value* result = translate_SUB(
//             state, 
//             builder, 
//             state->getRegister(instruction.getOperand(1).getReg()),
//             builder.CreateAdd(rhs, builder.CreateIntCast(C, Type::getInt32Ty(state->getContext()), false)),
//             instruction.getOperand(5).isReg() && instruction.getOperand(5).getReg() == 3);
//             
//     state->setRegister(instruction.getOperand(0).getReg(), result);
//     next.addNextInstruction(NextInstructions::FLOW_NORMAL, instruction.getProgramCounter() + instruction.getSize(), state);
// }
// 
// static Value* getShiftedOp(IRBuilder<>& builder, Value* val, unsigned shift_val) {
//     switch (ARM_AM::getSORegShOp(shift_val)) {
//         case ARM_AM::lsl:
//             return builder.CreateShl(val, builder.getInt32(ARM_AM::getSORegOffset(shift_val)));  
//         case ARM_AM::lsr:
//             return builder.CreateLShr(val, builder.getInt32(ARM_AM::getSORegOffset(shift_val)));
//         case ARM_AM::asr:
//             return builder.CreateAShr(val, builder.getInt32(ARM_AM::getSORegOffset(shift_val)));
//         default:
//             assert(false);
//     }
// }
// 
// static void translate_RSBrsi(ExtendedMCInst& instruction, ReverseTranslationConfiguration* config, UnfinishedInstructionTranslationUnit* unit, MemoryObject* memory, NextInstructions& next) {
//     assert(instruction.getOperand(4).isImm() && instruction.getOperand(4).getImm() == 14 && "Cond needs to be AL");
//     assert(instruction.getOperand(5).isReg() && instruction.getOperand(5).getReg() == 0 && "Op4 needs to be reg 0");
//     IRBuilder<> builder(state->getBasicBlock());
//     
//     Value* lhs = getShiftedOp(builder, state->getRegister(instruction.getOperand(2).getReg()), instruction.getOperand(3).getImm());
//     Value* result = translate_SUB(
//             state, 
//             builder, 
//             lhs,
//             state->getRegister(instruction.getOperand(1).getReg()),
//             instruction.getOperand(6).isReg() && instruction.getOperand(6).getReg() == 3);
//             
//     state->setRegister(instruction.getOperand(0).getReg(), result);
//     next.addNextInstruction(NextInstructions::FLOW_NORMAL, instruction.getProgramCounter() + instruction.getSize(), state);
// }
// 
// static void translate_MOVi(ExtendedMCInst& instruction, ReverseTranslationConfiguration* config, UnfinishedInstructionTranslationUnit* unit, MemoryObject* memory, NextInstructions& next) {
//     assert(instruction.getNumOperands() >= 3 && "needs at least four operands");
//     assert(instruction.getOperand(0).isReg() && "Dest needs to be register");
//     assert(instruction.getOperand(1).isImm() && "Src needs to be immediate"); 
//     assert(instruction.getOperand(2).isImm() && instruction.getOperand(2).getImm() == 14 && "Cond needs to be AL");
//     assert(instruction.getOperand(3).isReg() && instruction.getOperand(3).getReg() == 0 && "Op4 needs to be reg 0");
//     assert(instruction.getOperand(4).isReg() && instruction.getOperand(4).getReg() == 0 && "Op4 needs to be reg 0");
//     IRBuilder<> builder(state->getBasicBlock());
//     
//     Constant* imm = builder.getInt32(instruction.getOperand(1).getImm());
// 	state->setRegister(instruction.getOperand(0).getReg(), imm);
// 
// //Rd = Rd + immed_8 
// //N Flag = Rd[31] 
// //Z Flag = if Rd == 0 then 1 else 0 
// //C Flag = CarryFrom(Rd + immed_8) 
// //V Flag = OverflowFrom(Rd + immed_8)
// //	state->setCondition(ARM::FLAG_Z, builder.CreateICmpEQ(imm, builder.getInt32(0), "flag_z."));
// //	state->setCondition(ARM::FLAG_N, builder.CreateLShr(imm, builder.getInt32(31), "flag_n."));
//     next.addNextInstruction(NextInstructions::FLOW_NORMAL, instruction.getProgramCounter() + instruction.getSize(), state);
// }
// 
// static void translate_Bcc(ExtendedMCInst& instr, ReverseTranslationConfiguration* config, UnfinishedInstructionTranslationUnit* unit, MemoryObject* memory, NextInstructions& next) {
//     assert(instr.getOperand(0).isImm() && "Dest needs to be Imm");
//     assert(instr.getOperand(1).isImm() && "Cond needs to be Imm");
//     assert(instr.getOperand(2).isReg() && instr.getOperand(2).getReg() == 3 && "CPSR not specified as arg");
//     IRBuilder<> builder(state->getBasicBlock());
//     
//     uint64_t offset = instr.getOperand(0).getImm();
//     unsigned condition = instr.getOperand(1).getImm();
//     uint64_t cond_false_next_pc = instr.getProgramCounter() + instr.getSize();
//     uint64_t cond_true_next_pc = instr.getProgramCounter() + 8 + offset;
//     ReverseTranslateBasicBlock* cond_false_bb = config->getBasicBlockTranslationStateManager()->getOrCreateReverseTranslateBasicBlock(state, cond_false_next_pc);
//     ReverseTranslateBasicBlock* cond_true_bb = config->getBasicBlockTranslationStateManager()->getOrCreateReverseTranslateBasicBlock(state, cond_true_next_pc);
//     
//     if (!cond_false_bb->isTranslationFinished()) 
//         static_cast<PathState*>(cond_false_bb)->setThumb(false);
//     if (!cond_true_bb->isTranslationFinished()) 
//         static_cast<PathState*>(cond_true_bb)->setThumb(false);
//     
//     
//     builder.CreateCondBr(getCondition(state, condition), 
//                          cond_true_bb->getBasicBlock(),
//                          cond_false_bb->getBasicBlock());
//     next.addNextInstruction(NextInstructions::FLOW_NORMAL, cond_false_next_pc, cond_false_bb);
//     next.addNextInstruction(NextInstructions::FLOW_CONDITIONAL_BRANCH, cond_true_next_pc, cond_true_bb);
// }
// 
static void translate_tADDrr(ExtendedMCInst& inst, ReverseTranslationConfiguration& config, UnfinishedInstructionTranslationUnit* unit, MemoryObject* memory) 
{

    assert(inst.getOperand(4).isImm() && inst.getOperand(4).getImm() == ARMCC::AL && "Cond needs to be AL");
    assert(inst.getOperand(5).isReg() && inst.getOperand(5).getReg() == 0 && "Last operand needs to be reg 0");
    assert(inst.getOperand(1).isReg() && inst.getOperand(1).getReg() == ARM::CPSR && "register 1 needs to be CPSR");
    BasicBlock *bb = unit->getTranslationUnit().getHead();
    IRBuilder<> builder(bb);  
    
    Value* rn = unit->getValue(inst.getOperand(2).getReg());
    Value* rm = unit->getValue(inst.getOperand(3).getReg());
    Value* rd = builder.CreateAdd(rn, rm, "add.");

    unit->setValue(inst.getOperand(0).getReg(), rd);
    unit->setValue(ARM::FLAG_Z, builder.CreateICmpEQ(rd, builder.getInt32(0), "flag_z."));
    unit->setValue(ARM::FLAG_N, builder.CreateICmpNE(builder.CreateLShr(rd, builder.getInt32(31)), builder.getInt32(0), "flag_n."));
    unit->setValue(ARM::FLAG_C, builder.CreateOr(
        builder.CreateICmpUGT(rm, rn),
        builder.CreateICmpULT(rd, rn), "flag_c."));
    Value* lhs_sign = builder.CreateICmpNE(builder.CreateLShr(rn, builder.getInt32(31)), builder.getInt32(0));
    Value* rhs_sign = builder.CreateICmpNE(builder.CreateLShr(rm, builder.getInt32(31)), builder.getInt32(0));
    Value* result_sign = builder.CreateICmpNE(builder.CreateLShr(rd, builder.getInt32(31)), builder.getInt32(0));
    unit->setValue(ARM::FLAG_V, builder.CreateAnd(
        builder.CreateNot(builder.CreateXor(lhs_sign, rhs_sign)), 
        builder.CreateXor(lhs_sign, result_sign), "flag_v."));
    InstructionTranslationUnitCache::getInstance().find_create(config, unit->getTranslationUnit().getFunction(), inst.getProgramCounter() + inst.getSize(), unit->getValues(), bb);
}
// 
// static void translate_tRSB(ExtendedMCInst& instruction, ReverseTranslationConfiguration* config, UnfinishedInstructionTranslationUnit* unit, MemoryObject* memory, NextInstructions& next) {
//     assert(instruction.getOperand(1).isReg() && instruction.getOperand(1).getReg() == 3 && "Op1 needs to be CPSR");
//     assert(instruction.getOperand(3).isImm() && instruction.getOperand(3).getImm() == 14 && "Cond needs to be AL");
//     assert(instruction.getOperand(4).isReg() && instruction.getOperand(4).getReg() == 0 && "Op4 needs to be reg 0");
//     IRBuilder<> builder(state->getBasicBlock());
//     
//     Value* result = translate_SUB(
//             state, 
//             builder, 
//             builder.getInt32(0),
//             state->getRegister(instruction.getOperand(2).getReg()),
//             true);
//             
//     state->setRegister(instruction.getOperand(0).getReg(), result);
//     next.addNextInstruction(NextInstructions::FLOW_NORMAL, instruction.getProgramCounter() + instruction.getSize(), state);
// }
// 
// static void translate_tBX(ExtendedMCInst& instruction, ReverseTranslationConfiguration* config, UnfinishedInstructionTranslationUnit* unit, MemoryObject* memory, NextInstructions& next) {
//     assert(instruction.getOperand(0).isReg() && instruction.getOperand(0).getReg() == ARM::LR && "Only implemented for LR");
//     assert(instruction.getOperand(1).isImm() && instruction.getOperand(1).getImm() == ARMCC::AL && "Has to be AL");
//     assert(instruction.getOperand(2).isReg() && instruction.getOperand(2).getReg() == 0 && "Op2 needs to be 0");
//     IRBuilder<> builder(state->getBasicBlock());
//     
//     builder.CreateRet(state->getRegister(ARM::R0));
//     next.addNextInstruction(NextInstructions::FLOW_RETURN, 0, 0);
// }
// 
// static void translate_SUBrsi(ExtendedMCInst& instruction, ReverseTranslationConfiguration* config, UnfinishedInstructionTranslationUnit* unit, MemoryObject* memory, NextInstructions& next) {
//     assert(instruction.getOperand(4).isImm() && instruction.getOperand(4).getImm() == 14 && "Cond needs to be AL");
//     assert(instruction.getOperand(6).isReg() && instruction.getOperand(6).getReg() == 0 && "Op4 needs to be reg 0");
//     IRBuilder<> builder(state->getBasicBlock());
//     ReverseTranslateBasicBlock* next_path = 0;
//     if (instruction.getOperand(4).getImm() != ARMCC::AL) {
//         BasicBlock* bb = BasicBlock::Create(state->getContext(), "", state->getFunction());
//         next_path = config->getBasicBlockTranslationStateManager()->getOrCreateReverseTranslateBasicBlock(state, instruction.getProgramCounter() + instruction.getSize());
//         builder.CreateCondBr(getCondition(state, instruction.getOperand(4).getImm()), bb, next_path->getBasicBlock());
//         builder.SetInsertPoint(bb);
//     }
//     
//     /* TODO: Does the state track the right registers? */
//     Value* rhs = getShiftedOp(builder, state->getRegister(instruction.getOperand(2).getReg()), instruction.getOperand(3).getImm());
//     Value* result = translate_SUB(
//             state, 
//             builder, 
//             state->getRegister(instruction.getOperand(1).getReg()),
//             rhs,
//             instruction.getOperand(5).isReg() && instruction.getOperand(5).getReg() == 3);
//             
//     state->setRegister(instruction.getOperand(0).getReg(), result);
//     
//     if (instruction.getOperand(4).getImm() != ARMCC::AL) {
//         builder.CreateBr(next_path->getBasicBlock());
//     }
//     
//     next.addNextInstruction(NextInstructions::FLOW_NORMAL, instruction.getProgramCounter() + instruction.getSize(), state);
//     next.addNextInstruction(NextInstructions::FLOW_CONDITIONAL_INSTRUCTION, instruction.getProgramCounter() + instruction.getSize(), state);
// }
// 
static void translate_tAND(ExtendedMCInst& inst, ReverseTranslationConfiguration& config, UnfinishedInstructionTranslationUnit* unit, MemoryObject* memory) 
{
    assert(inst.getOperand(4).isImm() && inst.getOperand(4).getImm() == ARMCC::AL && "Cond needs to be AL");
    assert(inst.getOperand(5).isReg() && inst.getOperand(5).getReg() == 0 && "Last operand needs to be reg 0");
    assert(inst.getOperand(1).isReg() && inst.getOperand(1).getReg() == ARM::CPSR && "register 1 needs to be CPSR");
    BasicBlock *bb = unit->getTranslationUnit().getHead();
    IRBuilder<> builder(bb);  
    
    Value* rd = unit->getValue(inst.getOperand(2).getReg());
    Value* rm = unit->getValue(inst.getOperand(3).getReg());
    
    Value* result = builder.CreateAnd(rd, rm, "and.");
    unit->setValue(inst.getOperand(0).getReg(), result);
    unit->setValue(ARM::FLAG_Z, builder.CreateICmpEQ(result, builder.getInt32(0), "flag_z."));
    unit->setValue(ARM::FLAG_N, builder.CreateICmpNE(builder.CreateLShr(result, builder.getInt32(31)), builder.getInt32(0), "flag_n."));
    InstructionTranslationUnitCache::getInstance().find_create(config, unit->getTranslationUnit().getFunction(), inst.getProgramCounter() + inst.getSize(), unit->getValues(), bb);
}


static void translate_tLSRri(ExtendedMCInst& inst, ReverseTranslationConfiguration& config, UnfinishedInstructionTranslationUnit* unit, MemoryObject* memory) 
{
    assert(inst.getOperand(4).isImm() && inst.getOperand(4).getImm() == ARMCC::AL && "Cond needs to be AL");
    assert(inst.getOperand(5).isReg() && inst.getOperand(5).getReg() == 0 && "Last operand needs to be reg 0");
    assert(inst.getOperand(1).isReg() && inst.getOperand(1).getReg() == ARM::CPSR && "register 1 needs to be CPSR");
    BasicBlock *bb = unit->getTranslationUnit().getHead();
    IRBuilder<> builder(bb);  
    
    Value* rm = unit->getValue(inst.getOperand(2).getReg());
    uint64_t imm = inst.getOperand(3).getImm();
    Value* rd;
    Value* C;
    
    if (inst.getOperand(3).getImm() == 0)
    {
        rd = builder.getInt32(0);
        C = builder.CreateICmpNE(builder.CreateLShr(rm, builder.getInt32(31), "flag_c."), builder.getInt32(0));
    }
    else {
        rd = builder.CreateLShr(rm, imm);
        C = builder.CreateICmpNE(builder.CreateAnd(
                builder.CreateLShr(rd,  inst.getOperand(3).getImm() - 1),
                builder.getInt32(1)), builder.getInt32(0), "flag_c.");
    }
    unit->setValue(inst.getOperand(0).getReg(), rd);
    unit->setValue(ARM::FLAG_Z, builder.CreateICmpEQ(rd, builder.getInt32(0), "flag_z."));
    unit->setValue(ARM::FLAG_N, builder.CreateICmpNE(builder.CreateLShr(rd, builder.getInt32(31)), builder.getInt32(0), "flag_n."));
    unit->setValue(ARM::FLAG_C, C);
    InstructionTranslationUnitCache::getInstance().find_create(config, unit->getTranslationUnit().getFunction(), inst.getProgramCounter() + inst.getSize(), unit->getValues(), bb);
}

void getArmReverseTranslators(std::map<unsigned, ReverseInstructionTranslatorFunction>& translators)
{
    translators[ARM::tPUSH] = translate_tPUSH;
    translators[ARM::tLDRpciASM] = translate_tLDRpciASM;
    translators[ARM::tLDRHi] = translate_tLDRHi;
    translators[ARM::tCMPr] = translate_tCMPr;
     translators[ARM::tBcc] = translate_tBcc;
//     translators[ARM::tMOVSr] = translate_tMOVSr;
    translators[ARM::tADDi8] = translate_tADDi8;
    translators[ARM::tMOVi8] = translate_tMOVi8;
// 	translators[ARM::tSTRspi] = translate_tSTRspi;
    translators[ARM::tSTRHi] = translate_tSTRHi;
    translators[ARM::tLSLri] = translate_tLSLri;
    translators[ARM::tORR] = translate_tORR;
	translators[ARM::tBIC] = translate_tBIC;
//     translators[ARM::tLDRspi] = translate_tLDRspi;
//     translators[ARM::tBL] = translate_tBL;
    translators[ARM::tB] = translate_tB;
    translators[ARM::tCMPi8] = translate_tCMPi8;
    translators[ARM::tSTRi] = translate_tSTRi;
//     translators[ARM::tPOP] = translate_tPOP;
//     translators[ARM::tMVN] = translate_tMVN;
//     translators[ARM::tSUBrr] = translate_tSUBrr;
//     translators[ARM::tBLXi] = translate_tBLXi;
    translators[ARM::tADDi3] = translate_tADDi8;
    translators[ARM::tSUBi8] = translate_tSUBi8;
    translators[ARM::tLDRi] = translate_tLDRi;

//     translators[ARM::SUBrr] = translate_SUBrr;
//     translators[ARM::SBCrr] = translate_SBCrr;
//     translators[ARM::BX] = translate_BX;
//     translators[ARM::RSBrsi] = translate_RSBrsi;
//     translators[ARM::MOVi] = translate_MOVi;
//     translators[ARM::Bcc] = translate_Bcc;
    translators[ARM::tADDrr] = translate_tADDrr;
//     translators[ARM::tRSB] = translate_tRSB;
//     translators[ARM::tBX] = translate_tBX;
//     translators[ARM::SUBrsi] = translate_SUBrsi;
    translators[ARM::tAND] = translate_tAND;
    translators[ARM::tLSRri] = translate_tLSRri;
}
