#include "Pin.h"

#include "Scope.h"

namespace hcl::core::frontend {


    OutputPin::OutputPin(const Bit &bit) {
        m_pinNode = DesignScope::createNode<hlim::Node_Pin>();
        m_pinNode->connect(bit.getReadPort());
    }

    OutputPins::OutputPins(const BVec &bitVector) {
        m_pinNode = DesignScope::createNode<hlim::Node_Pin>();
        m_pinNode->connect(bitVector.getReadPort());
    }
    
    InputPin::InputPin() {
        m_pinNode = DesignScope::createNode<hlim::Node_Pin>();
        m_pinNode->setBool();
    }

    InputPins::InputPins(unsigned width) {
        m_pinNode = DesignScope::createNode<hlim::Node_Pin>();
        m_pinNode->setWidth(width);
    }

}