#include "Bit.h"
#include "BitVector.h"
#include "ConditionalScope.h"
#include "Constant.h"

#include <hcl/hlim/coreNodes/Node_Constant.h>
#include <hcl/hlim/coreNodes/Node_Rewire.h>
#include <hcl/hlim/coreNodes/Node_Multiplexer.h>

namespace hcl::core::frontend {

    Bit::Bit()
    {
        createNode();
    }

    Bit::Bit(const Bit& rhs) : Bit(rhs.getReadPort())
    {
    }

    Bit::Bit(Bit&& rhs) : Bit()
    {
        assign(rhs.getReadPort());
        rhs.assign(SignalReadPort{ m_node });
    }

    Bit::~Bit()
    {
        m_node->removeRef();
    }

    Bit::Bit(const SignalReadPort& port)
    {
        createNode();
        m_node->connectInput(port);
    }

    Bit::Bit(hlim::Node_Signal* node, size_t offset) :
        m_node(node),
        m_offset(offset)
    {
        m_node->addRef();
    }

    Bit& Bit::operator=(Bit&& rhs)
    {
        assign(rhs.getReadPort());

        SignalReadPort outRange{ m_node };
        hlim::ConnectionType type = m_node->getOutputConnectionType(0);
        if (type.interpretation != hlim::ConnectionType::BOOL)
        {
            auto* rewire = DesignScope::createNode<hlim::Node_Rewire>(1);
            rewire->setName(std::string(getName()));
            rewire->connectInput(0, outRange);
            rewire->changeOutputType(getConnType());

            size_t offset = std::min(m_offset, type.width - 1); // used for msb alias, but can alias any future offset
            rewire->setExtract(offset, 1);

            outRange = SignalReadPort(rewire);
        }
        rhs.assign(outRange);
        return *this;
    }

    BitWidth Bit::getWidth() const
    {
        return 1_b;
    }

    hlim::ConnectionType Bit::getConnType() const
    {
        return hlim::ConnectionType{
            .interpretation = hlim::ConnectionType::BOOL,
            .width = 1
        };
    }

    SignalReadPort Bit::getReadPort() const
    {
        SignalReadPort ret = getRawDriver();

        hlim::ConnectionType type = ret.node->getOutputConnectionType(ret.port);
        if (type.interpretation != hlim::ConnectionType::BOOL)
        {
            // TODO: cache rewire node if m_node's input is unchanged
            auto* rewire = DesignScope::createNode<hlim::Node_Rewire>(1);
            rewire->setName(std::string(getName()));
            rewire->connectInput(0, ret);
            rewire->changeOutputType(getConnType());

            size_t offset = std::min(m_offset, type.width-1); // used for msb alias, but can alias any future offset
            rewire->setExtract(offset, 1);

            ret = SignalReadPort(rewire);
        }
        return ret;
    }

    std::string_view Bit::getName() const
    {
        return m_node->getName();
    }

    void Bit::setName(std::string name)
    {
        if (m_node->getDriver(0).node != nullptr)
            m_node->getDriver(0).node->setName(name);
        m_node->setName(move(name));
    }

    void Bit::addToSignalGroup(hlim::SignalGroup *signalGroup)
    {
        m_node->moveToSignalGroup(signalGroup);
    }

    void Bit::createNode()
    {
        HCL_ASSERT(!m_node);
        m_node = DesignScope::createNode<hlim::Node_Signal>();
        m_node->addRef();
        m_node->setConnectionType(getConnType());
        m_node->recordStackTrace();
    }

    void Bit::assign(bool value)
    {
        auto* constant = DesignScope::createNode<hlim::Node_Constant>(
            parseBit(value), hlim::ConnectionType::BOOL
            );
        assign(SignalReadPort(constant));
    }

    void Bit::assign(char value)
    {
        auto* constant = DesignScope::createNode<hlim::Node_Constant>(
            parseBit(value), hlim::ConnectionType::BOOL
            );
        assign(SignalReadPort(constant));
    }

    void Bit::assign(SignalReadPort in)
    {
        hlim::ConnectionType type = m_node->getOutputConnectionType(0);

        if (getName().empty())
            setName(in.node->getName());

        if (type.interpretation != hlim::ConnectionType::BOOL)
        {
            std::string in_name = in.node->getName();
            
            auto* rewire = DesignScope::createNode<hlim::Node_Rewire>(2);
            rewire->connectInput(0, getRawDriver());
            rewire->connectInput(1, in);
            rewire->changeOutputType(type);

            size_t offset = std::min(m_offset, type.width - 1); // used for msb alias, but can alias any future offset
            rewire->setReplaceRange(offset);

            in = SignalReadPort(rewire);
            {
                auto* signal = DesignScope::createNode<hlim::Node_Signal>();
                signal->connectInput(in);
                signal->setName(in_name);
                signal->recordStackTrace();
                in = SignalReadPort(signal);
            }
        }
        
        if (auto* scope = ConditionalScope::get(); scope && scope->getId() > m_initialScopeId)
        {
            auto* signal_in = DesignScope::createNode<hlim::Node_Signal>();
            signal_in->connectInput(getRawDriver());
            signal_in->setName(m_node->getName());

            auto* mux = DesignScope::createNode<hlim::Node_Multiplexer>(2);
            mux->connectInput(0, {.node = signal_in, .port = 0});
            mux->connectInput(1, in); // assign rhs last in case previous port was undefined
            mux->connectSelector(scope->getFullCondition());
            mux->setConditionId(scope->getId());

            in = SignalReadPort(mux);
        }
        {
            auto* signal = DesignScope::createNode<hlim::Node_Signal>();
            signal->connectInput(in);
            signal->setName(m_node->getName());
            signal->recordStackTrace();
            in = SignalReadPort(signal);
        }

        m_node->connectInput(in);
    }

    SignalReadPort Bit::getRawDriver() const
    {
        hlim::NodePort driver = m_node->getDriver(0);
        if (!driver.node)
            driver = hlim::NodePort{ .node = m_node, .port = 0ull };
        return SignalReadPort(driver);
    }

    bool Bit::valid() const
    {
        return true;
    }
    

}
