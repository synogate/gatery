
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
using namespace mhdl::utils;
using namespace mhdl::core::vhdl;

int main() { 
   
    try {
        try {

            DesignScope design;

            {
                UartTransmitter uart(8, 1, 1000);
                
                BitVector data(8);
                data.setName("data");
                
                Bit send, idle, outputLine;
                send.setName("send");
                idle.setName("idle");
                outputLine.setName("outputLine");
                
                uart(data, send, outputLine, idle);
                
                Bit useIdle = idle;
                Bit useOutputLine = outputLine;
            }
            
            design.getCircuit().cullUnnamedSignalNodes();
            
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
