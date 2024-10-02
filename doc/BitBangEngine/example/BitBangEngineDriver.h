#pragma once
#include <boost/asio.hpp>
#include <optional>
#include <bitset>
#include <span>
#include <iostream>

namespace bitbang
{
	inline uint8_t CLK  = 0b0001;
	inline uint8_t DOUT = 0b0010;
	inline uint8_t DIN  = 0b0100;
	inline uint8_t CS   = 0b1000;

	enum class PinGroup {
		low = 0,
		high = 1,
	};

	enum class SignalLevel {
		low = 0,
		high = 1,
	};

	enum class ClockEdge {
		rising = 0,
		falling = 1,
	};

	inline std::ostream& operator<<(std::ostream& os, std::span<const uint8_t> buffer)
	{
		for (uint8_t value : buffer)
			os << std::format("0x{:02X}, ", value);
		return os;
	}

	struct TransferSettings {
		bool read = false;
		bool write = false;
		ClockEdge clockEdgeSetup = ClockEdge::falling;
		ClockEdge clockEdgeCapture = ClockEdge::rising;
		bool bitMode = false;
		bool lsbFirst = false;
		bool tmsMode = false;

		static TransferSettings i2c(bool bitMode = true) {
			return {
				.read = true,
				.write = true,
				.bitMode = bitMode,
			};
		}
	};

	template<typename Container>
	void transfer(Container& cmdBuf, TransferSettings config, size_t length)
	{
		if (length == 0)
			return;
		length--;

		uint8_t cmd = 0;
		if (config.clockEdgeSetup == ClockEdge::falling)
			cmd |= 0x01;
		if (config.bitMode)
			cmd |= 0x02;
		if (config.clockEdgeCapture == ClockEdge::falling)
			cmd |= 0x04;
		if (config.lsbFirst)
			cmd |= 0x08;
		if (config.write)
			cmd |= 0x10;
		if (config.read)
			cmd |= 0x20;
		if (config.tmsMode)
			cmd |= 0x40;

		cmdBuf.insert(cmdBuf.end(), cmd);
		cmdBuf.insert(cmdBuf.end(), uint8_t(length));
		if (!config.bitMode)
			cmdBuf.insert(cmdBuf.end(), uint8_t(length >> 8));
	}

	template<typename Container>
	void set_pin(Container& cmdBuf, uint8_t value, uint8_t direction, PinGroup pinGroup = PinGroup::low)
	{
		cmdBuf.insert(cmdBuf.end(), 0x80 | uint8_t(pinGroup) * 2);
		cmdBuf.insert(cmdBuf.end(), value);
		cmdBuf.insert(cmdBuf.end(), direction);
	}

	template<typename Container>
	void set_pin_fast(Container& cmdBuf, std::bitset<4> value)
	{
		cmdBuf.insert(cmdBuf.end(), uint8_t(0xC0 | value.to_ulong()));
	}

	template<typename Container>
	void set_open_drain(Container& cmdBuf, uint16_t openDrain)
	{
		cmdBuf.insert(cmdBuf.end(), 0x9E);
		cmdBuf.insert(cmdBuf.end(), uint8_t(openDrain));
		cmdBuf.insert(cmdBuf.end(), uint8_t(openDrain >> 8));
	}

	template<typename Container>
	void set_three_phase_clocking(Container& cmdBuf, bool enable)
	{
		cmdBuf.insert(cmdBuf.end(), enable ? 0x8C : 0x8D);
	}

	template<typename Container>
	void set_external_loopback(Container& cmdBuf, bool enable)
	{
		cmdBuf.insert(cmdBuf.end(), enable ? 0x84 : 0x85);
	}

	template<typename Container>
	void set_clock_divider(Container& cmdBuf, uint16_t divider)
	{
		cmdBuf.insert(cmdBuf.end(), 0x86);
		cmdBuf.insert(cmdBuf.end(), uint8_t(divider));
		cmdBuf.insert(cmdBuf.end(), uint8_t(divider >> 8));
	}

