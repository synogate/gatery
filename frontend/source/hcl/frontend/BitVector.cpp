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
        // TODO: set constant to undefined
        auto* constant = DesignScope::createNode<hlim::Node_Constant>(
            undefinedBVec(width.value), 
            hlim::ConnectionType::BITVEC
            );
        assign(SignalReadPort(constant, expansionPolicy));
    }

    const BVec BVec::operator*() const
    {
        if (!m_range.subset)
            return SignalReadPort(m_node, m_expansionPolicy);

        auto* rewire = DesignScope::createNode<hlim::Node_Rewire>(1);
        rewire->connectInput(0, { .node = m_node, .port = 0 });
        rewire->setOp(pickSelection(m_range));
        return SignalReadPort(rewire, m_expansionPolicy);
    }

    void BVec::resize(size_t width)
    {
        HCL_DESIGNCHECK_HINT(!m_range.subset, "BVec::resize is not allowed for alias BVec's. use zext instead.");
        HCL_DESIGNCHECK_HINT(m_node->getDirectlyDriven(0).empty(), "BVec::resize is allowed for unused signals (final)");
        HCL_DESIGNCHECK_HINT(width > getWidth(), "BVec::resize width decrease not allowed");
        HCL_DESIGNCHECK_HINT(width <= getWidth() || m_expansionPolicy != Expansion::none, "BVec::resize width increase only allowed when expansion policy is set");

        if (width == getWidth())
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
        SignalReadPort driver(m_node->getDriver(0), m_expansionPolicy);
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

    void BVec::setName(std::string name)
    {
        if (m_node->getDriver(0).node != nullptr)
            m_node->getDriver(0).node->setName(name);
        m_node->setName(move(name));
    }
    
    void BVec::addToSignalGroup(hlim::SignalGroup *signalGroup, unsigned index)
    {
        m_node->moveToSignalGroup(signalGroup, index);
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
        {
            m_range.width = width(in);
            m_expansionPolicy = in.expansionPolicy;

            m_node = DesignScope::createNode<hlim::Node_Signal>();
            m_node->setConnectionType(getConnType());
            m_node->recordStackTrace();
        }

        // TODO: handle implicit width expansion
        HCL_ASSERT(width(in) <= m_range.width);

        in = in.expand(m_range.width, hlim::ConnectionType::BITVEC);

        if (m_range.subset)
        {
            std::string in_name = in.node->getName();
            
            auto* rewire = DesignScope::createNode<hlim::Node_Rewire>(2);
            rewire->connectInput(0, m_node->getDriver(0));
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

        if (ConditionalScope::get())
        {
            auto* signal = DesignScope::createNode<hlim::Node_Signal>();
            signal->connectInput(m_node->getDriver(0));
            signal->setName(m_node->getName());
            signal->recordStackTrace();

            auto* mux = DesignScope::createNode<hlim::Node_Multiplexer>(2);
            mux->connectInput(0, {.node = signal, .port = 0});
            mux->connectInput(1, in); // assign rhs last in case previous port was undefined
            mux->connectSelector(ConditionalScope::getCurrentConditionPort());
            in.node = mux;
            in.port = 0;
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
            port = port.expand(bit.getWidth() + increment, hlim::ConnectionType::BITVEC);
        return BVec(port);
    }

    BVec ext(const Bit& bit, size_t increment, Expansion policy)
    {
        SignalReadPort port = bit.getReadPort();
        port.expansionPolicy = policy;
        if (increment)
            port = port.expand(bit.getWidth() + increment, hlim::ConnectionType::BITVEC);
        return BVec(port);
    }

    BVec ext(const BVec& bvec, size_t increment)
    {
        SignalReadPort port = bvec.getReadPort();
        if (increment)
            port = port.expand(bvec.getWidth() + increment, hlim::ConnectionType::BITVEC);
        return BVec(port);
    }

    BVec ext(const BVec& bvec, size_t increment, Expansion policy)
    {
        SignalReadPort port = bvec.getReadPort();
        port.expansionPolicy = policy;
        if (increment)
            port = port.expand(bvec.getWidth() + increment, hlim::ConnectionType::BITVEC);
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
