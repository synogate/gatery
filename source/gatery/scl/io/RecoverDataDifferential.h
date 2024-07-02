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
#include "../stream/Stream.h"

namespace gtry::scl
{
	struct SingleEnded {
		Bit zero;
	};

	VStream<Bit, SingleEnded> recoverDataDifferentialOversampling(const gtry::Clock &signalClock, Bit p, Bit n);
	VStream<Bit, SingleEnded> recoverDataDifferentialEqualsamplingDirty(const gtry::Clock &signalClock, Bit p, Bit n);
	VStream<Bit, SingleEnded> recoverDataDifferentialEqualsamplingCyclone10(const gtry::Clock &signalClock, Bit p, Bit n);
	VStream<Bit, SingleEnded> recoverDataDifferential(const gtry::Clock &signalClock, Bit p, Bit n);
}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::SingleEnded, zero);
