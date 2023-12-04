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
#include "../../tilelink/tilelink.h"
#include "URAM288.h"

namespace gtry::scl::arch::xilinx 
{
	struct UltraRamSettings
	{
		std::string_view name;
		BitWidth aSourceW = 0_b;
		BitWidth bSourceW = 0_b;
		std::optional<size_t> latency;
	};

	std::array<TileLinkUL, 2> ultraRam(size_t numWords, UltraRamSettings&& cfg = {});
	TileLinkUL ultraRamPort(BitWidth addrW, URAM288& inRam, URAM288& outRam, URAM288::Port port, BitWidth sourceW, size_t latency);
}
