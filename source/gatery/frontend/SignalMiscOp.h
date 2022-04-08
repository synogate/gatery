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

#include "Signal.h"
#include "Bit.h"
#include "BVec.h"
#include "Scope.h"
#include "ConditionalScope.h"

#include <gatery/hlim/coreNodes/Node_Multiplexer.h>
#include <gatery/hlim/supportNodes/Node_SignalTap.h>
#include <gatery/hlim/NodePtr.h>
#include <gatery/utils/Preprocessor.h>
#include <gatery/utils/Traits.h>


#include <boost/spirit/home/support/container.hpp>
#include <boost/hana/adapt_struct.hpp>
#include <boost/hana/for_each.hpp>
#include <boost/hana/fwd/accessors.hpp>

#include <boost/format.hpp>


#include <type_traits>

namespace gtry {

/**
 * @addtogroup gtry_compare Miscellaneous Operations for Signals
 * @ingroup gtry_frontend
 * @brief All signal operations that don't fit into other categories
 * @{
 */



///@todo overload for compound signals
template<typename ContainerType>//, typename = std::enable_if_t<utils::isContainer<ContainerType>::value>>
typename ContainerType::value_type mux(const ElementarySignal &selector, const ContainerType &table) {

    const SignalReadPort selPort = selector.getReadPort();
    const size_t selPortWidth = selector.width().value;
    size_t tableSize = table.size();

    if (tableSize > (1ull << selPortWidth))
    {
        HCL_DESIGNCHECK_HINT(selPort.expansionPolicy == Expansion::zero, "The number of mux inputs is larger than can be addressed with it's selector input's width!");
        tableSize = 1ull << selPortWidth;
    }

    hlim::Node_Multiplexer *node = DesignScope::createNode<hlim::Node_Multiplexer>(tableSize);
    node->recordStackTrace();
    node->connectSelector(selPort);

    const auto it_end = begin(table) + tableSize;

    hlim::ConnectionType elementType;
    for (auto it = begin(table); it != it_end; ++it)
    {
        hlim::ConnectionType t = it->getConnType();
        if (t.width > elementType.width)
            elementType = t;
    }

    size_t idx = 0;
    for (auto it = begin(table); it != it_end; ++it, ++idx)
        node->connectInput(idx, it->getReadPort().expand(elementType.width, elementType.interpretation));

    return SignalReadPort(node);
}

template<typename ElemType>
ElemType mux(const ElementarySignal &selector, const std::initializer_list<ElemType> &table) {
    return mux<std::initializer_list<ElemType>>(selector, table);
}

BVec muxWord(UInt selector, BVec flat_array);
BVec muxWord(Bit selector, BVec flat_array);

UInt muxWord(UInt selector, UInt flat_array);
UInt muxWord(Bit selector, UInt flat_array);

SInt muxWord(UInt selector, SInt flat_array);
SInt muxWord(Bit selector, SInt flat_array);

UInt demux(const UInt& selector, const Bit& input, const Bit& inactiveOutput = '0');
UInt demux(const UInt& selector); // optimized version for input = '1' and invactive output = '0'

BVec swapEndian(const BVec& word, BitWidth byteSize, BitWidth wordSize);
BVec swapEndian(const BVec& word, BitWidth byteSize = 8_b);

UInt swapEndian(const UInt& word, BitWidth byteSize, BitWidth wordSize);
UInt swapEndian(const UInt& word, BitWidth byteSize = 8_b);

SInt swapEndian(const SInt& word, BitWidth byteSize, BitWidth wordSize);
SInt swapEndian(const SInt& word, BitWidth byteSize = 8_b);


class SignalTapHelper
{
    public:
        SignalTapHelper(hlim::Node_SignalTap::Level level);

        void triggerIf(const Bit &condition);
        void triggerIfNot(const Bit &condition);

        SignalTapHelper &operator<<(const std::string &msg);

        template<typename type, typename = std::enable_if_t<std::is_arithmetic<type>::value>>
        SignalTapHelper &operator<<(type number) { return (*this) << boost::lexical_cast<std::string>(number); }

        template<BaseSignal SignalType>
        SignalTapHelper &operator<<(const SignalType &signal) {
            unsigned port = (unsigned)addInput(signal.getReadPort());
            m_node->addMessagePart(hlim::Node_SignalTap::FormattedSignal{.inputIdx = port, .format = 0});
            return *this;
        }
    protected:
        size_t addInput(hlim::NodePort nodePort);
        hlim::NodePtr<hlim::Node_SignalTap> m_node;
};




SignalTapHelper sim_assert(const Bit &condition);
SignalTapHelper sim_warnIf(const Bit &condition);

SignalTapHelper sim_debug();
SignalTapHelper sim_debugAlways();
SignalTapHelper sim_debugIf(const Bit &condition);



template<typename Signal, typename std::enable_if_t<std::is_base_of_v<ElementarySignal, Signal>>* = nullptr  >
void sim_tap(const Signal& signal)
{
    auto *node = DesignScope::createNode<hlim::Node_SignalTap>();
    node->recordStackTrace();
    node->setLevel(hlim::Node_SignalTap::LVL_WATCH);
    node->setName(std::string(signal.getName()));

    node->addInput(signal.getReadPort());
}

template<typename Compound, typename std::enable_if_t<!std::is_base_of_v<ElementarySignal, Compound>>* = nullptr  >
void sim_tap(const Compound& compound)
{
    if constexpr (boost::spirit::traits::is_container<Compound>::value)
    {

        for (auto it = begin(compound); it != end(compound); ++it)
            sim_tap(*it);
    }
    else if constexpr (boost::hana::Struct<Compound>::value)
    {
        boost::hana::for_each(boost::hana::accessors<std::remove_cv_t<Compound>>(), [&](auto member) {
            auto& src_item = boost::hana::second(member)(compound);
            sim_tap(src_item);
        });
    }
}


/**@}*/

}
