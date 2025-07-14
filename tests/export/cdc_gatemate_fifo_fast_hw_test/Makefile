PRFLAGS += -ccf $(TOP).ccf -cCP
TOP = top

YOSYS = yosys
PR    = p_r

# the oss-cad openFPGALoader was unable to communicate
OFL   = "C:\Program Files\cc-toolchain-win\bin\openFPGALoader\openFPGALoader.exe"

all: jtag

clean:
	del *.bit
	del *.json
	del *.txt

# p_r flow
#net_$(TOP)_synth.v: $(TOP).vhd
#	$(YOSYS) -ql net_$(TOP)_synth.log -p 'ghdl --std=08 --warn-no-binding -C --ieee=synopsys $^ -e $(TOP); synth_gatemate -top $(TOP) -nomx8 -vlog $@'
#
#impl/$(TOP)_00.cfg: net_$(TOP)_synth.v $(TOP).ccf
#	$(PR) -i net_$(TOP)_synth.v --fpga_mode 3 --time_mode 2 +crf -o impl/$(TOP) $(PRFLAGS) > $@.log
#
jtag: bitfile.bit
	$(OFL) $(OFLFLAGS) -c gatemate_pgm $<

# nextpnr flow
synth.json: $(TOP).v
	yosys -ql net_$(TOP)_synth.log -p 'synth_gatemate -top $(TOP) -json $@' $<

bitfile.txt: synth.json
	nextpnr-himbaechel --json $< --device CCGM1A1 --sdc .\top.sdc -o ccf=top.ccf -o out=bitfile.txt --write pnr.json

%.bit: %.txt
	gmpack $< $@

%.v: %.vhd
	ghdl -a --std=08 --warn-no-binding -C --ieee=synopsys $<
	ghdl -e --std=08 --warn-no-binding -C --ieee=synopsys $(TOP)
	ghdl synth --std=08 --out=verilog > $@

