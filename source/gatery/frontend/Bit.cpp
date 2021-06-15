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
#include "gatery/pch.h"

#include "Bit.h"
#include "BitVector.h"
#include "ConditionalScope.h"
#include "Constant.h"
#include "Scope.h"

#include <gatery/hlim/coreNodes/Node_Constant.h>
#include <gatery/hlim/coreNodes/Node_Rewire.h>
#include <gatery/hlim/coreNodes/Node_Multiplexer.h>
#include <gatery/hlim/supportNodes/Node_Default.h>
#include <gatery/hlim/supportNodes/Node_ExportOverride.h>
#include <gatery/hlim/supportNodes/Node_Attributes.h>


namespace gtry {

    BitDefault::BitDefault(const Bit& rhs)
    {
        m_nodePort = rhs.getReadPort();
    }

    void BitDefault::assign(bool value)
    {
        auto* constant = DesignScope::createNode<hlim::Node_Constant>(
            parseBit(value), hlim::ConnectionType::BOOL
            );
        m_nodePort = {.node = constant, .port = 0ull };
    }

    void BitDefault::assign(char value)
    {
        auto* constant = DesignScope::createNode<hlim::Node_Constant>(
            parseBit(value), hlim::ConnectionType::BOOL
            );
        m_nodePort = {.node = constant, .port = 0ull };
    }



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

    Bit::Bit(const BitDefault &defaultValue) : Bit()
    {
        (*this) = defaultValue;
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

    Bit::Bit(hlim::Node_Signal* node, size_t offset, size_t initialScopeId) :
        m_node(node),
        m_offset(offset)
    {
        m_node->addRef();
        m_initialScopeId = initialScopeId;
    }

    Bit& Bit::operator=(Bit&& rhs)
    {
        assign(rhs.getReadPort());

        SignalReadPort outRange{ m_node };
        hlim::ConnectionType type = m_node->getOutputConnectionType(0);
        if (type.interpretation != hlim::ConnectionType::BOOL)
        {
            auto* rewire = DesignScope::createNode<hlim::Node_Rewire>(1);
            rewire->connectInput(0, outRange);
            rewire->changeOutputType(getConnType());

            size_t offset = std::min(m_offset, type.width - 1); // used for msb alias, but can alias any future offset
            rewire->setExtract(offset, 1);

            outRange = SignalReadPort(rewire);
        }
        rhs.assign(outRange);
        return *this;
    }

    Bit& Bit::operator=(const BitDefault &defaultValue)
    {
        hlim::Node_Default* node = DesignScope::createNode<hlim::Node_Default>();
        node->recordStackTrace();
        node->connectInput(getReadPort());
        node->connectDefault(defaultValue.getNodePort());

        assign(SignalReadPort(node));
        return *this;
    }

    void Bit::setExportOverride(const Bit& exportOverride)
    {
        auto* expOverride = DesignScope::createNode<hlim::Node_ExportOverride>();
        expOverride->connectInput(getReadPort());
        expOverride->connectOverride(exportOverride.getReadPort());
        assign(SignalReadPort(expOverride));
    }

    void Bit::setAttrib(hlim::SignalAttributes attributes)
    {
        auto* node = DesignScope::createNode<hlim::Node_Attributes>();
        node->getAttribs() = std::move(attributes);
        node->connectInput(getReadPort());
        assign(SignalReadPort(node));
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
        return rewireAlias(ret);
    }

    SignalReadPort Bit::getOutPort() const
    {
        SignalReadPort ret{ m_node };
        return rewireAlias(ret);
    }

    SignalReadPort gtry::Bit::rewireAlias(SignalReadPort port) const
    {
        hlim::ConnectionType type = hlim::getOutputConnectionType(port);
        if (type.interpretation != hlim::ConnectionType::BOOL)
        {
            // TODO: cache rewire node if m_node's input is unchanged
            auto* rewire = DesignScope::createNode<hlim::Node_Rewire>(1);
            rewire->connectInput(0, port);
            rewire->changeOutputType(getConnType());

            size_t offset = std::min(m_offset, type.width - 1); // used for msb alias, but can alias any future offset
            rewire->setExtract(offset, 1);

            port = SignalReadPort(rewire);
        }
        return port;
    }

    std::string_view Bit::getName() const
    {
        if (auto *sigNode = dynamic_cast<hlim::Node_Signal*>(m_node->getDriver(0).node))
            return sigNode->getName();
        return {};
    }

    void Bit::setName(std::string name)
    {
        auto* signal = DesignScope::createNode<hlim::Node_Signal>();
        signal->connectInput(getReadPort());
        signal->setName(name);
        signal->recordStackTrace();

        assign(SignalReadPort(signal));
    }

    void Bit::addToSignalGroup(hlim::SignalGroup *signalGroup)
    {
        m_node->moveToSignalGroup(signalGroup);
    }

    void Bit::setResetValue(bool v)
    {
        m_resetValue = v;
    }

    void Bit::setResetValue(char v)
    {
        HCL_ASSERT(v == '1' || v == '0'); setResetValue(v == '1');
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
/*
            {
                auto* signal = DesignScope::createNode<hlim::Node_Signal>();
                signal->connectInput(in);
                signal->recordStackTrace();
                in = SignalReadPort(signal);
            }
*/
        }

        if (auto* scope = ConditionalScope::get(); scope && scope->getId() > m_initialScopeId)
        {
            auto* signal_in = DesignScope::createNode<hlim::Node_Signal>();
            signal_in->connectInput(getRawDriver());

            auto* mux = DesignScope::createNode<hlim::Node_Multiplexer>(2);
            mux->connectInput(0, {.node = signal_in, .port = 0});
            mux->connectInput(1, in); // assign rhs last in case previous port was undefined
            mux->connectSelector(scope->getFullCondition());
            mux->setConditionId(scope->getId());

            in = SignalReadPort(mux);
        }
/*
        if (dynamic_cast<hlim::Node_Signal*>(in.node) == nullptr) {
            auto* signal = DesignScope::createNode<hlim::Node_Signal>();
            signal->connectInput(in);
            signal->recordStackTrace();
            in = SignalReadPort(signal);
        }
*/
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
