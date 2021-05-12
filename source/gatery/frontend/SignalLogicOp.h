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
#include "Registers.h"

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

    template<typename SignalType> SignalType& operator &= (SignalType& lhs, const BVec& rhs) { return lhs = land(lhs, rhs); }
    template<typename SignalType> SignalType& operator |= (SignalType& lhs, const BVec& rhs) { return lhs = lor(lhs, rhs); }
    template<typename SignalType> SignalType& operator ^= (SignalType& lhs, const BVec& rhs) { return lhs = lxor(lhs, rhs); }



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

    inline Bit& operator &= (Bit& lhs, const Bit& rhs) { return lhs = land(lhs, rhs); }
    inline Bit& operator |= (Bit& lhs, const Bit& rhs) { return lhs = lor(lhs, rhs); }
    inline Bit& operator ^= (Bit& lhs, const Bit& rhs) { return lhs = lxor(lhs, rhs); }



    inline BVec land(const BVec& lhs, const Bit& rhs) { return makeNode(hlim::Node_Logic::AND, {lhs, sext(rhs)}); }
    inline BVec lnand(const BVec& lhs, const Bit& rhs) { return makeNode(hlim::Node_Logic::NAND, {lhs, sext(rhs)}); }
    inline BVec lor(const BVec& lhs, const Bit& rhs) { return makeNode(hlim::Node_Logic::OR, {lhs, sext(rhs)}); }
    inline BVec lnor(const BVec& lhs, const Bit& rhs) { return makeNode(hlim::Node_Logic::NOR, {lhs, sext(rhs)}); }
    inline BVec lxor(const BVec& lhs, const Bit& rhs) { return makeNode(hlim::Node_Logic::XOR, {lhs, sext(rhs)}); }
    inline BVec lxnor(const BVec& lhs, const Bit& rhs) { return makeNode(hlim::Node_Logic::EQ, {lhs, sext(rhs)}); }


    inline BVec operator & (const BVec& lhs, const Bit& rhs) { return land(lhs, rhs); }
    inline BVec operator | (const BVec& lhs, const Bit& rhs) { return lor(lhs, rhs); }
    inline BVec operator ^ (const BVec& lhs, const Bit& rhs) { return lxor(lhs, rhs); }
    inline BVec operator & (const Bit& lhs, const BVec& rhs) { return land(rhs, lhs); }
    inline BVec operator | (const Bit& lhs, const BVec& rhs) { return lor(rhs, lhs); }
    inline BVec operator ^ (const Bit& lhs, const BVec& rhs) { return lxor(rhs, lhs); }

    inline BVec& operator &= (BVec& lhs, const Bit& rhs) { return lhs = land(lhs, rhs); }
    inline BVec& operator |= (BVec& lhs, const Bit& rhs) { return lhs = lor(lhs, rhs); }
    inline BVec& operator ^= (BVec& lhs, const Bit& rhs) { return lhs = lxor(lhs, rhs); }



}

