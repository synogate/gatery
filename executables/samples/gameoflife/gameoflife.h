#pragma once

#include "pch.h"
#include <numeric>

using namespace mhdl::core::frontend;
using namespace mhdl::core::hlim;
using namespace mhdl::utils;
using namespace mhdl::core::vhdl;

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
