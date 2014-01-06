#include "lldc/InstrumentMemoryAccessPass.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

using namespace llvm;

InstrumentMemoryAccessPass::InstrumentMemoryAccessPass(char& pid, Value* read_handler, Value* write_handler)
    : BasicBlockPass(pid), read_handler(read_handler), write_handler(write_handler)
{
}


bool InstrumentMemoryAccessPass::doInitialization(Function &F)
{
    return false;
}

bool InstrumentMemoryAccessPass::runOnBasicBlock(BasicBlock &bb)
{
    for (BasicBlock::iterator itr = bb.begin();
         itr != bb.end();
         itr++)
    {
        //Only do instructions that have been marked as translated, otherwise the pre/post-amble is also translated
        if (!itr->getMetadata("translated_memory_access"))
            continue;
        
        if (itr->getOpcode() == Instruction::Load) 
        {
            Value* address = cast<LoadInst>(*itr).getPointerOperand();
            uint32_t size = 0;
            
            if (address->getType()->isPointerTy() && address->getType()->getPointerElementType()->isIntegerTy())
            {
                switch(cast<IntegerType>(address->getType()->getPointerElementType())->getBitWidth())
                {
                    case 8:
                        size = 1;
                        break;
                    case 16:
                        size = 2;
                        break;
                    case 32:
                        size = 4;
                        break;
                    default:
                        assert(false && "Unknown integer type size");
                }
            }
            else
            {
                assert(false && "Unknown type as argument to LoadInst");
            }
            
            SmallVector<Value*, 2> args;
            Value* castAddress = CastInst::CreatePointerCast(address, Type::getInt8PtrTy(bb.getContext()), "load_addr.", itr);
            args.push_back(castAddress);
            args.push_back(ConstantInt::get(Type::getInt32Ty(bb.getContext()), size));
            Value* value = CallInst::Create(read_handler, args, "handle_read.", itr);
            ReplaceInstWithInst(bb.getInstList(), itr, CastInst::CreateIntegerCast(value, itr->getType(), false));
        }
        else if (itr->getOpcode() == Instruction::Store)
        {
            Value* address = cast<StoreInst>(*itr).getPointerOperand();
            Value* value = cast<StoreInst>(*itr).getValueOperand();
            uint32_t size = 0;
            
            if (address->getType()->isPointerTy() && address->getType()->getPointerElementType()->isIntegerTy())
            {
                switch(cast<IntegerType>(address->getType()->getPointerElementType())->getBitWidth())
                {
                    case 8:
                        size = 1;
                        break;
                    case 16:
                        size = 2;
                        break;
                    case 32:
                        size = 4;
                        break;
                    default:
                        assert(false && "Unknown integer type size");
                }
            }
            else
            {
                assert(false && "Unknown type as argument to LoadInst");
            }
            
            SmallVector<Value*, 3> args;
            Value* castAddress = CastInst::CreatePointerCast(address, Type::getInt8PtrTy(bb.getContext()), "store_addr.", itr);
            Value* castValue = CastInst::CreateIntegerCast(value, Type::getInt32Ty(bb.getContext()), false, "store_val.", itr);
            args.push_back(castAddress);
            args.push_back(ConstantInt::get(Type::getInt32Ty(bb.getContext()), size));
            args.push_back(castValue);
            ReplaceInstWithInst(bb.getInstList(), itr, CallInst::Create(write_handler, args));
        }
    }

    return false;
}

bool InstrumentMemoryAccessPass::doFinalization(Function &F)
{
    return false;
}

void InstrumentMemoryAccessPass::getAnalysisUsage(AnalysisUsage &info) const
{
    info.setPreservesCFG();
}

const char * InstrumentMemoryAccessPass::getPassName() const
{
    return "Instrument Memory Access";
}