#include "Node_Arithmetic.h"

#include "../../utils/BitManipulation.h"

namespace mhdl::core::hlim {

Node_Arithmetic::Node_Arithmetic(Op op) : Node(2, 1), m_op(op) 
{ 
    
}

void Node_Arithmetic::connectInput(size_t operand, const NodePort &port) 
{ 
    NodeIO::connectInput(operand, port); 
    updateConnectionType();
}

void Node_Arithmetic::updateConnectionType()
{
    auto lhs = getDriver(0);
    auto rhs = getDriver(1);
    
    ConnectionType desiredConnectionType = getOutputConnectionType(0);
    
    if (lhs.node != nullptr) {
        if (rhs.node != nullptr) {
            desiredConnectionType = lhs.node->getOutputConnectionType(lhs.port);
            desiredConnectionType.width = std::max(desiredConnectionType.width, rhs.node->getOutputConnectionType(rhs.port).width);
            MHDL_ASSERT_HINT(lhs.node->getOutputConnectionType(lhs.port).interpretation == rhs.node->getOutputConnectionType(rhs.port).interpretation, "Mixing different interpretations not yet implemented!");
        } else
            desiredConnectionType = lhs.node->getOutputConnectionType(lhs.port);
    } else if (rhs.node != nullptr)
        desiredConnectionType = rhs.node->getOutputConnectionType(rhs.port);
    
    setOutputConnectionType(0, desiredConnectionType);
}


void Node_Arithmetic::disconnectInput(size_t operand) 
{ 
    NodeIO::disconnectInput(operand); 
}


void Node_Arithmetic::simulateEvaluate(sim::DefaultBitVectorState &state, size_t *inputOffsets, size_t *outputOffsets)
{
    MHDL_ASSERT_HINT(getOutputConnectionType(0).width <= 64, "Arithmetic with more than 64 bits not yet implemented!");
    auto leftDriver = getNonSignalDriver(0);
    auto rightDriver = getNonSignalDriver(1);
    if (leftDriver.node == nullptr || rightDriver.node == nullptr) return;
    
    const auto &leftType = leftDriver.node->getOutputConnectionType(leftDriver.port);
    const auto &rightType = rightDriver.node->getOutputConnectionType(rightDriver.port);
    MHDL_ASSERT_HINT(leftType.width <= 64, "Arithmetic with more than 64 bits not yet implemented!");
    MHDL_ASSERT_HINT(rightType.width <= 64, "Arithmetic with more than 64 bits not yet implemented!");
    
    if (!state.allDefinedNonStraddling(inputOffsets[0], leftType.width)) return;
    if (!state.allDefinedNonStraddling(inputOffsets[1], rightType.width)) return;
   
    std::uint64_t left = state.extractNonStraddling(sim::DefaultConfig::VALUE, inputOffsets[0], leftType.width);
    std::uint64_t right = state.extractNonStraddling(sim::DefaultConfig::VALUE, inputOffsets[1], rightType.width);
    std::uint64_t result;
    
    switch (getOutputConnectionType(0).interpretation) {
        case ConnectionType::BOOL:
            MHDL_ASSERT_HINT(false, "Can't do arithmetic on booleans!");
        break;
        case ConnectionType::RAW:
            MHDL_ASSERT_HINT(false, "Can't do arithmetic on raw data!");
        break;
        case ConnectionType::ONE_HOT:
            MHDL_ASSERT_HINT(false, "Can't do arithmetic on one hot data!");
        break;
        case ConnectionType::FLOAT:
            MHDL_ASSERT_HINT(false, "Can't do arithmetic on float data yet!");
        break;
        case ConnectionType::UNSIGNED:
            switch (m_op) {
                case ADD:
                    result = left + right;
                break;
                case SUB:
                    result = left - right;
                break;
                case MUL:
                    result = left * right;
                break;
                case DIV:
                    result = left / right;
                break;
                case REM:
                    result = left % right;
                break;
                default:
                    MHDL_ASSERT_HINT(false, "Unhandled case!");
            }
        break;
        case ConnectionType::SIGNED_2COMPLEMENT:
            switch (m_op) {
                case ADD:
                    result = left + right;
                break;
                case SUB:
                    result = left - right;
                break;
                case MUL:
                    MHDL_ASSERT_HINT(false, "Multiplication of signed data types not yet implemented!");
                break;
                case DIV:
                    MHDL_ASSERT_HINT(false, "Division of signed data types not yet implemented!");
                break;
                case REM:
                    MHDL_ASSERT_HINT(false, "Remainder of signed data types not yet implemented!");
                break;
                default:
                    MHDL_ASSERT_HINT(false, "Unhandled case!");
            }
        break;
        default:
            MHDL_ASSERT_HINT(false, "Unhandled case!");
    }
    
    state.insertNonStraddling(sim::DefaultConfig::VALUE, outputOffsets[0], getOutputConnectionType(0).width, result);
    state.insertNonStraddling(sim::DefaultConfig::DEFINED, outputOffsets[0], getOutputConnectionType(0).width, ~0ull);
}


std::string Node_Arithmetic::getTypeName() const 
{ 
    switch (m_op) {
        case ADD: return "add";
        case SUB: return "sub";
        case MUL: return "mul";
        case DIV: return "div";
        case REM: return "remainder";
        default: return "Arithmetic"; 
    }
}

void Node_Arithmetic::assertValidity() const 
{ 
    
}

std::string Node_Arithmetic::getInputName(size_t idx) const 
{ 
    return idx==0?"a":"b"; 
}

std::string Node_Arithmetic::getOutputName(size_t idx) const 
{ 
    return "out";
}

        
}
