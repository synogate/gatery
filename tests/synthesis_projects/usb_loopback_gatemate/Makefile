PRFLAGS += -ccf $(TOP).ccf -cCP
TOP = top

YOSYS = yosys
PR    = p_r
OFL   = openFPGALoader

all: jtag

clean:
	rm *_synth*; rm .\impl\*

net_$(TOP)_synth.v: usb_loopback_gatemate.vhd
	$(YOSYS) -ql net_$(TOP)_synth.log -p 'ghdl --std=08 --warn-no-binding -C --ieee=synopsys $^ -e $(TOP); synth_gatemate -top $(TOP) -nomx8 -vlog net_$(TOP)_synth.v'
#net_$(TOP)_synth.v: test.v
#	$(YOSYS) -ql net_$(TOP)_synth.log -p 'read -sv $^; synth_gatemate -top $(TOP) -nomx8 -vlog net_$(TOP)_synth.v'

impl/$(TOP)_00.cfg: net_$(TOP)_synth.v $(TOP).ccf
	$(PR) -i net_$(TOP)_synth.v --fpga_mode 3 --time_mode 2 +crf -o impl/$(TOP) $(PRFLAGS) > $@.log

jtag: impl/$(TOP)_00.cfg
	$(OFL) $(OFLFLAGS) -c gatemate_pgm $<

flash: impl/$(TOP)_00.cfg
	$(OFL) $(OFLFLAGS) -f --verify -c gatemate_pgm $<

evb_jtag: impl/$(TOP)_00.cfg
	$(OFL) $(OFLFLAGS) -b gatemate_evb_jtag $<
