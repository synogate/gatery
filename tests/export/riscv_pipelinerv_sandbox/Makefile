TOP = top

# the oss-cad openFPGALoader was unable to communicate
OFL   = openFPGALoader
SED		= wsl sed

all: jtag

clean:
	del *.bit
	del *.json
	del *.txt

# p_r flow
synth.v: $(TOP).v
	yosys -ql net_$(TOP)_synth.log -p 'synth_gatemate -top $(TOP) -nomx8 -vlog $@' $<

impl/$(TOP)_00.cfg: synth.v top.ccf
	p_r -i synth.v --fpga_mode 3 --time_mode 2 +crf -o impl/$(TOP) -ccf top.ccf -cCP > $@.log

jtag: bitfile.bit
	$(OFL) $(OFLFLAGS) -c gatemate_pgm $<

# nextpnr flow
synth.json: $(TOP).v
	yosys -ql net_$(TOP)_synth.log -p 'synth_gatemate -top $(TOP) -luttree -nomx8 -json $@' $<

bitfile.txt: synth.json top.sdc top.ccf Makefile
	nextpnr-himbaechel --json $< --device CCGM1A1 --router router2 --seed 13 --sdc .\top.sdc -o ccf=top.ccf -o out=bitfile.txt --write pnr.json -o fpga_mode=speed -o time_mode=typical

%.bit: %.txt
	gmpack $< $@

%.v: %.vhd
	ghdl -a --std=08 --warn-no-binding -C --ieee=synopsys $<
	ghdl -e --std=08 --warn-no-binding -C --ieee=synopsys $(TOP)
	ghdl synth --std=08 --out=verilog > $@.tmp
	wsl sh ghdl_yosys_memory_hotfix.sh
