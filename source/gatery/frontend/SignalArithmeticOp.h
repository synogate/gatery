/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Michael Offel, Andreas Ley

    Gatery is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 3 of the License, or (at your option) any later version.

    Gatery is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#pragma once

#include "Signal.h"
#include "Bit.h"
#include "BitVector.h"
#include "Scope.h"
#include "Registers.h"

#include <gatery/hlim/coreNodes/Node_Signal.h>
#include <gatery/hlim/coreNodes/Node_Arithmetic.h>

#include <gatery/utils/Preprocessor.h>
#include <gatery/utils/Traits.h>


#include <boost/format.hpp>


namespace gtry {

    SignalReadPort makeNode(hlim::Node_Arithmetic op, NormalizedWidthOperands ops);

    inline BVec add(const BVec& lhs, const BVec& rhs) { return makeNode(hlim::Node_Arithmetic::ADD, { lhs, rhs }); }
    inline BVec sub(const BVec& lhs, const BVec& rhs) { return makeNode(hlim::Node_Arithmetic::SUB, { lhs, rhs }); }
    inline BVec mul(const BVec& lhs, const BVec& rhs) { return makeNode(hlim::Node_Arithmetic::MUL, { lhs, rhs }); }
    inline BVec div(const BVec& lhs, const BVec& rhs) { return makeNode(hlim::Node_Arithmetic::DIV, { lhs, rhs }); }
    inline BVec rem(const BVec& lhs, const BVec& rhs) { return makeNode(hlim::Node_Arithmetic::REM, { lhs, rhs }); }

    inline BVec add(const BVec& lhs, const Bit& rhs) { return makeNode(hlim::Node_Arithmetic::ADD, { lhs, zext(rhs) }); }
    inline BVec sub(const BVec& lhs, const Bit& rhs) { return makeNode(hlim::Node_Arithmetic::SUB, { lhs, zext(rhs) }); }
    inline BVec add(const Bit& lhs, const BVec& rhs) { return makeNode(hlim::Node_Arithmetic::ADD, { zext(lhs), rhs }); }
    inline BVec sub(const Bit& lhs, const BVec& rhs) { return makeNode(hlim::Node_Arithmetic::SUB, { zext(lhs), rhs }); }

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
