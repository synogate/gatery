/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Michael Offel, Andreas Ley

    Gatery is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 3 of the License, or (at your option) any later version.

    Gatery is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#pragma once

#include "../hlim/NodePort.h"
#include "BitVectorState.h"

#include <boost/spirit/home/support/container.hpp>

namespace gtry {
    class Bit;
    class BVec;
}

namespace gtry::sim {

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

        hlim::NodePort getOutput() const { return m_output; }
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
            state.insertNonStraddling(sim::DefaultConfig::VALUE, offset, sizeof(*it) * 8, *it);
            state.insertNonStraddling(sim::DefaultConfig::DEFINED, offset, sizeof(*it) * 8, -1);
            offset += sizeof(*it) * 8;
        }
    }
    *this = state;
}

}