	template<typename Container>
	void set_clock_frequency(Container& cmdBuf, size_t frequency)
	{
		size_t divider = 6'000'000 / frequency - 1;
		if (divider > 0xFFFF)
			throw std::runtime_error("Frequency too low");

		set_clock_divider(cmdBuf, uint16_t(divider));
	}

	template<typename Container>
	void wait_for(Container& cmdBuf, SignalLevel level)
	{
		cmdBuf.insert(cmdBuf.end(), level == SignalLevel::low ? 0x89 : 0x88);
	}

	template<typename Container>
	void wait_for_with_clock(Container& cmdBuf, SignalLevel level, std::optional<size_t> timeoutInClockCycles)
	{
		if (timeoutInClockCycles)
		{
			cmdBuf.insert(cmdBuf.end(), level == SignalLevel::low ? 0x9D : 0x9C);

			size_t timeoutValue = *timeoutInClockCycles + 7 / 8 - 1;
			cmdBuf.insert(cmdBuf.end(), uint8_t(timeoutValue));
			cmdBuf.insert(cmdBuf.end(), uint8_t(timeoutValue >> 8));
		}
		else
		{
			cmdBuf.insert(cmdBuf.end(), level == SignalLevel::low ? 0x95 : 0x94);
		}
	}

	template<typename Container>
	void invalid_command(Container& cmdBuf)
	{
		cmdBuf.insert(cmdBuf.end(), 0xAA);
	}

	template<typename Container>
	void reset(Container& cmdBuf)
	{
		set_pin(cmdBuf, 0, 0, PinGroup::low);
		set_pin(cmdBuf, 0, 0, PinGroup::high);
		set_open_drain(cmdBuf, 0);
		set_three_phase_clocking(cmdBuf, false);
		set_external_loopback(cmdBuf, false);
		set_clock_divider(cmdBuf, 0);
	}

	void device_send_command(boost::asio::serial_port& serial, std::function<void(std::vector<uint8_t>&)> generator)
	{
		std::vector<uint8_t> cmdBuf;
		generator(cmdBuf);
		write(serial, boost::asio::buffer(cmdBuf));
	}

	void device_ping(boost::asio::serial_port& serial)
	{
		char badCommand[] = { (char)0xAA };
		serial.write_some(boost::asio::buffer(badCommand));

		size_t result = 0;
		serial.read_some(boost::asio::buffer(&result, sizeof(result)));

		if(result != 0xAAFA)
			throw std::runtime_error("Device did not respond to ping. Wrong device selected?");
	}

	// flush device command buffer by sending 0xAA (bad command) until the device responds with 0xFAAA. 
	// Then send 0xAB (other bad command) once and wait for 0xFAAB to flush all the extra 0xAA commands.
	void device_flush(boost::asio::io_context& ctx, boost::asio::serial_port& serial)
	{
		bool stop_flush = false;

		std::function<void()> writeBadCommand = [&]() {
			boost::asio::async_write(serial, boost::asio::buffer("\xAA", 1),
				[&](const boost::system::error_code& ec, size_t N) {
					if (ec)
						throw boost::system::system_error(ec);
					if (!stop_flush)
						writeBadCommand();
					else
						boost::asio::async_write(serial, boost::asio::buffer("\xAB", 1), 
							[&](const boost::system::error_code& ec, size_t N) {}
						);
				}
			);
		};
		writeBadCommand();

		boost::asio::streambuf inbuf;
		boost::asio::async_read_until(serial, inbuf, "\xFA\xAA", 
			[&](const boost::system::error_code& ec, size_t N) {
				if (ec)
					throw boost::system::system_error(ec);
				stop_flush = true;

				boost::asio::async_read_until(serial, inbuf, "\xFA\xAB",
					[&](const boost::system::error_code& ec, size_t N) {
						if (ec)
							throw boost::system::system_error(ec);
					}
				);
			}
		);

		ctx.run();
	}

