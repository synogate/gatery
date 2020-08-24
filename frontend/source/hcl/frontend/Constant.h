#pragma once

#include <hcl/simulation/BitVectorState.h>

#include <hcl/utils/Preprocessor.h>
#include <hcl/utils/Traits.h>

#include <string_view>

namespace hcl::core::frontend 
{
    sim::DefaultBitVectorState parseBit(char value);
    sim::DefaultBitVectorState parseBit(bool value);
    sim::DefaultBitVectorState parseBVec(std::string_view);
    sim::DefaultBitVectorState parseBVec(uint64_t value, size_t width);
    
    class BVec;

    BVec ConstBVec(uint64_t value, size_t width);
    BVec ConstBVec(size_t width); // undefined constant
}
