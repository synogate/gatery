#pragma once

#include "Signal.h"
#include "Scope.h"
#include "Bit.h"

#include "SignalLogicOp.h"

#include <hcl/hlim/coreNodes/Node_Signal.h>
#include <hcl/hlim/coreNodes/Node_Compare.h>

#include <hcl/utils/Preprocessor.h>
#include <hcl/utils/Traits.h>

namespace hcl::core::frontend {

    class SignalCompareOp
    {
    public:
        SignalCompareOp(hlim::Node_Compare::Op op) : m_op(op) { }

        template<typename SignalType, typename = std::enable_if_t<utils::isElementarySignal<SignalType>::value>>
        Bit operator()(const SignalType& lhs, const SignalType& rhs) { return create(lhs, rhs); }
        
        template<typename SignalType, typename DerivedSignalType, typename = std::enable_if_t<utils::isElementarySignal<SignalType>::value && std::is_base_of<SignalType, DerivedSignalType>::value>>
        Bit operator()(const SignalType& lhs, const DerivedSignalType& rhs) { return create(lhs, rhs); }
        template<typename SignalType, typename DerivedSignalType, typename = std::enable_if_t<utils::isElementarySignal<SignalType>::value && std::is_base_of<SignalType, DerivedSignalType>::value>>
        Bit operator()(const DerivedSignalType& lhs, const SignalType& rhs) { return create(lhs, rhs); }

        inline hlim::Node_Compare::Op getOp() const { return m_op; }
    protected:
        hlim::Node_Compare::Op m_op;
        /// extend etc.
        template<typename LhsSignalType, typename RhsSignalType>
        Bit create(const LhsSignalType& lhs, const RhsSignalType& rhs);
    };


    template<typename LhsSignalType, typename RhsSignalType>
    Bit SignalCompareOp::create(const LhsSignalType& lhs, const RhsSignalType& rhs) {
        hlim::Node_Signal* lhsSignal = lhs.getNode();
        hlim::Node_Signal* rhsSignal = rhs.getNode();
        HCL_ASSERT(lhsSignal != nullptr);
        HCL_ASSERT(rhsSignal != nullptr);

        //if constexpr (!utils::isIntegerSignal<SignalType>::value)
            HCL_DESIGNCHECK_HINT(lhs.getWidth() == rhs.getWidth(), "Signal comparison is needs equal width for non auto extendable types.");

        auto* node = DesignScope::createNode<hlim::Node_Compare>(m_op);
        node->recordStackTrace();
        node->connectInput(0, { .node = lhsSignal, .port = 0 });
        node->connectInput(1, { .node = rhsSignal, .port = 0 });

        return Bit({ .node = node, .port = 0 });
    }


#define BUILD_OP(cpp_op, nodeOP) \
    template<typename SignalType, typename = std::enable_if_t<utils::isElementarySignal<SignalType>::value>>                                                                                                   \
    inline Bit operator cpp_op (const SignalType& l, const SignalType& r) { SignalCompareOp op(nodeOP); return op(l, r); }                                                                                         \
                                                                                                                                                                                                               \
    template<typename SignalType, typename DerivedSignalType, typename = std::enable_if_t<utils::isElementarySignal<SignalType>::value && std::is_base_of<SignalType, DerivedSignalType>::value>>              \
    inline Bit operator cpp_op (const SignalType& l, const DerivedSignalType& r) { SignalCompareOp op(nodeOP); return op(l, r); }                                                                                  \
                                                                                                                                                                                                               \
    template<typename SignalType, typename DerivedSignalType, typename = std::enable_if_t<utils::isElementarySignal<SignalType>::value && std::is_base_of<SignalType, DerivedSignalType>::value>>              \
    inline Bit operator cpp_op (const DerivedSignalType& l, const SignalType& r) { SignalCompareOp op(nodeOP); return op(l, r); }

    BUILD_OP(== , hlim::Node_Compare::EQ);
    BUILD_OP(!= , hlim::Node_Compare::NEQ);
    BUILD_OP(> , hlim::Node_Compare::GT);
    BUILD_OP(< , hlim::Node_Compare::LT);
    BUILD_OP(>= , hlim::Node_Compare::GEQ);
    BUILD_OP(<= , hlim::Node_Compare::LEQ);
#undef BUILD_OP

    inline Bit operator == (const Bit& l, bool r) { return r ? l : ~l; }
    inline Bit operator == (bool l, const Bit& r) { return r == l; }

    inline Bit operator != (const Bit& l, bool r) { return r ? ~l : l; }
    inline Bit operator != (bool l, const Bit& r) { return r != l; }

}

