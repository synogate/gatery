#include "Signal.h"
#include "Scope.h"
#include "ConditionalScope.h"

#include <hcl/utils/Exceptions.h>
#include <hcl/hlim/coreNodes/Node_Signal.h>
#include <hcl/hlim/coreNodes/Node_Multiplexer.h>

#include <iostream>

namespace hcl::core::frontend {
    ElementarySignal::ElementarySignal(const hlim::ConnectionType& connType)
    {
        init(connType);
    }

    ElementarySignal::ElementarySignal(const hlim::NodePort& port)
    {
        init(port.node->getOutputConnectionType(port.port));
        m_entryNode->connectInput(port);
    }

    ElementarySignal::~ElementarySignal()
    {
    }


    void ElementarySignal::setName(std::string name)
    {
        m_entryNode->setName(name);
        m_valueNode->setName(name);
        m_exitNode->setName(name);
    }

    void ElementarySignal::init(const hlim::ConnectionType& connType)
    {
        m_entryNode = DesignScope::createNode<hlim::Node_Signal>();
        m_entryNode->setConnectionType(connType);
        m_entryNode->recordStackTrace();
        m_valueNode = m_entryNode;

        m_exitNode = DesignScope::createNode<hlim::Node_Signal>();
        m_exitNode->setConnectionType(connType);
        m_exitNode->recordStackTrace();
        m_exitNode->connectInput(getReadPort());
    }

    void ElementarySignal::assign(const ElementarySignal& rhs) {

        if (!m_exitNode)
            init(rhs.getConnType());

        if (getName().empty())
            setName(rhs.getName());

        const hlim::NodePort previousOutput = getReadPort();

        m_valueNode = DesignScope::createNode<hlim::Node_Signal>();
        m_valueNode->recordStackTrace();
        m_valueNode->setConnectionType(rhs.getConnType());
        m_valueNode->setName(getName());

        if (ConditionalScope::get() == nullptr)
        {
            m_valueNode->connectInput(rhs.getReadPort());
        }
        else
        {
            hlim::Node_Multiplexer* mux = DesignScope::createNode<hlim::Node_Multiplexer>(2);
            mux->connectInput(0, previousOutput);
            mux->connectInput(1, rhs.getReadPort());
            mux->connectSelector(ConditionalScope::getCurrentConditionPort());

            m_valueNode->connectInput({ .node = mux, .port = 0ull });
        }

        m_exitNode->connectInput(getReadPort());
    }

    void ElementarySignal::setConnectionType(const hlim::ConnectionType& connectionType)
    {
        if (connectionType.width != getWidth())
        {
            HCL_DESIGNCHECK_HINT(m_exitNode->isOrphaned(), "Can not resize signal once it is connected (driving or driven).");

            m_entryNode->setConnectionType(connectionType);
            m_exitNode->setConnectionType(connectionType);
        }

        m_valueNode->setConnectionType(connectionType);
    }

}
