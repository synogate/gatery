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

#include "DesignScope.h"

#include <gatery/hlim/coreNodes/Node_Pin.h>
#include <gatery/hlim/coreNodes/Node_Signal.h>
#include <gatery/hlim/NodePtr.h>

namespace gtry {

    class Bit;
    class BVec;
    class UInt;
    class SInt;

    class OutputPin {
        public:
            OutputPin(const Bit &bit);

            inline OutputPin &setName(std::string name) { m_pinNode->setName(std::move(name)); return *this; }
            inline hlim::Node_Pin *getNode() { return m_pinNode.get(); }
            inline hlim::Node_Pin *getNode() const { return m_pinNode.get(); }

            inline OutputPin &setDifferential(std::string_view posPrefix = "_p", std::string_view negPrefix = "_n") { m_pinNode->setDifferential(posPrefix, negPrefix); return *this; }
        protected:
            hlim::NodePtr<hlim::Node_Pin> m_pinNode;
    };

    class OutputPins {
        public:
            OutputPins(const BVec &bitVector);
            OutputPins(const UInt &bitVector);
            OutputPins(const SInt &bitVector);

            inline OutputPins &setName(std::string name) { m_pinNode->setName(std::move(name)); return *this; }
            inline hlim::Node_Pin *getNode() { return m_pinNode.get(); }
            inline hlim::Node_Pin *getNode() const { return m_pinNode.get(); }

            inline OutputPins &setDifferential(std::string_view posPrefix = "_p", std::string_view negPrefix = "_n") { m_pinNode->setDifferential(posPrefix, negPrefix); return *this; }
        protected:
            hlim::NodePtr<hlim::Node_Pin> m_pinNode;
    };

    class InputPin {
        public:
            InputPin();

            operator Bit () const;

            inline InputPin &setName(std::string name) { m_pinNode->setName(std::move(name)); return *this; }
            inline hlim::Node_Pin *getNode() { return m_pinNode.get(); }
            inline hlim::Node_Pin *getNode() const { return m_pinNode.get(); }
        protected:
            hlim::NodePtr<hlim::Node_Pin> m_pinNode;
    };

    class InputPins {
        public:
            InputPins(BitWidth width);
            operator BVec () const;
            operator UInt () const;
            operator SInt () const;

            inline InputPins &setName(std::string name) { m_pinNode->setName(std::move(name)); return *this; }
            inline hlim::Node_Pin *getNode() { return m_pinNode.get(); }
            inline hlim::Node_Pin *getNode() const { return m_pinNode.get(); }
        protected:
            hlim::NodePtr<hlim::Node_Pin> m_pinNode;
    };

    inline OutputPin pinOut(const Bit &bit) { return OutputPin(bit); }
    inline OutputPins pinOut(const BVec &bitVector) { return OutputPins(bitVector); }
    inline OutputPins pinOut(const UInt &bitVector) { return OutputPins(bitVector); }
    inline OutputPins pinOut(const SInt &bitVector) { return OutputPins(bitVector); }

    OutputPins pinOut(const InputPins &input);

    inline InputPin pinIn() { return InputPin(); }
    inline InputPins pinIn(BitWidth width) { return InputPins(width); }
}
