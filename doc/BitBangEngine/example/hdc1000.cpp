#include "BitBangEngineDriver.h"
#include <iostream>

int main()
{
	try {
		boost::asio::io_service io;
		boost::asio::serial_port serial = bitbang::device_open(io, "COM15"); // Change to your COM port device name (COM0, /dev/acm0, /dev/ttyUSB0, etc)
		bitbang::i2c::setup(serial);

		uint8_t deviceAddress = 0x40;

		// Example on how to write to the configuration register
		// enable 'low' precision temperature and humidity measurements
		//bitbang::i2c::register_set(serial, deviceAddress, 0x02, 0x1600, 2);

		std::cout << std::hex;
		std::cout << "Manufacturer ID: " 
			<< bitbang::i2c::register_get(serial, deviceAddress, 0xFE, 2)
			<< " (5449 expected)" << std::endl;
		std::cout << "      Device ID: " 
			<< bitbang::i2c::register_get(serial, deviceAddress, 0xFF, 2) 
			<< " (1000 expected)" << std::endl;
		std::cout << "      Serial ID: " 
			<< bitbang::i2c::register_get(serial, deviceAddress, 0xFB, 2) 
			<< bitbang::i2c::register_get(serial, deviceAddress, 0xFC, 2) 
			<< bitbang::i2c::register_get(serial, deviceAddress, 0xFD, 2) 
			<< std::endl;
		std::cout << "  Configuration: "
			<< bitbang::i2c::register_get(serial, deviceAddress, 0x02, 2) << std::endl;
		std::cout << '\n';

		while (true)
		{
			uint32_t reading = (uint32_t)bitbang::i2c::register_get(serial, deviceAddress, 0x00, 4, 32);
			double temperature = (reading >> 16) / double(1 << 16) * 165 - 40;
			double humidity = (reading & 0xFFFF) / double(1 << 16) * 100;
			std::cout << temperature << "C " << humidity << "%\r";
		}
	}
	catch (std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
	}
}
