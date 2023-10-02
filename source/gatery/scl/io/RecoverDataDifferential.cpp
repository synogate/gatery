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
#include "gatery/pch.h"
#include "RecoverDataDifferential.h"
#include "../cdc.h"
#include "../Counter.h"

gtry::scl::VStream<gtry::UInt> gtry::scl::recoverDataDifferential(const gtry::Clock &signalClock, Bit ioP, Bit ioN)
{
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
