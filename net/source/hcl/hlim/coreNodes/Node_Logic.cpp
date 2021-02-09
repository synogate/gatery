#include "Node_Logic.h"

namespace hcl::core::hlim {

Node_Logic::Node_Logic(Op op) : Node(op==NOT?1:2, 1), m_op(op) 
{ 
    
}

void Node_Logic::connectInput(size_t operand, const NodePort &port) 
{ 
    NodeIO::connectInput(operand, port); 
    updateConnectionType();
}

void Node_Logic::disconnectInput(size_t operand) 
{ 
    NodeIO::disconnectInput(operand); 
}

void Node_Logic::simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const
{
    size_t width = getOutputConnectionType(0).width;

    NodePort leftDriver = getNonSignalDriver(0);
    bool leftAllUndefined = leftDriver.node == nullptr;
    /*
    if (leftDriver.node == nullptr) {
        state.setRange(sim::DefaultConfig::DEFINED, outputOffsets[0], getOutputConnectionType(0).width, false);
        return;
    }
    */

    NodePort rightDriver; 
    bool rightAllUndefined = true;
    if (m_op != NOT) {
        rightDriver = getNonSignalDriver(0);
        rightAllUndefined = rightDriver.node == nullptr;
    }

    size_t offset = 0;
    
    while (offset < width) {
        size_t chunkSize = std::min<size_t>(64, width-offset);
        
        std::uint64_t left, leftDefined, right, rightDefined;
        
        if (leftAllUndefined)
            leftDefined = 0;
        else {
            leftDefined = state.extractNonStraddling(sim::DefaultConfig::DEFINED, inputOffsets[0]+offset, chunkSize);
            left = state.extractNonStraddling(sim::DefaultConfig::VALUE, inputOffsets[0]+offset, chunkSize);
        }
        
        if (rightAllUndefined || m_op == NOT)
            rightDefined = 0;
        else {
            rightDefined = state.extractNonStraddling(sim::DefaultConfig::DEFINED, inputOffsets[1]+offset, chunkSize);
            right = state.extractNonStraddling(sim::DefaultConfig::VALUE, inputOffsets[1]+offset, chunkSize);
        }
        
        std::uint64_t result, resultDefined;
        switch (m_op) {
            case AND:
                result = left & right;
                resultDefined = (leftDefined & !left) | (rightDefined & !right) | (leftDefined & rightDefined);
            break;
            case NAND:
                result = ~(left & right);
                resultDefined = (leftDefined & !left) | (rightDefined & !right) | (leftDefined & rightDefined);
            break;
            case OR:
                result = left | right;
                resultDefined = (leftDefined & left) | (rightDefined & right) | (leftDefined & rightDefined);
            break;
            case NOR:
                result = ~(left | right);
                resultDefined = (leftDefined & left) | (rightDefined & right) | (leftDefined  & rightDefined);
            break;
            case XOR:
                result = left ^ right;
                resultDefined = leftDefined & rightDefined;
            break;
            case EQ:
                result = ~(left ^ right);
                resultDefined = leftDefined & rightDefined;
            break;
            case NOT:
                result = ~left;
                resultDefined = leftDefined;
            break;
        };
                    
        state.insertNonStraddling(sim::DefaultConfig::VALUE, outputOffsets[0] + offset, chunkSize, result);        
        state.insertNonStraddling(sim::DefaultConfig::DEFINED, outputOffsets[0] + offset, chunkSize, resultDefined);        
        offset += chunkSize;
    }
}

std::string Node_Logic::getTypeName() const 
{ 
    switch (m_op) {
        case AND: return "and";
        case NAND: return "nand";
        case OR: return "or";
        case NOR: return "nor";
        case XOR: return "xor";
        case EQ: return "bitwise-equal";
        case NOT: return "not";
        default: 
            return "Logic"; 
    }
}

void Node_Logic::assertValidity() const 
{ 
    
}

std::string Node_Logic::getInputName(size_t idx) const 
{ 
    return idx==0?"a":"b"; 
}

std::string Node_Logic::getOutputName(size_t idx) const 
{ 
    return "output"; 
}

    
void Node_Logic::updateConnectionType()
{
    auto lhs = getDriver(0);
    NodePort rhs;
    if (m_op != NOT)
        rhs = getDriver(1);
    
    ConnectionType desiredConnectionType = getOutputConnectionType(0);
    
    if (lhs.node != nullptr) {
        if (rhs.node != nullptr) {
            desiredConnectionType = lhs.node->getOutputConnectionType(lhs.port);
            HCL_ASSERT_HINT(desiredConnectionType == rhs.node->getOutputConnectionType(rhs.port), "Support for differing types of input to logic node not yet implemented");
            //desiredConnectionType.width = std::max(desiredConnectionType.width, rhs.node->getOutputConnectionType(rhs.port).width);
        } else
            desiredConnectionType = lhs.node->getOutputConnectionType(lhs.port);
    } else if (rhs.node != nullptr)
        desiredConnectionType = rhs.node->getOutputConnectionType(rhs.port);
    
    setOutputConnectionType(0, desiredConnectionType);
}


std::unique_ptr<BaseNode> Node_Logic::cloneUnconnected() const 
{
    std::unique_ptr<BaseNode> res(new Node_Logic(m_op));
    copyBaseToClone(res.get());
    return res;
}


}
