TOP ?= top
SEED ?= 1
GRADE ?= -o fpga_mode=speed -o time_mode=typical

YOSYS ?= yosys
PR    ?= p_r
OFL   ?= openFPGALoader

all: jtag

clean:
	del *.bit
	del *.json
	del *.txt

synth.json: $(TOP).v
	yosys -ql net_$(TOP)_synth.log -p 'synth_gatemate -top $(TOP) -luttree -nomx8 -json $@' $<

bitfile.txt: synth.json $(TOP).ccf
	nextpnr-himbaechel --json $< --device CCGM1A1 --router router2 $(GRADE) --sdc .\top.sdc --seed $(SEED) -o ccf=$(TOP).ccf -o out=bitfile.txt --write pnr.json

%.bit: %.txt
	gmpack $< $@

%.v: %.vhd
	ghdl -a --std=08 --warn-no-binding -C --ieee=synopsys $<
	ghdl -e --std=08 --warn-no-binding -C --ieee=synopsys $(TOP)
	ghdl synth --std=08 --out=verilog > $@

jtag: bitfile.bit
	$(OFL) $(OFLFLAGS) -c gatemate_pgm $<
