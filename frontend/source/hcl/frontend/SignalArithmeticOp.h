#pragma once

#include "Signal.h"
#include "Bit.h"
#include "BitVector.h"
#include "Scope.h"
#include "Registers.h"

#include <hcl/hlim/coreNodes/Node_Signal.h>
#include <hcl/hlim/coreNodes/Node_Arithmetic.h>

#include <hcl/utils/Preprocessor.h>
#include <hcl/utils/Traits.h>


#include <boost/format.hpp>


namespace hcl::core::frontend {

class SignalArithmeticOp
{
    public:
        SignalArithmeticOp(hlim::Node_Arithmetic::Op op) : m_op(op) { }
        
        BVec operator()(const BVec &lhs, const BVec &rhs);

        inline hlim::Node_Arithmetic::Op getOp() const { return m_op; }
    protected:
        hlim::Node_Arithmetic::Op m_op;
        
};


#define HCL_BUILD_ARITHMETIC_OPERATOR(cppOp, Op)                                    \
    inline BVec cppOp(const BVec &lhs, const BVec &rhs)  {                          \
        SignalArithmeticOp op(Op);                                                  \
        return op(lhs, rhs);                                                        \
    }                                                                               \
    
// add_ext for add extend    
    
HCL_BUILD_ARITHMETIC_OPERATOR(add, hlim::Node_Arithmetic::ADD)
HCL_BUILD_ARITHMETIC_OPERATOR(operator+, hlim::Node_Arithmetic::ADD)
HCL_BUILD_ARITHMETIC_OPERATOR(sub, hlim::Node_Arithmetic::SUB)
HCL_BUILD_ARITHMETIC_OPERATOR(operator-, hlim::Node_Arithmetic::SUB)
HCL_BUILD_ARITHMETIC_OPERATOR(mul, hlim::Node_Arithmetic::MUL)
HCL_BUILD_ARITHMETIC_OPERATOR(operator*, hlim::Node_Arithmetic::MUL)
HCL_BUILD_ARITHMETIC_OPERATOR(div, hlim::Node_Arithmetic::DIV)
HCL_BUILD_ARITHMETIC_OPERATOR(operator/, hlim::Node_Arithmetic::DIV)
HCL_BUILD_ARITHMETIC_OPERATOR(rem, hlim::Node_Arithmetic::REM)
HCL_BUILD_ARITHMETIC_OPERATOR(operator%, hlim::Node_Arithmetic::REM)

#undef HCL_BUILD_ARITHMETIC_OPERATOR


#define HCL_BUILD_ARITHMETIC_ASSIGNMENT_OPERATOR(typeTrait, cppOp, Op)                          \
    inline BVec &cppOp(BVec &lhs, const BVec &rhs)  {                                           \
        SignalArithmeticOp op(Op);                                                              \
        return lhs = op(lhs, rhs);                                                              \
    }                                                                                           \
    template<typename SignalType>                                                               \
    inline Register<SignalType> &cppOp(Register<SignalType> &lhs, const BVec &rhs)  {           \
        SignalArithmeticOp op(Op);                                                              \
        return lhs = op(lhs, rhs);                                                              \
    }                                                                                           \
    inline void cppOp(BVecSlice &&lhs, const BVec &rhs)  {                                        \
        SignalArithmeticOp op(Op);                                                              \
        lhs = op(lhs, rhs);                                                                           \
    } 
    

    
HCL_BUILD_ARITHMETIC_ASSIGNMENT_OPERATOR(utils::isBitVectorSignalLike, operator+=, hlim::Node_Arithmetic::ADD)
HCL_BUILD_ARITHMETIC_ASSIGNMENT_OPERATOR(utils::isBitVectorSignalLike, operator-=, hlim::Node_Arithmetic::SUB)
HCL_BUILD_ARITHMETIC_ASSIGNMENT_OPERATOR(utils::isBitVectorSignalLike, operator*=, hlim::Node_Arithmetic::MUL)
HCL_BUILD_ARITHMETIC_ASSIGNMENT_OPERATOR(utils::isBitVectorSignalLike, operator/=, hlim::Node_Arithmetic::DIV)
HCL_BUILD_ARITHMETIC_ASSIGNMENT_OPERATOR(utils::isBitVectorSignalLike, operator%=, hlim::Node_Arithmetic::REM)
    
#undef HCL_BUILD_ARITHMETIC_ASSIGNMENT_OPERATOR

}
