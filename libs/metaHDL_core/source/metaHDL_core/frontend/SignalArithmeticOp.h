#pragma once

#include "Signal.h"
#include "Bit.h"
#include "BitVector.h"
#include "Scope.h"

#include "../hlim/coreNodes/Node_Signal.h"
#include "../hlim/coreNodes/Node_Arithmetic.h"

#include "../utils/Preprocessor.h"
#include "../utils/Traits.h"


#include <boost/format.hpp>


namespace mhdl::core::frontend {

class SignalArithmeticOp
{
    public:
        SignalArithmeticOp(hlim::Node_Arithmetic::Op op) : m_op(op) { }
        
        hlim::ConnectionType getResultingType(const hlim::ConnectionType &lhs, const hlim::ConnectionType &rhs);
        
        template<typename SignalType, typename = std::enable_if_t<utils::isNumberSignal<SignalType>::value>>
        SignalType operator()(const SignalType &lhs, const SignalType &rhs);

        inline hlim::Node_Arithmetic::Op getOp() const { return m_op; }
    protected:
        hlim::Node_Arithmetic::Op m_op;
        /// extend etc.
};


template<typename SignalType, typename>
SignalType SignalArithmeticOp::operator()(const SignalType &lhs, const SignalType &rhs) {
    hlim::Node_Signal *lhsSignal = lhs.getNode();
    hlim::Node_Signal *rhsSignal = rhs.getNode();
    MHDL_ASSERT(lhsSignal != nullptr);
    MHDL_ASSERT(rhsSignal != nullptr);
    
    hlim::Node_Arithmetic *node = DesignScope::createNode<hlim::Node_Arithmetic>(m_op);
    node->recordStackTrace();
    node->connectInput(0, {.node = lhsSignal, .port = 0});
    node->connectInput(1, {.node = rhsSignal, .port = 0});

    return SignalType({.node = node, .port = 0});
}


#define MHDL_BUILD_ARITHMETIC_OPERATOR(typeTrait, cppOp, Op)                                    \
    template<typename SignalType, typename = std::enable_if_t<typeTrait<SignalType>::value>>    \
    SignalType cppOp(const SignalType &lhs, const SignalType &rhs)  {                           \
        SignalArithmeticOp op(Op);                                                              \
        return op(lhs, rhs);                                                                    \
    }
    
MHDL_BUILD_ARITHMETIC_OPERATOR(utils::isNumberSignal, add, hlim::Node_Arithmetic::ADD)
MHDL_BUILD_ARITHMETIC_OPERATOR(utils::isNumberSignal, operator+, hlim::Node_Arithmetic::ADD)
MHDL_BUILD_ARITHMETIC_OPERATOR(utils::isNumberSignal, sub, hlim::Node_Arithmetic::SUB)
MHDL_BUILD_ARITHMETIC_OPERATOR(utils::isNumberSignal, operator-, hlim::Node_Arithmetic::SUB)
MHDL_BUILD_ARITHMETIC_OPERATOR(utils::isNumberSignal, mul, hlim::Node_Arithmetic::MUL)
MHDL_BUILD_ARITHMETIC_OPERATOR(utils::isNumberSignal, operator*, hlim::Node_Arithmetic::MUL)
MHDL_BUILD_ARITHMETIC_OPERATOR(utils::isNumberSignal, div, hlim::Node_Arithmetic::DIV)
MHDL_BUILD_ARITHMETIC_OPERATOR(utils::isNumberSignal, operator/, hlim::Node_Arithmetic::DIV)
MHDL_BUILD_ARITHMETIC_OPERATOR(utils::isNumberSignal, rem, hlim::Node_Arithmetic::REM)
MHDL_BUILD_ARITHMETIC_OPERATOR(utils::isNumberSignal, operator%, hlim::Node_Arithmetic::REM)

#undef MHDL_BUILD_ARITHMETIC_OPERATOR


#define MHDL_BUILD_ARITHMETIC_ASSIGNMENT_OPERATOR(typeTrait, cppOp, Op)                         \
    template<typename SignalType, typename = std::enable_if_t<typeTrait<SignalType>::value>>    \
    SignalType &cppOp(SignalType lhs, const SignalType &rhs)  {                                 \
        SignalArithmeticOp op(Op);                                                              \
        return lhs = op(lhs, rhs);                                                              \
    }

MHDL_BUILD_ARITHMETIC_ASSIGNMENT_OPERATOR(utils::isNumberSignal, operator+=, hlim::Node_Arithmetic::ADD)
MHDL_BUILD_ARITHMETIC_ASSIGNMENT_OPERATOR(utils::isNumberSignal, operator-=, hlim::Node_Arithmetic::SUB)
MHDL_BUILD_ARITHMETIC_ASSIGNMENT_OPERATOR(utils::isNumberSignal, operator*=, hlim::Node_Arithmetic::MUL)
MHDL_BUILD_ARITHMETIC_ASSIGNMENT_OPERATOR(utils::isNumberSignal, operator/=, hlim::Node_Arithmetic::DIV)
MHDL_BUILD_ARITHMETIC_ASSIGNMENT_OPERATOR(utils::isNumberSignal, operator%=, hlim::Node_Arithmetic::REM)
    
#undef MHDL_BUILD_ARITHMETIC_ASSIGNMENT_OPERATOR

}
