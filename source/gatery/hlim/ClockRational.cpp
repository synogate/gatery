/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2023 Michael Offel, Andreas Ley

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
#include "ClockRational.h"

namespace gtry::hlim {
	void formatTime(std::ostream &stream, ClockRational time) {
		const char *unit = "sec";
		if (time.denominator() > 1) { unit = "ms"; time *= 1000; }
		if (time.denominator() > 1) { unit = "us"; time *= 1000; }
		if (time.denominator() > 1) { unit = "ns"; time *= 1000; }
		if (time.denominator() > 1) { unit = "ps"; time *= 1000; }
		if (time.denominator() > 1) { unit = "fs"; time *= 1000; }

		stream << time.numerator() / time.denominator() << ' ' << unit;
	}	
}
