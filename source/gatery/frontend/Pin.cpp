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

#include "Scope.h"

namespace gtry {


    OutputPin::OutputPin(const Bit &bit) {
        m_pinNode = DesignScope::createNode<hlim::Node_Pin>();
        m_pinNode->connect(bit.getReadPort());
        m_pinNode->setName(std::string(bit.getName()));
    }



    OutputPins::OutputPins(const BVec &bitVector) {
        m_pinNode = DesignScope::createNode<hlim::Node_Pin>();
        m_pinNode->connect(bitVector.getReadPort());
        m_pinNode->setName(std::string(bitVector.getName()));
    }

    InputPin::InputPin() {
        m_pinNode = DesignScope::createNode<hlim::Node_Pin>();
        m_pinNode->setBool();
    }

    InputPins::InputPins(BitWidth width) {
        m_pinNode = DesignScope::createNode<hlim::Node_Pin>();
        m_pinNode->setWidth(width.value);
    }

}
