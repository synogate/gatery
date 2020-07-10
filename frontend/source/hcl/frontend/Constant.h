#pragma once

#include "Scope.h"
#include "Signal.h"
#include "Bit.h"
#include "BitVector.h"
#include "Integers.h"

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

    inline BitVector ConstBitVector(uint64_t value, size_t width)
    {
        return Constant<BitVector>(
            hlim::ConstantData{ value, width },
            hlim::ConnectionType{
                .interpretation = hlim::ConnectionType::RAW,
                .width = width
            });
    }

    inline UnsignedInteger ConstUnsignedInteger(std::uint64_t value, size_t width)
    {
        return Constant<UnsignedInteger>(
            hlim::ConstantData{ value, width },
            hlim::ConnectionType{
                .interpretation = hlim::ConnectionType::UNSIGNED,
                .width = width
            });
    }

    inline SignedInteger ConstSignedInteger(std::int64_t value, size_t width)
    {
        return Constant<SignedInteger>(
            hlim::ConstantData{ uint64_t(value), width },
            hlim::ConnectionType{
                .interpretation = hlim::ConnectionType::SIGNED_2COMPLEMENT,
                .width = width
            });
    }

    inline namespace literal
    {
        inline BitVector operator ""_vec(const char* _val)
        {
            hlim::ConstantData lit{ _val };
            hlim::ConnectionType type{
                .interpretation = hlim::ConnectionType::RAW,
                .width = lit.bitVec.size()
            };
            return Constant<BitVector>(std::move(lit), type);
        }

        inline UnsignedInteger operator ""_uvec(const char* _val)
        {
            hlim::ConstantData lit{ _val };
            hlim::ConnectionType type{
                .interpretation = hlim::ConnectionType::UNSIGNED,
                .width = lit.bitVec.size()
            };
            return Constant<UnsignedInteger>(std::move(lit), type);
        }
        inline SignedInteger operator ""_svec(const char* _val)
        {
            hlim::ConstantData lit{ _val };
            hlim::ConnectionType type{
                .interpretation = hlim::ConnectionType::SIGNED_2COMPLEMENT,
                .width = lit.bitVec.size()
            };
            return Constant<SignedInteger>(std::move(lit), type);
        }
    }
}
