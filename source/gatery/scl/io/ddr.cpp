/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2022 Michael Offel, Andreas Ley

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
#include "gatery/pch.h"

#include "ddr.h"

namespace gtry::scl {


Bit ddr(Bit D0, Bit D1, const DDROutParams &params)
{
	Area area{"scl_oddr", true};
	
	setName(D0, "D0");
	setName(D1, "D1");

	if (params.resetValue) {
		D0 = reg(D0, *params.resetValue);
		D1 = reg(D1, *params.resetValue);
	} else {
		D0 = reg(D0);
		D1 = reg(D1);
	}

	auto &clock = ClockScope::getClk();

	Bit sel = clock.clkSignal();
	setName(sel, "CLK");

	Bit O;

	IF (sel)
		O = D0;
	ELSE
		O = D1;

	setName(O, "O");

	return O;
}

}
