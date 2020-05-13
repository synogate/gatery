
#include "uart.h"

#include <metaHDL_core/frontend/Constant.h>
#include <metaHDL_core/frontend/Registers.h>
#include <metaHDL_core/frontend/BitVector.h>
#include <metaHDL_core/frontend/Integers.h>
#include <metaHDL_core/frontend/Scope.h>
#include <metaHDL_core/utils/Preprocessor.h>
#include <metaHDL_core/utils/StackTrace.h>
#include <metaHDL_core/utils/Exceptions.h>

#include <metaHDL_vis_Qt/MainWindowSimulate.h>
#include <QApplication>

#include <locale.h>

using namespace mhdl::core::frontend;
using namespace mhdl::core::hlim;
using namespace mhdl::utils;

int main(int argc, char *argv[]) { 
    setlocale(LC_ALL, "en_US.UTF-8");


    
    DesignScope design;

    {
        GroupScope area(NodeGroup::GRP_AREA);
        area.setName("all");
        
        auto clk = design.createClock<mhdl::core::hlim::RootClock>("clk", mhdl::core::hlim::ClockRational(10'000));
        RegisterConfig regConf = {.clk = clk, .resetName = "rst"};

        UartTransmitter uart(8, 1, 1000);
        
        BitVector data(8);
        
        Bit send, idle, outputLine;
        data = 0b01010101_vec;
        send = 1_bit;

        data.setName("data_of_uart0");
        send.setName("send_of_uart0");
        idle.setName("idle_of_uart0");
        outputLine.setName("outputLine_of_uart0");
        
        
        uart(data, send, outputLine, idle, regConf);
        
        Bit useIdle = idle;
        MHDL_NAMED(useIdle);
        Bit useOutputLine = outputLine;
        MHDL_NAMED(useOutputLine);
    }
    
    design.getCircuit().cullUnnamedSignalNodes();
    design.getCircuit().cullOrphanedSignalNodes();
            
    QApplication a(argc, argv);

    mhdl::vis::MainWindowSimulate w(nullptr, design.getCircuit());
    w.show();

    return a.exec();   
}
