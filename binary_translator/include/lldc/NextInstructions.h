#ifndef _NEXT_INSTRUCTIONS_H
#define _NEXT_INSTRUCTIONS_H

#define MAX_NEXT_INSTRUCTIONS 5

class ReverseTranslateBasicBlock;

class NextInstructions
{
public:
    enum FlowType {
        FLOW_NONE,
        FLOW_NORMAL,
        FLOW_BRANCH,
        FLOW_CALL,
        FLOW_RETURN,
        FLOW_CONDITIONAL_BRANCH,
        FLOW_CONDITIONAL_CALL,
        FLOW_CONDITIONAL_RETURN,
        FLOW_CONDITIONAL_INSTRUCTION,
        FLOW_EXCEPTION
    };
    
private:
    struct NextInstruction {
        FlowType flow_type;
        uint64_t pc;
        ReverseTranslateBasicBlock* bb;
    };
    
    struct NextInstruction nextInstructions[MAX_NEXT_INSTRUCTIONS];
    unsigned numNextInstructions;
    
public:
    NextInstructions() 
    : numNextInstructions(0) {
    }
    
    void addNextInstruction(FlowType flow_type, uint64_t pc, ReverseTranslateBasicBlock* bb) {
        this->nextInstructions[this->numNextInstructions].pc = pc;
        this->nextInstructions[this->numNextInstructions].flow_type = flow_type;
        this->nextInstructions[this->numNextInstructions].bb = bb;
        this->numNextInstructions += 1;
    }
    
    uint64_t getProgramCounter(unsigned idx) {
        return this->nextInstructions[idx].pc;
    }
    
    FlowType getFlowType(unsigned idx) {
        return this->nextInstructions[idx].flow_type;
    }
    
    //TODO: Rename, does not return a BB
    ReverseTranslateBasicBlock* getBasicBlock(unsigned idx) {
        return this->nextInstructions[idx].bb;
    }
    
    unsigned getNumNextInstructions() {
        return this->numNextInstructions;
    }
    
    void clear() {
        this->numNextInstructions = 0;
    }
    
    bool endsBasicBlock() {
        for (unsigned i = 0; i < this->getNumNextInstructions(); i++)
    		if (this->getFlowType(i) != NextInstructions::FLOW_NORMAL && 
    		    this->getFlowType(i) != NextInstructions::FLOW_CALL) 
    		{
                return true;
    		}
        return false;
    }
};

#endif /* _NEXT_INSTRUCTIONS_H */