	inline boost::asio::serial_port device_open(boost::asio::io_service& ctx, std::string devicePath)
	{
		boost::asio::serial_port serial{ ctx, devicePath };

		// set magic baud rate and parity to signal to the device that it should enter bitbang mode
		// otherwise it is in simple uart mode
		serial.set_option(boost::asio::serial_port::baud_rate(57600));
		serial.set_option(boost::asio::serial_port::parity(boost::asio::serial_port::parity::even));

		// make sure command fifo is empty
		device_flush(ctx, serial);
		return serial;
	}

#ifdef _WIN32
	std::string find_device_path(uint16_t vid, uint16_t pid)
	{
		std::string name;
		std::string regPath = std::format("SYSTEM\\CurrentControlSet\\Enum\\USB\\VID_{:04X}&PID_{:04X}", vid, pid);

		HKEY hUSB;
		if (RegOpenKeyA(HKEY_LOCAL_MACHINE, regPath.c_str(), &hUSB) != ERROR_SUCCESS)
			return "COM1";

		char subkeyName[256];
		DWORD index = 0;
		while (RegEnumKeyA(hUSB, index++, subkeyName, sizeof(subkeyName)) == ERROR_SUCCESS)
		{
			std::string regSubPath = std::string(subkeyName) + "\\Device Parameters";
			DWORD len = sizeof(subkeyName);
			if (RegGetValueA(hUSB, regSubPath.c_str(), "PortName", RRF_RT_REG_SZ, NULL, subkeyName, &len) == ERROR_SUCCESS)
			{
				try {
					boost::asio::io_service io;
					boost::asio::serial_port serial(io, subkeyName);
				}
				catch (const boost::system::system_error& e) {
					if (e.code().value() != 2) // keep if not file not found to improve error message for user
						name = subkeyName;
					continue;
				}
				name = subkeyName;
				break;
			}
		}

		RegCloseKey(hUSB);
		if (name.empty())
		{
			std::cerr << "no device found\n";
			return "COM1";
		}
		return name;
	}
#endif

}

namespace bitbang::spi
{
	template<typename Container>
	void setup(Container& cmdBuf, size_t spiMode, size_t busClock = 6'000'000)
	{
		reset(cmdBuf);
		// setup pin directions out for CSn, MOSI and SCK
		// setup initial logic levels according to spi mode
		set_pin(cmdBuf, CS | (spiMode / 2 ? CLK : 0), CS | DOUT | CLK);
		set_clock_frequency(cmdBuf, busClock);
	}

	bool setup(boost::asio::serial_port& serial, size_t spiMode, size_t busClock = 6'000'000)
	{
		std::vector<uint8_t> cmdBuf;
		setup(cmdBuf, spiMode, busClock);
		return write(serial, boost::asio::buffer(cmdBuf)) == cmdBuf.size();
	}

	inline uint8_t mode2cmd(size_t spiMode)
	{
		switch (spiMode)
		{
		case 0: return 0x1;
		case 1: return 0x4;
		case 2: return 0x4;
		case 3: return 0x1;
		default:
			throw std::runtime_error("Invalid SPI mode");
		}
	}

	template<typename Container>
	void send_bytes(Container& cmdBuf, uint64_t data, size_t bitLength, size_t spiMode = 0, bool receive = false)
	{
		// Serial Engine instruction byte
		cmdBuf.insert(cmdBuf.end(), 
			mode2cmd(spiMode) // clock edge
			| 0x02 // bit mode
			| 0x10 // write
			| (receive ? 0x20 : 0) // read
		);
		// Number of bits to transfer
		cmdBuf.insert(cmdBuf.end(), uint8_t(bitLength - 1));

		// Data
		size_t numBytes = (bitLength + 7) / 8;
		for (size_t i = 0; i < numBytes; ++i)
			cmdBuf.insert(cmdBuf.end(), uint8_t(data >> ((numBytes - 1 - i) * 8)));
	}

