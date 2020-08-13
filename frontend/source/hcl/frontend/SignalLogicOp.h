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



    inline BVec band(const BVec& lhs, const Bit& rhs) { return makeNode(hlim::Node_Logic::AND, {lhs, rhs}); }
    inline BVec bnand(const BVec& lhs, const Bit& rhs) { return makeNode(hlim::Node_Logic::NAND, {lhs, rhs}); }
    inline BVec bor(const BVec& lhs, const Bit& rhs) { return makeNode(hlim::Node_Logic::OR, {lhs, rhs}); }
    inline BVec bnor(const BVec& lhs, const Bit& rhs) { return makeNode(hlim::Node_Logic::NOR, {lhs, rhs}); }
    inline BVec bxor(const BVec& lhs, const Bit& rhs) { return makeNode(hlim::Node_Logic::XOR, {lhs, rhs}); }
    inline BVec bxnor(const BVec& lhs, const Bit& rhs) { return makeNode(hlim::Node_Logic::EQ, {lhs, rhs}); }


    inline BVec operator & (const BVec& lhs, const Bit& rhs) { return band(lhs, rhs); }
    inline BVec operator | (const BVec& lhs, const Bit& rhs) { return bor(lhs, rhs); }
    inline BVec operator ^ (const BVec& lhs, const Bit& rhs) { return bxor(lhs, rhs); }
    inline BVec operator & (const Bit& rhs, const BVec& lhs) { return band(lhs, rhs); }
    inline BVec operator | (const Bit& rhs, const BVec& lhs) { return bor(lhs, rhs); }
    inline BVec operator ^ (const Bit& rhs, const BVec& lhs) { return bxor(lhs, rhs); }

    inline BVec& operator &= (BVec& lhs, const Bit& rhs) { return lhs = band(lhs, rhs); }
    inline BVec& operator |= (BVec& lhs, const Bit& rhs) { return lhs = bor(lhs, rhs); }
    inline BVec& operator ^= (BVec& lhs, const Bit& rhs) { return lhs = bxor(lhs, rhs); }



}

