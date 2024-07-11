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

#include <gatery/scl_pch.h>
#include "analyzePhaseAlignment.h"

namespace gtry::scl {
	Enum<PhaseCommand> analyzePhaseAlignment(Bit input, ClockEdge alignmentEdge) {
		Area area{ "analyze_phase", true };
		setName(input, "delayed_input");

		Clock fallingEdgeTriggerClk = ClockScope::getClk().deriveClock(ClockConfig{ .triggerEvent = Clock::TriggerEvent::FALLING });

		Clock alignmentClock = fallingEdgeTriggerClk;
		Clock misalignmentClock = ClockScope::getClk();
		if (alignmentEdge == ClockEdge::rising)
			std::swap(alignmentClock, misalignmentClock);

		Bit cdcAlignmentInput = allowClockDomainCrossing(input, ClockScope::getClk(), alignmentClock);
		Bit cdcMisalignmentInput = allowClockDomainCrossing(input, ClockScope::getClk(), misalignmentClock);

		std::array<Bit, 3> samples;
		samples[0] = reg(cdcMisalignmentInput, '0', RegisterSettings{ .clock = misalignmentClock });
		samples[1] = reg(cdcAlignmentInput, '0', RegisterSettings{ .clock = alignmentClock });
		samples[2] = reg(samples[0], '0', RegisterSettings{ .clock = misalignmentClock });

		samples[0] = allowClockDomainCrossing(samples[0], misalignmentClock, ClockScope::getClk());
		samples[1] = allowClockDomainCrossing(samples[1], alignmentClock, ClockScope::getClk());
		samples[2] = allowClockDomainCrossing(samples[2], misalignmentClock, ClockScope::getClk());

		HCL_NAMED(samples);

		Enum<PhaseCommand> ret = PhaseCommand::do_nothing;
		IF(samples[0] != samples[2]) {
			IF(samples[0] != samples[1])
				ret = PhaseCommand::delay;
			ELSE
				ret = PhaseCommand::anticipate;
		}
		ELSE IF(samples[2] != samples[1]) {
			ret = PhaseCommand::delay;
		}

		HCL_NAMED(ret);
		return ret;
	}
}
