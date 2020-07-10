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

core::frontend::BitVector tmdsEncode(core::hlim::BaseClock *pixelClock, core::frontend::Bit sendData, core::frontend::UnsignedInteger data, core::frontend::BitVector ctrl);

core::frontend::BitVector tmdsEncodeReduceTransitions(const core::frontend::BitVector& data);
core::frontend::BitVector tmdsDecodeReduceTransitions(const core::frontend::BitVector& data);

core::frontend::BitVector tmdsEncodeBitflip(const core::frontend::RegisterFactory& clk, const core::frontend::BitVector& data);
core::frontend::BitVector tmdsDecodeBitflip(const core::frontend::BitVector& data);

class Transmitter
{
    public:
        
    protected:
};

}
