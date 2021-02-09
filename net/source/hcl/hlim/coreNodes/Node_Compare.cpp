#include "Node_Compare.h"


namespace hcl::core::hlim {

Node_Compare::Node_Compare(Op op) : Node(2, 1), m_op(op)
{
    ConnectionType conType;
    conType.width = 1;
    conType.interpretation = ConnectionType::BOOL;
    setOutputConnectionType(0, conType);
}

void Node_Compare::simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const
{
    auto leftDriver = getNonSignalDriver(0);
    auto rightDriver = getNonSignalDriver(1);
    if (leftDriver.node == nullptr || rightDriver.node == nullptr) {
        state.setRange(sim::DefaultConfig::DEFINED, outputOffsets[0], getOutputConnectionType(0).width, false);
        return;
    }
    
    const auto &leftType = leftDriver.node->getOutputConnectionType(leftDriver.port);
    const auto &rightType = rightDriver.node->getOutputConnectionType(rightDriver.port);
    HCL_ASSERT_HINT(leftType.width <= 64, "Compare with more than 64 bits not yet implemented!");
    HCL_ASSERT_HINT(rightType.width <= 64, "Compare with more than 64 bits not yet implemented!");
    HCL_ASSERT_HINT(leftType.interpretation == rightType.interpretation, "Comparing signals with different interpretations not yet implemented!");
    
    if (!allDefinedNonStraddling(state, inputOffsets[0], leftType.width)) {
        state.setRange(sim::DefaultConfig::DEFINED, outputOffsets[0], getOutputConnectionType(0).width, false);
        return;
    }
    if (!allDefinedNonStraddling(state, inputOffsets[1], rightType.width)) {
        state.setRange(sim::DefaultConfig::DEFINED, outputOffsets[0], getOutputConnectionType(0).width, false);
        return;
    }
   
    std::uint64_t left = state.extractNonStraddling(sim::DefaultConfig::VALUE, inputOffsets[0], leftType.width);
    std::uint64_t right = state.extractNonStraddling(sim::DefaultConfig::VALUE, inputOffsets[1], rightType.width);
    bool result;
    
    switch (leftType.interpretation) {
        case ConnectionType::BOOL:
            switch (m_op) {
                case EQ:
                    result = left == right;
                break;
                case NEQ:
                    result = left != right;
                break;
                default:
                    HCL_ASSERT_HINT(false, "Unhandled case!");
            }
        break;
        case ConnectionType::BITVEC:
            switch (m_op) {
                case EQ:
                    result = left == right;
                break;
                case NEQ:
                    result = left != right;
                break;
                case LT:
                    result = left < right;
                break;
                case GT:
                    result = left > right;
                break;
                case LEQ:
                    result = left <= right;
                break;
                case GEQ:
                    result = left >= right;
                break;
                default:
                    HCL_ASSERT_HINT(false, "Unhandled case!");
            }
        break;
        default:
            HCL_ASSERT_HINT(false, "Unhandled case!");
    }
    
    state.insertNonStraddling(sim::DefaultConfig::VALUE, outputOffsets[0], 1, result?1:0);
    state.insertNonStraddling(sim::DefaultConfig::DEFINED, outputOffsets[0], 1, 1);
}


std::string Node_Compare::getTypeName() const 
{ 
    switch (m_op) {
        case EQ:  return "==";
        case NEQ: return "!=";
        case LT:  return "<";
        case GT:  return ">";
        case LEQ: return "<=";
        case GEQ: return ">=";
        default:  return "Compare"; 
    }
}

void Node_Compare::assertValidity() const 
{ 
    
}

std::string Node_Compare::getInputName(size_t idx) const 
{ 
    return idx==0?"a":"b"; 
}

std::string Node_Compare::getOutputName(size_t idx) const 
{ 
    return "out"; 
}

std::unique_ptr<BaseNode> Node_Compare::cloneUnconnected() const 
{
    std::unique_ptr<BaseNode> res(new Node_Compare(m_op));
    copyBaseToClone(res.get());
    return res;
}


}