	template<typename Container>
	void transfer_command(Container& cmdBuf, uint64_t data, size_t bitLength, size_t spiMode = 0, bool send = true, bool receive = true)
	{
		if (bitLength == 0)
			return;

		uint8_t direction = (send ? 0x10 : 0) | (receive ? 0x20 : 0);
		
		if (bitLength <= 256)
		{
			// Serial Engine instruction byte
			cmdBuf.insert(cmdBuf.end(),
				mode2cmd(spiMode) // clock edge
				| 0x02 // bit mode
				| direction
			);
			// Number of bits to transfer
			cmdBuf.insert(cmdBuf.end(), uint8_t(bitLength - 1));

			// Append send data
			if (send)
			{
				size_t numBytes = (bitLength + 7) / 8;
				for (size_t i = 0; i < numBytes; ++i)
					cmdBuf.insert(cmdBuf.end(), uint8_t(data >> ((numBytes - 1 - i) * 8)));
			}
		}
		else
		{
			// Serial Engine instruction byte
			cmdBuf.insert(cmdBuf.end(),
				mode2cmd(spiMode) // clock edge
				| direction
			);
			// Number of bytes to transfer
			size_t byteValue = bitLength / 8 - 1;
			if (byteValue > 0xFFFF)
				throw std::runtime_error("Too many bytes to transfer");
			cmdBuf.insert(cmdBuf.end(), uint8_t(byteValue));
			cmdBuf.insert(cmdBuf.end(), uint8_t(byteValue >> 8));

			// Append full bytes of send data
			if (send)
			{
				size_t numBytes = bitLength / 8;
				for (size_t i = 0; i < numBytes; ++i)
					cmdBuf.insert(cmdBuf.end(), uint8_t(data >> ((numBytes - 1 - i) * 8)));
			}

			transfer_command(cmdBuf, data, bitLength % 8, spiMode, send, receive);
		}
	}

	template<typename Container>
	void start(Container& cmdBuf, size_t spiMode = 0)
	{
		uint8_t idleState = (spiMode / 2 ? CLK : 0);
		set_pin_fast(cmdBuf, idleState); // chip select
	}

	template<typename Container>
	void stop(Container& cmdBuf, size_t spiMode = 0)
	{
		uint8_t idleState = (spiMode / 2 ? CLK : 0);
		set_pin_fast(cmdBuf, idleState | CS); // chip deselect
	}

	template<typename Container, typename TData>
	void send(Container& cmdBuf, TData data, size_t spiMode = 0)
	{
		start(cmdBuf, spiMode);
		send_bytes(cmdBuf, data, spiMode);
		stop(cmdBuf, spiMode);
	}

	struct TransferParameter {
		size_t spiMode = 0;
		std::optional<uint8_t> commandByte; // is sent before the data and does not return data
		bool receive = true; // if true, the data is read back and returned
	};

	inline uint64_t transfer(boost::asio::serial_port& serial, uint64_t data, size_t bitLength, TransferParameter param = {})
	{
		std::vector<uint8_t> cmdBuf;

		start(cmdBuf, param.spiMode);
		if(param.commandByte)
		{
			transfer_command(cmdBuf, *param.commandByte, 8, param.spiMode, *param.commandByte != 0, false);
			if (data == 0 && *param.commandByte != 0) // restore data line to low level
				start(cmdBuf, param.spiMode);
		}
		transfer_command(cmdBuf, data, bitLength, param.spiMode, data != 0, param.receive);
		stop(cmdBuf, param.spiMode);
		write(serial, boost::asio::buffer(cmdBuf));

		uint64_t result = 0;
		if (param.receive)
		{
			size_t numBytes = (bitLength + 7) / 8;
			std::vector<uint8_t> inbuf(numBytes);
			read(serial, boost::asio::buffer(inbuf));

			for (size_t i = 0; i < numBytes; ++i)
				result |= uint64_t(inbuf[i]) << ((numBytes - 1 - i) * 8);
		}
		return result;
	}

	inline void write(boost::asio::serial_port& serial, uint8_t command, uint64_t data, size_t bitLength, size_t spiMode = 0)
	{
		spi::transfer(serial, data, bitLength, { .spiMode = spiMode, .commandByte = command, .receive = false });
	}

