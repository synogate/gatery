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
#include "Stream.h"
#include "streamFifo.h"

namespace gtry::scl::strm
{
	struct Credit
	{
		Reverse<Bit> increment;
		size_t initialCredit = 0;
		size_t maxCredit = 64;
	};

	template<StreamSignal T>
	auto creditStream(T&& in, size_t initialCredit = 0, size_t maxCredit = 64)
	{
		Area ent{ "scl_toCreditStream", true };
		HCL_NAMED(in);
		HCL_DESIGNCHECK(maxCredit != 0);
		HCL_DESIGNCHECK(initialCredit <= maxCredit);

		Stream out = in.template remove<scl::Ready>().add(Credit{
			.initialCredit = initialCredit,
			.maxCredit = maxCredit
		});

		UInt credits = BitWidth::count(maxCredit);
		ready(in) = reg(credits != 0, '0');
		credits = reg(credits, initialCredit);
		HCL_NAMED(credits);

		UInt change = ConstUInt(0, credits.width());
		const Bit& incCredit = *out.template get<Credit>().increment;

		IF(transfer(in) & !incCredit)
			change |= '1'; //effectively makes change = -1

		IF(!transfer(in) & incCredit)
			change = 1;
		HCL_NAMED(change);
		credits += change;

		valid(out) = transfer(in);
		HCL_NAMED(out);
		return out;
	}

	inline auto creditStream(size_t initialCredit = 0, size_t maxCredit = 64)
	{
		return [=](auto&& in) { return creditStream(std::forward<decltype(in)>(in), initialCredit, maxCredit); };
	}

	template<StreamSignal T>
	auto creditStreamToVStream(T&& in, Bit incrementCredit)
	{
		*in.template get<Credit>().increment = incrementCredit;
		return in.template remove<Credit>();
	}

	inline auto creditStreamToVStream(Bit incrementCredit)
	{
		return [=](auto&& in) { return creditStreamToVStream(std::forward<decltype(in)>(in), incrementCredit); };
	}

	template<StreamSignal T>
	auto creditStreamToRvStream(T&& in)
	{
		Area ent{ "scl_creditStreamToRvStream", true };

		Credit& inCredit = in.template get<Credit>();

		HCL_DESIGNCHECK_HINT(inCredit.initialCredit != 0, "Initial credit is 0. This will cause a deadlock.");
		auto out = fifo(in.template remove<Credit>().add(Ready{}), inCredit.initialCredit);
		*inCredit.increment = transfer(out);
		return out;
	}

	inline auto creditStreamToRvStream()
	{
		return [=](auto&& in) { return creditStreamToRvStream(std::forward<decltype(in)>(in)); };
	}

	using gtry::reg;

	template<StreamSignal T> requires (T::template has<Credit>())
	T reg(T&& in, const RegisterSettings& settings = {})
	{
		T out = constructFrom(in);
		valid(in).resetValue('0');
		out.template get<Credit>().increment->resetValue('0');

		downstream(out) = reg(copy(downstream(in)), settings);
		upstream(in) = reg(copy(upstream(out)), settings);
		return out;
	}

	template<StreamSignal T> requires (T::template has<Credit>())
	T delay(T&& in, size_t cycles)
	{
		T out = move(in);
		for (size_t i = 0; i < cycles; ++i)
			out = reg(move(out));
		return out;
	}

	template<StreamSignal T> requires (T::template has<Credit>())
	T delayAutoPipeline(T&& in, size_t maxDelay = 15)
	{
		Area ent{ "scl_delayAutoPipeline", true };
		HCL_DESIGNCHECK(maxDelay > 2);
		T out = constructFrom(in);

		const std::string groupName = ent.instancePath();

		// downstream signals
		{
			Clock autoPipelineClkDown = ClockScope::getClk().deriveClock(ClockConfig{});
			hlim::RegisterAttributes& regAttr = autoPipelineClkDown.getClk()->getRegAttribs();
			regAttr.autoPipelineLimit = maxDelay - 2;
			regAttr.autoPipelineGroup = groupName + "_down";
			
			valid(in).resetValueRemove();
			downstream(out) = reg(reg(copy(downstream(in)), { .clock = autoPipelineClkDown }));
		}

		// upstream signals
		{
			Clock autoPipelineClkUp = ClockScope::getClk().deriveClock(ClockConfig{});
			hlim::RegisterAttributes& regAttr = autoPipelineClkUp.getClk()->getRegAttribs();
			regAttr.autoPipelineLimit = maxDelay - 2;
			regAttr.autoPipelineGroup = groupName + "_up";

			out.template get<Credit>().increment->resetValueRemove();
			upstream(in) = reg(reg(copy(upstream(out)), { .clock = autoPipelineClkUp }));
		}

		ClockScope::getClk().getClk()->setMinResetCycles(maxDelay);
		return out;
	}

	inline auto delayAutoPipeline(size_t maxDelay = 15)
	{
		return [=](auto&& in) { return delayAutoPipeline(std::forward<decltype(in)>(in), maxDelay); };
	}

	template<StreamSignal T> requires (!T::template has<Credit>())
	T delayAutoPipeline(T&& in, size_t maxDelay = 15)
	{
		Area ent{ "scl_delayAutoPipelineFifo", true };

		const size_t maxCredits = (maxDelay + 1) * 2;
		return move(in) | creditStream(maxCredits, maxCredits) | delayAutoPipeline(maxDelay) | creditStreamToRvStream();
	}
}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::strm::Credit, increment, initialCredit, maxCredit);
