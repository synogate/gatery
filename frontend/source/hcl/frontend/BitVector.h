#pragma once

#include "Bit.h"
#include "Signal.h"
#include "Scope.h"

#include <hcl/hlim/coreNodes/Node_Signal.h>
#include <hcl/hlim/coreNodes/Node_Rewire.h>

#include <hcl/utils/Exceptions.h>

#include <vector>
#include <algorithm>

namespace mhdl::core::frontend {

// concat x = a && b && c;
    
    
struct Selection {
    // some selection descriptor
    
    static Selection From(int start);
    static Selection Range(int start, int end);
    static Selection RangeIncl(int start, int endIncl);
    static Selection StridedRange(int start, int end, int stride);

    static Selection Slice(int offset, size_t size);
    static Selection StridedSlice(int offset, size_t size, int stride);
};

class ElementaryVector : public ElementarySignal
{
public:
    using ElementarySignal::ElementarySignal;

    virtual void resize(size_t width);

    Bit operator[](size_t idx) const;

    void setBit(size_t idx, const Bit& in);

    Bit lsb() const { return (*this)[0]; }
    Bit msb() const { return (*this)[getWidth()-1]; }
};

/**
 * @todo write docs
 */
template<typename FinalType>
class BaseBitVector : public ElementaryVector
{
    public:
        using isBitVectorSignal = void;
        
        BaseBitVector(const FinalType &rhs) { assign(rhs); }
        
        FinalType zext(size_t width) const;
        FinalType sext(size_t width) const { return bext(width, msb()); }
        FinalType bext(size_t width, const Bit& bit) const;

        FinalType operator()(int offset, size_t size) { return operator()(Selection::Range(offset, size)); }
        FinalType operator()(const Selection &selection) { }

        FinalType &operator=(const FinalType &rhs) { assign(rhs); return *this; }
    protected:
        BaseBitVector() = default;
        BaseBitVector(const hlim::NodePort &port) : ElementaryVector(port) { }
};


class BitVector : public BaseBitVector<BitVector>
{
    public:
        MHDL_SIGNAL
        
        using isUntypedBitvectorSignal = void;        

        BitVector() = default;
        BitVector(size_t width);
        BitVector(const hlim::NodePort &port);
    protected:
        virtual hlim::ConnectionType getSignalType(size_t width) const override;

};


template<typename FinalType>
inline FinalType BaseBitVector<FinalType>::zext(size_t width) const
{
    hlim::Node_Rewire* node = DesignScope::createNode<hlim::Node_Rewire>(1);
    node->recordStackTrace();

    node->connectInput(0, { .node = m_node, .port = 0 });

    hlim::Node_Rewire::RewireOperation rewireOp;
    if (width > 0 && getWidth() > 0)
    {
        rewireOp.ranges.push_back({
                .subwidth = std::min(width, getWidth()),
                .source = hlim::Node_Rewire::OutputRange::INPUT,
            });
    }

    if (width > getWidth())
    {
        rewireOp.ranges.push_back({
                .subwidth = width - getWidth(),
                .source = hlim::Node_Rewire::OutputRange::CONST_ZERO,
            });
    }

    node->setOp(std::move(rewireOp));
    return FinalType(hlim::NodePort{ .node = node });
}

template<typename FinalType>
inline FinalType BaseBitVector<FinalType>::bext(size_t width, const Bit& bit) const
{
    hlim::Node_Rewire* node = DesignScope::createNode<hlim::Node_Rewire>(2);
    node->recordStackTrace();

    node->connectInput(0, { .node = m_node, .port = 0 });
    node->connectInput(1, { .node = bit.getNode(), .port = 0 });

    hlim::Node_Rewire::RewireOperation rewireOp;
    if (width > 0 && getWidth() > 0)
    {
        rewireOp.ranges.push_back({
                .subwidth = std::min(width, getWidth()),
                .source = hlim::Node_Rewire::OutputRange::INPUT,
            });
    }

    if (width > getWidth())
    {
        rewireOp.ranges.resize(width - getWidth() + rewireOp.ranges.size(), {
                .subwidth = 1,
                .source = hlim::Node_Rewire::OutputRange::INPUT,
                .inputIdx = 1,
            });
    }

    node->setOp(std::move(rewireOp));
    return FinalType(hlim::NodePort{.node = node});
}

}
