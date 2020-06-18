
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
#include <fstream>


using namespace mhdl::core::frontend;
using namespace mhdl::core::hlim;
using namespace mhdl::utils;
using namespace mhdl::core::vhdl;

int main() { 
   
    try {
        try {

            DesignScope design;

            {
                auto clk = design.createClock<mhdl::core::hlim::RootClock>("clk", mhdl::core::hlim::ClockRational(10'000));
                
                RegisterConfig regConf = {.clk = clk, .resetName = "rst"};
                
                UartTransmitter uart(8, 1, 1000);
                
                BitVector data(8);
                data.setName("data_of_uart0");
                
                Bit send, idle, outputLine;
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
            
            VHDLExport vhdl("VHDL_out/");
            vhdl(design.getCircuit());
            
            std::fstream testbench("VHDL_out/testbench.vhdl", std::fstream::out);
            testbench << R"Delim(

LIBRARY ieee;
USE ieee.std_logic_1164.ALL;
USE ieee.numeric_std.all;
use std.textio.all;

entity clockGen is
    port ( 
        clk : out STD_LOGIC
    );
end clockGen;

architecture impl of clockGen is
begin
    clk_process: process
    begin
        clk <= '0';
        wait for 500 ns;
        clk <= '1';
        wait for 500 ns;
    end process;
end impl;



LIBRARY ieee;
USE ieee.std_logic_1164.ALL;
USE ieee.numeric_std.all;
use std.textio.all;


entity testbench is
end testbench;

architecture impl of testbench is
    SIGNAL clk : STD_LOGIC; 
    SIGNAL in_data : STD_LOGIC_VECTOR(7 downto 0); 
    SIGNAL out_idle : STD_LOGIC; 
    SIGNAL in_send : STD_LOGIC; 
    SIGNAL out_outputLine : STD_LOGIC; 
    SIGNAL reset : STD_LOGIC; 
begin

    clockGenerator : entity work.clockGen(impl) port map (clk => clk);
    
    inst_UartTransmitter : entity work.UartTransmitter(impl) port map (
        clk => clk,
        reset => reset,
        in_data_of_uart0 => in_data,
        in_send => in_send,
        out_idle => out_idle,
        out_outputLine => out_outputLine
    );    


    process
        variable l : line;
    begin
        write (l, String'("Running testbench!"));    
        writeline (output, l);
        
        reset <= '1';        
        in_send <= '0';
        in_data <= "00000000";
        wait for 2 us;
        
        reset <= '0';        
        wait for 2 us;
        
        in_data <= "11001100";
        in_send <= '1';
        wait for 2 us;        
        in_send <= '0';
        wait for 10 us;

        in_data <= "01001010";
        in_send <= '1';
        wait for 2 us;        
        in_send <= '0';
        wait for 2 us;
        
        in_data <= "10101010";
        in_send <= '1';
        wait for 15 us;        
        in_send <= '0';
        wait for 10 us;
        
        
        write (l, String'("Done!"));
        writeline (output, l);
        wait;
    end process;
end impl;
                
)Delim";
            
            std::fstream script("VHDL_out/compile_and_run.sh", std::fstream::out);
            script << R"Delim(#!/bin/bash
ghdl -a --std=08 --ieee=synopsys UartTransmitter.vhdl
ghdl -a --std=08 --ieee=synopsys testbench.vhdl
ghdl -e --std=08 --ieee=synopsys testbench
./testbench --vcd=signals.vcd --wave=signals.ghw --stop-time=50us
)Delim";
            
        } catch (const mhdl::utils::InternalError &exception) {
            std::cerr << "Internal error occured!" << std::endl << exception << std::endl;
            return -1;
        }
    } catch (const mhdl::utils::DesignError &exception) {
        std::cout << "Design error!" << std::endl << exception << std::endl;
    }
    return 0;
}
