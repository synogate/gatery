#include "gen_mmap.h"
#include "logging.h"

void main()
{
	*dev_uart_baud = 115200 * 2;
	//*dev_uart_baud = 1000000;

	while(1)
	{
		unsigned len = getchar();
		unsigned done = 0;

	    *dev_spi_cs = 1;
		for(unsigned i = 0; i < len; ++i)
		{
			*dev_spi_put_payload = getchar();
			*dev_spi_put_valid = 1;

			if(*dev_spi_get_valid)
			{
				sendChar(*dev_spi_get_payload);
				*dev_spi_get_ready = 1;
				done++;
			}

			while(*dev_spi_put_valid);
		}

		for(; done < len; ++done)
		{
			while(!*dev_spi_get_valid);
			sendChar(*dev_spi_get_payload);
			*dev_spi_get_ready = 1;
		}
	    *dev_spi_cs = 0;
	}

	//while(1)
	//{
	//	sendChar(getchar());
	//}
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
