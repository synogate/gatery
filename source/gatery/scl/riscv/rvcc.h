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

namespace gtry::scl::riscv
{
	// this has been a test if we can include a small c compiler into gatery
	// git clone https://github.com/mausimus/rvcc into source/external/ to activate this
	// rvcc does no optimization and thus generates too many instruction for an embedded system
	// it also tends to crash easiely. make sure your first character is not a whitespace or newline and define main.
	// make sure to have 10GiB of memory or modify rvcc's MAX_BLOCKS :/
	// see rvcc_test for an example

	std::vector<uint32_t> rvcc(std::string code, uint32_t codeOffset = 0);
}
