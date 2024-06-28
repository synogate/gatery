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
#include <gatery/scl/Counter.h>

namespace gtry::scl {

	enum class PhaseCommand {
		delay,
		anticipate,
		do_nothing,
		reset
	};
	
	/**
	 * @brief analyzes the phase and returns a command according to the position of the phase with respect to the falling clock edge of the signal
	 * @param input The signal to analyze
	 * @warning needs access to falling edge registers or an inverted clock.
	 * @return subset of phase commands: delay, anticipate or do nothing
	 */
	static Enum<PhaseCommand> analyzePhase(Bit input) {
		Area area{ "analyze_phase", true};
		setName(input, "delayed_input");
		input.resetValue('0');
		bool addSynchRegs = false;
		Clock fallingEdgeTriggerClk = ClockScope::getClk().deriveClock(ClockConfig{ .triggerEvent = Clock::TriggerEvent::FALLING });
		Bit cdcInput = allowClockDomainCrossing(input, ClockScope::getClk(), fallingEdgeTriggerClk);

		std::array<Bit, 4> samples;
		samples[0] = reg(input); 
		samples[1] = reg(cdcInput, RegisterSettings{ .clock = fallingEdgeTriggerClk });
		samples[2] = reg(samples[0]);
		samples[3] = reg(samples[1], RegisterSettings{ .clock = fallingEdgeTriggerClk });


		HCL_NAMED(samples); tap(samples);
		samples[1] = allowClockDomainCrossing(samples[1], fallingEdgeTriggerClk, ClockScope::getClk());
		samples[3] = allowClockDomainCrossing(samples[3], fallingEdgeTriggerClk, ClockScope::getClk());
		
		Enum<PhaseCommand> ret = PhaseCommand::do_nothing;
		IF(samples[0] != samples[2]){
			IF(samples[0] != samples[1]) 
				ret = PhaseCommand::delay;
			ELSE
				ret = PhaseCommand::anticipate;
		}
		ELSE IF(samples[2] != samples[1]) {
			ret = PhaseCommand::delay;
		}

#if 0
		ELSE IF(samples[1] != samples[3]) {
			IF(samples[1] != samples[2])
				ret = PhaseCommand::delay;
			ELSE
				ret = PhaseCommand::anticipate;
		}
#endif

		HCL_NAMED(ret);
		return ret;
	}


	struct DifPair {
		Bit p;
		Bit n;
	};

	Bit detectSingleEnded(DifPair input, Bit polarity) {
		Area area{ "detectSingleEnded", true};
		input.p.resetValue('0');
		input.n.resetValue('0');
		Clock fallingEdgeTriggerClk = ClockScope::getClk().deriveClock(ClockConfig{ .triggerEvent = Clock::TriggerEvent::FALLING });

		DifPair cdcInput = allowClockDomainCrossing(input, ClockScope::getClk(), fallingEdgeTriggerClk);

		std::array<DifPair, 2> samples;
		samples[0] = reg(cdcInput, RegisterSettings{ .clock = fallingEdgeTriggerClk });
		samples[1] = reg(input);

		samples[0] = allowClockDomainCrossing(samples[0], fallingEdgeTriggerClk, ClockScope::getClk());

		Bit se = samples[0].n == polarity & samples[0].p == polarity & samples[1].n == polarity & samples[1].p == polarity;
		HCL_NAMED(se);
		return se;
	}

	UInt setDelay(Enum<PhaseCommand> command, Bit mustReset, BitWidth delayW) {
		Counter delay(delayW, delayW.mask() / 2);

		IF(command == PhaseCommand::delay) {
			IF(!delay.isLast())
				delay.inc();
		}
		IF(command == PhaseCommand::anticipate){
			IF(!delay.isFirst())
				delay.dec();
		}

		IF(mustReset)
			delay.load(delayW.mask() / 2);

		return delay.value();
	}

}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::DifPair, p, n);
