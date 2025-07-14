#include "logging.h"
#include "gen_mmap.h"

void sendChar(char c)
{
	while(*dev_uart_io_tx_valid);
	*dev_uart_io_tx_payload = c;
	*dev_uart_io_tx_valid = 1;
}

int getchar()
{
	while(!*dev_uart_io_rx_valid);
	int r = *dev_uart_io_rx_payload;
	*dev_uart_io_rx_ready = 1;
	return r;
}

void sendMsg(const char* msg)
{
	while(*msg)
        sendChar(*msg++);
}

static char tohex(char n)
{
	if(n < 10)
		return '0' + n;
	else
		return 'A' - 10 + n;
}

void sendNumber(unsigned n)
{
    for(int i = 28; i >= 0; i -= 4)
        sendChar(tohex((n >> i) & 0xF));
}

void sendByteHex(unsigned char c)
{
	sendChar(tohex(c >> 4));
	sendChar(tohex(c & 0xF));
}
