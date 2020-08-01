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
    

    hlim::NodePort makeNode(hlim::Node_Logic::Op op, const SignalPort& in);
    hlim::NodePort makeNode(hlim::Node_Logic::Op op, const SignalPort& lhs, const SignalPort& rhs);
    hlim::NodePort makeNodeEqWidth(hlim::Node_Logic::Op op, const SignalPort& lhs, const BitSignalPort& rhs);

    // and, or, xor, not are c++ keywords so use l prefix for Logic, b for bitwise
    inline BVec land(BVecSignalPort lhs, BVecSignalPort rhs) { return makeNode(hlim::Node_Logic::AND, lhs, rhs); }
    inline BVec lnand(BVecSignalPort lhs, BVecSignalPort rhs) { return makeNode(hlim::Node_Logic::NAND, lhs, rhs); }
    inline BVec lor(BVecSignalPort lhs, BVecSignalPort rhs) { return makeNode(hlim::Node_Logic::OR, lhs, rhs); }
    inline BVec lnor(BVecSignalPort lhs, BVecSignalPort rhs) { return makeNode(hlim::Node_Logic::NOR, lhs, rhs); }
    inline BVec lxor(BVecSignalPort lhs, BVecSignalPort rhs) { return makeNode(hlim::Node_Logic::XOR, lhs, rhs); }
    inline BVec lxnor(BVecSignalPort lhs, BVecSignalPort rhs) { return makeNode(hlim::Node_Logic::EQ, lhs, rhs); }
    inline BVec lnot(BVecSignalPort lhs) { return makeNode(hlim::Node_Logic::NOT, lhs); }

    inline BVec operator & (BVecSignalPort lhs, BVecSignalPort rhs) { return land(lhs, rhs); }
    inline BVec operator | (BVecSignalPort lhs, BVecSignalPort rhs) { return lor(lhs, rhs); }
    inline BVec operator ^ (BVecSignalPort lhs, BVecSignalPort rhs) { return lxor(lhs, rhs); }
    inline BVec operator ~ (BVecSignalPort lhs) { return lnot(lhs); }

    template<typename SignalType> SignalType& operator &= (SignalType& lhs, BVecSignalPort rhs) { return lhs = land(lhs, rhs); }
    template<typename SignalType> SignalType& operator |= (SignalType& lhs, BVecSignalPort rhs) { return lhs = lor(lhs, rhs); }
    template<typename SignalType> SignalType& operator ^= (SignalType& lhs, BVecSignalPort rhs) { return lhs = lxor(lhs, rhs); }



    inline Bit land(BitSignalPort lhs, BitSignalPort rhs) { return makeNode(hlim::Node_Logic::AND, lhs, rhs); }
    inline Bit lnand(BitSignalPort lhs, BitSignalPort rhs) { return makeNode(hlim::Node_Logic::NAND, lhs, rhs); }
    inline Bit lor(BitSignalPort lhs, BitSignalPort rhs) { return makeNode(hlim::Node_Logic::OR, lhs, rhs); }
    inline Bit lnor(BitSignalPort lhs, BitSignalPort rhs) { return makeNode(hlim::Node_Logic::NOR, lhs, rhs); }
    inline Bit lxor(BitSignalPort lhs, BitSignalPort rhs) { return makeNode(hlim::Node_Logic::XOR, lhs, rhs); }
    inline Bit lxnor(BitSignalPort lhs, BitSignalPort rhs) { return makeNode(hlim::Node_Logic::EQ, lhs, rhs); }
    inline Bit lnot(BitSignalPort lhs) { return makeNode(hlim::Node_Logic::NOT, lhs); }

    inline Bit operator & (BitSignalPort lhs, BitSignalPort rhs) { return land(lhs, rhs); }
    inline Bit operator | (BitSignalPort lhs, BitSignalPort rhs) { return lor(lhs, rhs); }
    inline Bit operator ^ (BitSignalPort lhs, BitSignalPort rhs) { return lxor(lhs, rhs); }
    inline Bit operator ~ (BitSignalPort lhs) { return lnot(lhs); }
    inline Bit operator ! (BitSignalPort lhs) { return lnot(lhs); }

    inline Bit& operator &= (Bit& lhs, BitSignalPort rhs) { return lhs = land(lhs, rhs); }
    inline Bit& operator |= (Bit& lhs, BitSignalPort rhs) { return lhs = lor(lhs, rhs); }
    inline Bit& operator ^= (Bit& lhs, BitSignalPort rhs) { return lhs = lxor(lhs, rhs); }



    inline BVec band(BVecSignalPort lhs, BitSignalPort rhs) { return makeNodeEqWidth(hlim::Node_Logic::AND, lhs, rhs); }
    inline BVec bnand(BVecSignalPort lhs, BitSignalPort rhs) { return makeNodeEqWidth(hlim::Node_Logic::NAND, lhs, rhs); }
    inline BVec bor(BVecSignalPort lhs, BitSignalPort rhs) { return makeNodeEqWidth(hlim::Node_Logic::OR, lhs, rhs); }
    inline BVec bnor(BVecSignalPort lhs, BitSignalPort rhs) { return makeNodeEqWidth(hlim::Node_Logic::NOR, lhs, rhs); }
    inline BVec bxor(BVecSignalPort lhs, BitSignalPort rhs) { return makeNodeEqWidth(hlim::Node_Logic::XOR, lhs, rhs); }
    inline BVec bxnor(BVecSignalPort lhs, BitSignalPort rhs) { return makeNodeEqWidth(hlim::Node_Logic::EQ, lhs, rhs); }


    inline BVec operator & (BVecSignalPort lhs, BitSignalPort rhs) { return band(lhs, rhs); }
    inline BVec operator | (BVecSignalPort lhs, BitSignalPort rhs) { return bor(lhs, rhs); }
    inline BVec operator ^ (BVecSignalPort lhs, BitSignalPort rhs) { return bxor(lhs, rhs); }
    inline BVec operator & (BitSignalPort rhs, BVecSignalPort lhs) { return band(lhs, rhs); }
    inline BVec operator | (BitSignalPort rhs, BVecSignalPort lhs) { return bor(lhs, rhs); }
    inline BVec operator ^ (BitSignalPort rhs, BVecSignalPort lhs) { return bxor(lhs, rhs); }

    inline BVec& operator &= (BVec& lhs, BitSignalPort rhs) { return lhs = band(lhs, rhs); }
    inline BVec& operator |= (BVec& lhs, BitSignalPort rhs) { return lhs = bor(lhs, rhs); }
    inline BVec& operator ^= (BVec& lhs, BitSignalPort rhs) { return lhs = bxor(lhs, rhs); }



}

