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

#include "MemoryInitializationFile.h"

namespace gtry::scl::arch::intel {

void writeMemoryInitializationFile(std::ostream &dst, size_t width, const sim::DefaultBitVectorState &values)
{
	HCL_DESIGNCHECK_HINT(values.size() % width == 0, "Memory initialization file content size must a multiple of width");
	size_t depth = values.size() / width;

	dst << "-- Memory initialization file produced from gatery\n";
	dst << "DEPTH = " << depth << ";\nWIDTH = " << width << ";\nADDRESS_RADIX = HEX;\n";

	std::uint32_t undefinedPattern = 0xCDCDCDCD;

	dst << "DATA_RADIX = BIN;\n";
	dst << "CONTENT BEGIN\n";
	for (auto addr : utils::Range(depth)) {
		dst << std::hex << addr << " : ";

		for (auto j : utils::Range(width)) {
			size_t bitIdx = addr * width + width - 1 - j;
			bool defined = values.get(sim::DefaultConfig::DEFINED, bitIdx);
			bool value = values.get(sim::DefaultConfig::VALUE, bitIdx);

			if (!defined)
				value = undefinedPattern & (1 << (j % (sizeof(undefinedPattern)*8)));
			
			dst << (value?'1':'0');
		}

		dst << ";\n";
	}
	dst << "END;\n";
}

}
