#pragma once

#include "Signal.h"
#include "Scope.h"

#include <hcl/hlim/coreNodes/Node_Signal.h>
#include <hcl/utils/Exceptions.h>

#include <vector>

namespace hcl::core::frontend {
    
class Bit : public ElementarySignal
{
    public:
        HCL_SIGNAL
        using isBitSignal = void;
        
        Bit();
        Bit(const Bit &rhs);
        Bit(const hlim::NodePort &port);
        
        Bit &operator=(const Bit &rhs) { assign(rhs); return *this; }
    protected:
        virtual hlim::ConnectionType getSignalType(size_t width) const override;
};

}
