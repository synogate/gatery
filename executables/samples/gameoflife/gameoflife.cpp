#include "pch.h"

#include "gameoflife.h"

#include <numeric>

using namespace mhdl::core::frontend;
using namespace mhdl::core::hlim;
using namespace mhdl::utils;
using namespace mhdl::core::vhdl;


GameOfLife::GameOfLife(size_t width) :
    m_Width(width)
{}

BitStream GameOfLife::operator() (RegisterFactory& clock, const BitStream& in) const
{
    GroupScope entity(NodeGroup::GRP_ENTITY);
    entity.setName("GameOfLife");

    GroupScope area(NodeGroup::GRP_AREA);
    area.setName("all"); // ???


    
    std::array<Bit, 9> neighborBits;
    {
        GroupScope entity(NodeGroup::GRP_ENTITY);
        entity.setName("cacheNeighbors");

        GroupScope area(NodeGroup::GRP_AREA);
        area.setName("all"); // ???

        neighborBits[0] = in.data;

        for (size_t i = 1; i < neighborBits.size(); ++i)
        {
            if (i % 3 == 0)
                neighborBits[i] = delay(clock, neighborBits[i - 1], in.valid, 0_bit, m_Width - 3);
            else
                neighborBits[i] = clock(neighborBits[i-1], in.valid, 0_bit);
        }

        for (size_t i = 0; i < neighborBits.size(); ++i)
            neighborBits[i].setName("neighbor_bit" + std::to_string(i));
    }

    std::array<UnsignedInteger, 9> neighbors;
    {
        GroupScope entity(NodeGroup::GRP_ENTITY);
        entity.setName("bitextendNeighbors");

        GroupScope area(NodeGroup::GRP_AREA);
        area.setName("all"); // ???

        for (size_t i = 0; i < neighbors.size(); ++i) {
            neighbors[i] = 0b0000_uvec;
            neighbors[i].setBit(0, neighborBits[i]);
            neighbors[i].setName("neighbor" + std::to_string(i));
        }
    }
    
    UnsignedInteger sum;
    {
        GroupScope entity(NodeGroup::GRP_ENTITY);
        entity.setName("sumNeighbors");

        GroupScope area(NodeGroup::GRP_AREA);
        area.setName("all"); // ???

        sum = neighbors[0] + neighbors[1] + neighbors[2] +  
              neighbors[3]                + neighbors[5] +  
              neighbors[6] + neighbors[7] + neighbors[8];
        sum.setName("sum");
    }

    PriorityConditional<Bit> sel;
    sel.addCondition(sum == 0b0011_uvec, 1_bit);
    sel.addCondition(sum == 0b0010_uvec, neighbors[1 * 3 + 1][0]);

    BitStream out;
    out.data = sel(0_bit);
    MHDL_NAMED(out.data);
    out.valid = in.valid;
    MHDL_NAMED(out.valid);
    return out;
}

