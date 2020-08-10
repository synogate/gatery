#include "Signal.h"
#include "Scope.h"
#include "ConditionalScope.h"

#include <hcl/utils/Exceptions.h>
#include <hcl/hlim/coreNodes/Node_Signal.h>
#include <hcl/hlim/coreNodes/Node_Multiplexer.h>

#include <iostream>

namespace hcl::core::frontend {

    ElementarySignal::ElementarySignal(InitInvalid)
    {
    }
    
    ElementarySignal::ElementarySignal(const hlim::ConnectionType& connType, InitUnconnected)
    {
        init(connType);
    }

    ElementarySignal::ElementarySignal(const hlim::NodePort& port, InitOperation)
    {
        init(port.node->getOutputConnectionType(port.port));
        m_node->connectInput(port);
    }

    ElementarySignal::ElementarySignal(const ElementarySignal& rhs, InitCopyCtor)
    {
        init(rhs.getConnType());
        m_node->connectInput(rhs.getReadPort());
    }
    
    ElementarySignal::ElementarySignal(const ElementarySignal& ancestor, InitSuccessor)
    {
        initSuccessor(ancestor);
    }

    ElementarySignal::~ElementarySignal()
    {
    }

    void ElementarySignal::setName(std::string name)
    {
        m_node->setName(name);
    }

    void ElementarySignal::init(const hlim::ConnectionType& connType)
    {
        m_node = DesignScope::createNode<hlim::Node_Signal>();
        m_node->setConnectionType(connType);
        m_node->recordStackTrace();
    }

    void ElementarySignal::initSuccessor(const ElementarySignal& ancestor)
    {
        init(ancestor.getConnType());

        m_node->connectInput({.node = ancestor.m_node, .port = 0});
    }

    void ElementarySignal::assign(const ElementarySignal& rhs) {

        if (!m_node)
            init(rhs.getConnType());

        if (getName().empty())
            setName(rhs.getName());

#define INSERT_BRIDGE_NODES
        
#ifdef INSERT_BRIDGE_NODES
        hlim::Node_Signal *bridgeNode = DesignScope::createNode<hlim::Node_Signal>();
        bridgeNode->setConnectionType(rhs.getConnType());
        bridgeNode->recordStackTrace();
        bridgeNode->setName(m_node->getName());
#else
        bridgeNode = m_node;
#endif

        if (ConditionalScope::get() == nullptr)
        {
            bridgeNode->connectInput(rhs.getReadPort());
        }
        else
        {
            hlim::Node_Multiplexer* mux = DesignScope::createNode<hlim::Node_Multiplexer>(2);
            mux->connectInput(0, getReadPort());
            mux->connectInput(1, rhs.getReadPort()); // assign rhs last in case previous port was undefined
            mux->connectSelector(ConditionalScope::getCurrentConditionPort());

            bridgeNode->connectInput({ .node = mux, .port = 0ull });
        }
#ifdef INSERT_BRIDGE_NODES
        m_node->connectInput({ .node = bridgeNode, .port = 0ull });
#endif
    }

    void ElementarySignal::setConnectionType(const hlim::ConnectionType& connectionType)
    {
        if (connectionType.width != getWidth())
            HCL_DESIGNCHECK_HINT(m_node->isOrphaned(), "Can not resize signal once it is connected (driving or driven).");

        m_node->setConnectionType(connectionType);
    }

}
