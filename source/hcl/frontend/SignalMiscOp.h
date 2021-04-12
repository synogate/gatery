/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Synogate GbR

    Gatery is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

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
#include "BitVector.h"
#include "Scope.h"
#include "ConditionalScope.h"

#include <hcl/hlim/coreNodes/Node_Multiplexer.h>
#include <hcl/hlim/supportNodes/Node_SignalTap.h>
#include <hcl/hlim/NodePtr.h>
#include <hcl/utils/Preprocessor.h>
#include <hcl/utils/Traits.h>


#include <boost/spirit/home/support/container.hpp>
#include <boost/hana/adapt_struct.hpp>
#include <boost/hana/for_each.hpp>
#include <boost/hana/fwd/accessors.hpp>

#include <boost/format.hpp>


#include <type_traits>

namespace hcl::core::frontend {

///@todo overload for compound signals
template<typename ContainerType>//, typename = std::enable_if_t<utils::isContainer<ContainerType>::value>>
typename ContainerType::value_type mux(const ElementarySignal &selector, const ContainerType &table) {

    const SignalReadPort selPort = selector.getReadPort();
    const size_t selPortWidth = selector.getWidth().value;
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



///@todo overload for compound signals
///@todo doesn't work yet
#if 0
template<typename SelectorType, typename ContainerType, typename SignalType = typename ContainerType::value_type, typename = std::enable_if_t<
                                                                                                utils::isElementarySignal<SignalType>::value &&
                                                                                                utils::isUnsignedIntegerSignal<SelectorType>::value &&
                                                                                                utils::isContainer<ContainerType>::value
                                                                                            >>
SignalType mux(const SelectorType &selector, const ContainerType &inputs)  {
    HCL_DESIGNCHECK_HINT(!inputs.empty(), "Inputs can not be empty");

    const hlim::Node_Signal *firstSignal = inputs.begin()->getNode();
    HCL_ASSERT(firstSignal != nullptr);

    hlim::Node_Multiplexer *node = DesignScope::createNode<hlim::Node_Multiplexer>(inputs.size());
    node->recordStackTrace();
    selector.getNode()->getOutput(0).connect(node->getInput(0));
    for (const auto &pair : ConstEnumerate(inputs)) {
        const hlim::Node_Signal *thisSignal = pair.second.getNode();
        HCL_ASSERT(thisSignal != nullptr);
        HCL_DESIGNCHECK_HINT(firstSignal->getConnectionType() == thisSignal->getConnectionType(), "Can only multiplex operands of same type (e.g. width).");

        pair.second.getNode()->getOutput(0).connect(node->getInput(pair.first+1));
    }

    return SignalType(&node->getOutput(0), firstSignal->getConnectionType());
}
#endif

BVec swapEndian(const BVec& word, size_t byteSize = 8);

/*
template<typename SignalType, typename = std::enable_if_t<utils::isBitVectorSignal<SignalType>::value>>
class hex
{
};

todo: pretty printing functions
*/


class SignalTapHelper
{
    public:
        SignalTapHelper(hlim::Node_SignalTap::Level level);

        void triggerIf(const Bit &condition);
        void triggerIfNot(const Bit &condition);

        SignalTapHelper &operator<<(const std::string &msg);

        template<typename type, typename = std::enable_if_t<std::is_arithmetic<type>::value>>
        SignalTapHelper &operator<<(type number) { return (*this) << boost::lexical_cast<std::string>(number); }

        template<typename SignalType, typename = std::enable_if_t<utils::isElementarySignal<SignalType>::value>>
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




}
