rv32cc = riscv64-unknown-elf-gcc -march=rv32i -mabi=ilp32

firmware.elf: main.c logging.c gen_mmap.h Makefile ministub.h
	$(rv32cc) -nostdlib -nodefaultlibs -fno-exceptions -nostartfiles -Os -flto -Wl,--section-start=.text=0x9000000 -fno-pie -o $@ main.c logging.c

