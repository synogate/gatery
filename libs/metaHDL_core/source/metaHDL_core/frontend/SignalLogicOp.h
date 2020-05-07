#pragma once

#include "Signal.h"
#include "Bit.h"
#include "BitVector.h"

#include "../hlim/coreNodes/Node_Logic.h"

#include "../utils/Preprocessor.h"
#include "../utils/Traits.h"


#include <boost/format.hpp>


namespace mhdl::core::frontend {
    
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
    MHDL_DESIGNCHECK_HINT(lhsSignal->getOutputConnectionType(0) == rhsSignal->getOutputConnectionType(0), "Can only perform logic operations on operands of same type (e.g. width).");
    
    hlim::Node_Logic *node = DesignScope::createNode<hlim::Node_Logic>(m_op);
    node->recordStackTrace();
    node->connectInput(0, {.node = lhsSignal, .port = 0ull});
    node->connectInput(1, {.node = rhsSignal, .port = 0ull});

    return SignalType({.node = node, .port = 0ull});
}

template<typename SignalType, typename>
SignalType SignalLogicOp::operator()(const SignalType &lhs) {
    MHDL_DESIGNCHECK_HINT(m_op == hlim::Node_Logic::NOT, "Trying to perform a non-not operation with one operand.");
    
    hlim::Node_Signal *lhsSignal = lhs.getNode();
    MHDL_ASSERT(lhsSignal != nullptr);
    
    hlim::Node_Logic *node = DesignScope::createNode<hlim::Node_Logic>(m_op);
    node->recordStackTrace();
    node->connectInput(0, {.node = lhsSignal, .port = 0ull});

    return SignalType({.node = node, .port = 0ull});
}



#define MHDL_BUILD_LOGIC_OPERATOR(typeTrait, cppOp, Op)                                         \
    template<typename SignalType, typename = std::enable_if_t<typeTrait<SignalType>::value>>    \
    SignalType cppOp(const SignalType &lhs, const SignalType &rhs)  {                           \
        SignalLogicOp op(Op);                                                                   \
        return op(lhs, rhs);                                                                    \
    }

#define MHDL_BUILD_LOGIC_OPERATOR_UNARY(typeTrait, cppOp, Op)                                   \
    template<typename SignalType, typename = std::enable_if_t<typeTrait<SignalType>::value>>    \
    SignalType cppOp(const SignalType &lhs)  {                                                  \
        SignalLogicOp op(Op);                                                                   \
        return op(lhs);                                                                         \
    }
    
    
MHDL_BUILD_LOGIC_OPERATOR(utils::isElementarySignal, operator&, hlim::Node_Logic::AND)
MHDL_BUILD_LOGIC_OPERATOR(utils::isElementarySignal, operator|, hlim::Node_Logic::OR)
MHDL_BUILD_LOGIC_OPERATOR(utils::isElementarySignal, operator^, hlim::Node_Logic::XOR)
MHDL_BUILD_LOGIC_OPERATOR_UNARY(utils::isElementarySignal, operator~, hlim::Node_Logic::NOT)

MHDL_BUILD_LOGIC_OPERATOR(utils::isElementarySignal, nand, hlim::Node_Logic::NAND)
MHDL_BUILD_LOGIC_OPERATOR(utils::isElementarySignal, nor, hlim::Node_Logic::NOR)
MHDL_BUILD_LOGIC_OPERATOR(utils::isElementarySignal, bitwiseEqual, hlim::Node_Logic::EQ)
    
MHDL_BUILD_LOGIC_OPERATOR(utils::isBitSignal, operator&&, hlim::Node_Logic::AND)
MHDL_BUILD_LOGIC_OPERATOR(utils::isBitSignal, operator||, hlim::Node_Logic::OR)
MHDL_BUILD_LOGIC_OPERATOR_UNARY(utils::isBitSignal, operator!, hlim::Node_Logic::NOT)

#undef MHDL_BUILD_LOGIC_OPERATOR
#undef MHDL_BUILD_LOGIC_OPERATOR_UNARY


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

