rv32cc = riscv64-unknown-elf-g++ -march=rv32i -mabi=ilp32

CXXFLAGS += -nostdlib -nodefaultlibs -fno-exceptions -fno-rtti -nostartfiles
CXXFLAGS += -std=c++20
CXXFLAGS += -Os -flto
CXXFLAGS += -Wl,--section-start=.text=0x9000000 -fno-pie

SRCS += $(wildcard *.cpp) $(wildcard ./**/*.cpp)
HEADER += $(wildcard *.h) $(wildcard ./**/*.h)

DRIVER_DIR = ../../../../source/gatery/scl/driver
HEADER += $(wildcard $(DRIVER_DIR)/**/*.h)

firmware.elf: Makefile $(SRCS) $(HEADER)
	$(rv32cc) -I$(DRIVER_DIR) $(CXXFLAGS) -o $@ $(SRCS)

