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

#include "Signal.h"

#include <gatery/hlim/coreNodes/Node_Pin.h>
#include <gatery/hlim/coreNodes/Node_Signal.h>
#include <gatery/hlim/NodePtr.h>

namespace gtry {

    class Bit;
    class BVec;
    class UInt;
    class SInt;

    class BaseOutputPin {
        public:
            BaseOutputPin(hlim::NodePort nodePort, std::string name);

            inline hlim::Node_Pin *getNode() { return m_pinNode.get(); }
            inline hlim::Node_Pin *getNode() const { return m_pinNode.get(); }
        protected:
            hlim::NodePtr<hlim::Node_Pin> m_pinNode;
    };    

    class OutputPin : public BaseOutputPin {
        public:
            OutputPin(const Bit &bit);

            inline OutputPin &setName(std::string name) { m_pinNode->setName(std::move(name)); return *this; }
            inline OutputPin &setDifferential(std::string_view posPrefix = "_p", std::string_view negPrefix = "_n") { m_pinNode->setDifferential(posPrefix, negPrefix); return *this; }

    };

    class OutputPins : public BaseOutputPin {
        public:
            template<BitVectorDerived T>
            OutputPins(const T &bitVector) : BaseOutputPin(bitVector.getReadPort(), std::string(bitVector.getName())) { }

            inline OutputPins &setName(std::string name) { m_pinNode->setName(std::move(name)); return *this; }
            inline OutputPins &setDifferential(std::string_view posPrefix = "_p", std::string_view negPrefix = "_n") { m_pinNode->setDifferential(posPrefix, negPrefix); return *this; }
    };


    class BaseInputPin { 
        public:
            BaseInputPin();

            inline hlim::Node_Pin *getNode() { return m_pinNode.get(); }
            inline hlim::Node_Pin *getNode() const { return m_pinNode.get(); }
        protected:
            hlim::NodePtr<hlim::Node_Pin> m_pinNode;    
    };

    class InputPin : public BaseInputPin {
        public:
            InputPin();
            operator Bit () const;
            inline InputPin &setName(std::string name) { m_pinNode->setName(std::move(name)); return *this; }
    };

    class InputPins : public BaseInputPin {
        public:
            InputPins(BitWidth width);
            operator UInt () const;

            template<BitVectorDerived T> requires (!std::same_as<UInt, T>)
            explicit operator T () const { return T(SignalReadPort({.node=m_pinNode, .port=0ull})); }

            inline InputPins &setName(std::string name) { m_pinNode->setName(std::move(name)); return *this; }
    };

    inline OutputPin pinOut(const Bit &bit) { return OutputPin(bit); }
    template<BitVectorValue T>
    inline OutputPins pinOut(const T &bitVector) { return OutputPins((ValueToBaseSignal<T>)bitVector); }

    OutputPins pinOut(const InputPins &input);

    inline InputPin pinIn() { return InputPin(); }
    inline InputPins pinIn(BitWidth width) { return InputPins(width); }
}
