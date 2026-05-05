#include "gen_mmap.h"

using device_uart_t = dev::uart_t;
#include <devices/uart.h>

static char stub_get() { return uart_getc(); }
static void stub_msg_put(char c) { uart_putc(c); }
#include <firmware/ministub.h>

void clearInstructionCache()
{
	unsigned cacheSize = 4096;
	unsigned* mem = (unsigned*)(8*1024*1024);
	for(unsigned* it = mem; it != mem + cacheSize/4; ++it)
		*it = 0x13; // NOP
	mem[cacheSize/4] = 0x8067; // RET

	void (*clearICache)() = (void (*)(void))mem;
	clearICache();
}

int main()
{
	// clear instruction cache in case we returned from an application and rewrite text memory
	clearInstructionCache();

	uart_puts("+$S05#b8");

	t_entry_point entry;
	unsigned x = 0xC0FFEEu;
	while(1)
	{
#if 0 // simple memory tester
		unsigned x2 = x;
		volatile unsigned* mem = (volatile unsigned*)(4);
		unsigned len = 1600*900/16;
		for(volatile unsigned* it = mem; it != mem + len; ++it)
		{
			x ^= x << 13;
			x ^= x >> 17;
			x ^= x << 5;		
			*it = x;
		}
		
		unsigned e = 0;
		for(volatile unsigned* it = mem; it != mem + len; ++it)
		{
			x2 ^= x2 << 13;
			x2 ^= x2 >> 17;
			x2 ^= x2 << 5;		
			if(*it != x2)
				e++;
		}
		uart_putc(e ? 'e' : '.');
#else
		stub_enter(&entry);
#endif
	}
}

inline void start_memory()
{
	volatile unsigned short* hyperReg = (volatile unsigned short*)(2<<11);
	dev::root().hyperram_register_access = 1;
#if 1 // octal interface (IS66WVO)
	hyperReg[0] = 0b1'001'000'0'0000'0'0'10; // latency 3, 52 Ohm, dynamic latency
#else // hyperram interface
	hyperReg[0] = 0xBFE0; // latency 3, 50 Ohm, dynamic latency
	hyperReg[1] = 0xFF81; // differential clock, full refresh
#endif
	dev::root().hyperram_register_access = 0;
}

inline void start_devices()
{
	uart().baudrate(115200 * 2);
}

#define START_STACK_POINTER (32*1024*1024)
#include <firmware/start.inl>
