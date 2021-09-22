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
#include "BitVector.h"
#include "ConditionalScope.h"
#include "Constant.h"

#include <gatery/hlim/coreNodes/Node_Constant.h>
#include <gatery/hlim/coreNodes/Node_Rewire.h>
#include <gatery/hlim/coreNodes/Node_Multiplexer.h>
#include <gatery/hlim/supportNodes/Node_ExportOverride.h>
#include <gatery/hlim/supportNodes/Node_Attributes.h>
#include <gatery/hlim/supportNodes/Node_Default.h>

namespace gtry {

    Selection Selection::All()
    {
        return { .untilEndOfSource = true };
    }

    Selection Selection::From(int start)
    {
        return {
            .start = start,
            .width = 0,
            .stride = 1,
            .untilEndOfSource = true,
        };
    }

    Selection Selection::Range(int start, int end)
    {
        return {
            .start = start,
            .width = end - start,
            .stride = 1,
            .untilEndOfSource = false,
        };
    }

    Selection Selection::RangeIncl(int start, int endIncl)
    {
        return {
            .start = start,
            .width = endIncl - start + 1,
            .stride = 1,
            .untilEndOfSource = false,
        };
    }

    Selection Selection::StridedRange(int start, int end, size_t stride)
    {
        return {
            .start = start,
            .width = (end - start) / int(stride),
            .stride = stride,
            .untilEndOfSource = false,
        };
    }

    Selection Selection::Slice(size_t offset, size_t size)
    {
        return {
            .start = (int)offset,
            .width = (int)size,
            .stride = 1,
            .untilEndOfSource = false,
        };
    }

    Selection Selection::StridedSlice(int offset, int size, size_t stride)
    {
        return {
            .start = offset,
            .width = size,
            .stride = stride,
            .untilEndOfSource = false,
        };
    }

    Selection Selection::Symbol(int idx, BitWidth symbolWidth)
    {
        return {
            .start = idx * int(symbolWidth.value),
            .width = int(symbolWidth.value),
            .stride = 1,
            .untilEndOfSource = false
        };
    }

    static hlim::Node_Rewire::RewireOperation pickSelection(const BVec::Range& range)
    {
        hlim::Node_Rewire::RewireOperation op;
        if (range.stride == 1)
        {
            op.addInput(0, range.offset, range.width);
        }
        else
        {
            for (size_t i = 0; i < range.width; ++i)
                op.addInput(0, range.bitOffset(i), 1);
        }
        return op;
    }

    static hlim::Node_Rewire::RewireOperation replaceSelection(const BVec::Range& range, size_t width)
    {
        HCL_ASSERT(range.bitOffset(range.width - 1) < width);

        hlim::Node_Rewire::RewireOperation op;
        if (range.stride == 1)
        {
            op.addInput(0, 0, range.offset);
            op.addInput(1, 0, range.width);
            op.addInput(0, range.offset + range.width, width - (range.offset + range.width));
        }
        else
        {
            size_t offset0 = 0;
            size_t offset1 = 0;

            for (size_t i = 0; i < range.width; ++i)
            {
                op.addInput(0, offset0, range.bitOffset(i));
                op.addInput(1, offset1++, 1);
                offset0 = range.bitOffset(i) + 1;
            }
            op.addInput(0, offset0, width - offset0);
        }

        return op;
    }

    BVecDefault::BVecDefault(const BVec& rhs) {
        m_nodePort = rhs.getReadPort();
    }

    void BVecDefault::assign(std::string_view value) {
        auto* constant = DesignScope::createNode<hlim::Node_Constant>(
            parseBVec(value),
            hlim::ConnectionType::BITVEC
        );

        m_nodePort = {.node = constant, .port = 0ull };
    }


    BVec::BVec(BVec&& rhs) :
        ElementarySignal()
    {
        if (rhs.m_node)
        {
            assign(rhs.getReadPort());
            rhs.assign(SignalReadPort(m_node, rhs.m_expansionPolicy));
        }
    }

    BVec::~BVec()
    {
        if (m_node) m_node->removeRef();
    }

