#pragma once

#include <hcl/frontend.h>

namespace hcl::stl::hdmi {
    
struct SerialTMDSPair {
    core::frontend::Bit pos;
    core::frontend::Bit neg;
};

struct SerialTMDS {
    SerialTMDSPair data[3];
    SerialTMDSPair clock;
};

core::frontend::BVec tmdsEncode(core::hlim::BaseClock *pixelClock, core::frontend::Bit sendData, core::frontend::BVec data, core::frontend::BVec ctrl);

core::frontend::BVec tmdsEncodeReduceTransitions(const core::frontend::BVec& data);
core::frontend::BVec tmdsDecodeReduceTransitions(const core::frontend::BVec& data);

core::frontend::BVec tmdsEncodeBitflip(const core::frontend::RegisterFactory& clk, const core::frontend::BVec& data);
core::frontend::BVec tmdsDecodeBitflip(const core::frontend::BVec& data);

class Transmitter
{
    public:
        
    protected:
};

}
