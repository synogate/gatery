#pragma once

#include "Signal.h"
#include "SignalPort.h"
#include "Bit.h"
#include "BitVector.h"

#include "SignalLogicOp.h"

#include <hcl/hlim/coreNodes/Node_Compare.h>

#include <hcl/utils/Preprocessor.h>

namespace hcl::core::frontend {

    inline hlim::NodePort makeNode(hlim::Node_Compare::Op op, const SignalPort& lhs, const SignalPort& rhs)
    {
        HCL_DESIGNCHECK_HINT(lhs.getWidth() == rhs.getWidth(), "Signal comparison is needs equal width for non auto extendable types.");

        auto* node = DesignScope::createNode<hlim::Node_Compare>(op);
        node->recordStackTrace();
        node->connectInput(0, lhs.getReadPort());
        node->connectInput(1, rhs.getReadPort());

        return { .node = node, .port = 0 };
    }

    inline Bit eq(BVecSignalPort lhs, BVecSignalPort rhs) { return makeNode(hlim::Node_Compare::EQ, lhs, rhs); }
    inline Bit neq(BVecSignalPort lhs, BVecSignalPort rhs) { return makeNode(hlim::Node_Compare::NEQ, lhs, rhs); }
    inline Bit gt(BVecSignalPort lhs, BVecSignalPort rhs) { return makeNode(hlim::Node_Compare::GT, lhs, rhs); }
    inline Bit lt(BVecSignalPort lhs, BVecSignalPort rhs) { return makeNode(hlim::Node_Compare::LT, lhs, rhs); }
    inline Bit geq(BVecSignalPort lhs, BVecSignalPort rhs) { return makeNode(hlim::Node_Compare::GEQ, lhs, rhs); }
    inline Bit leq(BVecSignalPort lhs, BVecSignalPort rhs) { return makeNode(hlim::Node_Compare::LEQ, lhs, rhs); }

    inline Bit operator == (BVecSignalPort lhs, BVecSignalPort rhs) { return eq(lhs, rhs); }
    inline Bit operator != (BVecSignalPort lhs, BVecSignalPort rhs) { return neq(lhs, rhs); }
    inline Bit operator <= (BVecSignalPort lhs, BVecSignalPort rhs) { return leq(lhs, rhs); }
    inline Bit operator >= (BVecSignalPort lhs, BVecSignalPort rhs) { return geq(lhs, rhs); }
    inline Bit operator < (BVecSignalPort lhs, BVecSignalPort rhs) { return lt(lhs, rhs); }
    inline Bit operator > (BVecSignalPort lhs, BVecSignalPort rhs) { return gt(lhs, rhs); }


    inline Bit eq(BitSignalPort lhs, BitSignalPort rhs) { return makeNode(hlim::Node_Compare::EQ, lhs, rhs); }
    inline Bit neq(BitSignalPort lhs, BitSignalPort rhs) { return makeNode(hlim::Node_Compare::NEQ, lhs, rhs); }

    inline Bit operator == (BitSignalPort lhs, BitSignalPort rhs) { return eq(lhs, rhs); }
    inline Bit operator != (BitSignalPort lhs, BitSignalPort rhs) { return neq(lhs, rhs); }
}

