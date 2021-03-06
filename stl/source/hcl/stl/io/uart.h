#pragma once

#include <hcl/frontend.h>

namespace hcl::stl {

struct UART
{
    unsigned stabilize_rx = 2;

    bool deriveClock = false;
    unsigned startBits = 1;
    unsigned stopBits = 1;
    unsigned dataBits = 8;
    unsigned baudRate = 19200;

    struct Stream {
        core::frontend::BVec data;
        core::frontend::Bit valid;
        core::frontend::Bit ready;
    };

    Stream recieve(core::frontend::Bit rx);
};

}