#include <iostream>
#include "BitBangEngineDriver.h"
#include <numeric>

#ifdef _WIN32
static std::string g_device_path = bitbang::find_device_path(0x1D50, 0x0000);
#else
static const char* g_device_path = "/dev/ttyACM0";
#endif

//                                    TEMP_CFG_REG|CTRLREG1| 2      | 3      | 4      | 5      | 6      | REFCAP
static const uint64_t lis2dh13_config = 0b11000000'11100111'00000000'00000000'10000000'00000000'00000000'00000000;

#define WHO_AM_I 0x0F
#define OUT_TEMP_L 0x0C
#define OUT_X_L 0x28
#define OUT_Y_L 0x2A
#define OUT_Z_L 0x2C
#define I2C_AUTO_INCREMENT 0x80

void lis2dh13_i2c(boost::asio::serial_port& serial)
{
	std::cout << "LIS2DH12 I2C\n";
	bitbang::i2c::setup(serial);

	uint8_t deviceAddress = 0b0011001;
	bitbang::i2c::register_set(serial, deviceAddress, I2C_AUTO_INCREMENT | 0x1F, lis2dh13_config, 8);

	std::cout << std::hex;
	std::cout << "Who am I: "
		<< bitbang::i2c::register_get(serial, deviceAddress, WHO_AM_I, 1) << " (33 expected)" << std::endl;
	std::cout << "Configuration: "
		<< bitbang::i2c::register_get(serial, deviceAddress, I2C_AUTO_INCREMENT | 0x1F, 8) << std::endl;
	std::cout << '\n';

	while (true)
	{
		int8_t temperature = int8_t(bitbang::i2c::register_get(serial, deviceAddress, I2C_AUTO_INCREMENT | OUT_TEMP_L, 2));
		std::cout << std::dec << (int)temperature << "C ";

		std::array<double,3> acceleration;
		for (uint8_t i = 0; i < acceleration.size(); ++i)
		{
			uint16_t reading = (uint16_t)bitbang::i2c::register_get(serial, deviceAddress, I2C_AUTO_INCREMENT | OUT_X_L + i * 2, 2);
			acceleration[i] = int16_t(reading << 8 | reading >> 8) / double(1 << 16) * 4;
			std::cout << char('X' + i) << std::format("={:+05.3f}g ", acceleration[i]);
		}

		std::cout << " length: " << std::inner_product(acceleration.begin(), acceleration.end(), acceleration.begin(), 0.);
		std::cout << '\r' << std::flush;

		std::this_thread::sleep_for(std::chrono::milliseconds(200));
	}
}

#define SPI_READ 0x80
#define SPI_AUTO_INCREMENT 0x40

void lis2dh13_spi(boost::asio::serial_port& serial)
{
	std::cout << "LIS2DH12 SPI\n";

	size_t spiMode = 3;
	bitbang::spi::setup(serial, spiMode, 6'000'000 / 2);
	bitbang::spi::write(serial, SPI_AUTO_INCREMENT | 0x1F, lis2dh13_config, 64, spiMode);

	std::cout << std::hex;
	std::cout << "Who am I: "
		<< bitbang::spi::read(serial, SPI_READ | WHO_AM_I, 8, spiMode) << " (33 expected)" << std::endl;
	std::cout << "Configuration: "
		<< bitbang::spi::read(serial, SPI_READ | SPI_AUTO_INCREMENT | 0x1F, 64, spiMode) << std::endl;
	std::cout << '\n';

	while (true)
	{
		int8_t temperature = int8_t(bitbang::spi::read(serial, SPI_READ | SPI_AUTO_INCREMENT | OUT_TEMP_L, 16, spiMode));
		std::cout << std::dec << (int)temperature << "C ";

		std::array<double, 3> acceleration;
		for (uint8_t i = 0; i < acceleration.size(); ++i)
		{
			uint16_t reading = (uint16_t)bitbang::spi::read(serial, SPI_READ | SPI_AUTO_INCREMENT | OUT_X_L + i * 2, 16, spiMode);
			acceleration[i] = int16_t(reading << 8 | reading >> 8) / double(1 << 16) * 4;
			std::cout << char('X' + i) << std::format("={:+05.3f}g ", acceleration[i]);
		}

		std::cout << " length: " << std::inner_product(acceleration.begin(), acceleration.end(), acceleration.begin(), 0.);
		std::cout << '\r' << std::flush;

		std::this_thread::sleep_for(std::chrono::milliseconds(200));
	}
}

void lis2dh13_threewire(boost::asio::serial_port& serial)
{
	std::cout << "LIS2DH12 3-wire\n";

	size_t spiMode = 3;
	bitbang::threewire::setup(serial, spiMode, 6'000'000 / 2);
	bitbang::threewire::write(serial, SPI_AUTO_INCREMENT | 0x1F, lis2dh13_config | (1ull << 24), 64, spiMode);

	std::cout << std::hex;
	std::cout << "Who am I: "
		<< bitbang::threewire::read(serial, SPI_READ | WHO_AM_I, 8, spiMode) << " (33 expected)" << std::endl;
	std::cout << "Configuration: "
		<< bitbang::threewire::read(serial, SPI_READ | SPI_AUTO_INCREMENT | 0x1F, 64, spiMode) << std::endl;
	std::cout << '\n';

	while (true)
	{
		int8_t temperature = int8_t(bitbang::threewire::read(serial, SPI_READ | SPI_AUTO_INCREMENT | OUT_TEMP_L, 16, spiMode));
		std::cout << std::dec << (int)temperature << "C ";

		std::array<double, 3> acceleration;
		for (uint8_t i = 0; i < acceleration.size(); ++i)
		{
			uint16_t reading = (uint16_t)bitbang::threewire::read(serial, SPI_READ | SPI_AUTO_INCREMENT | OUT_X_L + i * 2, 16, spiMode);
			acceleration[i] = int16_t(reading << 8 | reading >> 8) / double(1 << 16) * 4;
			std::cout << char('X' + i) << std::format("={:+05.3f}g ", acceleration[i]);
		}

		std::cout << " length: " << std::inner_product(acceleration.begin(), acceleration.end(), acceleration.begin(), 0.);
		std::cout << '\r' << std::flush;

		std::this_thread::sleep_for(std::chrono::milliseconds(200));
	}
}

int main()
{
	try {
		boost::asio::io_service io;
		boost::asio::serial_port serial = bitbang::device_open(io, g_device_path);

		//lis2dh13_i2c(serial);
		lis2dh13_spi(serial);
		//lis2dh13_threewire(serial);
	}
	catch (std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
	}
}
