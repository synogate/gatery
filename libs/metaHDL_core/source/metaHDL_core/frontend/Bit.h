#pragma once

#include "Signal.h"
#include "Scope.h"

#include "../hlim/coreNodes/Node_Signal.h"

#include "../utils/Exceptions.h"

#include <vector>

namespace mhdl::core::frontend {
    
class Bit : public ElementarySignal
{
    public:
        MHDL_SIGNAL
        using isBitSignal = void;
        
        Bit();
        Bit(const Bit &rhs);
        Bit(hlim::Node::OutputPort *port, const hlim::ConnectionType &connectionType);
        
        Bit &operator=(const Bit &rhs) { assign(rhs); return *this; }
    protected:
        virtual hlim::ConnectionType getSignalType(size_t width) const override;
};

}
