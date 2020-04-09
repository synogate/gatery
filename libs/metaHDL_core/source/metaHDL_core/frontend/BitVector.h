#pragma once

#include "Bit.h"
#include "Signal.h"
#include "Scope.h"

#include "../hlim/coreNodes/Node_Signal.h"

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
        using isBitVector = void;
        
        virtual void resize(unsigned width);
        
        
        FinalType operator<<(unsigned amount) { }
        FinalType operator>>(unsigned amount) { }
        Bit operator[](int idx) { }

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
    hlim::Node_Signal *signalNode = dynamic_cast<hlim::Node_Signal*>(m_port->node);
    MHDL_ASSERT(signalNode != nullptr);
    MHDL_DESIGNCHECK_HINT(signalNode->isOrphaned(), "Can not resize signal once it is connected (driving or driven).");
    
    m_width = width;
    signalNode->setConnectionType(getSignalType());    
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
        virtual hlim::ConnectionType getSignalType() const override;

};


}
}
}
