#pragma once

#include "BitVector.h"
#include "SignalArithmeticOp.h"

namespace hcl::core::frontend {

class UnsignedInteger : public BaseBitVector<UnsignedInteger>
{
    public:
        HCL_SIGNAL
        
        using isUnsignedIntegerSignal = void;

        UnsignedInteger ext(size_t width) const { return zext(width); }
        
        UnsignedInteger() = default;
        UnsignedInteger(size_t width) { resize(width); }
        UnsignedInteger(const hlim::NodePort &port) : BaseBitVector<UnsignedInteger>(port) { }
        
    protected:
        virtual hlim::ConnectionType getSignalType(size_t width) const override;
        
};

class SignedInteger : public BaseBitVector<SignedInteger>
{
    public:
        HCL_SIGNAL
        
        using isSignedIntegerSignal = void;
        
        SignedInteger ext(size_t width) const { return sext(width); }

        SignedInteger() = default;
        SignedInteger(size_t width) { resize(width); }
        SignedInteger(const hlim::NodePort &port) : BaseBitVector<SignedInteger>(port) { }
        
    protected:
        virtual hlim::ConnectionType getSignalType(size_t width) const override;
        
};

}
