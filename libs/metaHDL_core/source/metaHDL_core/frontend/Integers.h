#pragma once

#include "BitVector.h"

namespace mhdl {
namespace core {    
namespace frontend {

class UnsignedInteger : public BaseBitVector<UnsignedInteger>
{
    public:
        UnsignedInteger(unsigned width) : BaseBitVector<UnsignedInteger>(width) { }
        
        UnsignedInteger operator+(UnsignedInteger rhs);
};

}
}
}
