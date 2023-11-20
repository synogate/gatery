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

#include "axi.h"

namespace gtry::scl
{
	void simInit(Axi4& axi);
	SimFunction<std::tuple<uint64_t, uint64_t, bool>> simGet(Axi4& axi, uint64_t address, uint64_t logByteSize, const Clock& clk);
	SimFunction<bool> simPut(Axi4& axi, uint64_t address, uint64_t logByteSize, uint64_t data, const Clock& clk);
}
