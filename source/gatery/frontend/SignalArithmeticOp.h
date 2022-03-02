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
#include "BVec.h"
#include "UInt.h"
#include "SInt.h"
#include "Scope.h"

#include <gatery/hlim/coreNodes/Node_Signal.h>
#include <gatery/hlim/coreNodes/Node_Arithmetic.h>

#include <gatery/utils/Preprocessor.h>
#include <gatery/utils/Traits.h>


#include <boost/format.hpp>


namespace gtry {

    SignalReadPort makeNode(hlim::Node_Arithmetic op, NormalizedWidthOperands ops);

    inline SInt add(const SInt& lhs, const SInt& rhs) { return makeNode(hlim::Node_Arithmetic::ADD, { lhs, rhs }); }
    inline SInt sub(const SInt& lhs, const SInt& rhs) { return makeNode(hlim::Node_Arithmetic::SUB, { lhs, rhs }); }
    inline SInt mul(const SInt& lhs, const SInt& rhs) { return makeNode(hlim::Node_Arithmetic::MUL, { lhs, rhs }); }

    inline UInt add(const UInt& lhs, const UInt& rhs) { return makeNode(hlim::Node_Arithmetic::ADD, { lhs, rhs }); }
    inline UInt sub(const UInt& lhs, const UInt& rhs) { return makeNode(hlim::Node_Arithmetic::SUB, { lhs, rhs }); }
    inline UInt mul(const UInt& lhs, const UInt& rhs) { return makeNode(hlim::Node_Arithmetic::MUL, { lhs, rhs }); }
    inline UInt div(const UInt& lhs, const UInt& rhs) { return makeNode(hlim::Node_Arithmetic::DIV, { lhs, rhs }); }
    inline UInt rem(const UInt& lhs, const UInt& rhs) { return makeNode(hlim::Node_Arithmetic::REM, { lhs, rhs }); }


    // Adding or subtracting bits always involves zero extension
    // No implicit conversion allowed (do we want this?)
    template<ArithmeticValue Type>
    inline Type add(const Type& lhs, const Bit& rhs) { return makeNode(hlim::Node_Arithmetic::ADD, { lhs, zext(rhs) }); }
    template<ArithmeticValue Type>
    inline Type sub(const Type& lhs, const Bit& rhs) { return makeNode(hlim::Node_Arithmetic::SUB, { lhs, zext(rhs) }); }
    template<ArithmeticValue Type>
    inline Type add(const Bit& lhs, const Type& rhs) { return makeNode(hlim::Node_Arithmetic::ADD, { zext(lhs), rhs }); }
    template<SIntValue Type>
    inline Type sub(const Bit& lhs, const Type& rhs) { return makeNode(hlim::Node_Arithmetic::SUB, { zext(lhs), rhs }); }


    inline SInt operator + (const SInt& lhs, const SInt& rhs) { return add(lhs, rhs); }
    inline SInt operator - (const SInt& lhs, const SInt& rhs) { return sub(lhs, rhs); }
    inline SInt operator * (const SInt& lhs, const SInt& rhs) { return mul(lhs, rhs); }

    inline UInt operator + (const UInt& lhs, const UInt& rhs) { return add(lhs, rhs); }
    inline UInt operator - (const UInt& lhs, const UInt& rhs) { return sub(lhs, rhs); }
    inline UInt operator * (const UInt& lhs, const UInt& rhs) { return mul(lhs, rhs); }
    inline UInt operator / (const UInt& lhs, const UInt& rhs) { return div(lhs, rhs); }
    inline UInt operator % (const UInt& lhs, const UInt& rhs) { return rem(lhs, rhs); }

    // Adding or subtracting bits allows implicit conversion
    template<typename Type> requires UIntValue<Type> || SIntValue<Type>
    inline auto operator + (const Type& lhs, const Bit& rhs) { return add(lhs, rhs); }
    template<typename Type> requires UIntValue<Type> || SIntValue<Type>
    inline auto operator - (const Type& lhs, const Bit& rhs) { return sub(lhs, rhs); }
    template<typename Type> requires UIntValue<Type> || SIntValue<Type>
    inline auto operator + (const Bit& lhs, const Type& rhs) { return add(lhs, rhs); }
    template<typename Type> requires UIntValue<Type> || SIntValue<Type>
    inline auto operator - (const Bit& lhs, const Type& rhs) { return sub(lhs, rhs); }


    template<ArithmeticSignal LType, std::convertible_to<LType> RType>
    LType& operator += (LType& lhs, const RType& rhs) { return lhs = add(lhs, rhs); }
    template<ArithmeticSignal LType, std::convertible_to<LType> RType>
    LType& operator -= (LType& lhs, const RType& rhs) { return lhs = sub(lhs, rhs); }
    template<ArithmeticSignal LType, std::convertible_to<LType> RType>
    LType& operator *= (LType& lhs, const RType& rhs) { return lhs = mul(lhs, rhs); }
    template<ArithmeticSignal LType, std::convertible_to<LType> RType>
    LType& operator /= (LType& lhs, const RType& rhs) { return lhs = div(lhs, rhs); }
    template<ArithmeticSignal LType, std::convertible_to<LType> RType>
    LType& operator %= (LType& lhs, const RType& rhs) { return lhs = rem(lhs, rhs); }

    template<ArithmeticSignal SignalType> SignalType& operator += (SignalType& lhs, const Bit& rhs) { return lhs = add(lhs, rhs); }
    template<ArithmeticSignal SignalType> SignalType& operator -= (SignalType& lhs, const Bit& rhs) { return lhs = sub(lhs, rhs); }
}
