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

gtry::scl::VStream<gtry::UInt> gtry::scl::recoverDataDifferential(const gtry::Clock &signalClock, Bit ioP, Bit ioN)
{

	auto scope = Area{ "scl_recoverDataDifferential" }.enter();

	const auto samplesRatio = ClockScope::getClk().absoluteFrequency() / signalClock.absoluteFrequency();
	HCL_DESIGNCHECK_HINT(samplesRatio.denominator() == 1, "clock must be divisible by signalClock");
	const size_t samples = samplesRatio.numerator();

	ioP.resetValue('0');
	ioN.resetValue('1');

	VStream<UInt> out;
	if(samples == 1) { //"dirty" method
		// avoid meta stable inputs with dirty synchronizer
		Bit p = reg(allowClockDomainCrossing(ioP, signalClock, ClockScope::getClk())); HCL_NAMED(p);
		Bit n = reg(allowClockDomainCrossing(ioN, signalClock, ClockScope::getClk())); HCL_NAMED(n);

		*out = cat(n, p);
		valid(out) = '1';
	}
	else {
		HCL_DESIGNCHECK_HINT(samples >= 3, "we need at least 3 samples per cycle to recover data with the current oversampling method");
		// avoid meta stable inputs
		Bit p = synchronize(ioP, signalClock, ClockScope::getClk(), {.outStages = 3, .inStage = false}); HCL_NAMED(p);
		Bit n = synchronize(ioN, signalClock, ClockScope::getClk(), {.outStages = 3, .inStage = false}); HCL_NAMED(n);

		Counter phaseCounter{ samples };

		// sample data based on clock estimate
		*out = cat(n, p);
		valid(out) = phaseCounter.isLast();

		//recover clock and shift sample point
		IF(p != reg(p, '1') | n != reg(n, '0'))
		{
			phaseCounter.load((samples + 1) / 2);
			valid(out) = '0'; // prevent double sampling
		}
	}

	HCL_NAMED(out);
	return out;
}
