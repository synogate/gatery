/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Synogate GbR

    Gatery is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

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

#include <hcl/hlim/coreNodes/Node_Signal.h>
#include <hcl/hlim/coreNodes/Node_Rewire.h>

#include <hcl/utils/Preprocessor.h>
#include <hcl/utils/Traits.h>


#include <boost/format.hpp>

#include <optional>


namespace hcl::core::frontend {

class SignalBitShiftOp
{
    public:
        SignalBitShiftOp(int shift) : m_shift(shift) { }

        inline SignalBitShiftOp &setFillLeft(bool bit) { m_fillLeft = bit; return *this; }
        inline SignalBitShiftOp &setFillRight(bool bit) { m_fillRight = bit; return *this; }
        inline SignalBitShiftOp &duplicateLeft() { m_duplicateLeft = true; m_rotate = false; return *this; }
        inline SignalBitShiftOp &duplicateRight() { m_duplicateRight = true; m_rotate = false; return *this; }
        inline SignalBitShiftOp &rotate() { m_rotate = true; m_duplicateLeft = m_duplicateRight = false; return *this; }

        hlim::ConnectionType getResultingType(const hlim::ConnectionType &operand);

        BVec operator()(const BVec &operand);
    protected:
        int m_shift;
        bool m_duplicateLeft = false;
        bool m_duplicateRight = false;
        bool m_rotate = false;
        bool m_fillLeft = false;
        bool m_fillRight = false;
};


BVec operator<<(const BVec &signal, int amount); // TODO (remove)
BVec operator>>(const BVec &signal, int amount); // TODO (remove)
BVec &operator<<=(BVec &signal, int amount); // TODO (remove)
BVec &operator>>=(BVec &signal, int amount); // TODO (remove)

BVec rot(const BVec& signal, int amount); // TODO (remove)
inline BVec rotl(const BVec& signal, int amount) { return rot(signal, amount); }  // TODO (remove)
inline BVec rotr(const BVec& signal, int amount) { return rot(signal, -amount); }  // TODO (remove)
inline BVec rotl(const BVec& signal, size_t amount) { return rot(signal, int(amount)); }  // TODO (remove)
inline BVec rotr(const BVec& signal, size_t amount) { return rot(signal, -int(amount)); }  // TODO (remove)


BVec zshl(const BVec& signal, const BVec& amount);
BVec oshl(const BVec& signal, const BVec& amount);
BVec sshl(const BVec& signal, const BVec& amount);
BVec zshr(const BVec& signal, const BVec& amount);
BVec oshr(const BVec& signal, const BVec& amount);
BVec sshr(const BVec& signal, const BVec& amount);
BVec rotl(const BVec& signal, const BVec& amount);
BVec rotr(const BVec& signal, const BVec& amount);

inline BVec operator<<(const BVec& signal, const BVec& amount) { return zshl(signal, amount); }
inline BVec operator>>(const BVec& signal, const BVec& amount) { return zshr(signal, amount); }
inline BVec& operator<<=(BVec& signal, const BVec& amount) { return signal = zshl(signal, amount); }
inline BVec& operator>>=(BVec& signal, const BVec& amount) { return signal = zshr(signal, amount); }

}
