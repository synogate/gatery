#pragma once
#include <hcl/frontend.h>

namespace hcl::stl
{
    // returns a combinatorial bit vector of matches if write == '0'
    core::frontend::BVec constructTCAMCell(
        const core::frontend::BVec& searchKey, 
        const core::frontend::Bit& write, 
        const std::vector<core::frontend::BVec>& writeData);

}
