#pragma once

#include "Signal.h"
#include "Bit.h"
#include "BitVector.h"

#include "../hlim/coreNodes/Node_Logic.h"

#include "../utils/Preprocessor.h"
#include "../utils/Traits.h"


#include <boost/format.hpp>


namespace mhdl {
namespace core {    
namespace frontend {
    
class SignalLogicOp
{
    public:
        SignalLogicOp(hlim::Node_Logic::Op op) : m_op(op) { }
        
        hlim::ConnectionType getResultingType(const hlim::ConnectionType &lhs, const hlim::ConnectionType &rhs);
        
        template<typename SignalType, typename = std::enable_if_t<utils::isElementarySignal<SignalType>::value>>
        SignalType operator()(const SignalType &lhs, const SignalType &rhs);

        template<typename SignalType, typename = std::enable_if_t<utils::isElementarySignal<SignalType>::value>>
        SignalType operator()(const SignalType &operand);

        inline hlim::Node_Logic::Op getOp() const { return m_op; }
    protected:
        hlim::Node_Logic::Op m_op;
};

template<typename SignalType, typename>
SignalType SignalLogicOp::operator()(const SignalType &lhs, const SignalType &rhs) {
    MHDL_DESIGNCHECK_HINT(m_op != hlim::Node_Logic::NOT, "Trying to perform a not operation with two operands.");
    
    hlim::Node_Signal *lhsSignal = lhs.getNode();
    hlim::Node_Signal *rhsSignal = rhs.getNode();
    MHDL_ASSERT(lhsSignal != nullptr);
    MHDL_ASSERT(rhsSignal != nullptr);
    MHDL_DESIGNCHECK_HINT(lhsSignal->getConnectionType() == rhsSignal->getConnectionType(), "Can only perform logic operations on operands of same type (e.g. width).");
    
    hlim::Node_Logic *node = Scope::getCurrentNodeGroup()->addNode<hlim::Node_Logic>(m_op);
    node->recordStackTrace();
    lhsSignal->getOutput(0).connect(node->getInput(0));
    rhsSignal->getOutput(0).connect(node->getInput(1));

    return SignalType(&node->getOutput(0), getResultingType(lhsSignal->getConnectionType(), rhsSignal->getConnectionType()));
}

template<typename SignalType, typename>
SignalType SignalLogicOp::operator()(const SignalType &lhs) {
    MHDL_DESIGNCHECK_HINT(m_op == hlim::Node_Logic::NOT, "Trying to perform a non-not operation with one operand.");
    
    hlim::Node_Signal *lhsSignal = lhs.getNode();
    MHDL_ASSERT(lhsSignal != nullptr);
    
    hlim::Node_Logic *node = Scope::getCurrentNodeGroup()->addNode<hlim::Node_Logic>(m_op);
    node->recordStackTrace();
    lhsSignal->getOutput(0).connect(node->getInput(0));

    return SignalType(&node->getOutput(0), lhsSignal->getConnectionType());
}



#define MHDL_BUILD_LOGIC_OPERATOR(typeTrait, cppOp, Op)                                         \
    template<typename SignalType, typename = std::enable_if_t<typeTrait<SignalType>::value>>    \
    SignalType cppOp(const SignalType &lhs, const SignalType &rhs)  {                           \
        SignalLogicOp op(Op);                                                                   \
        return op(lhs, rhs);                                                                    \
    }
    
MHDL_BUILD_LOGIC_OPERATOR(utils::isElementarySignal, operator&, hlim::Node_Logic::AND)
MHDL_BUILD_LOGIC_OPERATOR(utils::isElementarySignal, operator|, hlim::Node_Logic::OR)
MHDL_BUILD_LOGIC_OPERATOR(utils::isElementarySignal, operator^, hlim::Node_Logic::XOR)
//MHDL_BUILD_LOGIC_OPERATOR(utils::isElementarySignal, operator~, hlim::Node_Logic::NOT)

MHDL_BUILD_LOGIC_OPERATOR(utils::isElementarySignal, nand, hlim::Node_Logic::NAND)
MHDL_BUILD_LOGIC_OPERATOR(utils::isElementarySignal, nor, hlim::Node_Logic::NOR)
MHDL_BUILD_LOGIC_OPERATOR(utils::isElementarySignal, bitwiseEqual, hlim::Node_Logic::EQ)
    
MHDL_BUILD_LOGIC_OPERATOR(utils::isBitSignal, operator&&, hlim::Node_Logic::AND)
MHDL_BUILD_LOGIC_OPERATOR(utils::isBitSignal, operator||, hlim::Node_Logic::OR)
MHDL_BUILD_LOGIC_OPERATOR(utils::isBitSignal, operator==, hlim::Node_Logic::EQ)

#undef MHDL_BUILD_LOGIC_OPERATOR


#define MHDL_BUILD_LOGIC_ASSIGNMENT_OPERATOR(typeTrait, cppOp, Op)                         \
    template<typename SignalType, typename = std::enable_if_t<typeTrait<SignalType>::value>>    \
    SignalType &cppOp(SignalType lhs, const SignalType &rhs)  {                                 \
        SignalLogicOp op(Op);                                                              \
        return lhs = op(lhs, rhs);                                                              \
    }

MHDL_BUILD_LOGIC_ASSIGNMENT_OPERATOR(utils::isElementarySignal, operator&=, hlim::Node_Logic::AND)
MHDL_BUILD_LOGIC_ASSIGNMENT_OPERATOR(utils::isElementarySignal, operator|=, hlim::Node_Logic::OR)
MHDL_BUILD_LOGIC_ASSIGNMENT_OPERATOR(utils::isElementarySignal, operator^=, hlim::Node_Logic::XOR)

#undef MHDL_BUILD_LOGIC_ASSIGNMENT_OPERATOR



}
}
}

