rv32cc = riscv64-unknown-elf-gcc -march=rv32i -mabi=ilp32

firmware.elf: main.c logging.c gen_mmap.h Makefile
	$(rv32cc) -nostdlib -nodefaultlibs -fno-exceptions -nostartfiles -Os -fpie -flto -Wl,--section-start=.text=0x20000 -o $@ main.c logging.c

