#include "gameoflife.h"

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
