#include "gen_mmap.h"
#include "logging.h"


void main()
{
	*dev_uart_baud = 115200;
	sendMsg("Gurkensalat!\n");

	volatile unsigned char* mem8 = (void*)0x20000000;
	volatile unsigned short* mem16 = (void*)mem8;
	volatile unsigned int* mem32 = (void*)mem8;

#if 1
	// setup HyperRAM 
	*dev_registerAccess = 1;
	mem16[(1<<11)+0] = 0xBFE0; // latency 3, 50 Ohm, dynamic latency
	mem16[(1<<11)+1] = 0xFF81; // differential clock, full refresh
	*dev_latency = 3;

	if(mem16[0] != 0xE86)
		sendMsg("chip id check failed\n");
	
	*dev_registerAccess = 0;
#endif

	// single word write and read
	mem16[0] = 0xA55A;
	if(mem16[0] != 0xA55A)
		sendMsg("single word test failed");

	// burst write
	mem32[0] = 0x5555AAAA;
	if(mem32[0] != 0x5555AAAA)
		sendMsg("burst test failed");

	// byte mask
	mem8[0] = 0x11;
	mem8[1] = 0x22;
	mem8[2] = 0x33;
	mem8[3] = 0x44;
	if(mem32[0] != 0x44332211)
		sendMsg("byte mask test failed");

	// write and read full memory
	const unsigned mem_size = 32*1024*1024;
	const unsigned magic = 0xAA550FF0;
	
test_start:
	for(unsigned i = 0; i < mem_size/4; ++i)
	{
		mem32[i] = (i ^ magic);
	}

	while(1)
	{
		for(unsigned i = 0; i < mem_size/4; ++i)
		{
			if(mem32[i] != (i ^ magic))
			{
				sendNumber(i);
				sendChar('\n');
				goto test_start;
			}
		}
	}
	
	sendMsg("\nEND\n");
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
		: : "r" (4096)
		);

	main();
	while (1);
}

#pragma GCC pop_options
