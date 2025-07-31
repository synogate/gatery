#include "gen_mmap.h"

static void uart_put(char c)
{
	while(*dev_uart_io_tx_valid);
	*dev_uart_io_tx_payload = c;
	*dev_uart_io_tx_valid = 1;
}

static void uart_puts(const char* msg)
{
	while(*msg)
		uart_put(*msg++);
}

static char num2hex(char c)
{
    return ((c < 10) ? '0' : 'a' - 10) + c;
}

static void sendMsg(const char* msg)
{
	char hex[4] = {0};
	char checksum = 'O';

	uart_puts("+$O");
	while(*msg)
	{
		hex[0] = num2hex(*msg >> 4);
		hex[1] = num2hex(*msg & 0xF);
		uart_puts(hex);
		checksum += hex[0];
		checksum += hex[1];
		msg++;
	}
	uart_put('#');
	hex[0] = num2hex(checksum >> 4);
	hex[1] = num2hex(checksum & 0xF);
	uart_puts(hex);
}


void main()
{
	for(int i = 0; i < 16; ++i)
		sendMsg("Hello World!\n");
	return;
}

#pragma GCC push_options
#pragma GCC optimize ("O0")

void _start()
{
	asm volatile(
        ".option push;"
        ".option norelax;"
		"nop;"
        "la gp, __global_pointer$;"
        ".option pop;"
		: : 
		);

	main();
}

#pragma GCC pop_options
