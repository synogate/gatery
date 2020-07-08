#pragma once

#include "Bit.h"
#include "Signal.h"
#include "Scope.h"

#include <hcl/hlim/coreNodes/Node_Signal.h>
#include <hcl/hlim/coreNodes/Node_Rewire.h>

#include <hcl/utils/Exceptions.h>

#include <vector>
#include <algorithm>


namespace hcl::core::frontend {

// concat x = a && b && c;
    
    
struct Selection {
    int start = 0;
    int end = 0;
    int stride = 1;
    bool untilEndOfSource = false;    
    
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


template<typename FinalType>
class BaseBitVector;

template<typename SignalType>
class BitVectorSlice
{
    public:
        ~BitVectorSlice();
        
        BitVectorSlice<SignalType> &operator=(const ElementarySignal &signal);
        operator SignalType() const;
    protected:
        BitVectorSlice(const BitVectorSlice<SignalType> &) = delete;
        BitVectorSlice<SignalType> &operator=(const BitVectorSlice<SignalType> &) = delete;
        BitVectorSlice(BaseBitVector<SignalType> *signal, const Selection &selection);
        
        
        BaseBitVector<SignalType> *m_signal;
        Selection m_selection;
        
        hlim::NodePort m_lastSignalNodePort;
        
        friend class BaseBitVector<SignalType>;
        
        void unregisterSignal();
};

/**
 * @todo write docs
 */
template<typename FinalType>
class BaseBitVector : public ElementaryVector
{
    public:
        using isBitVectorSignal = void;
        
        BaseBitVector(const BaseBitVector<FinalType> &rhs) { assign(rhs); }
        ~BaseBitVector() { for (auto slice : m_slices) slice->unregisterSignal(); }
        
        FinalType zext(size_t width) const;
        FinalType sext(size_t width) const { return bext(width, msb()); }
        FinalType bext(size_t width, const Bit& bit) const;

        BitVectorSlice<FinalType> operator()(int offset, size_t size) { return BitVectorSlice<FinalType>(this, Selection::Slice(offset, size)); }
        BitVectorSlice<FinalType> operator()(const Selection &selection) { return BitVectorSlice<FinalType>(this, selection); }

        BaseBitVector<FinalType> &operator=(const BaseBitVector<FinalType> &rhs) { assign(rhs); return *this; }
    protected:
        BaseBitVector() = default;
        BaseBitVector(const hlim::NodePort &port) : ElementaryVector(port) { }
        
        std::set<BitVectorSlice<FinalType>*> m_slices;
        friend class BitVectorSlice<FinalType>;
        void unregisterSlice(BitVectorSlice<FinalType> *slice) { m_slices.erase(slice); }
};


class BitVector : public BaseBitVector<BitVector>
{
    public:
        HCL_SIGNAL
        
        using isUntypedBitvectorSignal = void;        

