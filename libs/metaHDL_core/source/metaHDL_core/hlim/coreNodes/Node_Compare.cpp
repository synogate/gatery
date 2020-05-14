#include "Node_Compare.h"


namespace mhdl::core::hlim {

Node_Compare::Node_Compare(Op op) : Node(2, 1), m_op(op)
{
    ConnectionType conType;
    conType.width = 1;
    conType.interpretation = ConnectionType::BOOL;
    setOutputConnectionType(0, conType);
}

void Node_Compare::simulateEvaluate(sim::DefaultBitVectorState &state, const size_t internalOffset, const size_t *inputOffsets, const size_t *outputOffsets) const 
{
    auto leftDriver = getNonSignalDriver(0);
    auto rightDriver = getNonSignalDriver(1);
    if (leftDriver.node == nullptr || rightDriver.node == nullptr) return;
    
    const auto &leftType = leftDriver.node->getOutputConnectionType(leftDriver.port);
    const auto &rightType = rightDriver.node->getOutputConnectionType(rightDriver.port);
    MHDL_ASSERT_HINT(leftType.width <= 64, "Compare with more than 64 bits not yet implemented!");
    MHDL_ASSERT_HINT(rightType.width <= 64, "Compare with more than 64 bits not yet implemented!");
    
    if (!allDefinedNonStraddling(state, inputOffsets[0], leftType.width)) return;
    if (!allDefinedNonStraddling(state, inputOffsets[1], rightType.width)) return;
   
    std::uint64_t left = state.extractNonStraddling(sim::DefaultConfig::VALUE, inputOffsets[0], leftType.width);
    std::uint64_t right = state.extractNonStraddling(sim::DefaultConfig::VALUE, inputOffsets[1], rightType.width);
    bool result;
    
    switch (getOutputConnectionType(0).interpretation) {
        case ConnectionType::BOOL:
        case ConnectionType::RAW:
            switch (m_op) {
                case EQ:
                    result = left == right;
                break;
                case NEQ:
                    result = left != right;
                break;
                default:
                    MHDL_ASSERT_HINT(false, "Unhandled case!");
            }
        break;
        case ConnectionType::ONE_HOT:
            MHDL_ASSERT_HINT(false, "Can't do compare on one hot data yet!");
        break;
        case ConnectionType::FLOAT:
            MHDL_ASSERT_HINT(false, "Can't do compare on float data yet!");
        break;
        case ConnectionType::UNSIGNED:
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
                    MHDL_ASSERT_HINT(false, "Unhandled case!");
            }
        break;
        case ConnectionType::SIGNED_2COMPLEMENT:
            switch (m_op) {
                case EQ:
                    result = left == right;
                break;
                case NEQ:
                    result = left != right;
                break;
                default:
                    MHDL_ASSERT_HINT(false, "Case not yet implemented!");
            }
        break;
        default:
            MHDL_ASSERT_HINT(false, "Unhandled case!");
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


}
