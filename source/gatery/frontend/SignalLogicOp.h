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

#include <gatery/hlim/coreNodes/Node_Logic.h>

#include <gatery/utils/Preprocessor.h>
#include <gatery/utils/Traits.h>


#include <boost/format.hpp>


namespace gtry {
    
    SignalReadPort makeNode(hlim::Node_Logic::Op op, const ElementarySignal& in);
    SignalReadPort makeNode(hlim::Node_Logic::Op op, NormalizedWidthOperands ops);

    // and, or, xor, not are c++ keywords so use l prefix for Logic, b for bitwise
    inline BVec land(const BVec& lhs, const BVec& rhs) { return makeNode(hlim::Node_Logic::AND, {lhs, rhs}); }
    inline BVec lnand(const BVec& lhs, const BVec& rhs) { return makeNode(hlim::Node_Logic::NAND, {lhs, rhs}); }
    inline BVec lor(const BVec& lhs, const BVec& rhs) { return makeNode(hlim::Node_Logic::OR, {lhs, rhs}); }
    inline BVec lnor(const BVec& lhs, const BVec& rhs) { return makeNode(hlim::Node_Logic::NOR, {lhs, rhs}); }
    inline BVec lxor(const BVec& lhs, const BVec& rhs) { return makeNode(hlim::Node_Logic::XOR, {lhs, rhs}); }
    inline BVec lxnor(const BVec& lhs, const BVec& rhs) { return makeNode(hlim::Node_Logic::EQ, {lhs, rhs}); }
    inline BVec lnot(const BVec& lhs) { return makeNode(hlim::Node_Logic::NOT, lhs); }

    inline BVec operator & (const BVec& lhs, const BVec& rhs) { return land(lhs, rhs); }
    inline BVec operator | (const BVec& lhs, const BVec& rhs) { return lor(lhs, rhs); }
    inline BVec operator ^ (const BVec& lhs, const BVec& rhs) { return lxor(lhs, rhs); }
    inline BVec operator ~ (const BVec& lhs) { return lnot(lhs); }



    inline UInt land(const UInt& lhs, const UInt& rhs) { return makeNode(hlim::Node_Logic::AND, {lhs, rhs}); }
    inline UInt lnand(const UInt& lhs, const UInt& rhs) { return makeNode(hlim::Node_Logic::NAND, {lhs, rhs}); }
    inline UInt lor(const UInt& lhs, const UInt& rhs) { return makeNode(hlim::Node_Logic::OR, {lhs, rhs}); }
    inline UInt lnor(const UInt& lhs, const UInt& rhs) { return makeNode(hlim::Node_Logic::NOR, {lhs, rhs}); }
    inline UInt lxor(const UInt& lhs, const UInt& rhs) { return makeNode(hlim::Node_Logic::XOR, {lhs, rhs}); }
    inline UInt lxnor(const UInt& lhs, const UInt& rhs) { return makeNode(hlim::Node_Logic::EQ, {lhs, rhs}); }
    inline UInt lnot(const UInt& lhs) { return makeNode(hlim::Node_Logic::NOT, lhs); }

    inline UInt operator & (const UInt& lhs, const UInt& rhs) { return land(lhs, rhs); }
    inline UInt operator | (const UInt& lhs, const UInt& rhs) { return lor(lhs, rhs); }
    inline UInt operator ^ (const UInt& lhs, const UInt& rhs) { return lxor(lhs, rhs); }
    inline UInt operator ~ (const UInt& lhs) { return lnot(lhs); }




    inline SInt land(const SInt& lhs, const SInt& rhs) { return makeNode(hlim::Node_Logic::AND, {lhs, rhs}); }
    inline SInt lnand(const SInt& lhs, const SInt& rhs) { return makeNode(hlim::Node_Logic::NAND, {lhs, rhs}); }
    inline SInt lor(const SInt& lhs, const SInt& rhs) { return makeNode(hlim::Node_Logic::OR, {lhs, rhs}); }
    inline SInt lnor(const SInt& lhs, const SInt& rhs) { return makeNode(hlim::Node_Logic::NOR, {lhs, rhs}); }
    inline SInt lxor(const SInt& lhs, const SInt& rhs) { return makeNode(hlim::Node_Logic::XOR, {lhs, rhs}); }
    inline SInt lxnor(const SInt& lhs, const SInt& rhs) { return makeNode(hlim::Node_Logic::EQ, {lhs, rhs}); }
    inline SInt lnot(const SInt& lhs) { return makeNode(hlim::Node_Logic::NOT, lhs); }

    inline SInt operator & (const SInt& lhs, const SInt& rhs) { return land(lhs, rhs); }
    inline SInt operator | (const SInt& lhs, const SInt& rhs) { return lor(lhs, rhs); }
    inline SInt operator ^ (const SInt& lhs, const SInt& rhs) { return lxor(lhs, rhs); }
    inline SInt operator ~ (const SInt& lhs) { return lnot(lhs); }




    template<BitVectorDerived SignalType, std::convertible_to<SignalType> RType> 
    inline SignalType& operator &= (SignalType& lhs, const RType& rhs) { return lhs = land(lhs, rhs); }
    template<BitVectorDerived SignalType, std::convertible_to<SignalType> RType> 
    inline SignalType& operator |= (SignalType& lhs, const RType& rhs) { return lhs = lor(lhs, rhs); }
    template<BitVectorDerived SignalType, std::convertible_to<SignalType> RType> 
    inline SignalType& operator ^= (SignalType& lhs, const RType& rhs) { return lhs = lxor(lhs, rhs); }



