#pragma once

#include "pch.h"
#include <numeric>

using namespace mhdl::core::frontend;
using namespace mhdl::core::hlim;
using namespace mhdl::utils;
using namespace mhdl::core::vhdl;

template<typename T>
T delay(RegisterFactory& clock, const T& in, const Bit& enable, const T& resetValue, size_t count)
{
    GroupScope entity(NodeGroup::GRP_ENTITY);
    entity.setName(std::string("delay_by_") + std::to_string(count));

    GroupScope area(NodeGroup::GRP_AREA);
    area.setName("all"); // ???

    T out = in;
    for (size_t i = 0; i < count; ++i)
        out = clock(out, enable, resetValue);
    return out;
}


struct BitStream
{
    Bit valid;
    Bit data;
};

class GameOfLife
{
    public:
        GameOfLife(size_t width);
        BitStream operator() (RegisterFactory& clock, const BitStream& in) const;
    private:
        size_t m_Width;
};
