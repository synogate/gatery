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

gtry::scl::DownStream<gtry::UInt> gtry::scl::recoverDataDifferential(hlim::ClockRational signalClock, Bit ioP, Bit ioN)
{
	auto scope = Area{ "scl_recoverDataDifferential" }.enter();

	const auto samplesRatio = ClockScope::getClk().absoluteFrequency() / signalClock;
	HCL_DESIGNCHECK_HINT(samplesRatio.denominator() == 1, "clock must be devisible by signalClock");
	const size_t samples = samplesRatio.numerator();
	HCL_DESIGNCHECK_HINT(samples >= 3, "we need at least 3 samples per cycle to recover data");

	// avoid meta stable inputs
	Bit p = synchronize(ioP, ClockScope::getClk(), ClockScope::getClk(), 3, false);
	Bit n = synchronize(ioN, ClockScope::getClk(), ClockScope::getClk(), 3, false);
	HCL_NAMED(p);
	HCL_NAMED(n);

	Counter phaseCounter{ samples };
	
	// recover clock and shift sample point
	Bit edgeDetected;
	IF(p != reg(p) | n != reg(n))
		edgeDetected = '1';
	IF(edgeDetected & p != n)
	{
		phaseCounter.load((samples + 1) / 2);
		edgeDetected = '0';
	}
	edgeDetected = reg(edgeDetected, '0');
	
	// sample data based on clock estimate
	DownStream<UInt> out;
	out.valid = phaseCounter.isLast();
	out.data = cat(n, p);
	HCL_NAMED(out);
	return out;
}
