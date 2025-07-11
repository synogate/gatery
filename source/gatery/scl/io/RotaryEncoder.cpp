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
#include "RotaryEncoder.h"

#include "../cdc.h"
#include "../stream/strm.h"

namespace gtry::scl
{
	VStream<Enum<RotaryDirection>> rotaryEncoder(Bit a, Bit b)
	{
		Area ent{ "scl_rotary_encoder", true };
		HCL_NAMED(a);
		HCL_NAMED(b);
		
		const Clock& clock = ClockScope::getClk();
		a.resetValue('0');
		Bit clk = scl::debounce(scl::synchronize(a, clock, clock), 100us) | name("clk");

		Enum<RotaryDirection> dir = RotaryDirection::clockwise;
		b.resetValue('0');
		IF(clk == scl::synchronize(b, clock, clock))
			dir = RotaryDirection::counterclockwise;
		HCL_NAMED(dir);
		return strm::createVStream(dir, edge(clk)) | name("out");
	}
}
