#**************************************************************
# This .sdc file is created by Terasic Tool.
# Users are recommended to modify this file to match users logic.
#**************************************************************

#**************************************************************
# Create Clock
#**************************************************************
#create_clock -period 100 [get_ports ADC_CLK_10]
#create_clock -period 20 [get_ports MAX10_CLK1_50]
#create_clock -period 20 [get_ports MAX10_CLK2_50]
create_clock -period 20 [get_ports DDR3_CLK_50]

#**************************************************************
# Create Generated Clock
#**************************************************************
derive_pll_clocks



#**************************************************************
# Set Clock Latency
#**************************************************************



#**************************************************************
# Set Clock Uncertainty
#**************************************************************
derive_clock_uncertainty



#**************************************************************
# Set Input Delay
#**************************************************************



#**************************************************************
# Set Output Delay
#**************************************************************



#**************************************************************
# Set Clock Groups
#**************************************************************



#**************************************************************
# Set False Path
#**************************************************************



#**************************************************************
# Set Multicycle Path
#**************************************************************
#set_multicycle_path -from {scl_tileLinkDynamicToFixedLatencyCdc:scl_tileLinkDynamicToFixedLatencyCdc0|*_out_a_*_delayed1* scl_tileLinkDynamicToFixedLatencyCdc:scl_tileLinkDynamicToFixedLatencyCdc0|s_out_d_valid_2} -setup 2 -end -to [get_clocks {ddr3_uniphy_inst|ddr3_uniphy_inst|pll0|upll_memphy|auto_generated|pll1|clk[4]_afi_clk}]
#set_multicycle_path -from {scl_tileLinkDynamicToFixedLatencyCdc:scl_tileLinkDynamicToFixedLatencyCdc0|*_out_a_*_delayed1* scl_tileLinkDynamicToFixedLatencyCdc:scl_tileLinkDynamicToFixedLatencyCdc0|s_out_d_valid_2} -hold 1 -end -to [get_clocks {ddr3_uniphy_inst|ddr3_uniphy_inst|pll0|upll_memphy|auto_generated|pll1|clk[4]_afi_clk}]


#**************************************************************
# Set Maximum Delay
#**************************************************************



#**************************************************************
# Set Minimum Delay
#**************************************************************



#**************************************************************
# Set Input Transition
#**************************************************************



#**************************************************************
# Set Load
#**************************************************************



