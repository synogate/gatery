
#include <metaHDL_core/frontend/Registers.h>
#include <metaHDL_core/frontend/BitVector.h>
#include <metaHDL_core/frontend/Integers.h>
#include <metaHDL_core/frontend/Scope.h>
#include <metaHDL_core/utils/Preprocessor.h>
#include <metaHDL_core/utils/StackTrace.h>
#include <metaHDL_core/utils/Exceptions.h>

#include <metaHDL_core/export/vhdl/VHDLExport.h>

#include "uart.h"


#include <iostream>


using namespace mhdl::core::frontend;
using namespace mhdl::core::hlim;
using namespace mhdl::utils;
using namespace mhdl::core::vhdl;

int main() { 
   
    try {
        try {

            DesignScope design;

            {
                GroupScope area(NodeGroup::GRP_AREA);
                area.setName("all");
                
                UartTransmitter uart(8, 1, 1000);
                
                BitVector data(8);
                data.setName("data_of_uart0");
                
                Bit send, idle, outputLine;
                send.setName("send_of_uart0");
                idle.setName("idle_of_uart0");
                outputLine.setName("outputLine_of_uart0");
                
                uart(data, send, outputLine, idle);
                
                Bit useIdle = idle;
                MHDL_NAMED(useIdle);
                Bit useOutputLine = outputLine;
                MHDL_NAMED(useOutputLine);
            }
            
            design.getCircuit().cullUnnamedSignalNodes();
            design.getCircuit().cullOrphanedSignalNodes();
            
            VHDLExport vhdl("VHDL_out/");
            vhdl(design.getCircuit());
            
        } catch (const mhdl::utils::InternalError &exception) {
            std::cerr << "Internal error occured!" << std::endl << exception << std::endl;
            return -1;
        }
    } catch (const mhdl::utils::DesignError &exception) {
        std::cout << "Design error!" << std::endl << exception << std::endl;
    }
    return 0;
}
