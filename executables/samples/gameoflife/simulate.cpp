
#include "gameoflife.h"

#include <metaHDL_core/frontend/Constant.h>
#include <metaHDL_core/frontend/Registers.h>
#include <metaHDL_core/frontend/BitVector.h>
#include <metaHDL_core/frontend/Integers.h>
#include <metaHDL_core/frontend/Scope.h>
#include <metaHDL_core/utils/Preprocessor.h>
#include <metaHDL_core/utils/StackTrace.h>
#include <metaHDL_core/utils/Exceptions.h>
#include <metaHDL_core/hlim/supportNodes/Node_SignalGenerator.h>
#include <metaHDL_core/simulation/BitVectorState.h>

#include <metaHDL_vis_Qt/MainWindowSimulate.h>
#include <QApplication>

#include <locale.h>


using namespace mhdl::core::frontend;
using namespace mhdl::core::hlim;
using namespace mhdl::core;
using namespace mhdl::utils;




int main(int argc, char *argv[]) { 
    setlocale(LC_ALL, "en_US.UTF-8");

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
        
        in.data = 1_bit;
        in.valid = 1_bit;

        BitStream out = game(registerFactory, in);
    }

    design.getCircuit().cullUnnamedSignalNodes();
    design.getCircuit().cullOrphanedSignalNodes();

            
    QApplication a(argc, argv);

    mhdl::vis::MainWindowSimulate w(nullptr, design.getCircuit());
    w.show();

    return a.exec();   
}
