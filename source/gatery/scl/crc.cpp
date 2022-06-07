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
#include "gatery/pch.h"
#include "crc.h"

gtry::scl::CrcParams gtry::scl::CrcParams::init(CrcWellKnownParams standard)
{
	switch(standard)
	{
	case CrcWellKnownParams::CRC_5_USB:
		return {
			.order = 5,
			.polynomial = 0x5u,
			.initialRemainder = 0,
		};
	case CrcWellKnownParams::CRC_16_CCITT:
		return {
			.order = 16,
			.polynomial = 0x1021u,
			.initialRemainder = 0,
		};
	case CrcWellKnownParams::CRC_16_IBM:
		return {
			.order = 16,
			.polynomial = 0x8005u,
			.initialRemainder = 0,
		};
	case CrcWellKnownParams::CRC_32:
		return {
			.order = 32,
			.polynomial = 0x04C11DB7u,
			.initialRemainder = 0,
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
