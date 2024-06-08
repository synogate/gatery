/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2021 Michael Offel, Andreas Ley

	Gatery is free software; you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 3 of the License, or (at your option) any later version.

	Gatery is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public
	License along with this library; if not, write to the Free Software
	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include "scl/pch.h"
#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

#include <gatery/simulation/waveformFormats/VCDSink.h>
#include <gatery/export/vhdl/VHDLExport.h>

#include <gatery/scl/crc.h>

#define TEST_WITH_BOOST_CRC 0
#ifdef TEST_WITH_BOOST_CRC
#include <boost/crc.hpp>
#endif

using namespace boost::unit_test;
using namespace gtry;

template<typename T>
T crc_ref(T remainder, uint8_t data, T polynomial)
{
	remainder ^= (T)data << (sizeof(T) * 8 - 8);
	for(size_t i = 0; i < 8; ++i)
	{
		if(remainder & (1 << (sizeof(T) * 8 - 1)))
			remainder = (remainder << 1) ^ polynomial;
		else
			remainder <<= 1;
	}
	return remainder;
}

BOOST_FIXTURE_TEST_CASE(crc8, BoostUnitTestSimulationFixture)
{
	uint8_t ref = 0; 
	UInt rem = "8b"; 

	for(size_t i = 0; i < 9; ++i)
	{
		size_t data = '0' + i;
		ref = crc_ref<uint8_t>(ref, (uint8_t)data, 7);

		rem = scl::crc(rem, ConstUInt(data, 8_b), "8x07");
		sim_assert(rem == ref) << rem << " == " << ref;
	}

	design.postprocess();
	eval();
}

BOOST_FIXTURE_TEST_CASE(crc8split, BoostUnitTestSimulationFixture)
{
	uint8_t ref = 0;
	UInt rem = "8b";

	for(size_t i = 0; i < 9; ++i)
	{
		size_t data = '0' + i;
		ref = crc_ref<uint8_t>(ref, (uint8_t)data, 7);

		rem = scl::crc(rem, ConstUInt(data >> 4, 4_b), "8x07");
		rem = scl::crc(rem, ConstUInt(data & 0xF, 4_b), "8x07");
		sim_assert(rem == ref) << rem << " == " << ref;
	}

	design.postprocess();
	eval();
}

BOOST_FIXTURE_TEST_CASE(crc8multibyte, BoostUnitTestSimulationFixture)
{
	uint8_t ref = 0;
	UInt rem = "8b";

	for(size_t i = 0; i < 9; i += 2)
	{
		ref = crc_ref<uint8_t>(ref, (uint8_t)('0' + i * 2), 7);
		ref = crc_ref<uint8_t>(ref, (uint8_t)('1' + i * 2), 7);

		size_t data = (('0' + i * 2) << 8) | ('1' + i * 2);
		rem = scl::crc(rem, ConstUInt(data, 16_b), "8x07");
		sim_assert(rem == ref) << i << ": " << rem << " == " << (size_t)ref;
	}

	design.postprocess();
	eval();
}

BOOST_FIXTURE_TEST_CASE(crc16byte, BoostUnitTestSimulationFixture)
{
	uint16_t ref = 0;
	UInt rem = "16b";

	for(size_t i = 0; i < 9; i++)
	{
		ref = crc_ref<uint16_t>(ref, (uint8_t)('0' + i), 0x8005);

		size_t data = '0' + i;
		rem = scl::crc(rem, ConstUInt(data, 8_b), "16x8005");
		sim_assert(rem == ref) << i << ": " << rem << " == " << ref;
	}

	//dbg::WebSocksInterface::create(1337);
	//dbg::awaitDebugger();
	//dbg::stopInDebugger();

	design.postprocess();
	eval();
}

BOOST_FIXTURE_TEST_CASE(crc16state, BoostUnitTestSimulationFixture)
{
	for(auto testValue : {
		std::pair{0x000, 0x02},
		std::pair{0x547, 0x17},
		std::pair{0x2e5, 0x1C},
		std::pair{0x072, 0x13},
		std::pair{0x400, 0x16},
		})
	{
		scl::CrcState state{
			.params = scl::CrcParams::init(scl::CrcWellKnownParams::CRC_5_USB)
		};

		state.init();
		state.update(ConstUInt(testValue.first, 11_b));
		UInt crcValue = state.checksum();

		sim_assert(crcValue == testValue.second) << testValue.first << " should be " << testValue.second << " is " << crcValue;
	}

	design.postprocess();
	eval();
}

BOOST_FIXTURE_TEST_CASE(usb_crc5_testvector, BoostUnitTestSimulationFixture)
{
	const uint16_t data[] = {
		// |<crc>|< 11b data >|
		{ 0b11101'000'00000001 },
		{ 0b11101'111'00010101 },
		{ 0b00111'101'00111010 },
		{ 0b01110'010'01110000 },
	};

	for (size_t i = 0; i < sizeof(data) / sizeof(*data); ++i)
	{
#ifdef TEST_WITH_BOOST_CRC
		size_t crc = boost::crc<5, 5, 0x1F, 0x1F, true, true>(&data[i], 2);
		BOOST_TEST(crc == 0x19);
#endif

		BOOST_TEST(scl::simu_crc5_usb_verify(data[i]));
		BOOST_TEST(scl::simu_crc5_usb_generate(gtry::utils::bitfieldExtract(data[i], 0, 11)) == data[i]);
	}
}
