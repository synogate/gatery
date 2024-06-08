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
#include "gatery/scl_pch.h"
#include "crc.h"

gtry::scl::CrcParams gtry::scl::CrcParams::init(CrcWellKnownParams standard)
{
	switch(standard)
	{
	case CrcWellKnownParams::CRC_5_USB: // verified
		return {
			.polynomial = "5b101",
			.initialRemainder = "5b11111",
			.reverseData = '1',
			.reverseCrc = '1',
			.xorOut = "5b11111",
		};
	case CrcWellKnownParams::CRC_16_CCITT:
		return {
			.polynomial = "16x1021",
			.initialRemainder = "16x1D0F",
			.reverseData = '0',
			.reverseCrc = '0',
			.xorOut = "16x",
		};
	case CrcWellKnownParams::CRC_16_USB: // verified
		return {
			.polynomial = "16x8005",
			.initialRemainder = "16xFFFF",
			.reverseData = '1',
			.reverseCrc = '1',
			.xorOut = "16xFFFF",
		};
	case CrcWellKnownParams::CRC_32:
		return {
			.polynomial = "32x04C11DB7",
			.initialRemainder = "32xFFFFFFFF",
			.reverseData = '1',
			.reverseCrc = '1',
			.xorOut = "32xFFFFFFFF",
		};
	case CrcWellKnownParams::CRC_32C:
		return {
			.polynomial = "32x1EDC6F41",
			.initialRemainder = "32xFFFFFFFF",
			.reverseData = '1',
			.reverseCrc = '1',
			.xorOut = "32xFFFFFFFF",
		};
	case CrcWellKnownParams::CRC_32D:
		return {
			.polynomial = "32xA833982B",
			.initialRemainder = "32xFFFFFFFF",
			.reverseData = '1',
			.reverseCrc = '1',
			.xorOut = "32xFFFFFFFF",
		};
	case CrcWellKnownParams::CRC_32Q:
		return {
			.polynomial = "32x814141AB",
			.initialRemainder = "32x0",
			.reverseData = '0',
			.reverseCrc = '0',
			.xorOut = "32x0",
		};
	default:
		HCL_DESIGNCHECK_HINT(false, "unkown crc standard");
		return {};
	}
}

gtry::UInt gtry::scl::crc(UInt remainder, UInt data, UInt polynomial)
{
	auto area = Area{ "crc" }.enter();
	HCL_NAMED(remainder);
	HCL_NAMED(data);
	HCL_NAMED(polynomial);

	UInt rem = ConstUInt(0, BitWidth{std::max(remainder.size(), data.size())});
	rem.upper(remainder.width()) = remainder;
	rem.upper(data.width()) ^= data;
	HCL_NAMED(rem);

	for(size_t i = 0; i < data.size(); ++i)
	{
		Bit sub = rem.msb();
		rem <<= 1;
		IF(sub)
			rem.upper(polynomial.width()) ^= polynomial;
	}

	if(rem.size() > remainder.size())
		return rem.upper(remainder.width());
	return rem;
}

void gtry::scl::CrcState::init()
{
	remainder = params.initialRemainder;
}

void gtry::scl::CrcState::update(UInt data)
{
	IF(params.reverseData)
		data = swapEndian(data, 1_b);

	remainder = crc(remainder, data, params.polynomial);
}

gtry::UInt gtry::scl::CrcState::checksum() const
{
	UInt res = remainder ^ params.xorOut;

	IF(params.reverseCrc)
		res = swapEndian(res, 1_b);

	return res;
}

namespace gtry::scl
{
	uint8_t simu_crc5_usb(uint16_t data, size_t bits)
	{
		uint8_t remainder = 0x1F;
		for (size_t b = 0; b < bits; ++b)
		{
			const uint8_t dataBit = (data >> b) & 1;
			remainder = (remainder >> 1) ^ ((remainder & 1) != dataBit ? 0b10100 : 0);
		}
		return remainder ^ 0x1F;
	}

	bool simu_crc5_usb_verify(uint16_t data)
	{
		return simu_crc5_usb(data, 16) == 0x19;
	}

	uint16_t simu_crc5_usb_generate(uint16_t data)
	{
		return data | (uint16_t(simu_crc5_usb(data, 11)) << 11);
	}
}
