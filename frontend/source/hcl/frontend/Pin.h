#pragma once

#include "Bit.h"
#include "BitVector.h"

#include <hcl/hlim/coreNodes/Node_Pin.h>

namespace hcl::core::frontend {

    class OutputPin {
        public:
            OutputPin(const Bit &bit);

            inline OutputPin &setName(std::string name) { m_pinNode->setName(std::move(name)); return *this; }
        protected:
            hlim::Node_Pin *m_pinNode;
    };
    class OutputPins {
        public:
            OutputPins(const BVec &bitVector);           

            inline OutputPins &setName(std::string name) { m_pinNode->setName(std::move(name)); return *this; }
        protected:
            hlim::Node_Pin *m_pinNode;
    };

    class InputPin {
        public:
            InputPin();

            inline operator Bit () { return Bit(SignalReadPort({.node=m_pinNode, .port=0ull})); }

            inline InputPin &setName(std::string name) { m_pinNode->setName(std::move(name)); return *this; }
        protected:
            hlim::Node_Pin *m_pinNode;
    };

    class InputPins {
        public:
            InputPins(BitWidth width);
            inline operator BVec () { return BVec(SignalReadPort({.node=m_pinNode, .port=0ull})); }

            inline InputPins &setName(std::string name) { m_pinNode->setName(std::move(name)); return *this; }
        protected:
            hlim::Node_Pin *m_pinNode;
    };

    inline OutputPin pinOut(const Bit &bit) { return OutputPin(bit); }
    inline OutputPins pinOut(const BVec &bitVector) { return OutputPins(bitVector); }

    inline InputPin pinIn() { return InputPin(); }
    inline InputPins pinIn(BitWidth width) { return InputPins(width); }
}