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
#include "cdc.h"
#include "flag.h"
#include <gatery/hlim/supportNodes/Node_CDC.h>

gtry::BVec gtry::scl::grayEncode(UInt val)
{
	return (BVec)(val ^ (val >> 1));
}

gtry::UInt gtry::scl::grayDecode(BVec val)
{
	UInt ret = ConstUInt(0, val.width());

	ret.msb() = val.msb();
	for(size_t i = ret.width().bits() - 2; i < ret.width().bits(); i--)
		ret[i] = ret[i + 1] ^ val[i];

	return ret;
}

gtry::Bit gtry::scl::synchronizeEvent(Bit eventIn, const Clock& inClock, const Clock& outClock)
{
	Area area("synchronizeEvent", true);
	ClockScope csIn{ inClock };

	Bit state;
	state = reg(eventIn ^ state, '0');

	ClockScope csOut{ outClock };

	return edge(synchronize(state, '0', inClock, outClock, {.outStages = 3, .inStage = false}));
}

gtry::Bit gtry::scl::synchronizeRelease(Bit reset, const Clock& inClock, const Clock& outClock, ClockConfig::ResetActive resetActive)
{
	Area area("synchronizeRelease", true);

	auto resetClock = outClock.deriveClock({
		.resetType = ClockConfig::ResetType::ASYNCHRONOUS,
		.resetActive = resetActive,
		.synchronizationRegister = true
	});
	Bit dummyReset;
	dummyReset.exportOverride(allowClockDomainCrossing(reset, inClock, resetClock));
	resetClock.overrideRstWith(dummyReset);

	Bit val = resetActive == ClockConfig::ResetActive::HIGH ? '0' : '1';
	for (size_t i = 0; i < 3; ++i)
		val = reg(val, resetActive == ClockConfig::ResetActive::HIGH ? '1' : '0', {.clock = resetClock});
	return val;
}

gtry::UInt gtry::scl::synchronizeGrayCode(UInt in, const Clock& inClock, const Clock& outClock, SynchronizeParams params)
{
	params.cdcParams.isGrayCoded = true;
	return grayDecode(synchronize(grayEncode(in), inClock, outClock, params));
}

gtry::UInt gtry::scl::synchronizeGrayCode(UInt in, UInt reset, const Clock& inClock, const Clock& outClock, SynchronizeParams params)
{
	params.cdcParams.isGrayCoded = true;
	return grayDecode(synchronize(grayEncode(in), grayEncode(reset), inClock, outClock, params));
}
