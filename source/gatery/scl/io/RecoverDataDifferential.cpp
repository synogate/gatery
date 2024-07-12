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
#include "differential.h"
#include "../cdc.h"
#include "../Counter.h"
#include <gatery/scl/stream/utils.h>
#include <gatery/scl/arch/intel/recoverDataDifferential.h>
#include <gatery/scl/arch/sky130/recoverDataDifferential.h>
#include <gatery/scl/arch/intel/ALTPLL.h>

namespace gtry::scl {
	VStream<Bit, SingleEnded> recoverDataDifferentialOversampling(const Clock& signalClock, Bit ioP, Bit ioN) {
		auto scope = Area{ "scl_recoverDataDifferential_oversampling" }.enter();

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
		
		Bit se0 = p == '0' & n == '0';

		// sample data based on clock estimate
		VStream<Bit, SingleEnded> out = strm::createVStream(p, phaseCounter.isLast())
			.add(SingleEnded{.zero = se0});

		// recover clock and shift sample point
		IF(p != reg(p, '1') | n != reg(n, '0'))
		{
			phaseCounter.load((samples + 1) / 2);
			valid(out) = '0'; // prevent double sampling
		}
		Bit validMaskExtraSe0 = flagInstantReset(valid(out) & se0, valid(out) & !se0);
		HCL_NAMED(validMaskExtraSe0);
		valid(out) &= !validMaskExtraSe0;

		HCL_NAMED(out);
		return out;
	}

	VStream<Bit, SingleEnded> recoverDataDifferentialEqualsamplingDirty(const Clock& signalClock, Bit ioP, Bit ioN) {
		Area area{ "scl_recoverDataDifferential_equalsampling_dirty", true };

		ioP.resetValue('0');
		ioN.resetValue('1');

		Bit p = allowClockDomainCrossing(ioP, signalClock, ClockScope::getClk());
		Bit n = allowClockDomainCrossing(ioN, signalClock, ClockScope::getClk());

		Bit se0 = detectSingleEnded({ p, n }, '0');

		p = reg(p); HCL_NAMED(p);
		n = reg(n); HCL_NAMED(n);

		VStream<Bit, SingleEnded> out = strm::createVStream(p, '1').add(SingleEnded{ .zero = se0 });
		valid(out) &= !flag(se0, !se0);

		return out;
	}

	VStream<Bit, SingleEnded> recoverDataDifferential(const Clock& signalClock, Bit ioP, Bit ioN)
	{
		auto scope = Area{ "scl_recoverDataDifferential" }.enter();

		const auto samplesRatio = ClockScope::getClk().absoluteFrequency() / signalClock.absoluteFrequency();
		HCL_DESIGNCHECK_HINT(samplesRatio.denominator() == 1, "clock must be divisible by signalClock");
		const size_t samples = samplesRatio.numerator();

		if (samples == 1) {
			//check if there is a config and use that
			if (auto config = scope.config("version")) {
				if(config.as<std::string>() == "dirty")
					return recoverDataDifferentialEqualsamplingDirty(signalClock, ioP, ioN);
				if(config.as<std::string>() == "altera")
					return recoverDataDifferentialEqualsamplingAltera(signalClock, ioP, ioN);
				if(config.as<std::string>() == "sky130")
					return arch::sky130::recoverDataDifferentialEqualsamplingSky130(signalClock, ioP, ioN);
			}
			
			//choose clean version if altera pll is found, otherwise use dirty version.
			auto* pll = dynamic_cast<arch::intel::ALTPLL*> (DesignScope::get()->getCircuit().findFirstNodeByName("ALTPLL"));
			if (pll != nullptr)
				return recoverDataDifferentialEqualsamplingAltera(signalClock, ioP, ioN);
			return recoverDataDifferentialEqualsamplingDirty(signalClock, ioP, ioN);
		}
		else
			return recoverDataDifferentialOversampling(signalClock, ioP, ioN);

	}
}
