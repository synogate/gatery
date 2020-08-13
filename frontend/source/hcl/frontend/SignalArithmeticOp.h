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

    SignalReadPort makeNode(hlim::Node_Arithmetic op, NormalizedWidthOperands ops);

    inline BVec add(const BVec& lhs, const BVec& rhs) { return makeNode(hlim::Node_Arithmetic::ADD, { lhs, rhs }); }
    inline BVec sub(const BVec& lhs, const BVec& rhs) { return makeNode(hlim::Node_Arithmetic::SUB, { lhs, rhs }); }
    inline BVec mul(const BVec& lhs, const BVec& rhs) { return makeNode(hlim::Node_Arithmetic::MUL, { lhs, rhs }); }
    inline BVec div(const BVec& lhs, const BVec& rhs) { return makeNode(hlim::Node_Arithmetic::DIV, { lhs, rhs }); }
    inline BVec rem(const BVec& lhs, const BVec& rhs) { return makeNode(hlim::Node_Arithmetic::REM, { lhs, rhs }); }

    inline BVec add(const BVec& lhs, const Bit& rhs) { return makeNode(hlim::Node_Arithmetic::ADD, { lhs, rhs }); }
    inline BVec sub(const BVec& lhs, const Bit& rhs) { return makeNode(hlim::Node_Arithmetic::SUB, { lhs, rhs }); }
    inline BVec add(const Bit& lhs, const BVec& rhs) { return makeNode(hlim::Node_Arithmetic::ADD, { lhs, rhs }); }
    inline BVec sub(const Bit& lhs, const BVec& rhs) { return makeNode(hlim::Node_Arithmetic::SUB, { lhs, rhs }); }

    inline BVec operator + (const BVec& lhs, const BVec& rhs) { return add(lhs, rhs); }
    inline BVec operator - (const BVec& lhs, const BVec& rhs) { return sub(lhs, rhs); }
    inline BVec operator * (const BVec& lhs, const BVec& rhs) { return mul(lhs, rhs); }
    inline BVec operator / (const BVec& lhs, const BVec& rhs) { return div(lhs, rhs); }
    inline BVec operator % (const BVec& lhs, const BVec& rhs) { return rem(lhs, rhs); }
    
    inline BVec operator + (const BVec& lhs, const Bit& rhs) { return add(lhs, rhs); }
    inline BVec operator - (const BVec& lhs, const Bit& rhs) { return sub(lhs, rhs); }
    inline BVec operator + (const Bit& lhs, const BVec& rhs) { return add(lhs, rhs); }
    inline BVec operator - (const Bit& lhs, const BVec& rhs) { return sub(lhs, rhs); }

    // TODO: use signal like type traits
    template<typename SignalType> SignalType& operator += (SignalType& lhs, const BVec& rhs) { return lhs = add(lhs, rhs); }
    template<typename SignalType> SignalType& operator -= (SignalType& lhs, const BVec& rhs) { return lhs = sub(lhs, rhs); }
    template<typename SignalType> SignalType& operator *= (SignalType& lhs, const BVec& rhs) { return lhs = mul(lhs, rhs); }
    template<typename SignalType> SignalType& operator /= (SignalType& lhs, const BVec& rhs) { return lhs = div(lhs, rhs); }
    template<typename SignalType> SignalType& operator %= (SignalType& lhs, const BVec& rhs) { return lhs = rem(lhs, rhs); }

    template<typename SignalType> SignalType& operator += (SignalType& lhs, const Bit& rhs) { return lhs = add(lhs, rhs); }
    template<typename SignalType> SignalType& operator -= (SignalType& lhs, const Bit& rhs) { return lhs = sub(lhs, rhs); }
}
