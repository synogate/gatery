#pragma once

#include <hcl/frontend.h>

namespace hcl::stl {

template<typename DataSignal>
struct BlockRamPort
{
    DataSignal data;
    core::frontend::BVec addr;
    core::hlim::Clock *clock;
    core::frontend::Bit enable;
};
    
    
class BlockRam
{
    public:
};

}
