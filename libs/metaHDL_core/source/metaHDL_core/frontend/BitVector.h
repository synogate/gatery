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

class ElementaryVector : public ElementarySignal
{
public:
    using ElementarySignal::ElementarySignal;

    virtual void resize(size_t width);

    Bit operator[](size_t idx);

    void setBit(size_t idx, const Bit& in);

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


}
