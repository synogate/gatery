#pragma once

#include "Signal.h"

#include <vector>

namespace mhdl {
namespace core {    
namespace frontend {

/**
 * @todo write docs
 */
template<typename FinalType>
class BaseBitVector : public Signal<FinalType>
{
    public:
        BaseBitVector(unsigned width) : m_width(width) { }
        
        FinalType operator<<(unsigned amount);
        FinalType operator>>(unsigned amount);
        FinalType operator|(const FinalType &rhs);
        FinalType operator&(const FinalType &rhs);
        FinalType operator^(const FinalType &rhs);
        FinalType operator~();
        FinalType operator[](unsigned idx);
        
        unsigned getWidth() const { return m_width; }
        
        virtual void assign(const FinalType &rhs) override { }
    protected:
        unsigned m_width;
};

template<class FinalType, class RhsType>
FinalType concat(FinalType &lhs, RhsType &rhs) {
    static_assert(std::is_base_of<BaseBitVector<FinalType>, FinalType>::value, "Left hand side must be derived from BaseBitVector");
    static_assert(std::is_base_of<BaseBitVector<RhsType>, RhsType>::value, "Right hand side must be derived from BaseBitVector");
    
    
    FinalType concatSignal(lhs.getWidth() + rhs.getWidth());
   
    // todo: do something
        
    return concatSignal;
}

template<class SelectorType, class FinalType>
FinalType select(SelectorType &selector, FinalType lhs, FinalType rhs) {
    static_assert(std::is_base_of<BaseBitVector<FinalType>, FinalType>::value, "Left and right hand side must be derived from BaseBitVector");
    static_assert(std::is_base_of<BaseBitVector<SelectorType>, SelectorType>::value, "Selector type must be derived from BaseBitVector");
    
    if (lhs.getWidth() != rhs.getWidth())
        throw "baaaah";
    
    FinalType result(lhs.getWidth());
   
    // todo: do something
        
    return result;
}


class BitVector : public BaseBitVector<BitVector>
{
    public:
        BitVector(unsigned width) : BaseBitVector<BitVector>(width) { }
};

}
}
}
