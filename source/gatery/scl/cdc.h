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

namespace gtry::scl
{
	BVec grayEncode(UInt val);
	UInt grayDecode(BVec val);

	template<Signal T, SignalValue Treset>
	T synchronize(T in, const Treset& reset, const Clock& inClock, const Clock& outClock, size_t outStages = 3, bool inStage = true);

	template<Signal T>
	T synchronize(T in, const Clock& inClock, const Clock& outClock, size_t outStages = 3, bool inStage = true);

	Bit synchronizeEvent(Bit event, const Clock& inClock, const Clock& outClock);

	UInt synchronizeGrayCode(UInt in, const Clock& inClock, const Clock& outClock, size_t outStages = 3, bool inStage = true);
	UInt synchronizeGrayCode(UInt in, UInt reset, const Clock& inClock, const Clock& outClock, size_t outStages = 3, bool inStage = true);

	template<Signal T>
	T synchronize(T val, const Clock& inClock, const Clock& outClock, size_t outStages, bool inStage)
	{
		HCL_DESIGNCHECK_HINT(outStages > 1, "Building a synchronizer chain with zero synchronization registers is probably a mistake!");

		if(inStage)
			val = reg(val, { .clock = inClock });

		val = allowClockDomainCrossing(val, inClock, outClock);

		Clock syncRegClock = outClock.deriveClock({ .synchronizationRegister = true });

		for(size_t i = 0; i < outStages; ++i)
			val = reg(val, { .clock = syncRegClock });

		return val;
	}

	template<Signal T, SignalValue Treset>
	T synchronize(T val, const Treset& reset, const Clock& inClock, const Clock& outClock, size_t outStages, bool inStage)
	{
		HCL_DESIGNCHECK_HINT(outStages > 1, "Building a synchronizer chain with zero synchronization registers is probably a mistake!");

		if(inStage)
			val = reg(val, reset, { .clock = inClock });

		val = allowClockDomainCrossing(val, inClock, outClock);

		Clock syncRegClock = outClock.deriveClock({ .synchronizationRegister = true });

		for(size_t i = 0; i < outStages; ++i)
			val = reg(val, reset, { .clock = syncRegClock });

		return val;
	}



	
}