    BVec::BVec(hlim::Node_Signal* node, Range range, Expansion expansionPolicy, size_t initialScopeId) :
        m_node(node),
        m_range(range),
        m_expansionPolicy(expansionPolicy)
    {
        auto connType = node->getOutputConnectionType(0);
        HCL_DESIGNCHECK(connType.interpretation == hlim::ConnectionType::BITVEC);
        HCL_DESIGNCHECK(m_range.width == 0 || connType.width > m_range.bitOffset(m_range.width-1));
        m_node->addRef();

        m_initialScopeId = initialScopeId;
    }

    BVec::BVec(BitWidth width, Expansion expansionPolicy)
    {
        createNode(width.value, expansionPolicy);
    }

    BVec::BVec(const BVecDefault &defaultValue)
    {
        (*this) = defaultValue;
    }

    BVec& BVec::operator=(BVec&& rhs)
    {
        assign(rhs.getReadPort());

        SignalReadPort outRange{ m_node, m_expansionPolicy };
        if (m_range.subset)
        {
            auto* rewire = DesignScope::createNode<hlim::Node_Rewire>(1);
            rewire->connectInput(0, outRange);
            rewire->setOp(pickSelection(m_range));
            outRange = SignalReadPort(rewire, m_expansionPolicy);
        }

        rhs.assign(outRange);
        return *this;
    }

    BVec& BVec::operator=(BitWidth width)
    {
        HCL_DESIGNCHECK(!m_node);
        createNode(width.value, m_expansionPolicy);
        return *this;
    }

    BVec& BVec::operator=(const BVecDefault &defaultValue)
    {
        hlim::Node_Default* node = DesignScope::createNode<hlim::Node_Default>();
        node->recordStackTrace();
        node->connectInput(getReadPort());
        node->connectDefault(defaultValue.getNodePort());

        assign(SignalReadPort(node));
        return *this;
    }

	void BVec::setExportOverride(const BVec& exportOverride)
	{
		auto* expOverride = DesignScope::createNode<hlim::Node_ExportOverride>();
		expOverride->connectInput(getReadPort());
		expOverride->connectOverride(exportOverride.getReadPort());
		assign(SignalReadPort(expOverride));
	}

    void BVec::setAttrib(hlim::SignalAttributes attributes)
    {
        auto* node = DesignScope::createNode<hlim::Node_Attributes>();
        node->getAttribs() = std::move(attributes);
        node->connectInput(getReadPort());
    }


    BVec& BVec::operator()(size_t offset, BitWidth size, size_t stride)
    {
        return (*this)(Selection::StridedSlice(int(offset), int(size.value), stride));
    }

    const BVec& BVec::operator()(size_t offset, BitWidth size, size_t stride) const
    {
        return (*this)(Selection::StridedSlice(int(offset), int(size.value), stride));
    }

    void BVec::resize(size_t width)
    {
        HCL_DESIGNCHECK_HINT(!m_range.subset, "BVec::resize is not allowed for alias BVec's. use zext instead.");
        HCL_DESIGNCHECK_HINT(m_node->getDirectlyDriven(0).empty(), "BVec::resize is allowed for unused signals (final)");
        HCL_DESIGNCHECK_HINT(width > size(), "BVec::resize width decrease not allowed");
        HCL_DESIGNCHECK_HINT(width <= size() || m_expansionPolicy != Expansion::none, "BVec::resize width increase only allowed when expansion policy is set");

        if (width == size())
            return;

        auto* rewire = DesignScope::createNode<hlim::Node_Rewire>(1);
        rewire->connectInput(0, getReadPort());

        switch (m_expansionPolicy)
        {
        case Expansion::sign: rewire->setPadTo(width); break;
        case Expansion::zero: rewire->setPadTo(width, hlim::Node_Rewire::OutputRange::CONST_ZERO); break;
        case Expansion::one: rewire->setPadTo(width, hlim::Node_Rewire::OutputRange::CONST_ONE); break;
        default: break;
        }

        m_node->connectInput({ .node = rewire, .port = 0 }); // unconditional (largest of all paths wins)
        m_range.width = width;
        m_bitAlias.clear();
    }

    hlim::ConnectionType BVec::getConnType() const
    {
        return hlim::ConnectionType{ .interpretation = hlim::ConnectionType::BITVEC, .width = m_range.width };
    }

