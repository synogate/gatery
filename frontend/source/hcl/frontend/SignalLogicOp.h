#pragma once

#include "Signal.h"
#include "Bit.h"
#include "BitVector.h"
#include "Registers.h"

#include <hcl/hlim/coreNodes/Node_Logic.h>

#include <hcl/utils/Preprocessor.h>
#include <hcl/utils/Traits.h>


#include <boost/format.hpp>


namespace hcl::core::frontend {
    
class SignalLogicOp
{
    public:
        SignalLogicOp(hlim::Node_Logic::Op op) : m_op(op) { }
        
        hlim::ConnectionType getResultingType(const hlim::ConnectionType &lhs, const hlim::ConnectionType &rhs);
        
        BVec operator()(const BVec &lhs, const BVec &rhs) { return create(lhs, rhs); }
        Bit operator()(const Bit &lhs, const Bit &rhs) { return create(lhs, rhs); }

        BVec operator()(const BVec &operand) { return create(operand); }
        Bit operator()(const Bit &operand) { return create(operand); }

        inline hlim::Node_Logic::Op getOp() const { return m_op; }
    protected:
        hlim::Node_Logic::Op m_op;
        
        template<typename SignalType>
        SignalType create(const SignalType &lhs, const SignalType &rhs);
        
        template<typename SignalType>
        SignalType create(const SignalType &lhs);
};

template<typename SignalType>
SignalType SignalLogicOp::create(const SignalType &lhs, const SignalType &rhs)
{
    HCL_DESIGNCHECK_HINT(m_op != hlim::Node_Logic::NOT, "Trying to perform a not operation with two operands.");
    HCL_DESIGNCHECK_HINT(lhs.getConnType() == lhs.getConnType(), "Can only perform logic operations on operands of same type (e.g. width).");
    
    hlim::Node_Logic *node = DesignScope::createNode<hlim::Node_Logic>(m_op);
    node->recordStackTrace();
    node->connectInput(0, lhs.getReadPort());
    node->connectInput(1, rhs.getReadPort());

    return SignalType({.node = node, .port = 0ull});
}

template<typename SignalType>
SignalType SignalLogicOp::create(const SignalType &lhs)
{
    HCL_DESIGNCHECK_HINT(m_op == hlim::Node_Logic::NOT, "Trying to perform a non-not operation with one operand.");
    
    hlim::Node_Logic *node = DesignScope::createNode<hlim::Node_Logic>(m_op);
    node->recordStackTrace();
    node->connectInput(0, lhs.getReadPort());

    return SignalType({.node = node, .port = 0ull});
}



#define HCL_BUILD_LOGIC_OPERATOR(cppOp, Op)                           \
    inline BVec cppOp(const BVec &lhs, const BVec &rhs) {                    \
        SignalLogicOp op(Op);                                         \
        return op(lhs, rhs);                                          \
    }                                                                 \
    inline BVec cppOp(const Bit &lhs, const BVec &rhs) {                     \
        SignalLogicOp op(Op);                                         \
        return op(lhs.sext(rhs.getWidth()), rhs);                     \
    }                                                                 \
    inline BVec cppOp(const BVec &lhs, const Bit &rhs) {                     \
        SignalLogicOp op(Op);                                         \
        return op(lhs, rhs.sext(lhs.getWidth()));                     \
    }                                                                 \
    inline Bit cppOp(const Bit &lhs, const Bit &rhs) {                       \
        SignalLogicOp op(Op);                                         \
        return op(lhs, rhs);                                          \
    }                                                                 \

#define HCL_BUILD_LOGIC_OPERATOR_UNARY(cppOp, Op)                      \
    inline BVec cppOp(const BVec &lhs)  {                                     \
        SignalLogicOp op(Op);                                          \
        return op(lhs);                                                \
    }                                                                  \
    inline Bit cppOp(const Bit &lhs)  {                                       \
        SignalLogicOp op(Op);                                          \
        return op(lhs);                                                \
    }

#define HCL_BUILD_LOGIC_OPERATOR_BIT_ONLY(cppOp, Op)                  \
    inline Bit cppOp(const Bit &lhs, const Bit &rhs) {                       \
        SignalLogicOp op(Op);                                         \
        return op(lhs, rhs);                                          \
    }                                                                 \

#define HCL_BUILD_LOGIC_OPERATOR_UNARY_BIT_ONLY(cppOp, Op)             \
    inline Bit cppOp(const Bit &lhs)  {                                       \
        SignalLogicOp op(Op);                                          \
        return op(lhs);                                                \
    }

    
HCL_BUILD_LOGIC_OPERATOR(operator&, hlim::Node_Logic::AND)
HCL_BUILD_LOGIC_OPERATOR(operator|, hlim::Node_Logic::OR)
HCL_BUILD_LOGIC_OPERATOR(operator^, hlim::Node_Logic::XOR)
HCL_BUILD_LOGIC_OPERATOR_UNARY(operator~, hlim::Node_Logic::NOT)

HCL_BUILD_LOGIC_OPERATOR(nand, hlim::Node_Logic::NAND)
HCL_BUILD_LOGIC_OPERATOR(nor, hlim::Node_Logic::NOR)
HCL_BUILD_LOGIC_OPERATOR(bitwiseEqual, hlim::Node_Logic::EQ)
    
HCL_BUILD_LOGIC_OPERATOR_BIT_ONLY(operator&&, hlim::Node_Logic::AND)
HCL_BUILD_LOGIC_OPERATOR_BIT_ONLY(operator||, hlim::Node_Logic::OR)
HCL_BUILD_LOGIC_OPERATOR_UNARY_BIT_ONLY(operator!, hlim::Node_Logic::NOT)

#undef HCL_BUILD_LOGIC_OPERATOR
#undef HCL_BUILD_LOGIC_OPERATOR_BIT_ONLY
#undef HCL_BUILD_LOGIC_OPERATOR_UNARY
#undef HCL_BUILD_LOGIC_OPERATOR_UNARY_BIT_ONLY


#define HCL_BUILD_LOGIC_ASSIGNMENT_OPERATOR(typeTrait, cppOp, Op)                               \
    inline BVec &cppOp(BVec &lhs, const BVec &rhs)  {                                           \
        SignalLogicOp op(Op);                                                                   \
        return lhs = op(lhs, rhs);                                                              \
    }                                                                                           \
    inline BVec &cppOp(BVec &lhs, const Bit &rhs)  {                                            \
        SignalLogicOp op(Op);                                                                   \
        return lhs = op(lhs, rhs.sext(lhs.getWidth()));                                         \
    }                                                                                           \
    inline Bit &cppOp(Bit &lhs, const Bit &rhs)  {                                              \
        SignalLogicOp op(Op);                                                                   \
        return lhs = op(lhs, rhs);                                                              \
    }                                                                                           \
    template<typename SignalType>                                                               \
    inline Register<SignalType> &cppOp(Register<SignalType> &lhs, const BVec &rhs)  {           \
        SignalLogicOp op(Op);                                                                   \
        return lhs = op(lhs, rhs);                                                              \
    }                                                                                           \
    inline void cppOp(BVecSlice &&lhs, const BVec &rhs)  {                                        \
        SignalLogicOp op(Op);                                                                   \
        lhs = op(lhs, rhs);                                                                           \
    }                                                                                           \
    inline void cppOp(BVecSlice &&lhs, const Bit &rhs)  {                                         \
        SignalLogicOp op(Op);                                                                   \
        lhs = op(lhs, rhs.sext(lhs.size()));                                                          \
    } 

HCL_BUILD_LOGIC_ASSIGNMENT_OPERATOR(utils::isElementarySignal, operator&=, hlim::Node_Logic::AND)
HCL_BUILD_LOGIC_ASSIGNMENT_OPERATOR(utils::isElementarySignal, operator|=, hlim::Node_Logic::OR)
HCL_BUILD_LOGIC_ASSIGNMENT_OPERATOR(utils::isElementarySignal, operator^=, hlim::Node_Logic::XOR)

#undef HCL_BUILD_LOGIC_ASSIGNMENT_OPERATOR



}

