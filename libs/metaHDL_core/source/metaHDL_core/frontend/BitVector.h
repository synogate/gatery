#pragma once

#include "Bit.h"
#include "Signal.h"
#include "Scope.h"

#include "../hlim/coreNodes/Node_Signal.h"
#include "../hlim/coreNodes/Node_Rewire.h"

#include "../utils/Exceptions.h"

#include <vector>

namespace mhdl {
namespace core {    
namespace frontend {

// concat x = a && b && c;
    
    
struct Selection {
    // some selection descriptor
    
    static Selection From(int start);
    static Selection Range(int start, int end);
    static Selection RangeIncl(int start, int endIncl);
    static Selection StridedRange(int start, int end, int stride);

    static Selection Slice(int offset, unsigned size);
    static Selection StridedSlice(int offset, unsigned size, int stride);
};

/**
 * @todo write docs
 */
template<typename FinalType>
class BaseBitVector : public ElementarySignal
{
    public:
        using isBitVectorSignal = void;
        
        virtual void resize(unsigned width);
        
        Bit operator[](int idx);

        FinalType operator()(int offset, unsigned size) { return operator()(Selection::Range(offset, size)); }
        FinalType operator()(const Selection &selection) { }

        FinalType &operator=(const FinalType &rhs) { assign(rhs); return *this; }
    protected:
        BaseBitVector() = default;
        BaseBitVector(hlim::Node::OutputPort *port, const hlim::ConnectionType &connectionType) : ElementarySignal(port, connectionType) { }
        
};


template<typename FinalType>
void BaseBitVector<FinalType>::resize(unsigned width)
{
    MHDL_DESIGNCHECK_HINT(m_node->isOrphaned(), "Can not resize signal once it is connected (driving or driven).");
    
    m_node->setConnectionType(getSignalType(width));    
}


template<typename FinalType>
Bit BaseBitVector<FinalType>::operator[](int idx)
{
    hlim::Node_Rewire *node = Scope::getCurrentNodeGroup()->addNode<hlim::Node_Rewire>(1);
    node->recordStackTrace();
    
    idx = (m_node->getConnectionType().width + (idx % m_node->getConnectionType().width)) % m_node->getConnectionType().width;
    
    hlim::Node_Rewire::RewireOperation rewireOp;
    rewireOp.ranges.push_back({
        .subwidth = 1,
        .source = hlim::Node_Rewire::OutputRange::INPUT,
        .inputIdx = 0,
        .inputOffset = (unsigned) idx,
    });
    node->setOp(std::move(rewireOp));
    
    m_node->getOutput(0).connect(node->getInput(0));
    hlim::ConnectionType connectionType;
    connectionType.interpretation = hlim::ConnectionType::BOOL;
    connectionType.width = 1;
    return Bit(&node->getOutput(0), connectionType);
}




class BitVector : public BaseBitVector<BitVector>
{
    public:
        MHDL_SIGNAL
        
        using isUntypedBitvectorSignal = void;        
        
        BitVector() = default;
        BitVector(unsigned width);
        BitVector(hlim::Node::OutputPort *port, const hlim::ConnectionType &connectionType);
    protected:
        virtual hlim::ConnectionType getSignalType(unsigned width) const override;

};


}
}
}
