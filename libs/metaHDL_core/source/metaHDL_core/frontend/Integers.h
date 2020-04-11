#pragma once

#include "BitVector.h"
#include "SignalArithmeticOp.h"

namespace mhdl {
namespace core {    
namespace frontend {

class UnsignedInteger : public BaseBitVector<UnsignedInteger>
{
    public:
        MHDL_SIGNAL
        
        using isUnsignedIntegerSignal = void;
        
        UnsignedInteger() = default;
        UnsignedInteger(unsigned width) { resize(width); }
        UnsignedInteger(hlim::Node::OutputPort *port, const hlim::ConnectionType &connectionType) : BaseBitVector<UnsignedInteger>(port, connectionType) { }
        
    protected:
        virtual hlim::ConnectionType getSignalType(unsigned width) const override;
        
};

class SignedInteger : public BaseBitVector<SignedInteger>
{
    public:
        MHDL_SIGNAL
        
        using isSignedIntegerSignal = void;
        
        SignedInteger() = default;
        SignedInteger(unsigned width) { resize(width); }
        SignedInteger(hlim::Node::OutputPort *port, const hlim::ConnectionType &connectionType) : BaseBitVector<SignedInteger>(port, connectionType) { }
        
    protected:
        virtual hlim::ConnectionType getSignalType(unsigned width) const override;
        
};

}
}
}
