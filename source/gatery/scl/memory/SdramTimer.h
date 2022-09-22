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
#include "SdramCommand.h"

namespace gtry::scl::sdram
{
	struct Timings
	{
		uint16_t cl;  // cycles read latency
		uint16_t rcd; // ns RAS -> CAS
		uint16_t ras; // ns RAS -> Precharge
		uint16_t rp;  // ns Precharge -> RAS
		uint16_t rc;  // ns RAS -> RAS
		uint16_t rrd; // ns RAS -> RAS (different bank)
		uint16_t refi; // ns average refresh interval

		uint16_t wr = 2; // cycles write recovery time

		Timings toCycles(hlim::ClockRational memClock) const;
	};

	class SdramTimer
	{
	public:
		struct CheckList
		{
			Bit activate;
			Bit precharge;
			Bit read;
			Bit write;
		};

		struct CheckCounter
		{
			UInt activate;
			UInt precharge;
			UInt read;
			UInt write;

			void init(const Timings& timings, size_t casLimit);
			void updateIfLess(const CheckCounter& min);
			void dec();
			void reg();
		};

	public:
		SdramTimer();

		virtual void generate(const Timings& timing, CommandBus cmd, UInt casLength, size_t casLimit);
		virtual Bit can(Enum<CommandCode> code, UInt bank);

	protected:
		virtual std::tuple<CheckCounter, CheckCounter> newLimits(const Timings& timing, CommandBus cmd, UInt casLength, size_t casLimit) const;

	private:
		Area m_area = { "SdramTimer", true };
		Timings m_timing;

		Vector<CheckList> m_bankState;
	};

}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::sdram::SdramTimer::CheckList, activate, precharge, read, write);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::sdram::SdramTimer::CheckCounter, activate, precharge, read, write);
