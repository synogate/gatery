#include "BitBangEngineDriver.h"
#include <iostream>
#include <fstream>

#ifdef _WIN32
static std::string g_device_path = bitbang::find_device_path(0x1D50, 0x0000);
#else
static const char* g_device_path = "/dev/ttyACM0";
#endif

int main()
{
	try {
		boost::asio::io_service io;
		boost::asio::serial_port serial = bitbang::device_open(io, g_device_path);

		size_t spiMode = 0;
		bitbang::threewire::setup(serial, spiMode, 1'000'000);

		// lm71 is special in that it always sends 16 bits of data before we can send data to it on the shared MOSI/MISO line

		std::cout << std::hex;
		bitbang::threewire::read_then_write(serial, 16, 0xFFFF, 16, spiMode); // shutdown mode, to read device id
		std::cout << "Manufacturer/Device ID: " 
			<< bitbang::threewire::read_then_write(serial, 16, 0, 0, spiMode) 
			<< " (800f expected)\n\n";
		
		bitbang::threewire::read_then_write(serial, 16, 0x0000, 16, spiMode); // continuous reading mode to read temperature
		while (true)
		{
			uint16_t reading = (uint16_t)bitbang::threewire::read_then_write(serial, 16, 0, 0, spiMode);
			double temperature = (reading >> 2) * 0.03125;
			std::cout << temperature << "C     \r";
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	}
	catch (std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
	}
}
