#pragma once

#include "../hlim/NodePort.h"
#include "BitVectorState.h"

#include <boost/spirit/home/support/container.hpp>

namespace hcl::core::frontend {
    class Bit;
    class BVec;
}

namespace hcl::core::sim {

class SigHandle {
    public:
        SigHandle(hlim::NodePort output) : m_output(output) { }
        void operator=(const SigHandle &rhs) { this->operator=(rhs.eval()); }


        template<typename T, typename Ten = std::enable_if_t<boost::spirit::traits::is_container<T>::value>>
        void operator=(const T& collection);

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


template<typename T, typename Ten>
inline void SigHandle::operator=(const T& collection)
{
    auto it_begin = begin(collection);
    auto it_end = end(collection);

    DefaultBitVectorState state;
    if (it_begin != it_end)
    {
        state.resize((it_end - it_begin) * sizeof(*it_begin) * 8);
        size_t offset = 0;
        for (auto it = it_begin; it != it_end; ++it)
        {
            state.insertNonStraddling(core::sim::DefaultConfig::VALUE, offset, sizeof(*it) * 8, *it);
            state.insertNonStraddling(core::sim::DefaultConfig::DEFINED, offset, sizeof(*it) * 8, -1);
            offset += sizeof(*it) * 8;
        }
    }
    *this = state;
}

}