        BitVector() = default;
        BitVector(size_t width);
        BitVector(const hlim::NodePort &port);
    protected:
        virtual hlim::ConnectionType getSignalType(size_t width) const override;

};



template<typename SignalType>
BitVectorSlice<SignalType>::BitVectorSlice(BaseBitVector<SignalType> *signal, const Selection &selection) : m_signal(signal), m_selection(selection)
{
}

template<typename SignalType>
BitVectorSlice<SignalType>::~BitVectorSlice() 
{ 
    if (m_signal != nullptr) 
        m_signal->unregisterSlice(this); 
}

template<typename SignalType>
BitVectorSlice<SignalType> &BitVectorSlice<SignalType>::operator=(const ElementarySignal &signal)
{
    HCL_ASSERT(m_signal != nullptr);
    
    
    size_t inputWidth = m_signal->getWidth();
    
    hlim::Node_Rewire* node = DesignScope::createNode<hlim::Node_Rewire>(2);
    node->recordStackTrace();

    node->connectInput(0, {.node = m_signal->getNode(), .port = 0ull});
    node->connectInput(1, {.node = signal.getNode(), .port = 0ull});

    hlim::Node_Rewire::RewireOperation rewireOp;
    HCL_ASSERT_HINT(m_selection.stride == 1, "Strided slices not yet implemented!");
    
    size_t selectionEnd = m_selection.end;
    if (m_selection.untilEndOfSource)
        selectionEnd = inputWidth;

    HCL_DESIGNCHECK(m_selection.start >= 0);
    HCL_DESIGNCHECK(m_selection.start < inputWidth);
    
    HCL_DESIGNCHECK(selectionEnd >= 0);
    HCL_DESIGNCHECK(selectionEnd <= inputWidth);

    HCL_DESIGNCHECK_HINT(selectionEnd - m_selection.start == signal.getWidth(), "When assigning a signal to a sliced signal, the widths of the assigned signal and the slicing range must match");

    
    if (m_selection.start != 0)
        rewireOp.ranges.push_back({
            .subwidth = m_selection.start,
            .source = hlim::Node_Rewire::OutputRange::INPUT,
            .inputIdx = 0,
            .inputOffset = 0,
        });

    rewireOp.ranges.push_back({
        .subwidth = selectionEnd - m_selection.start,
        .source = hlim::Node_Rewire::OutputRange::INPUT,
        .inputIdx = 1,
        .inputOffset = 0,
    });

    if (selectionEnd != inputWidth)
        rewireOp.ranges.push_back({
            .subwidth = inputWidth - selectionEnd,
            .source = hlim::Node_Rewire::OutputRange::INPUT,
            .inputIdx = 0,
            .inputOffset = selectionEnd,
        });
    
    node->setOp(std::move(rewireOp));
    node->changeOutputType(m_signal->getNode()->getOutputConnectionType(0));    
    
    m_signal->operator=(SignalType(hlim::NodePort{ .node = node, .port = 0ull }));
    
    return *this;
}

template<typename SignalType>
BitVectorSlice<SignalType>::operator SignalType() const
{
    hlim::NodePort currentInput;
    if (m_signal != nullptr) {
        currentInput = {.node = m_signal->getNode(), .port = 0ull};
    } else
        currentInput = m_lastSignalNodePort;
    
    size_t inputWidth = currentInput.node->getOutputConnectionType(currentInput.port).width;
    
    hlim::Node_Rewire* node = DesignScope::createNode<hlim::Node_Rewire>(1);
    node->recordStackTrace();

    node->connectInput(0, currentInput);

    hlim::Node_Rewire::RewireOperation rewireOp;
    HCL_ASSERT_HINT(m_selection.stride == 1, "Strided slices not yet implemented!");
    
    size_t selectionEnd = m_selection.end;
    if (m_selection.untilEndOfSource)
        selectionEnd = inputWidth;

    HCL_DESIGNCHECK(m_selection.start >= 0);
    HCL_DESIGNCHECK(m_selection.start < inputWidth);
    
    HCL_DESIGNCHECK(selectionEnd >= 0);
    HCL_DESIGNCHECK(selectionEnd <= inputWidth);
    
    rewireOp.ranges.push_back({
        .subwidth = selectionEnd - m_selection.start,
        .source = hlim::Node_Rewire::OutputRange::INPUT,
        .inputIdx = 0,
        .inputOffset = m_selection.start,
    });

    node->setOp(std::move(rewireOp));
    node->changeOutputType(currentInput.node->getOutputConnectionType(0));    
    return SignalType(hlim::NodePort{ .node = node, .port = 0ull });    
}

template<typename SignalType>
void BitVectorSlice<SignalType>::unregisterSignal() { 
    m_lastSignalNodePort = {.node = m_signal->getNode(), .port = 0ull};
    m_signal = nullptr;
}




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
    node->changeOutputType(m_node->getOutputConnectionType(0));    
    return FinalType(hlim::NodePort{ .node = node, .port = 0ull });
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
    node->changeOutputType(m_node->getOutputConnectionType(0));    
    return FinalType(hlim::NodePort{.node = node, .port = 0ull});
}

}
