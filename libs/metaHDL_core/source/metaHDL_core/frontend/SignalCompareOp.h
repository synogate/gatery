#pragma once

#include "Signal.h"
#include "Scope.h"
#include "Bit.h"

#include "../hlim/coreNodes/Node_Signal.h"
#include "../hlim/coreNodes/Node_Compare.h"

#include "../utils/Preprocessor.h"
#include "../utils/Traits.h"

namespace mhdl::core::frontend {

    class SignalCompareOp
    {
    public:
        SignalCompareOp(hlim::Node_Compare::Op op) : m_op(op) { }

        template<typename SignalType, typename = std::enable_if_t<utils::isElementarySignal<SignalType>::value>>
        Bit operator()(const SignalType& lhs, const SignalType& rhs);

        inline hlim::Node_Compare::Op getOp() const { return m_op; }
    protected:
        hlim::Node_Compare::Op m_op;
        /// extend etc.
    };


    template<typename SignalType, typename>
    Bit SignalCompareOp::operator()(const SignalType& lhs, const SignalType& rhs) {
        hlim::Node_Signal* lhsSignal = lhs.getNode();
        hlim::Node_Signal* rhsSignal = rhs.getNode();
        MHDL_ASSERT(lhsSignal != nullptr);
        MHDL_ASSERT(rhsSignal != nullptr);

        if constexpr (!utils::isIntegerSignal<SignalType>::value)
            MHDL_DESIGNCHECK_HINT(lhs.getWidth() == rhs.getWidth(), "Signal comparison is needs equal width for non auto extendable types.");

        auto* node = DesignScope::createNode<hlim::Node_Compare>(m_op);
        node->recordStackTrace();
        node->connectInput(0, { .node = lhsSignal, .port = 0 });
        node->connectInput(1, { .node = rhsSignal, .port = 0 });

        return Bit({ .node = node, .port = 0 });
    }


    template<typename SignalType, typename = std::enable_if_t<utils::isElementarySignal<SignalType>::value>>
    inline Bit operator == (const SignalType& l, const SignalType& r) { SignalCompareOp op(hlim::Node_Compare::EQ); return op(l, r); }

    template<typename SignalType, typename = std::enable_if_t<utils::isElementarySignal<SignalType>::value>>
    inline Bit operator != (const SignalType& l, const SignalType& r) { SignalCompareOp op(hlim::Node_Compare::NEQ); return op(l, r); }

    template<typename SignalType, typename = std::enable_if_t<utils::isNumberSignal<SignalType>::value>>
    inline Bit operator < (const SignalType& l, const SignalType& r)  { SignalCompareOp op(hlim::Node_Compare::LT); return op(l, r); }

    template<typename SignalType, typename = std::enable_if_t<utils::isNumberSignal<SignalType>::value>>
    inline Bit operator > (const SignalType& l, const SignalType& r)  { SignalCompareOp op(hlim::Node_Compare::GT); return op(l, r); }

    template<typename SignalType, typename = std::enable_if_t<utils::isNumberSignal<SignalType>::value>>
    inline Bit operator <= (const SignalType& l, const SignalType& r) { SignalCompareOp op(hlim::Node_Compare::LEQ); return op(l, r); }

    template<typename SignalType, typename = std::enable_if_t<utils::isNumberSignal<SignalType>::value>>
    inline Bit operator >= (const SignalType& l, const SignalType& r) { SignalCompareOp op(hlim::Node_Compare::GEQ); return op(l, r); }
}

