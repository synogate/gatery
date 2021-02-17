#include "BitVector.h"
#include "ConditionalScope.h"
#include "Constant.h"

#include <hcl/hlim/coreNodes/Node_Constant.h>
#include <hcl/hlim/coreNodes/Node_Rewire.h>
#include <hcl/hlim/coreNodes/Node_Multiplexer.h>

namespace hcl::core::frontend {

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

    Selection Selection::Slice(int offset, int size)
    {
        return {
            .start = offset,
            .width = size,
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

    Selection Selection::Symbol(int idx, size_t symbolWidth)
    {
        return {
            .start = idx * int(symbolWidth),
            .width = int(symbolWidth),
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



    BVec::BVec(BVec&& rhs) :
        ElementarySignal(rhs)
    {
        assign(rhs.getReadPort());
        rhs.assign(SignalReadPort(m_node, rhs.m_expansionPolicy));
    }

    BVec::BVec(hlim::Node_Signal* node, Range range, Expansion expansionPolicy) :
        m_node(node),
        m_range(range),
        m_expansionPolicy(expansionPolicy)
    {
        auto connType = node->getOutputConnectionType(0);
        HCL_DESIGNCHECK(connType.interpretation == hlim::ConnectionType::BITVEC);
        HCL_DESIGNCHECK(connType.width > m_range.bitOffset(m_range.width-1));
    }

    BVec::BVec(BitWidth width, Expansion expansionPolicy)
    {
        createNode(width.value, expansionPolicy);
    }

    BVec& BVec::operator=(BVec&& rhs)
    {
        assign(rhs.getReadPort());

        SignalReadPort outRange{ m_node, m_expansionPolicy };
        if (m_range.subset)
        {
            auto* rewire = DesignScope::createNode<hlim::Node_Rewire>(1);
            rewire->setName(std::string(getName()));
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
                rewire->setName(std::string(getName()));
                rewire->connectInput(0, m_readPort);
                rewire->setOp(pickSelection(m_range));
                m_readPort = SignalReadPort(rewire, m_expansionPolicy);
            }
        }
        return m_readPort;
    }

    void BVec::setName(std::string name)
    {
        m_name = std::move(name);

        if (m_node)
        {
            if (m_node->getDriver(0).node != nullptr)
                m_node->getDriver(0).node->setName(m_name);
            m_node->setName(m_name);
        }
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

    void BVec::assign(SignalReadPort in)
    {
        if (!m_node)
            createNode(width(in), in.expansionPolicy);

        if (getName().empty())
            setName(in.node->getName());

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
            HCL_ASSERT_HINT(m_node->getDriver(0).node, "latch or complete shadowing for loop not yet implemented");

            SignalReadPort oldSignal = getRawDriver();

            { // place optional signal node for graph debugging
                auto* signal = DesignScope::createNode<hlim::Node_Signal>();
                signal->connectInput(oldSignal);
                signal->setName(m_node->getName());
                signal->recordStackTrace();
                oldSignal = SignalReadPort{ signal };
            }

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

        {
            auto* signal = DesignScope::createNode<hlim::Node_Signal>();
            signal->connectInput(in);
            signal->setName(m_node->getName());
            signal->recordStackTrace();
            in = SignalReadPort(signal);
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

        if (!m_name.empty())
            m_node->setName(m_name);
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
                m_bitAlias.emplace_back(m_node, m_range.bitOffset(i));
        }
        return m_bitAlias;
    }

    Bit& BVec::aliasMsb() const
    {
        if (!m_msbAlias)
        {
            if (!m_range.subset)
                m_msbAlias.emplace(m_node, ~0u);
            else
                m_msbAlias.emplace(m_node, m_range.bitOffset(m_range.width - 1));
        }
        return *m_msbAlias;
    }

    Bit& BVec::aliasLsb() const
    {
        if (!m_lsbAlias)
            m_lsbAlias.emplace(m_node, m_range.bitOffset(0));
        return *m_lsbAlias;
    }

    BVec& BVec::aliasRange(const Range& range) const
    {
        auto [it, exists] = m_rangeAlias.try_emplace(range, m_node, range, m_expansionPolicy);
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

        if (s.width >= 0)
            width = size_t(s.width);
        else
        {
            HCL_DESIGNCHECK(s.stride <= 1); // not yet defined
            width = size_t(s.width + r.width);
        }

        stride = s.stride * r.stride;
        
        if(r.stride > 0)
            offset *= r.stride;
        offset += r.offset;

        subset = true;
        HCL_DESIGNCHECK(bitOffset(width - 1) <= r.bitOffset(r.width - 1));
    }

}
