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
#include "recoverDataDifferential.h"
#include <gatery/scl/io/dynamicDelay.h>
#include <gatery/scl/analyzePhaseAlignment.h>
#include <gatery/scl/Counter.h>
#include <gatery/scl/stream/utils.h>
#include "ALTPLL.h"

namespace gtry::scl {
	VStream<Bit, SingleEnded> recoverDataDifferentialEqualsamplingAltera(const Clock& signalClock, Bit ioP, Bit ioN) {
		Area area{ "scl_recoverDataDifferential_equalsampling_altera", true };

		Clock logicClk = ClockScope::getClk();

		Bit p = allowClockDomainCrossing(ioP, signalClock, logicClk);	setName(ioP, "in_p_pin");
		Bit n = allowClockDomainCrossing(ioN, signalClock, logicClk);	setName(ioN, "in_n_pin");

		BitWidth delayW = 5_b;
		UInt delay = delayW;
		{
			auto* pll = dynamic_cast<arch::intel::ALTPLL*> (DesignScope::get()->getCircuit().findFirstNodeByName("ALTPLL"));
			HCL_DESIGNCHECK_HINT(pll != nullptr, "there is no altera pll in your design.");

			hlim::ClockRational pllClkIn = pll->inClkFrequency();
			hlim::ClockRational targetFastClockFrequency = { 400'000'000 };

			size_t multiplier = (size_t)round(hlim::toDouble(targetFastClockFrequency / pllClkIn));

			Clock fastClk = pll->generateUnspecificClock(multiplier, 1, 50, 0);
			ClockScope fastScope(fastClk);

			delay = allowClockDomainCrossing(delay, logicClk, fastClk);
			p = allowClockDomainCrossing(ioP, signalClock, fastClk);	setName(p, "in_p_pin");
			n = allowClockDomainCrossing(ioN, signalClock, fastClk);	setName(n, "in_n_pin");

			p = delayChainWithTaps(p, delay, [&](Bit in) { return reg(in, '0'); }); setName(p, "in_p_delayed");
			n = delayChainWithTaps(n, delay, [&](Bit in) { return reg(in, '0'); }); setName(n, "in_n_delayed");

			p = allowClockDomainCrossing(p, fastClk, logicClk);
			n = allowClockDomainCrossing(n, fastClk, logicClk);
		}

		Bit se0 = detectSingleEnded({ p, n }, '0'); HCL_NAMED(se0);

		Enum<PhaseCommand> command = analyzePhaseAlignment(p);
		delay = counterUpDown(
			command == PhaseCommand::delay,
			command == PhaseCommand::anticipate,
			se0,
			delayW,
			delayW.mask() / 2); HCL_NAMED(delay); tap(delay);

		p = reg(p, '0'); HCL_NAMED(p); //temporary: should be removed because there is no cyclic dependency through the pins ( normally )

		VStream<Bit, SingleEnded> out = strm::createVStream(p, '1').add(SingleEnded{ .zero = se0 });
		valid(out) &= !flag(se0, !se0);

		return out;
	}
}
