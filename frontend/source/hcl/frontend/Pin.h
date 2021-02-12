#pragma once

#include "Bit.h"
#include "BitVector.h"

#include <hcl/hlim/coreNodes/Node_Pin.h>
#include <hcl/hlim/coreNodes/Node_Signal.h>

namespace hcl::core::frontend {

    class OutputPin {
        public:
            OutputPin(const Bit &bit);

            inline OutputPin &setName(std::string name) { m_pinNode->setName(std::move(name)); return *this; }
            inline hlim::Node_Pin *getNode() { return m_pinNode; }
            inline hlim::Node_Pin *getNode() const { return m_pinNode; }
        protected:
            hlim::Node_Pin *m_pinNode;
    };
    class OutputPins {
        public:
            OutputPins(const BVec &bitVector);

            inline OutputPins &setName(std::string name) { m_pinNode->setName(std::move(name)); return *this; }
            inline hlim::Node_Pin *getNode() { return m_pinNode; }
            inline hlim::Node_Pin *getNode() const { return m_pinNode; }
        protected:
            hlim::Node_Pin *m_pinNode;
    };

    class InputPin {
        public:
            InputPin();

            inline operator Bit () {
#if 0
                return Bit(SignalReadPort({.node=m_pinNode, .port=0ull}));
#else
                auto* signal = DesignScope::createNode<hlim::Node_Signal>();
                signal->connectInput({.node=m_pinNode, .port=0ull});
                signal->setName(m_pinNode->getName());
                signal->recordStackTrace();
                return Bit(SignalReadPort(signal));
#endif
            }

            inline InputPin &setName(std::string name) { m_pinNode->setName(std::move(name)); return *this; }
            inline hlim::Node_Pin *getNode() { return m_pinNode; }
            inline hlim::Node_Pin *getNode() const { return m_pinNode; }
        protected:
            hlim::Node_Pin *m_pinNode;
    };

    class InputPins {
        public:
            InputPins(BitWidth width);
            inline operator BVec () { return BVec(SignalReadPort({.node=m_pinNode, .port=0ull})); }

            inline InputPins &setName(std::string name) { m_pinNode->setName(std::move(name)); return *this; }
            inline hlim::Node_Pin *getNode() { return m_pinNode; }
            inline hlim::Node_Pin *getNode() const { return m_pinNode; }
        protected:
            hlim::Node_Pin *m_pinNode;
    };

    inline OutputPin pinOut(const Bit &bit) { return OutputPin(bit); }
    inline OutputPins pinOut(const BVec &bitVector) { return OutputPins(bitVector); }

    inline InputPin pinIn() { return InputPin(); }
    inline InputPins pinIn(BitWidth width) { return InputPins(width); }
}