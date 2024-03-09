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
#include "../ShiftReg.h"
#include "../stream/Packet.h"
#include "../tilelink/tilelink.h"

namespace gtry::scl::sdram
{
	struct AddressMap
	{
		Selection column;
		Selection row;
		Selection bank;
		Selection bankGroup;
	};

	struct CommandBus
	{
		Bit cke;	// Clock Enable
		Bit csn;	// Chip Select N
		Bit rasn;	// Row Access Strobe N
		Bit casn;	// Column Access Strobe N
		Bit wen;	// Write Enable N
		BVec a;		// Address
		BVec ba;	// Bank Address
		BVec dq;	// Write Data
		BVec dqm;	// Read/Write Data Mask

		UInt commandCode() const { return ~cat(wen, casn, rasn); }
	};

	enum class CommandCode
	{
		// RAS = 1, CAS = 2, WE = 4
		Nop = 0,
		Activate = 1,
		Read = 2, // use a[10] for auto precharge
		Refresh = 3, // use CKE to enter self refresh
		BurstStop = 4,
		Precharge = 5, // use a[10] to precharge all banks
		Write = 6, // use a[10] for auto precharge
		ModeRegisterSet = 7, // use bank address 1 for extended mode register
	};

	enum class DriveStrength
	{
		Weak,
		Full,
	};
}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::sdram::CommandBus, cke, csn, rasn, casn, wen, a, ba, dq, dqm);
