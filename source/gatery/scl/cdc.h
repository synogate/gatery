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
#include <gatery/hlim/supportNodes/Node_CDC.h>

#include "Counter.h"

namespace gtry::scl
{
	/// Converts an integer number coded in regular binary to gray code (such that neighboring integer values only change in one bit)
	BVec grayEncode(UInt val);
	/// Converts a grey coded integer number back to normal binary
	UInt grayDecode(BVec val);

	using CdcParameter = hlim::Node_CDC::CdcNodeParameter;

	struct SynchronizeParams {
		/// Clock domain crossing parameters such as timing constraints
		CdcParameter cdcParams;
		/// How many registers to build on the receiving side to prevent signal metastability
		size_t outStages = 3;
		/// Whether or not to build a register immediately before crossing the clock domain
		bool inStage = true;
	};

	/// Cross a signal from one clock domain into another through a synchronizer chain that ensures signal stability
	/// @details This also adds an explicit cdc node which prevents cdc crossing errors as well as storing timing constraint information
	/// that can be written to sdc/xdc files for supported tools.
	template<Signal T, SignalValue Treset>
	T synchronize(T in, const Treset& reset, const Clock& inClock, const Clock& outClock, const SynchronizeParams &params = {});

	/// Cross a signal from one clock domain into another through a synchronizer chain that ensures signal stability
	/// @details This also adds an explicit cdc node which prevents cdc crossing errors as well as storing timing constraint information
	/// that can be written to sdc/xdc files for supported tools.
	template<Signal T>
	T synchronize(T in, const Clock& inClock, const Clock& outClock, const SynchronizeParams &params = {});

	Bit synchronizeEvent(Bit event, const Clock& inClock, const Clock& outClock);
	Bit synchronizeRelease(Bit reset, const Clock& inClock, const Clock& outClock, ClockConfig::ResetActive resetActive = ClockConfig::ResetActive::HIGH);

	/// Cross an integer (e.g. counter value) from one clock domain into another but use grey code for the crossing.
	/// @details The gray code ensures that it is at most off by one when sampling as the integer increases
	/// This also adds an explicit cdc node which prevents cdc crossing errors as well as storing timing constraint information
	/// that can be written to sdc/xdc files for supported tools.
	UInt synchronizeGrayCode(UInt in, const Clock& inClock, const Clock& outClock, SynchronizeParams params = {});

	/// Cross an integer (e.g. counter value) from one clock domain into another but use grey code for the crossing.
	/// @details The gray code ensures that it is at most off by one when sampling as the integer increases
	/// This also adds an explicit cdc node which prevents cdc crossing errors as well as storing timing constraint information
	/// that can be written to sdc/xdc files for supported tools.
	UInt synchronizeGrayCode(UInt in, UInt reset, const Clock& inClock, const Clock& outClock, SynchronizeParams params = {});

	template<typename T, typename Targ>
	Vector<T> doublePump(std::function<T(const Targ&)> circuit, Vector<Targ> args, const Clock& fastClock);
}


namespace gtry::scl
{
	template<Signal T>
	T synchronize(T val, const Clock& inClock, const Clock& outClock, const SynchronizeParams &params)
	{
		HCL_DESIGNCHECK_HINT(params.outStages > 1, "Building a synchronizer chain with zero synchronization registers is probably a mistake!");

		if (params.inStage)
			val = reg(val, { .clock = inClock });

		val = allowClockDomainCrossing(val, inClock, outClock, params.cdcParams);

		Clock syncRegClock = outClock.deriveClock({ .synchronizationRegister = true });

		for(size_t i = 0; i < params.outStages; ++i)
			val = reg(val, { .clock = syncRegClock });

		return val;
	}

	template<Signal T, SignalValue Treset>
	T synchronize(T val, const Treset& reset, const Clock& inClock, const Clock& outClock, const SynchronizeParams &params)
	{
		HCL_DESIGNCHECK_HINT(params.outStages > 1, "Building a synchronizer chain with zero synchronization registers is probably a mistake!");

		if (params.inStage)
			val = reg(val, reset, { .clock = inClock });

		val = allowClockDomainCrossing(val, inClock, outClock, params.cdcParams);

		Clock syncRegClock = outClock.deriveClock({ .synchronizationRegister = true });

		for(size_t i = 0; i < params.outStages; ++i)
			val = reg(val, reset, { .clock = syncRegClock });

		return val;
	}

	template<typename T, typename Targ>
	Vector<T> doublePump(std::function<T(const Targ&)> circuit, Vector<Targ> args, const Clock& fastClock)
	{
		const Clock& clk = ClockScope::getClk();
		HCL_DESIGNCHECK_HINT(clk.absoluteFrequency() * args.size() == fastClock.absoluteFrequency(), "fast clock needs to be exactly the right multiple of the current clock for the given input");
		args = allowClockDomainCrossing(args, clk, fastClock);
		HCL_NAMED(args);

		ClockScope fastScope{ fastClock };

		scl::Counter ctr{ args.size() };
		ctr.inc();
		Targ beatArgs = reg(mux(ctr.value(), args));
		HCL_NAMED(beatArgs);
		T beatOut = circuit(beatArgs);
		HCL_NAMED(beatOut);

		Vector<T> outFast(args.size());
		outFast.back() = beatOut;
		for (int i = (int)outFast.size() - 2; i >= 0; --i)
			outFast[i] = reg(outFast[i + 1]);
		HCL_NAMED(outFast);

		Vector<T> outSlow = reg(allowClockDomainCrossing(outFast, fastClock, clk), { .clock = clk });
		HCL_NAMED(outSlow);
		return outSlow;
	}
}
