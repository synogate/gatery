TOP = top

# the oss-cad openFPGALoader was unable to communicate
OFL   = "C:\Program Files\cc-toolchain-win\bin\openFPGALoader\openFPGALoader.exe"
YOSYS = yosys
#"C:\Program Files\cc-toolchain-win\bin\yosys\yosys.exe"

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

bitfile.txt: synth.json top.sdc top.ccf
	nextpnr-himbaechel --json $< --device CCGM1A1 --router router2 --seed 1 --sdc .\top.sdc -o ccf=top.ccf -o out=bitfile.txt --write pnr.json

%.bit: %.txt
	gmpack $< $@

%.v: %.vhd
	ghdl -a --std=08 --warn-no-binding -C --ieee=synopsys $<
	ghdl -e --std=08 --warn-no-binding -C --ieee=synopsys $(TOP)
	ghdl synth --std=08 --out=verilog > $@

