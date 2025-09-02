#pragma once
#include "stream.h"
#include "device.h"
#include <cstdint>

struct UartDevice
{
protected:
	unsigned baud_;

public:
	StreamTxDevice<uint8_t> tx;
	StreamRxDevice<uint8_t> rx;

	void baudrate(uint32_t baud = 115'200) { baud_ = baud; }

};

inline constexpr UartDevice& uart() { return device<UartDevice, device_uart_t>(); }


static char uart_getc()
{
	while(!uart().rx.valid());
	char c = (char)uart().rx.payload();
	uart().rx.pop_unsafe();
	return c;
}

static void uart_putc(char c)
{
	uart().tx.push(c);
}

static void uart_puts(const char* msg)
{
	while (*msg)
		uart_putc(*msg++);
}

static char num2hex(char c)
{
	return ((c < 10) ? '0' : 'a' - 10) + c;
}

static void sendMsg(const char* msg)
{
	char hex[4] = { 0 };
	char checksum = 'O';

	uart_puts("+$O");
	while (*msg)
	{
		hex[0] = num2hex(*msg >> 4);
		hex[1] = num2hex(*msg & 0xF);
		uart_puts(hex);
		checksum += hex[0];
		checksum += hex[1];
		msg++;
	}
	uart_putc('#');
	hex[0] = num2hex(checksum >> 4);
	hex[1] = num2hex(checksum & 0xF);
	uart_puts(hex);
}

static void sendNumber(uint32_t v)
{
	char text[12];
	text[0] = '0';
	text[1] = 'x';

	for (int i = 0; i < 8; ++i)
		text[2 + (i ^ 0x7)] = num2hex(v >> i * 4 & 0xF);
	text[10] = ' ';
	text[11] = 0;
	sendMsg(text);
}
