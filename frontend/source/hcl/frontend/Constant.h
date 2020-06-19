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

namespace mhdl::core::frontend 
{
    template<typename SignalType, typename = std::enable_if_t<utils::isElementarySignal<SignalType>::value>>
    SignalType Constant(hlim::ConstantData&& value, const hlim::ConnectionType& connectionType)
    {
        auto* node = DesignScope::createNode<hlim::Node_Constant>(value, connectionType);
        return SignalType({.node = node, .port = 0ull});
    }

    inline namespace literal 
    {
        inline Bit operator ""_bit(const char* _val)
        {
            MHDL_DESIGNCHECK(_val[0] == '0' || _val[0] == '1');
            MHDL_DESIGNCHECK(_val[1] == 0);

            hlim::ConnectionType type{ 
                .interpretation = hlim::ConnectionType::BOOL,
                .width = 1
            };
            return Constant<Bit>(hlim::ConstantData{_val}, type);
        }

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
