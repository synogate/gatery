#pragma once

#include "Scope.h"
#include "Signal.h"
#include "Bit.h"
#include "BitVector.h"
#include "Integers.h"

#include "../hlim/coreNodes/Node_Constant.h"

#include "../utils/Preprocessor.h"
#include "../utils/Traits.h"

#include <string_view>

namespace mhdl::core::frontend 
{
    template<typename SignalType, typename = std::enable_if_t<utils::isElementarySignal<SignalType>::value>>
    SignalType Constant(hlim::ConstantData&& value, const hlim::ConnectionType& connectionType)
    {
        auto* node = Scope::getCurrentNodeGroup()->addNode<hlim::Node_Constant>(value);
        return SignalType(&node->getOutput(0), connectionType);
    }

    inline namespace literal 
    {
        Bit operator ""_bit(const char* _val)
        {
            MHDL_DESIGNCHECK(_val[0] == '0' || _val[0] == '1');
            MHDL_DESIGNCHECK(_val[1] == 0);

            hlim::ConnectionType type{ 
                .interpretation = hlim::ConnectionType::BOOL,
                .width = 1
            };
            return Constant<Bit>(hlim::ConstantData{_val}, type);
        }

        BitVector operator ""_vec(const char* _val)
        {
            hlim::ConstantData lit{ _val };
            hlim::ConnectionType type{
                .interpretation = hlim::ConnectionType::RAW,
                .width = lit.bitVec.size()
            };
            return Constant<BitVector>(std::move(lit), type);
        }

        UnsignedInteger operator ""_uvec(const char* _val)
        {
            hlim::ConstantData lit{ _val };
            hlim::ConnectionType type{
                .interpretation = hlim::ConnectionType::UNSIGNED,
                .width = lit.bitVec.size()
            };
            return Constant<UnsignedInteger>(std::move(lit), type);
        }

        SignedInteger operator ""_svec(const char* _val)
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
