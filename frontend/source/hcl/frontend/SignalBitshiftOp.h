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


BVec operator<<(const BVec &signal, int amount);
BVec operator>>(const BVec &signal, int amount);
BVec &operator<<=(BVec &signal, int amount);
BVec &operator>>=(BVec &signal, int amount);

BVec rot(const BVec& signal, int amount);
inline BVec rotl(const BVec& signal, int amount) { return rot(signal, amount); }
inline BVec rotr(const BVec& signal, int amount) { return rot(signal, -amount); }


}
