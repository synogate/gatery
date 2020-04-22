#pragma once

#include "BitVector.h"
#include "SignalArithmeticOp.h"

namespace mhdl::core::frontend {

class UnsignedInteger : public BaseBitVector<UnsignedInteger>
{
    public:
        MHDL_SIGNAL
        
        using isUnsignedIntegerSignal = void;
        
        UnsignedInteger() = default;
        UnsignedInteger(size_t width) { resize(width); }
        UnsignedInteger(const hlim::NodePort &port) : BaseBitVector<UnsignedInteger>(port) { }
        
    protected:
        virtual hlim::ConnectionType getSignalType(size_t width) const override;
        
};

class SignedInteger : public BaseBitVector<SignedInteger>
{
    public:
        MHDL_SIGNAL
        
        using isSignedIntegerSignal = void;
        
        SignedInteger() = default;
        SignedInteger(size_t width) { resize(width); }
        SignedInteger(const hlim::NodePort &port) : BaseBitVector<SignedInteger>(port) { }
        
    protected:
        virtual hlim::ConnectionType getSignalType(size_t width) const override;
        
};

}
