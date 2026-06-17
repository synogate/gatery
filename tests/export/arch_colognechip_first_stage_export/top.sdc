create_clock -name clk12 -period 83.0 [get_ports CLK12M]
create_clock -name clkdiv2 -period 55.4 [get_nets cc_bufg_inst_2_O]
