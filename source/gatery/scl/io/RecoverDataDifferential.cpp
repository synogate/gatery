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
#include "RecoverDataDifferential.h"
#include "../cdc.h"
#include "../Counter.h"
#include "bypassableDelayChain.h"
#include "usb/usbAdjustPhase.h"
#include <gatery/scl/arch/intel/ALTPLL.h>

namespace gtry::scl {
	VStream<UInt> recoverDataDifferentialOversampling(const Clock& signalClock, Bit ioP, Bit ioN) {
		auto scope = Area{ "scl_recoverDataDifferential" }.enter();

		const auto samplesRatio = ClockScope::getClk().absoluteFrequency() / signalClock.absoluteFrequency();
		HCL_DESIGNCHECK_HINT(samplesRatio.denominator() == 1, "clock must be divisible by signalClock");
		const size_t samples = samplesRatio.numerator();
		HCL_DESIGNCHECK_HINT(samples >= 3, "we need at least 3 samples per cycle to recover data");

		ioP.resetValue('0');
		ioN.resetValue('1');
		// avoid meta stable inputs
		Bit p = synchronize(ioP, signalClock, ClockScope::getClk(), {.outStages = 3, .inStage = false});
		Bit n = synchronize(ioN, signalClock, ClockScope::getClk(), {.outStages = 3, .inStage = false});
		HCL_NAMED(p);
		HCL_NAMED(n);

		Counter phaseCounter{ samples };
		phaseCounter.inc();

		// sample data based on clock estimate
		VStream<UInt> out{
			cat(n, p),
			Valid{ phaseCounter.isLast() }
		};

		// recover clock and shift sample point
		IF(p != reg(p, '1') | n != reg(n, '0'))
		{
			phaseCounter.load((samples + 1) / 2);
			valid(out) = '0'; // prevent double sampling
		}
		HCL_NAMED(out);
		return out;
	}

	VStream<UInt> recoverDataDifferentialEqualsamplingDirty(const Clock& signalClock, Bit ioP, Bit ioN) {
		Area area{ "scl_recoverDataDifferentialEqualsamplingDirty", true };

		ioP.resetValue('0');
		ioN.resetValue('1');

		Bit p = reg(allowClockDomainCrossing(ioP, signalClock, ClockScope::getClk())); HCL_NAMED(p);
		Bit n = reg(allowClockDomainCrossing(ioN, signalClock, ClockScope::getClk())); HCL_NAMED(n);

		return { cat(n,p), Valid{'1'} };
	}

	VStream<UInt> recoverDataDifferentialEqualsamplingCyclone10(const Clock& signalClock, Bit ioP, Bit ioN) {
		Area area{ "scl_recoverDataDifferentialEqualsamplingCyclone10", true };

		Clock logicClk = ClockScope::getClk();

		Bit p;
		Bit n;

		p = allowClockDomainCrossing(ioP, signalClock, logicClk);	setName(ioP, "in_p_pin"); tap(ioP);
		n = allowClockDomainCrossing(ioN, signalClock, logicClk);	setName(ioN, "in_n_pin"); tap(ioN);

		Bit resetDelay = detectSingleEnded({ p, n }, '0'); HCL_NAMED(resetDelay); tap(resetDelay);

		BitWidth delayW = 4_b;
		UInt delay = delayW;
		{
			auto* pll = dynamic_cast<arch::intel::ALTPLL*> (DesignScope::get()->getCircuit().findFirstNodeByName("ALTPLL"));
			HCL_DESIGNCHECK_HINT(pll != nullptr, "there is no altera pll in your design.");
			Clock fastClk = pll->generateOutClock(1, 16, 1, 50, 0); //creates a ~<400MHz clock

			Clock samplingClock = pll->generateOutClock(2, 16, 1, 50, 0); //creates a 100MHz
			ClockScope fastScp(fastClk);

			delay = allowClockDomainCrossing(delay, logicClk, fastClk);
			p = allowClockDomainCrossing(ioP, signalClock, fastClk);	setName(p, "in_p_pin"); tap(p);
			n = allowClockDomainCrossing(ioN, signalClock, fastClk);	setName(n, "in_n_pin"); tap(n);
			
			p = delayChainWithTaps(p, delay, fastRegisterChainDelay, 1); setName(p, "in_p_delayed"); tap(p);
			n = delayChainWithTaps(n, delay, fastRegisterChainDelay, 1); setName(n, "in_n_delayed"); tap(n);

			p = allowClockDomainCrossing(p, fastClk, logicClk);
			n = allowClockDomainCrossing(n, fastClk, logicClk);
		}

		delay = setDelay(analyzePhase(p), resetDelay, delayW); HCL_NAMED(delay); tap(delay);

		p = reg(p, '0'); HCL_NAMED(p); //temporary: should be removed because there is no cyclic dependency through the pins ( normally )
		n = reg(n, '1'); HCL_NAMED(n); //temporary: should be removed because there is no cyclic dependency through the pins ( normally )

		return { cat(n,p), Valid{'1'} };
	}

	VStream<UInt> recoverDataDifferential(const Clock& signalClock, Bit ioP, Bit ioN)
	{
		const auto samplesRatio = ClockScope::getClk().absoluteFrequency() / signalClock.absoluteFrequency();
		HCL_DESIGNCHECK_HINT(samplesRatio.denominator() == 1, "clock must be divisible by signalClock");
		const size_t samples = samplesRatio.numerator();

		VStream<UInt> out;
		if (samples == 1)
			return recoverDataDifferentialEqualsamplingDirty(signalClock, ioP, ioN);
		else
			return recoverDataDifferentialOversampling(signalClock, ioP, ioN);

	}
}
