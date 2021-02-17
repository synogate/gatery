#include "Pin.h"

#include "Scope.h"

namespace hcl::core::frontend {


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