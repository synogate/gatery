#pragma once

#include "Signal.h"
#include "Scope.h"

#include <hcl/hlim/coreNodes/Node_Signal.h>
#include <hcl/utils/Exceptions.h>

#include <vector>

namespace hcl::core::frontend {
    
class BVec;
    
class Bit : public ElementarySignal
{
    public:
        HCL_SIGNAL
        using isBitSignal = void;
        
        Bit();
        Bit(const Bit &rhs);
        Bit(const hlim::NodePort &port);
        Bit(bool value);
        
        BVec zext(size_t width) const;
        BVec sext(size_t width) const;
        BVec bext(size_t width, const Bit& bit) const;
        
        Bit& operator=(const Bit &rhs) { assign(rhs); return *this; }
        Bit& operator=(bool value);

        const Bit operator*() const;
    protected:
        Bit(const Bit &rhs, ElementarySignal::InitSuccessor);
        
        virtual hlim::ConnectionType getSignalType(size_t width) const override;
};

}
