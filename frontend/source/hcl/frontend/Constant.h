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
    template<typename SignalType, typename = std::enable_if_t<utils::isElementarySignal<SignalType>::value>>
    SignalType Constant(hlim::ConstantData&& value, const hlim::ConnectionType& connectionType)
    {
        auto* node = DesignScope::createNode<hlim::Node_Constant>(value, connectionType);
        return SignalType({.node = node, .port = 0ull});
    }

    inline BVec ConstBVec(uint64_t value, size_t width)
    {
        return Constant<BVec>(
            hlim::ConstantData{ value, width },
            hlim::ConnectionType{
                .interpretation = hlim::ConnectionType::BITVEC,
                .width = width
            });
    }

    inline namespace literal
    {
        inline BVec operator ""_bvec(const char* _val)
        {
            hlim::ConstantData lit{ _val };
            hlim::ConnectionType type{
                .interpretation = hlim::ConnectionType::BITVEC,
                .width = lit.bitVec.size()
            };
            return Constant<BVec>(std::move(lit), type);
        }
    }
}