	inline uint64_t read(boost::asio::serial_port& serial, uint8_t command, size_t bitLength, size_t spiMode = 0)
	{
		return spi::transfer(serial, 0, bitLength, { .spiMode = spiMode, .commandByte = command });
	}

}

namespace bitbang::threewire
{
	template<typename Container>
	void setup(Container& cmdBuf, size_t spiMode, size_t busClock = 6'000'000)
	{
		spi::setup(cmdBuf, spiMode, busClock);

		// MISO and MOSI are shared in this configuration. so make sure to not drive the MOSI line during reads.
		// read data from MOSI instead of MISO
		set_external_loopback(cmdBuf, true);
	}

	bool setup(boost::asio::serial_port& serial, size_t spiMode, size_t busClock = 6'000'000)
	{
		std::vector<uint8_t> cmdBuf;
		setup(cmdBuf, spiMode, busClock);
		return write(serial, boost::asio::buffer(cmdBuf)) == cmdBuf.size();
	}

	using spi::start;
	using spi::stop;
	using spi::write;

	inline uint64_t read(boost::asio::serial_port& serial, uint8_t command, size_t bitLength, size_t spiMode = 0)
	{
		std::vector<uint8_t> cmdBuf;

		uint8_t idleState = (spiMode / 2 ? CLK : 0);
		set_pin_fast(cmdBuf, idleState); // chip select
		spi::transfer_command(cmdBuf, command, 8, spiMode, true, false);

		set_pin(cmdBuf, idleState, CS | CLK); // set MOSI to input
		spi::transfer_command(cmdBuf, 0, bitLength, spiMode, false, true);
		set_pin(cmdBuf, idleState | CS, CS | CLK | DOUT); // reset MOSI to output
		write(serial, boost::asio::buffer(cmdBuf));

		uint64_t result = 0;
		size_t numBytes = (bitLength + 7) / 8;
		std::vector<uint8_t> inbuf(numBytes);
		read(serial, boost::asio::buffer(inbuf));

		for (size_t i = 0; i < numBytes; ++i)
			result |= uint64_t(inbuf[i]) << ((numBytes - 1 - i) * 8);
		return result;
	}

	inline uint64_t read_then_write(boost::asio::serial_port& serial, size_t bitLengthRead, uint64_t data, size_t bitLengthWrite, size_t spiMode = 0)
	{
		std::vector<uint8_t> cmdBuf;

		uint8_t idleState = (spiMode / 2 ? CLK : 0);
		set_pin(cmdBuf, idleState, CS | CLK); // set MOSI to input
		set_pin_fast(cmdBuf, idleState); // chip select
		spi::transfer_command(cmdBuf, 0, bitLengthRead, spiMode, false, true);
		set_pin(cmdBuf, idleState, CS | CLK | DOUT); // reset MOSI to output
		spi::transfer_command(cmdBuf, data, bitLengthWrite, spiMode, true, false);
		set_pin_fast(cmdBuf, idleState | CS); // chip deselect
		write(serial, boost::asio::buffer(cmdBuf));

		uint64_t result = 0;
		size_t numBytes = (bitLengthRead + 7) / 8;
		std::vector<uint8_t> inbuf(numBytes);
		read(serial, boost::asio::buffer(inbuf));

		for (size_t i = 0; i < numBytes; ++i)
			result |= uint64_t(inbuf[i]) << ((numBytes - 1 - i) * 8);
		return result;
	}
}

namespace bitbang::i2c
{
	enum class Access {
		write = 0,
		read = 1,
	};

	struct no_ack_received : std::runtime_error
	{
		no_ack_received() : std::runtime_error("no ACK received") {}
	};

	struct arbitration_lost : std::runtime_error
	{
		arbitration_lost() : std::runtime_error("arbitration lost") {}
	};

	struct not_ready : std::runtime_error
	{
		not_ready() : std::runtime_error("device is busy") {}
	};

