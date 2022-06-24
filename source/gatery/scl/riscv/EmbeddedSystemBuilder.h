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
#include "ElfLoader.h"
#include "../Avalon.h"
#include "../io/uart.h"

namespace gtry::scl::riscv
{
	class EmbeddedSystemBuilder
	{
		struct Segment
		{
			uint64_t offset;
			uint64_t start;
			BitWidth size;
			BitWidth addrWidth;
			sim::DefaultBitVectorState resetState;
		};

		struct BusWindow
		{
			uint64_t offset;
			uint64_t size;
			std::string name;

			bool overlap(const BusWindow& o) const;
		};

	public:
		EmbeddedSystemBuilder();

		void addCpu(const ElfLoader& elf, BitWidth scratchMemSize, bool dedicatedInstructionMemory, bool debugTrace);

		Bit addUART(uint64_t offset, UART& config, const Bit& rx);
		Bit addUART(uint64_t offset, size_t baudRate, const Bit& rx);

		AvalonMM addAvalonMemMapped(uint64_t offset, BitWidth addrWidth, std::string name = "");
	private:
		Segment loadSegment(ElfLoader::MegaSegment elf, BitWidth additionalMemSize) const;
		void addDataMemory(const ElfLoader& elf, BitWidth scratchMemSize);
		void addDataMemory(const Segment& seg, std::string_view name, bool writable);

		Area m_area;
		AvalonMM m_dataBus;
		std::vector<BusWindow> m_dataBusWindows;

		std::vector<uint32_t> m_initCode;

		Bit m_anyDeviceSelected;

	};


}

