#pragma once

#include "Signal.h"
#include "Bit.h"
#include "BitVector.h"

#include "SignalLogicOp.h"

#include <hcl/hlim/coreNodes/Node_Compare.h>

#include <hcl/utils/Preprocessor.h>

namespace hcl::core::frontend {

    inline SignalReadPort makeNode(hlim::Node_Compare::Op op, NormalizedWidthOperands ops)
    {
        auto* node = DesignScope::createNode<hlim::Node_Compare>(op);
        node->recordStackTrace();
        node->connectInput(0, ops.lhs);
        node->connectInput(1, ops.rhs);

        return SignalReadPort(node);
    }

    inline Bit eq(const BVec& lhs, const BVec& rhs) { return makeNode(hlim::Node_Compare::EQ, {lhs, rhs}); }
    inline Bit neq(const BVec& lhs, const BVec& rhs) { return makeNode(hlim::Node_Compare::NEQ, {lhs, rhs}); }
    inline Bit gt(const BVec& lhs, const BVec& rhs) { return makeNode(hlim::Node_Compare::GT, {lhs, rhs}); }
    inline Bit lt(const BVec& lhs, const BVec& rhs) { return makeNode(hlim::Node_Compare::LT, {lhs, rhs}); }
    inline Bit geq(const BVec& lhs, const BVec& rhs) { return makeNode(hlim::Node_Compare::GEQ, {lhs, rhs}); }
    inline Bit leq(const BVec& lhs, const BVec& rhs) { return makeNode(hlim::Node_Compare::LEQ, {lhs, rhs}); }

    inline Bit operator == (const BVec& lhs, const BVec& rhs) { return eq(lhs, rhs); }
    inline Bit operator != (const BVec& lhs, const BVec& rhs) { return neq(lhs, rhs); }
    inline Bit operator <= (const BVec& lhs, const BVec& rhs) { return leq(lhs, rhs); }
    inline Bit operator >= (const BVec& lhs, const BVec& rhs) { return geq(lhs, rhs); }
    inline Bit operator < (const BVec& lhs, const BVec& rhs) { return lt(lhs, rhs); }
    inline Bit operator > (const BVec& lhs, const BVec& rhs) { return gt(lhs, rhs); }


    inline Bit eq(const Bit& lhs, const Bit& rhs) { return makeNode(hlim::Node_Compare::EQ, { lhs, rhs }); }
    inline Bit neq(const Bit& lhs, const Bit& rhs) { return makeNode(hlim::Node_Compare::NEQ, { lhs, rhs }); }

    inline Bit operator == (const Bit& lhs, const Bit& rhs) { return eq(lhs, rhs); }
    inline Bit operator != (const Bit& lhs, const Bit& rhs) { return neq(lhs, rhs); }
}

