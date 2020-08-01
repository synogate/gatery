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

    inline BVec ConstBVec(uint64_t value)
    {
        const size_t width = utils::Log2C(value);

        return Constant<BVec>(
            hlim::ConstantData{ value, width },
            hlim::ConnectionType{
                .interpretation = hlim::ConnectionType::BITVEC,
                .width = width
            });
    }

    inline BVec ConstBVec(std::string_view value)
    {
        hlim::ConstantData data{ value };
        size_t width = data.bitVec.size();

        return Constant<BVec>(
            std::move(data),
            hlim::ConnectionType{
                .interpretation = hlim::ConnectionType::BITVEC,
                .width = width
            });
    }

    inline Bit ConstBit(char value)
    {
        hlim::ConnectionType type{
            .interpretation = hlim::ConnectionType::BOOL,
            .width = 1
        };

        hlim::ConstantData cv;
        cv.base = 2;
        cv.bitVec.push_back(value != '0');
        HCL_ASSERT(value == '0' || value == '1');

        auto* node = DesignScope::createNode<hlim::Node_Constant>(cv, type);
        return Bit({ .node = node, .port = 0ull });
    }

    inline Bit ConstBit(bool value)
    {
        return ConstBit(value ? '1' : '0');
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
