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
#include <string_view>
#include <span>
#include <filesystem>

namespace gtry::scl
{
	struct IntelHexRecord
	{
		std::string_view comment;
		uint8_t type = 0; // 0 = data, 1 = eof, 4 = extended linear address
		uint16_t offset = 0;
		std::span<const uint8_t> data;

		static IntelHexRecord eof() { return { .type = 1 }; }
		static IntelHexRecord base(const uint32_t* baseAddress);
	};

	std::ostream& operator << (std::ostream&, const IntelHexRecord&);

}
