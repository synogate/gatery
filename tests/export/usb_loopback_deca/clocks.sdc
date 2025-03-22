create_clock -period 100.000 [get_ports altera_reserved_tck]
create_clock -name usbclk -period 20 [get_ports MAX10_CLK1_50]
derive_clock_uncertainty
derive_pll_clocks

set_max_skew -to [get_ports USB_*] 1