    inline Bit land(const Bit& lhs, const Bit& rhs) { return makeNode(hlim::Node_Logic::AND, {lhs, rhs}); }
    inline Bit lnand(const Bit& lhs, const Bit& rhs) { return makeNode(hlim::Node_Logic::NAND, {lhs, rhs}); }
    inline Bit lor(const Bit& lhs, const Bit& rhs) { return makeNode(hlim::Node_Logic::OR, {lhs, rhs}); }
    inline Bit lnor(const Bit& lhs, const Bit& rhs) { return makeNode(hlim::Node_Logic::NOR, {lhs, rhs}); }
    inline Bit lxor(const Bit& lhs, const Bit& rhs) { return makeNode(hlim::Node_Logic::XOR, {lhs, rhs}); }
    inline Bit lxnor(const Bit& lhs, const Bit& rhs) { return makeNode(hlim::Node_Logic::EQ, {lhs, rhs}); }
    inline Bit lnot(const Bit& lhs) { return makeNode(hlim::Node_Logic::NOT, lhs); }

    inline Bit operator & (const Bit& lhs, const Bit& rhs) { return land(lhs, rhs); }
    inline Bit operator | (const Bit& lhs, const Bit& rhs) { return lor(lhs, rhs); }
    inline Bit operator ^ (const Bit& lhs, const Bit& rhs) { return lxor(lhs, rhs); }
    inline Bit operator ~ (const Bit& lhs) { return lnot(lhs); }
    inline Bit operator ! (const Bit& lhs) { return lnot(lhs); }


    // All bitvector derived types can be logically combined with bits, which are implicitly broadcast to the size of the vector.
    template<BitVectorDerived Type>
    inline Type land(const Type& lhs, const Bit& rhs) { return makeNode(hlim::Node_Logic::AND, {lhs, sext(rhs)}); }
    template<BitVectorDerived Type>
    inline Type lnand(const Type& lhs, const Bit& rhs) { return makeNode(hlim::Node_Logic::NAND, {lhs, sext(rhs)}); }
    template<BitVectorDerived Type>
    inline Type lor(const Type& lhs, const Bit& rhs) { return makeNode(hlim::Node_Logic::OR, {lhs, sext(rhs)}); }
    template<BitVectorDerived Type>
    inline Type lnor(const Type& lhs, const Bit& rhs) { return makeNode(hlim::Node_Logic::NOR, {lhs, sext(rhs)}); }
    template<BitVectorDerived Type>
    inline Type lxor(const Type& lhs, const Bit& rhs) { return makeNode(hlim::Node_Logic::XOR, {lhs, sext(rhs)}); }
    template<BitVectorDerived Type>
    inline Type lxnor(const Type& lhs, const Bit& rhs) { return makeNode(hlim::Node_Logic::EQ, {lhs, sext(rhs)}); }


    // allow implicit conversion from literals to signals when used in a logical operation with Bit
    template<BitVectorLiteral Type>
    inline auto land(const Type& lhs, const Bit& rhs) { return land<ValueToBaseSignal<Type>>(lhs, rhs); }
    template<BitVectorLiteral Type>
    inline auto lnand(const Type& lhs, const Bit& rhs) { return lnand<ValueToBaseSignal<Type>>(lhs, rhs); }
    template<BitVectorLiteral Type>
    inline auto lor(const Type& lhs, const Bit& rhs) { return lor<ValueToBaseSignal<Type>>(lhs, rhs); }
    template<BitVectorLiteral Type>
    inline auto lnor(const Type& lhs, const Bit& rhs) { return lnor<ValueToBaseSignal<Type>>(lhs, rhs); }
    template<BitVectorLiteral Type>
    inline auto lxor(const Type& lhs, const Bit& rhs) { return lxor<ValueToBaseSignal<Type>>(lhs, rhs); }
    template<BitVectorLiteral Type>
    inline auto lxnor(const Type& lhs, const Bit& rhs) { return lxnor<ValueToBaseSignal<Type>>(lhs, rhs); }

    template<BitVectorValue Type>
    inline auto operator & (const Type& lhs, const Bit& rhs) { return land(lhs, rhs); }
    template<BitVectorValue Type>
    inline auto operator | (const Type& lhs, const Bit& rhs) { return lor(lhs, rhs); }
    template<BitVectorValue Type>
    inline auto operator ^ (const Type& lhs, const Bit& rhs) { return lxor(lhs, rhs); }
    template<BitVectorValue Type>
    inline auto operator & (const Bit& lhs, const Type& rhs) { return land(rhs, lhs); }
    template<BitVectorValue Type>
    inline auto operator | (const Bit& lhs, const Type& rhs) { return lor(rhs, lhs); }
    template<BitVectorValue Type>
    inline auto operator ^ (const Bit& lhs, const Type& rhs) { return lxor(rhs, lhs); }



    template<BaseSignal SignalType>
    inline SignalType& operator &= (SignalType& lhs, const Bit& rhs) { return lhs = land(lhs, rhs); }
    template<BaseSignal SignalType>
    inline SignalType& operator |= (SignalType& lhs, const Bit& rhs) { return lhs = lor(lhs, rhs); }
    template<BaseSignal SignalType>
    inline SignalType& operator ^= (SignalType& lhs, const Bit& rhs) { return lhs = lxor(lhs, rhs); }



}

