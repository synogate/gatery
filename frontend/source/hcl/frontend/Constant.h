#pragma once

#include "Scope.h"
#include "Signal.h"
#include "Bit.h"
#include "BitVector.h"

#include <hcl/hlim/coreNodes/Node_Constant.h>

#include <hcl/utils/Preprocessor.h>
#include <hcl/utils/Traits.h>

#include <string_view>

namespace hcl::core::frontend 
{
    sim::DefaultBitVectorState parseBit(char value);
    sim::DefaultBitVectorState parseBit(bool value);
    sim::DefaultBitVectorState parseBVec(std::string_view);
    sim::DefaultBitVectorState parseBVec(uint64_t value, size_t width);
    sim::DefaultBitVectorState undefinedBVec(size_t width);

    inline BVec ConstBVec(uint64_t value, size_t width)
    {
        auto* node = DesignScope::createNode<hlim::Node_Constant>(parseBVec(value, width), hlim::ConnectionType::BITVEC);
        return SignalReadPort(node);
    }

}
