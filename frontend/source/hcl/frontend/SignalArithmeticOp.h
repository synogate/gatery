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


    hlim::NodePort makeNode(hlim::Node_Arithmetic op, BVecSignalPort lhs, BVecSignalPort rhs);

    inline BVec add(BVecSignalPort lhs, BVecSignalPort rhs) { return makeNode(hlim::Node_Arithmetic::ADD, lhs, rhs); }
    inline BVec sub(BVecSignalPort lhs, BVecSignalPort rhs) { return makeNode(hlim::Node_Arithmetic::SUB, lhs, rhs); }
    inline BVec mul(BVecSignalPort lhs, BVecSignalPort rhs) { return makeNode(hlim::Node_Arithmetic::MUL, lhs, rhs); }
    inline BVec div(BVecSignalPort lhs, BVecSignalPort rhs) { return makeNode(hlim::Node_Arithmetic::DIV, lhs, rhs); }
    inline BVec rem(BVecSignalPort lhs, BVecSignalPort rhs) { return makeNode(hlim::Node_Arithmetic::REM, lhs, rhs); }

    inline BVec add(BVecSignalPort lhs, BitSignalPort rhs) { BVec r{ 1 }; r.setBit(0, rhs); return makeNode(hlim::Node_Arithmetic::ADD, lhs, r); }
    inline BVec sub(BVecSignalPort lhs, BitSignalPort rhs) { BVec r{ 1 }; r.setBit(0, rhs); return makeNode(hlim::Node_Arithmetic::SUB, lhs, r); }
    inline BVec add(BitSignalPort lhs, BVecSignalPort rhs) { BVec l{ 1 }; l.setBit(0, lhs); return makeNode(hlim::Node_Arithmetic::ADD, l, rhs); }
    inline BVec sub(BitSignalPort lhs, BVecSignalPort rhs) { BVec l{ 1 }; l.setBit(0, lhs); return makeNode(hlim::Node_Arithmetic::SUB, l, rhs); }

    inline BVec operator + (BVecSignalPort lhs, BVecSignalPort rhs) { return add(lhs, rhs); }
    inline BVec operator - (BVecSignalPort lhs, BVecSignalPort rhs) { return sub(lhs, rhs); }
    inline BVec operator * (BVecSignalPort lhs, BVecSignalPort rhs) { return mul(lhs, rhs); }
    inline BVec operator / (BVecSignalPort lhs, BVecSignalPort rhs) { return div(lhs, rhs); }
    inline BVec operator % (BVecSignalPort lhs, BVecSignalPort rhs) { return rem(lhs, rhs); }
    
    inline BVec operator + (BVecSignalPort lhs, BitSignalPort rhs) { return add(lhs, rhs); }
    inline BVec operator - (BVecSignalPort lhs, BitSignalPort rhs) { return sub(lhs, rhs); }
    inline BVec operator + (BitSignalPort lhs, BVecSignalPort rhs) { return add(lhs, rhs); }
    inline BVec operator - (BitSignalPort lhs, BVecSignalPort rhs) { return sub(lhs, rhs); }

    template<typename SignalType> SignalType& operator += (SignalType& lhs, BVecSignalPort rhs) { return lhs = add(lhs, rhs); }
    template<typename SignalType> SignalType& operator -= (SignalType& lhs, BVecSignalPort rhs) { return lhs = sub(lhs, rhs); }
    template<typename SignalType> SignalType& operator *= (SignalType& lhs, BVecSignalPort rhs) { return lhs = mul(lhs, rhs); }
    template<typename SignalType> SignalType& operator /= (SignalType& lhs, BVecSignalPort rhs) { return lhs = div(lhs, rhs); }
    template<typename SignalType> SignalType& operator %= (SignalType& lhs, BVecSignalPort rhs) { return lhs = rem(lhs, rhs); }

    template<typename SignalType> SignalType& operator += (SignalType& lhs, BitSignalPort rhs) { return lhs = add(lhs, rhs); }
    template<typename SignalType> SignalType& operator -= (SignalType& lhs, BitSignalPort rhs) { return lhs = sub(lhs, rhs); }

}
