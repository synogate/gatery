PRFLAGS += -ccf $(TOP).ccf -cCP ++time_pathes 50
TOP = physical_memory_2

YOSYS = yosys
PR    = p_r
OFL   = openFPGALoader

all: jtag

clean:
	rm *_synth*; rm .\impl\*

# p_r flow
net_$(TOP)_synth.v: cdc_gatemate_fifo_hw_test.vhd
	$(YOSYS) -ql net_$(TOP)_synth.log -p 'ghdl --std=08 --warn-no-binding -C --ieee=synopsys $^ -e $(TOP); synth_gatemate -top $(TOP) -nomx8 -vlog $@'

impl/$(TOP)_00.cfg: net_$(TOP)_synth.v $(TOP).ccf
	$(PR) -i net_$(TOP)_synth.v --fpga_mode 3 --time_mode 2 +crf -o impl/$(TOP) $(PRFLAGS) > $@.log

# nextpnr flow
net_$(TOP)_synth.json: cdc_gatemate_fifo_hw_test.vhd
	$(YOSYS) -ql net_$(TOP)_synth.log -p 'ghdl --std=08 --warn-no-binding -C --ieee=synopsys $^ -e $(TOP); synth_gatemate -top $(TOP) -nomx8 -luttree -noaddf -json $@'

$(TOP).bit.txt: net_$(TOP)_synth.json $(TOP).ccf $(TOP).sdc
	nextpnr-himbaechel --json net_$(TOP)_synth.json --device CCGM1A1 --chipdb .\share\himbaechel\gatemate\chipdb-CCGM1A1.bin -o "ccf=$(TOP).ccf" -o "out=$@" --sdc $(TOP).sdc

jtag: impl/$(TOP)_00.cfg
	$(OFL) $(OFLFLAGS) -c gatemate_pgm $<

flash: impl/$(TOP)_00.cfg
	$(OFL) $(OFLFLAGS) -f --verify -c gatemate_pgm $<

evb_jtag: impl/$(TOP)_00.cfg
	$(OFL) $(OFLFLAGS) -b gatemate_evb_jtag $<
