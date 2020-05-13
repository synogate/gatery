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

template<typename T>
T delay(RegisterFactory& clock, const T& in, const Bit& enable, const T& resetValue, size_t count)
{
    T out = in;
    for (size_t i = 0; i < count; ++i)
        out = clock(out, enable, resetValue);
    return out;
}

class GameOfLife
{
public:
    GameOfLife(size_t width) :
        m_Width(width)
    {}

    BitStream operator() (RegisterFactory& clock, const BitStream& in) const
    {
        GroupScope entity(NodeGroup::GRP_ENTITY);
        entity.setName("GameOfLife");

        GroupScope area(NodeGroup::GRP_AREA);
        area.setName("all"); // ???


        std::array<UnsignedInteger, 9> neighbors;
        neighbors[0] = UnsignedInteger(1);
        neighbors[0].setBit(0, in.data);

        for (size_t i = 1; i < neighbors.size(); ++i)
        {
            if (i % 3 == 0)
                neighbors[i] = delay(clock, neighbors[i - 1], in.valid, 0b0_uvec, m_Width - 3);
            else
                neighbors[i] = clock(neighbors[i-1], in.valid, 0b0_uvec);
        }

        for (size_t i = 0; i < neighbors.size(); ++i)
            neighbors[i].setName("neighbor" + std::to_string(i));

        UnsignedInteger sum = std::accumulate(neighbors.begin(), neighbors.end(), 0x0_uvec);
        sum.setName("sum");

        PriorityConditional<Bit> sel;
        sel.addCondition(sum == 4_uvec, 1_bit);
        sel.addCondition(sum == 3_uvec, neighbors[1 * 3 + 1][0]);

        BitStream out;
        out.data = sel(0_bit);
        MHDL_NAMED(out.data);
        out.valid = in.valid;
        MHDL_NAMED(out.valid);
        return out;
    }


private:
    size_t m_Width;
};

int main()
{
    DesignScope design;

    {
        GroupScope area(NodeGroup::GRP_AREA);
        area.setName("all");

        auto clk = design.createClock<mhdl::core::hlim::RootClock>("clk", mhdl::core::hlim::ClockRational(10'000));
        RegisterConfig regConf = {.clk = clk, .resetName = "rst"};
        RegisterFactory registerFactory(regConf);

        GameOfLife game(32);

        BitStream in;
        MHDL_NAMED(in.data);
        MHDL_NAMED(in.valid);

        BitStream out = game(registerFactory, in);
    }

    design.getCircuit().cullUnnamedSignalNodes();
    design.getCircuit().cullOrphanedSignalNodes();

    VHDLExport vhdl("VHDL_out/");
    vhdl(design.getCircuit());

}
