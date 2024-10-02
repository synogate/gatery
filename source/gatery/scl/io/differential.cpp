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

#include <gatery/scl_pch.h>
#include "differential.h"

namespace gtry::scl {
	Bit detectSingleEnded(DiffPair input, Bit polarity) {
		Area area{ "detectSingleEnded", true};
		input.p.resetValue('0');
		input.n.resetValue('0');
		Clock fallingEdgeTriggerClk = ClockScope::getClk().deriveClock(ClockConfig{ .triggerEvent = Clock::TriggerEvent::FALLING });

		DiffPair cdcInput = allowClockDomainCrossing(input, ClockScope::getClk(), fallingEdgeTriggerClk);

		std::array<DiffPair, 2> samples;
		samples[0] = reg(input);
		samples[1] = reg(cdcInput, RegisterSettings{ .clock = fallingEdgeTriggerClk });

		samples[1] = allowClockDomainCrossing(samples[1], fallingEdgeTriggerClk, ClockScope::getClk());

		Bit singleEnded = samples[0].n == polarity & samples[0].p == polarity & samples[1].n == polarity & samples[1].p == polarity;
		HCL_NAMED(singleEnded);
		return singleEnded;
	}
}
