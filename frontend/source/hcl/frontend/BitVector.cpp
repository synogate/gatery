#include "BitVector.h"
#include "ConditionalScope.h"

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
            .end = 0,
            .stride = 1,
            .untilEndOfSource = true,
        };
    }

    Selection Selection::Range(int start, int end)
    {
        return {
            .start = start,
            .end = end,
            .stride = 1,
            .untilEndOfSource = false,
        };
    }

    Selection Selection::RangeIncl(int start, int endIncl)
    {
        return {
            .start = start,
            .end = endIncl + 1,
            .stride = 1,
            .untilEndOfSource = false,
        };
    }

    Selection Selection::StridedRange(int start, int end, int stride)
    {
        return {
            .start = start,
            .end = end,
            .stride = stride,
            .untilEndOfSource = false,
        };
    }

    Selection Selection::Slice(int offset, size_t size)
    {
        return {
            .start = offset,
            .end = offset + int(size),
            .stride = 1,
            .untilEndOfSource = false,
        };
    }

    Selection Selection::StridedSlice(int offset, size_t size, int stride)
    {
        return {
            .start = offset,
            .end = offset + int(size),
            .stride = stride,
            .untilEndOfSource = false,
        };
    }

    static hlim::Node_Rewire::RewireOperation pickSelection(const Selection& range)
    {
        HCL_DESIGNCHECK(!range.untilEndOfSource);
        HCL_ASSERT(range.start >= 0);

        hlim::Node_Rewire::RewireOperation op;
        if (range.stride == 1)
        {
            op.addInput(0, range.start, range.end - range.start);
        }
        else
        {
            HCL_ASSERT(range.stride > 1);

            for (size_t i = (size_t)range.start; i < range.end; i += range.stride)
                op.addInput(0, i, 1);
        }

        return op;
    }

    static hlim::Node_Rewire::RewireOperation replaceSelection(const Selection& range, size_t width)
    {
        HCL_DESIGNCHECK(!range.untilEndOfSource);
        HCL_DESIGNCHECK(range.end <= width);
        HCL_ASSERT(range.start >= 0);

        hlim::Node_Rewire::RewireOperation op;
        if (range.stride == 1)
        {
            op.addInput(0, 0, range.start);
            op.addInput(1, 0, range.end - range.start);
            op.addInput(0, range.end, width - range.end);
        }
        else
        {
            size_t offset0 = 0;
            size_t offset1 = 0;

            for (size_t i = (size_t)range.start; i < range.end; i += range.stride)
            {
                op.addInput(0, offset0, i);
                op.addInput(1, offset1++, 1);
                offset0 = i + 1;
            }
            op.addInput(0, offset0, width - offset0);
        }

        return op;
    }


    BVec::BVec(hlim::Node_Signal* node, Selection range, Expansion expansionPolicy) :
        m_node(node),
        m_selection(range),
        m_expansionPolicy(expansionPolicy)
    {
        auto connType = node->getOutputConnectionType(0);
        HCL_DESIGNCHECK(!range.untilEndOfSource); // no impl
        HCL_DESIGNCHECK(range.end <= connType.width);
        HCL_DESIGNCHECK(connType.interpretation == hlim::ConnectionType::BITVEC);

        m_width = (m_selection.end - m_selection.start) / m_selection.stride;
    }

    BVec::BVec(size_t width, Expansion expansionPolicy)
    {
        // TODO: set constant to undefined
        auto* constant = DesignScope::createNode<hlim::Node_Constant>(
            hlim::ConstantData(0, width), 
            hlim::ConnectionType{.interpretation = hlim::ConnectionType::BITVEC, .width = width}
            );
        assign(SignalReadPort(constant, expansionPolicy));
    }

    BVec::BVec(std::string_view rhs)
    {
        assign(rhs);
    }

    const BVec BVec::operator*() const
    {
        if (m_selection == Selection::All())
            return SignalReadPort(m_node, m_expansionPolicy);

        auto* rewire = DesignScope::createNode<hlim::Node_Rewire>(1);
        rewire->connectInput(0, { .node = m_node, .port = 0 });
        rewire->setOp(pickSelection(m_selection));
        return SignalReadPort(rewire, m_expansionPolicy);
    }

    void BVec::resize(size_t width)
    {
        HCL_DESIGNCHECK_HINT(m_selection == Selection::All(), "BVec::resize is not allowed for alias BVec's. use zext instead.");
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
        m_width = width;
        m_bitAlias.clear();
    }

    hlim::ConnectionType BVec::getConnType() const
    {
        return hlim::ConnectionType{ .interpretation = hlim::ConnectionType::BITVEC, .width = m_width };
    }

    SignalReadPort BVec::getReadPort() const
    {
        SignalReadPort driver(m_node->getDriver(0), m_expansionPolicy);
        if (m_readPortDriver != driver.node)
        {
            m_readPort = driver;
            m_readPortDriver = driver.node;

            if (m_selection != Selection::All())
            {
                auto* rewire = DesignScope::createNode<hlim::Node_Rewire>(1);
                rewire->connectInput(0, m_readPort);
                rewire->setOp(pickSelection(m_selection));
                m_readPort = SignalReadPort(rewire, m_expansionPolicy);
            }
        }
        return m_readPort;
    }

    void BVec::setName(std::string name)
    {
        m_node->setName(move(name));
    }

    void BVec::assign(std::string_view value)
    {
        auto data = hlim::ConstantData(value);
        auto* constant = DesignScope::createNode<hlim::Node_Constant>(
            data,
            hlim::ConnectionType{ .interpretation = hlim::ConnectionType::BITVEC, .width = data.bitVec.size() }
        );

        // TODO: set policy from string content
        assign(SignalReadPort(constant, Expansion::none));
    }

    void BVec::assign(SignalReadPort in)
    {
        HCL_ASSERT(connType(in).interpretation == hlim::ConnectionType::BITVEC);

        if (!m_node)
        {
            m_width = width(in);
            m_expansionPolicy = in.expansionPolicy;

            m_node = DesignScope::createNode<hlim::Node_Signal>();
            m_node->setConnectionType(getConnType());
            m_node->recordStackTrace();
        }

        // TODO: handle implicit width expansion
        HCL_ASSERT(width(in) <= m_width);

        in = in.expand(m_width);

        if (m_selection != Selection::All())
        {
            auto* rewire = DesignScope::createNode<hlim::Node_Rewire>(2);
            rewire->connectInput(0, m_node->getDriver(0));
            rewire->connectInput(1, in);
            rewire->setOp(replaceSelection(m_selection, m_node->getOutputConnectionType(0).width));
            in.node = rewire;
            in.port = 0;
        }

        if (ConditionalScope::get())
        {
            auto* mux = DesignScope::createNode<hlim::Node_Multiplexer>(2);
            mux->connectInput(0, m_node->getDriver(0));
            mux->connectInput(1, in); // assign rhs last in case previous port was undefined
            mux->connectSelector(ConditionalScope::getCurrentConditionPort());
            in.node = mux;
            in.port = 0;
        }

        m_node->connectInput(in);
    }

    std::vector<Bit>& BVec::aliasVec() const
    {
        if (m_bitAlias.size() != getWidth())
        {
            m_bitAlias.reserve(getWidth());
            for (size_t i = 0; i < getWidth(); ++i)
                m_bitAlias.emplace_back(m_node, i);
        }
        return m_bitAlias;
    }

    Bit& BVec::aliasMsb() const
    {
        if (!m_msbAlias)
            m_msbAlias.emplace(m_node, ~0u);
        return *m_msbAlias;
    }

    Bit& BVec::aliasLsb() const
    {
        if (!m_lsbAlias)
            m_lsbAlias.emplace(m_node, 0);
        return *m_lsbAlias;
    }

    BVec& BVec::aliasRange(const Selection& range) const
    {
        auto [it, exists] = m_rangeAlias.try_emplace(range, m_node, range, m_expansionPolicy);
        return it->second;
    }

    BVec ext(const Bit& bit, size_t increment)
    {
        SignalReadPort port = bit.getReadPort();
        return BVec(port.expand(bit.getWidth() + increment));
    }

    BVec ext(const Bit& bit, size_t increment, Expansion policy)
    {
        SignalReadPort port = bit.getReadPort();
        port.expansionPolicy = policy;
        return BVec(port.expand(bit.getWidth() + increment));
    }

    BVec ext(const BVec& bvec, size_t increment)
    {
        SignalReadPort port = bvec.getReadPort();
        return BVec(port.expand(bvec.getWidth() + increment));
    }

    BVec ext(const BVec& bvec, size_t increment, Expansion policy)
    {
        SignalReadPort port = bvec.getReadPort();
        port.expansionPolicy = policy;
        return BVec(port.expand(bvec.getWidth() + increment));
    }

}
