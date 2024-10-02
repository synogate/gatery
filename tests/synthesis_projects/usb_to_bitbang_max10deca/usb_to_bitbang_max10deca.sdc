create_clock -period 20 [get_ports CLK50M]
derive_pll_clocks
derive_clock_uncertainty

set_false_path -to [get_ports LED*]
#set_false_path -to [get_ports TX]
#set_false_path -from [get_ports RX]

set_max_skew -from [get_ports {SCL MOSI MISO CS}] 1
set_max_skew -to [get_ports {SCL MOSI MISO CS}] 1
set_max_skew -from [get_ports USB_D*] 0.3
set_max_skew -to [get_ports USB_D*] 0.3
