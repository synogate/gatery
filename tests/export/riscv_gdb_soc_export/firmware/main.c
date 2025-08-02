#include "gen_mmap.h"
#include "logging.h"

static char stub_get()
{
	return getchar();
}

static void stub_msg_put(char c)
{
	sendChar(c);
}

#include "ministub.h"

static volatile unsigned short* dev_hyperram = (volatile unsigned short*)(2<<11);

void clearInstructionCache()
{
	unsigned* mem = (void*)(8*1024*1024);
	for(unsigned* it = mem; it != mem + 4096; ++it)
		*it = 0x13; // NOP
	mem[4096] = 0x8067; // RET

	void (*clearICache)() = (void*)mem;
	clearICache();
}

void main()
{
	while(1)
	{
		t_entry_point entry;
		stub_enter(&entry);
		clearInstructionCache();
	}
}

#pragma GCC push_options
#pragma GCC optimize ("O0")

__attribute__((naked)) void _start()
{
	asm volatile(
        ".option push;"
        ".option norelax;"
        "la gp, __global_pointer$;"
		"mv sp, %0;"
        ".option pop;"
		: : "r" (1 << 25)
		);

	*dev_hyperram_register_access = 1;
	dev_hyperram[0] = 0xBFE0; // latency 3, 50 Ohm, dynamic latency
	dev_hyperram[1] = 0xFF81; // differential clock, full refresh
	*dev_hyperram_register_access = 0;

	*dev_uart_baud = 115200 * 2;

	main();
	while (1);
}

#pragma GCC pop_options
