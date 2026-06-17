TOP = top
OFL	= openFPGALoader

all: bitfile.bit

clean:
	rm *.bit
	rm *.json
	rm *.txt

# nextpnr flow
synth.json: $(TOP).v
	yosys -ql net_$(TOP)_synth.log -p 'synth_gatemate -top $(TOP) -luttree -nomx8 -json $@' $<

bitfile.txt: synth.json top.sdc top.ccf Makefile
	nextpnr-himbaechel --json $< --device CCGM1A1 --router router2 --seed 22 --sdc top.sdc -o ccf=top.ccf -o out=bitfile.txt --write pnr.json -o fpga_mode=speed -o time_mode=typical
	wsl sed -i 's/GPIO.BANK_CFG 1/GPIO.BANK_CFG 0/' bitfile.txt

%.bit: %.txt
	gmpack $< $@ --reconfig --bootaddr 262144

%.v: %.vhd
	ghdl -a --std=08 --warn-no-binding -C --ieee=synopsys $<
	ghdl -e --std=08 --warn-no-binding -C --ieee=synopsys $(TOP)
	ghdl synth --std=08 --out=verilog > $@.tmp
	wsl sh ghdl_yosys_memory_hotfix.sh

flash: bitfile.bit
	$(OFL) $(OFLFLAGS) -f -c gatemate_pgm $<

jtag: bitfile.bit
	$(OFL) $(OFLFLAGS) -c gatemate_pgm $<
