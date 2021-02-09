#pragma once

#include "../hlim/NodePort.h"
#include "BitVectorState.h"

namespace hcl::core::frontend {
    class Bit;
    class BVec;
}

namespace hcl::core::sim {

class SigHandle {
    public:
        SigHandle(hlim::NodePort output) : m_output(output) { }

        void operator=(std::uint64_t v);
        void operator=(const DefaultBitVectorState &state);

        operator std::uint64_t () const { return value(); }
        operator DefaultBitVectorState () const { return eval(); }

        std::uint64_t defined() const;
        std::uint64_t value() const;

        DefaultBitVectorState eval() const;
    protected:
        hlim::NodePort m_output;
};


}