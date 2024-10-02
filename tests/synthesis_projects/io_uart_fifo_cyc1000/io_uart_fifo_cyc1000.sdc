create_clock -period 83.333 [get_ports CLK12M]
derive_pll_clocks
derive_clock_uncertainty

set_false_path -to [get_ports LED*]
set_false_path -to [get_ports TX]
set_false_path -from [get_ports RX]
