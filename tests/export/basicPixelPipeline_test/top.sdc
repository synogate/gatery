create_clock -period 83.333 -name {CLK12M} [get_ports {CLK12M}]
create_clock -period 39 -name {pixelClk} [get_nets {scl_clockdiv100.out_unnamed}]
create_clock -period 3.9 -name {bitClk} [get_nets {scl_clockdiv100.o}]
