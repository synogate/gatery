read_verilog top.v
#synth_gatemate -top top -luttree -nomx8 -noflatten -noiopad
#flatten -scopename
synth_gatemate -top top -luttree -nomx8
#write_verilog synth.v
write_json synth.json
