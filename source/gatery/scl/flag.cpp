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
#include "flag.h"
#include "Counter.h"

namespace gtry::scl 
{
	Bit flag(const Bit& set, const Bit& reset, char resetValue)
	{
		Bit ret;
		ret |= set;
		ret &= !reset;
		ret = reg(ret, resetValue);
		return final(ret);
	}

	Bit flagInstantSet(const Bit& set, const Bit& reset, char resetValue)
	{
		Bit ret;
		ret &= !reset;
		ret = reg(ret, resetValue);
		ret |= set;
		return final(ret);
	}

	Bit flagInstantReset(const Bit& set, const Bit& reset, char resetValue)
	{
		Bit ret;
		ret |= set;
		ret = reg(ret, resetValue);
		ret &= !reset;
		return final(ret);
	}

	Bit pulseExtend(Bit input, UInt cycles, Bit reset) 
	{
		Area area{ "scl_pulseExtend", true };
		HCL_NAMED(input);
		HCL_NAMED(cycles);

		Bit ret = input;
		IF(cycles != 0)
		{
			Counter pulseCtr(cycles);
			IF(input)
				pulseCtr.reset();

			ret = flagInstantSet(input, (pulseCtr.isLast() & !input) | reset);
		}
		HCL_NAMED(ret);
		return ret;
	}
}
