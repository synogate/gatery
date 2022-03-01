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
#include "gatery/pch.h"
#include "Pin.h"

#include "Bit.h"
#include "BVec.h"
#include "UInt.h"
#include "SInt.h"

#include "Scope.h"

namespace gtry {


    OutputPin::OutputPin(const Bit &bit) {
        m_pinNode = DesignScope::createNode<hlim::Node_Pin>(false);
        m_pinNode->connect(bit.getReadPort());
        m_pinNode->setName(std::string(bit.getName()));
    }

    OutputPins::OutputPins(const BVec &bitVector) {
        m_pinNode = DesignScope::createNode<hlim::Node_Pin>(false);
        m_pinNode->connect(bitVector.getReadPort());
        m_pinNode->setName(std::string(bitVector.getName()));
    }

    OutputPins::OutputPins(const UInt &bitVector) {
        m_pinNode = DesignScope::createNode<hlim::Node_Pin>(false);
        m_pinNode->connect(bitVector.getReadPort());
        m_pinNode->setName(std::string(bitVector.getName()));
    }

    OutputPins::OutputPins(const SInt &bitVector) {
        m_pinNode = DesignScope::createNode<hlim::Node_Pin>(false);
        m_pinNode->connect(bitVector.getReadPort());
        m_pinNode->setName(std::string(bitVector.getName()));
    }

    InputPin::InputPin() {
        m_pinNode = DesignScope::createNode<hlim::Node_Pin>(true);
        m_pinNode->setBool();
    }


    InputPin::operator Bit () const
    {
#if 0
        return Bit(SignalReadPort({.node=m_pinNode, .port=0ull}));
#else
        auto* signal = DesignScope::createNode<hlim::Node_Signal>();
        signal->connectInput({.node=m_pinNode, .port=0ull});
        signal->recordStackTrace();
        return Bit(SignalReadPort(signal));
#endif
    }

    InputPins::InputPins(BitWidth width) {
        m_pinNode = DesignScope::createNode<hlim::Node_Pin>(true);
        m_pinNode->setWidth(width.value);
    }

    InputPins::operator BVec () const { return BVec(SignalReadPort({.node=m_pinNode, .port=0ull})); }
    InputPins::operator UInt () const { return UInt(SignalReadPort({.node=m_pinNode, .port=0ull})); }
    InputPins::operator SInt () const { return SInt(SignalReadPort({.node=m_pinNode, .port=0ull})); }

    OutputPins pinOut(const InputPins &input) { return OutputPins((BVec)input); }

}
