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
#include "ElfLoader.h"
#include "ElfUtil.h"

namespace gtry::scl::riscv
{
	sim::DefaultBitVectorState elfBuildMemoryImage(const ElfLoader& elf, size_t offset)
	{
		sim::DefaultBitVectorState ret;

		for (const ElfLoader::Segment& seg : elf.segments())
		{
			HCL_ASSERT(offset <= seg.offset);
			ret.resize((seg.offset - offset + seg.size.bytes()) * 8);
			ret.insert(sim::createDefaultBitVectorState(std::as_bytes(std::span(seg.data))), (seg.offset - offset) * 8);

			size_t zeroOffset = seg.offset - offset + seg.data.size();
			size_t zeroSize = seg.size.bytes() - seg.data.size();
			ret.setRange(sim::DefaultConfig::DEFINED, zeroOffset * 8, zeroSize * 8);
			ret.clearRange(sim::DefaultConfig::VALUE, zeroOffset * 8, zeroSize * 8);
		}

		return ret;
	}
}
