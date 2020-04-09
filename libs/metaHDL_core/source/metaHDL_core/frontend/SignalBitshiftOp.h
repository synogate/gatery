#pragma once

#include "Signal.h"
#include "Bit.h"
#include "BitVector.h"
#include "Scope.h"

#include "../hlim/coreNodes/Node_Signal.h"
#include "../hlim/coreNodes/Node_Rewire.h"

#include "../utils/Preprocessor.h"
#include "../utils/Traits.h"


#include <boost/format.hpp>


namespace mhdl {
namespace core {    
namespace frontend {
    
class SignalBitShift
{
    public:
        SignalBitShift(int shift) : m_shift(shift) { }
        
        SignalBitShift &setFillLeft(bool bit);
        SignalBitShift &setFillRight(bool bit);
        SignalBitShift &duplicateLeft();
        SignalBitShift &duplicateRight();
        
        hlim::ConnectionType getResultingType(const hlim::ConnectionType &operand);
        
        template<typename SignalType, typename = std::enable_if_t<utils::isNumberSignal<SignalType>::value>>
        SignalType operator()(const SignalType &lhs, const SignalType &rhs);
    protected:
        int m_shift;
        /// extend etc.
};


template<typename SignalType, typename = std::enable_if_t<utils::isNumberSignal<SignalType>::value>>
SignalType SignalArithmeticOp::operator()(const SignalType &lhs, const SignalType &rhs) {
    const hlim::Node_Signal *lhsSignal = dynamic_cast<const hlim::Node_Signal*>(lhs.getOutputPort()->node);
    const hlim::Node_Signal *rhsSignal = dynamic_cast<const hlim::Node_Signal*>(rhs.getOutputPort()->node);
    MHDL_ASSERT(lhsSignal != nullptr);
    MHDL_ASSERT(rhsSignal != nullptr);
    MHDL_DESIGNCHECK_HINT(lhsSignal->getConnectionType() == rhsSignal->getConnectionType(), "Can only perform arithmetic operations on operands of same type (e.g. width).");
    
    hlim::Node_Arithmetic *node = Scope::getCurrentNodeGroup()->addNode<hlim::Node_Arithmetic>(m_op);
    node->recordStackTrace();
    lhs.getOutputPort()->connect(node->getInput(0));
    rhs.getOutputPort()->connect(node->getInput(1));

    return SignalType(&node->getOutput(0), getResultingType(lhsSignal->getConnectionType(), rhsSignal->getConnectionType()));
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
}
}
