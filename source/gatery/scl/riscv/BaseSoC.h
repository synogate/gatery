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
#include <gatery/frontend/BitWidth.h>

namespace gtry::scl
{
	class MemoryMap;
	struct TileLinkU;

	class BaseSoC
	{
	public:
		virtual uint64_t ioBaseAddress() const = 0;
		virtual MemoryMap& mmap() = 0;
		virtual BaseSoC& attachDevice(uint64_t baseAddress, TileLinkU&& device) = 0;
		virtual TileLinkU& createDmaPort(std::string portName, BitWidth sourceW = 0_b, size_t burstLimitLog = 2) = 0;
	};
}
