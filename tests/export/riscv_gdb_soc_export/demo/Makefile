rv32cc = riscv64-unknown-elf-g++ -march=rv32i -mabi=ilp32

MCUFONT_SRC = $(wildcard mcufont/*.c)

OPT = -Os -flto -fno-pie -fno-exceptions -fno-rtti
#NO_GCCLIBS = -nostdlib -nodefaultlibs -fno-builtin -nostartfiles
#NO_GCCLIBS = -nostartfiles
#NO_GCCLIBS = -Wl,-wrap=_exit

DRIVER_DIR = ../../../../source/gatery/scl/driver
HEADER += $(wildcard $(DRIVER_DIR)/**/*.h)

demo.elf: main.cpp logging.c ../firmware/gen_mmap.h Makefile $(MCUFONT_SRC) $(HEADER)
	$(rv32cc) -I$(DRIVER_DIR) $(NO_GCCLIBS) $(OPT) -std=c++20 -o $@ main.cpp