	template<typename Container>
	void setup(Container& cmdBuf, size_t busClock = 100'000)
	{
		reset(cmdBuf);
		set_open_drain(cmdBuf, CLK | DOUT);
		set_pin(cmdBuf, CLK | DOUT, CLK | DOUT);
		set_three_phase_clocking(cmdBuf, true);
		set_external_loopback(cmdBuf, true);
		set_clock_frequency(cmdBuf, busClock * 3 / 2); // +50% for three phase clocking
	}

	bool setup(boost::asio::serial_port& serial, size_t busClock = 100'000)
	{
		std::vector<uint8_t> cmdBuf;
		setup(cmdBuf, busClock);
		return write(serial, boost::asio::buffer(cmdBuf)) == cmdBuf.size();
	}

	template<typename Container>
	void start(Container& cmdBuf)
	{
		set_pin_fast(cmdBuf, CLK); // SDA low
		set_pin_fast(cmdBuf, 0); // SDC low
	}

	template<typename Container>
	void stop(Container& cmdBuf)
	{
		set_pin_fast(cmdBuf, 0); // make sure to be in active idle state
		set_pin_fast(cmdBuf, DOUT); // SDA high
		set_pin_fast(cmdBuf, DOUT | CLK); // SDC high
	}

	void send_byte_checked(boost::asio::serial_port& serial, uint8_t value)
	{
		std::vector<uint8_t> cmdBuf;
		transfer(cmdBuf, TransferSettings::i2c(), 9);
		cmdBuf.insert(cmdBuf.end(), value);
		cmdBuf.insert(cmdBuf.end(), 0x80); // handshake
		write(serial, boost::asio::buffer(cmdBuf));

		uint8_t result[2] = {};
		boost::asio::read(serial, boost::asio::buffer(result));
		if (result[0] != value)
			throw arbitration_lost{};
		if (result[1] != 0)
			throw no_ack_received{};
	}

	void send_address(boost::asio::serial_port& serial, uint8_t address, Access access)
	{
		send_byte_checked(serial, address << 1 | (access == Access::read ? 1 : 0));
	}

	uint8_t receive_byte(boost::asio::serial_port& serial, bool ack)
	{
		std::vector<uint8_t> cmdBuf;
		transfer(cmdBuf, TransferSettings::i2c(), 9);
		cmdBuf.insert(cmdBuf.end(), 0xFF);
		cmdBuf.insert(cmdBuf.end(), ack ? 0x00 : 0x80);
		write(serial, boost::asio::buffer(cmdBuf));

		uint8_t inbuf[2] = {};
		read(serial, boost::asio::buffer(inbuf));

		return inbuf[0];
	}

	class frame
	{
	public:
		frame(boost::asio::serial_port& serial) : serial(serial) {
			device_send_command(serial, start<std::vector<uint8_t>>);
		}

		frame(boost::asio::serial_port& serial, uint8_t address, Access access) : frame(serial) {
			send_address(serial, address, access);
		}

		~frame() {
			device_send_command(serial, stop<std::vector<uint8_t>>);
		}

	private:
		boost::asio::serial_port& serial;
	};

	inline uint64_t register_get(boost::asio::serial_port& serial, uint8_t address, uint8_t regIndex, size_t numBytes, size_t retryReadCount = 0)
	{
		{
			frame frame(serial, address, Access::write);
			send_byte_checked(serial, regIndex);
		}

		size_t retryCounter = 0;
		while(true)
		{
			try {
				frame frame(serial, address, Access::read);

				uint64_t result = 0;
				for(size_t i = 0; i < numBytes; ++i)
				{
					bool lastByte = i + 1 == numBytes;
					uint64_t value = receive_byte(serial, !lastByte);
					result |= value << ((numBytes - 1 - i) * 8);
				}
				return result;
			}
			catch (const no_ack_received&) {
				if(retryCounter++ == retryReadCount)
					throw not_ready{};
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}
	}

	inline void register_set(boost::asio::serial_port& serial, uint8_t address, uint8_t regIndex, uint64_t value, size_t numBytes)
	{
		frame frame(serial, address, Access::write);
		send_byte_checked(serial, regIndex);
		for(size_t i = 0; i < numBytes; ++i)
			send_byte_checked(serial, uint8_t(value >> ((numBytes - 1 - i) * 8)));
	}
}
