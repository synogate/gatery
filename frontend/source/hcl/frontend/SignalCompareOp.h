#pragma once

#include "Signal.h"
#include "Scope.h"
#include "Bit.h"
#include "BitVector.h"

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

        Bit operator()(const BVec& lhs, const BVec& rhs) { return create(lhs, rhs); }
        Bit operator()(const Bit& lhs, const Bit& rhs) { return create(lhs, rhs); }

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


#define BUILD_OP_BVEC(cpp_op, nodeOP) \
    inline Bit operator cpp_op (const BVec& l, const BVec& r) { SignalCompareOp op(nodeOP); return op(l, r); }
    
#define BUILD_OP_BIT(cpp_op, nodeOP) \
    inline Bit operator cpp_op (const Bit& l, const Bit& r) { SignalCompareOp op(nodeOP); return op(l, r); }

    BUILD_OP_BVEC(== , hlim::Node_Compare::EQ)
    BUILD_OP_BVEC(!= , hlim::Node_Compare::NEQ)
    BUILD_OP_BVEC(> , hlim::Node_Compare::GT)
    BUILD_OP_BVEC(< , hlim::Node_Compare::LT)
    BUILD_OP_BVEC(>= , hlim::Node_Compare::GEQ)
    BUILD_OP_BVEC(<= , hlim::Node_Compare::LEQ)

    BUILD_OP_BIT(== , hlim::Node_Compare::EQ)
    BUILD_OP_BIT(!= , hlim::Node_Compare::NEQ)
#undef BUILD_OP_BVEC
#undef BUILD_OP_BIT

    inline Bit operator == (const Bit& l, bool r) { return r ? l : ~l; }
    inline Bit operator == (bool l, const Bit& r) { return r == l; }

    inline Bit operator != (const Bit& l, bool r) { return r ? ~l : l; }
    inline Bit operator != (bool l, const Bit& r) { return r != l; }

}