    SignalReadPort BVec::getReadPort() const
    {
        SignalReadPort driver = getRawDriver();
        if (m_readPortDriver != driver.node)
        {
            m_readPort = driver;
            m_readPortDriver = driver.node;

            if (m_range.subset)
            {
                auto* rewire = DesignScope::createNode<hlim::Node_Rewire>(1);
                rewire->connectInput(0, m_readPort);
                rewire->setOp(pickSelection(m_range));
                m_readPort = SignalReadPort(rewire, m_expansionPolicy);
            }
        }
        return m_readPort;
    }

    SignalReadPort BVec::getOutPort() const
    {
        SignalReadPort port{ m_node, m_expansionPolicy };

        if (m_range.subset)
        {
            auto* rewire = DesignScope::createNode<hlim::Node_Rewire>(1);
            rewire->connectInput(0, m_readPort);
            rewire->setOp(pickSelection(m_range));
            port = SignalReadPort(rewire, m_expansionPolicy);
        }
        return port;
    }

    std::string_view BVec::getName() const
    {
        if (auto *sigNode = dynamic_cast<hlim::Node_Signal*>(m_node->getDriver(0).node))
            return sigNode->getName();
        return {};
    }

    void BVec::setName(std::string name)
    {
        HCL_DESIGNCHECK_HINT(m_node != nullptr, "Can only set names to initialized BVecs!");

        auto* signal = DesignScope::createNode<hlim::Node_Signal>();
        signal->connectInput(getReadPort());
        signal->setName(name);
        signal->recordStackTrace();

        assign(SignalReadPort(signal), true);
    }

    void BVec::addToSignalGroup(hlim::SignalGroup *signalGroup)
    {
        m_node->moveToSignalGroup(signalGroup);
    }

    void BVec::assign(std::string_view value)
    {
        auto* constant = DesignScope::createNode<hlim::Node_Constant>(
            parseBVec(value),
            hlim::ConnectionType::BITVEC
        );

        // TODO: set policy from string content
        assign(SignalReadPort(constant, Expansion::none));
    }

    void BVec::assign(SignalReadPort in, bool ignoreConditions)
    {
        if (!m_node)
            createNode(width(in), in.expansionPolicy);

        const bool incrementWidth = width(in) > m_range.width;
        if(!incrementWidth)
            in = in.expand(m_range.width, hlim::ConnectionType::BITVEC);

        if (m_range.subset)
        {
            HCL_ASSERT(!incrementWidth);
            std::string in_name = in.node->getName();

            auto* rewire = DesignScope::createNode<hlim::Node_Rewire>(2);
            rewire->connectInput(0, getRawDriver());
            rewire->connectInput(1, in);
            rewire->setOp(replaceSelection(m_range, m_node->getOutputConnectionType(0).width));
            in.node = rewire;
            in.port = 0;
        }

        if (auto* scope = ConditionalScope::get(); !ignoreConditions && scope && scope->getId() > m_initialScopeId)
        {
            SignalReadPort oldSignal = getRawDriver();


            if (incrementWidth)
            {
                HCL_ASSERT(m_expansionPolicy != Expansion::none);
                HCL_ASSERT(!m_range.subset);

                auto* rewire = DesignScope::createNode<hlim::Node_Rewire>(1);
                rewire->connectInput(0, oldSignal);

                switch (m_expansionPolicy)
                {
                case Expansion::zero:   rewire->setPadTo(width(in), hlim::Node_Rewire::OutputRange::CONST_ZERO);    break;
                case Expansion::one:    rewire->setPadTo(width(in), hlim::Node_Rewire::OutputRange::CONST_ONE);    break;
                case Expansion::sign:   rewire->setPadTo(width(in)); break;
                default: break;
                }

                oldSignal = SignalReadPort{ rewire };
            }

            auto* mux = DesignScope::createNode<hlim::Node_Multiplexer>(2);
            mux->connectInput(0, oldSignal);
            mux->connectInput(1, in); // assign rhs last in case previous port was undefined
            mux->connectSelector(scope->getFullCondition());
            mux->setConditionId(scope->getId());
            in = SignalReadPort{ mux };
        }

        if (!m_node->getDirectlyDriven(0).empty() && incrementWidth)
        {
            const auto nodeInputs = m_node->getDirectlyDriven(0);

            auto* rewire = DesignScope::createNode<hlim::Node_Rewire>(1);
            rewire->connectInput(0, SignalReadPort{ m_node });
            rewire->setExtract(0, m_range.width);

            for (const hlim::NodePort& port : nodeInputs)
                port.node->rewireInput(port.port, SignalReadPort{ rewire });

            HCL_ASSERT(!m_range.subset);
            m_range.width = width(in);
        }

        m_node->connectInput(in);
    }

