rv32cc = riscv64-unknown-elf-gcc -march=rv32i -mabi=ilp32

demo.elf: main.c logging.c gen_mmap.h Makefile
	$(rv32cc) -nostdlib -nodefaultlibs -fno-exceptions -nostartfiles -Os -flto -fno-pie -o $@ main.c

