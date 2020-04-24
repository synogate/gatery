#pragma once

#include "Bit.h"
#include "Signal.h"
#include "Scope.h"

#include "../hlim/coreNodes/Node_Signal.h"
#include "../hlim/coreNodes/Node_Rewire.h"

#include "../utils/Exceptions.h"

#include <vector>

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

/**
 * @todo write docs
 */
template<typename FinalType>
class BaseBitVector : public ElementarySignal
{
    public:
        using isBitVectorSignal = void;
        
        BaseBitVector(const FinalType &rhs) { assign(rhs); }
        
        virtual void resize(size_t width);
        
        Bit operator[](size_t idx);

        FinalType operator()(int offset, size_t size) { return operator()(Selection::Range(offset, size)); }
        FinalType operator()(const Selection &selection) { }

        FinalType &operator=(const FinalType &rhs) { assign(rhs); return *this; }
    protected:
        BaseBitVector() = default;
        BaseBitVector(const hlim::NodePort &port) : ElementarySignal(port) { }        
};


template<typename FinalType>
void BaseBitVector<FinalType>::resize(size_t width)
{
    MHDL_DESIGNCHECK_HINT(m_node->isOrphaned(), "Can not resize signal once it is connected (driving or driven).");
    
    m_node->setConnectionType(getSignalType(width));    
}


template<typename FinalType>
Bit BaseBitVector<FinalType>::operator[](size_t idx)
{
    hlim::Node_Rewire *node = DesignScope::createNode<hlim::Node_Rewire>(1);
    node->recordStackTrace();
    
    size_t w = m_node->getOutputConnectionType(0).width;
    idx = (w + (idx % w)) % w;
    
    hlim::Node_Rewire::RewireOperation rewireOp;
    rewireOp.ranges.push_back({
        .subwidth = 1,
        .source = hlim::Node_Rewire::OutputRange::INPUT,
        .inputIdx = 0,
        .inputOffset = (size_t) idx,
    });
    node->setOp(std::move(rewireOp));
    
    node->connectInput(0, {.node = m_node, .port = 0ull});

    return Bit({.node = node, .port = 0ull});
}




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


}
