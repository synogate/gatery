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
#pragma once
#include <gatery/frontend.h>

namespace gtry::scl
{
	UInt crc(UInt remainder, UInt data, UInt polynomial);


	enum class CrcWellKnownParams
	{
		CRC_5_USB,
		CRC_16_CCITT,
		CRC_16_USB,
		CRC_32,
		CRC_32C,
		CRC_32D,
		CRC_32Q,
	};

	struct CrcParams
	{
		UInt polynomial; // generator polynomial
		UInt initialRemainder; // value of remainder before data added
		Bit reverseData; // bit reverse in data
		Bit reverseCrc; // bit reverse out checksum
		UInt xorOut; // bit flip out checksum

		static CrcParams init(CrcWellKnownParams standard);
	};

	struct CrcState
	{
		CrcParams params;
		UInt remainder;

		void init();
		void update(UInt data);
		UInt checksum() const;
	};

	uint8_t simu_crc5_usb(uint16_t data, size_t bits);
	bool simu_crc5_usb_verify(uint16_t data);
	uint16_t simu_crc5_usb_generate(uint16_t data);
}
