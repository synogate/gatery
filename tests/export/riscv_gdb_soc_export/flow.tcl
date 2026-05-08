read_verilog top.v
read_verilog -lib -specify +/gatemate/cells_sim.v +/gatemate/cells_bb.v
synth -top top -flatten -run :fine
splitnets
opt -fast -full
opt -full -sat
clean
#write_verilog test.v
synth_gatemate -luttree -nomx8
write_json synth.json