    void BVec::createNode(size_t width, Expansion policy)
    {
        HCL_ASSERT(!m_node);

        m_range.width = width;
        m_expansionPolicy = policy;

        m_node = DesignScope::createNode<hlim::Node_Signal>();
        m_node->setConnectionType(getConnType());
        m_node->recordStackTrace();
        m_node->addRef();
    }

    SignalReadPort BVec::getRawDriver() const
    {
        hlim::NodePort driver = m_node->getDriver(0);
        if (!driver.node)
            driver = hlim::NodePort{ .node = m_node, .port = 0ull };
        return SignalReadPort(driver, m_expansionPolicy);
    }

    std::vector<Bit>& BVec::aliasVec() const
    {
        if (m_bitAlias.size() != m_range.width)
        {
            m_bitAlias.reserve(m_range.width);
            for (size_t i = 0; i < m_range.width; ++i)
                m_bitAlias.emplace_back(m_node, m_range.bitOffset(i), m_initialScopeId);
        }
        return m_bitAlias;
    }

    Bit& BVec::aliasMsb() const
    {
        if (!m_msbAlias)
        {
            if (!m_range.subset)
                m_msbAlias.emplace(m_node, ~0u, m_initialScopeId);
            else
                m_msbAlias.emplace(m_node, m_range.bitOffset(m_range.width - 1), m_initialScopeId);
        }
        return *m_msbAlias;
    }

    Bit& BVec::aliasLsb() const
    {
        if (!m_lsbAlias)
            m_lsbAlias.emplace(m_node, m_range.bitOffset(0), m_initialScopeId);
        return *m_lsbAlias;
    }

    BVec& BVec::aliasRange(const Range& range) const
    {
        auto [it, exists] = m_rangeAlias.try_emplace(range, m_node, range, m_expansionPolicy, m_initialScopeId);
        return it->second;
    }

    BVec ext(const Bit& bit, size_t increment)
    {
        SignalReadPort port = bit.getReadPort();
        if (increment)
            port = port.expand(1 + increment, hlim::ConnectionType::BITVEC);
        return BVec(port);
    }

    BVec ext(const Bit& bit, size_t increment, Expansion policy)
    {
        SignalReadPort port = bit.getReadPort();
        port.expansionPolicy = policy;
        if (increment)
            port = port.expand(1 + increment, hlim::ConnectionType::BITVEC);
        return BVec(port);
    }

    BVec ext(const BVec& bvec, size_t increment)
    {
        SignalReadPort port = bvec.getReadPort();
        if (increment)
            port = port.expand(bvec.size() + increment, hlim::ConnectionType::BITVEC);
        return BVec(port);
    }

    BVec ext(const BVec& bvec, size_t increment, Expansion policy)
    {
        SignalReadPort port = bvec.getReadPort();
        port.expansionPolicy = policy;
        if (increment)
            port = port.expand(bvec.size() + increment, hlim::ConnectionType::BITVEC);
        return BVec(port);
    }

    BVec::Range::Range(const Selection& s, const Range& r)
    {
        if (s.start >= 0)
            offset = (size_t)s.start;
        else
        {
            offset = size_t(s.start + r.width);
        }

        if (s.untilEndOfSource) {
            HCL_DESIGNCHECK(s.stride <= 1); // not yet defined
            width = r.width - offset;
        } else if (s.width >= 0)
            width = size_t(s.width);
        else
        {
            HCL_DESIGNCHECK(s.stride <= 1); // not yet defined
            HCL_ASSERT_HINT(offset == 0, "TODO: Check this!");
            ///@todo Not sure if this is correct, wouldn't one have to subtract offset?
            width = size_t(s.width + r.width);
        }

        stride = s.stride * r.stride;

        if(r.stride > 0)
            offset *= r.stride;
        offset += r.offset;

        subset = true;

        HCL_DESIGNCHECK(width == 0 || bitOffset(width - 1) <= r.bitOffset(r.width - 1));
    }

}
