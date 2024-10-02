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
#include "SdramTimer.h"

namespace gtry::scl::sdram
{
	SdramTimer::SdramTimer()
	{
		m_area.leave();
	}

	void SdramTimer::generate(const Timings& timing, CommandBus cmd, UInt casLength, size_t casLimit)
	{
		auto ent = m_area.enter();
		HCL_NAMED(cmd);
		HCL_NAMED(casLength);

		auto [busLimit, bankLimit] = newLimits(timing, cmd, casLength, casLimit);

		CheckCounter busState = constructFrom(busLimit);
		busState.reg();
		HCL_NAMED(busState);
		busState.dec();
		IF(!cmd.csn & cmd.cke)
			busState.updateIfLess(busLimit);
		
		Vector<CheckCounter> bankState(cmd.ba.width().count());
		for (CheckCounter& c : bankState)
		{
			c = constructFrom(bankLimit);
			c.dec();
			c.reg();
		}
		HCL_NAMED(bankState);

		CheckCounter activeState = mux(cmd.ba, bankState);
		activeState.updateIfLess(bankLimit);
		IF(!cmd.csn & cmd.cke)
			demux(cmd.ba, bankState, activeState);

		m_bankState.resize(bankState.size());
		for (size_t i = 0; i < bankState.size(); ++i)
		{
			m_bankState[i].activate = reg(bankState[i].activate == 0 & busState.activate == 0, '0');
			m_bankState[i].precharge = reg(bankState[i].precharge == 0 & busState.precharge == 0, '0');
			m_bankState[i].read = reg(bankState[i].read == 0 & busState.read == 0, '0');
			m_bankState[i].write = reg(bankState[i].write == 0 & busState.write == 0, '0');
		}
		HCL_NAMED(m_bankState);
	}

	gtry::Bit SdramTimer::can(Enum<CommandCode> code, UInt bank)
	{
		auto ent = m_area.enter("can");
		HCL_NAMED(code);
		HCL_NAMED(bank);

		CheckList state = mux(bank, m_bankState);
		Bit active = '1';
		IF(code == CommandCode::Activate)
			active = state.activate;
		IF(code == CommandCode::Precharge)
			active = state.precharge;
		IF(code == CommandCode::Read)
			active = state.read;
		IF(code == CommandCode::Write)
			active = state.write;
		HCL_NAMED(active);
		return active;
	}

	std::tuple<SdramTimer::CheckCounter, SdramTimer::CheckCounter> SdramTimer::newLimits(const Timings& timing, CommandBus cmd, UInt casLength, size_t casLimit) const
	{
		CheckCounter bankLimit, busLimit;
		for (auto& l : { &bankLimit, &busLimit })
		{
			l->init(timing, casLimit);
			l->activate = 0;
			l->precharge = 0;
			l->read = 0;
			l->write = 0;
		}

		UInt code = cmd.commandCode();
		IF(code == (size_t)CommandCode::Activate)
		{
			// RC | RAS -> RAS same bank
			if (timing.rcd + timing.rp < timing.rc)
				bankLimit.activate = timing.rc - 1;

			// RCD | RAS -> CAS same bank
			bankLimit.read = timing.rcd - 1;
			bankLimit.write = timing.rcd - 1;

			// RAS | RAS -> Precharge same bank
			bankLimit.precharge = timing.ras - 1;

			// RRD | RAS -> RAS different bank
			busLimit.activate = timing.rrd - 1;
		}

		IF(code == (size_t)CommandCode::Precharge)
		{
			// RP | Precharge -> RAS same bank
			bankLimit.activate = timing.rp - 1;
		}

		// CCD not implemented, assumed to be one

		IF(code == (size_t)CommandCode::Read | code == (size_t)CommandCode::Write)
		{
			UInt casMinusOne = (casLength - 1).lower(-1_b);
			bankLimit.precharge = zext(casMinusOne);
			busLimit.read = zext(casMinusOne);
			busLimit.write = zext(casMinusOne);

			IF(code == (size_t)CommandCode::Read)
				busLimit.write += timing.wr + timing.cl;
		}

		HCL_NAMED(busLimit);
		HCL_NAMED(bankLimit);
		return { busLimit, bankLimit };
	}

	void SdramTimer::CheckCounter::init(const Timings& timings, size_t casLimit)
	{
		activate = BitWidth::last(timings.rc - 1);
		precharge = BitWidth::last(std::max({ (uint16_t)casLimit - 1, timings.rcd - 1, timings.ras - 1, timings.rp - 1 }));
		read = BitWidth::last(casLimit - 1);
		write = BitWidth::last(casLimit - 1 + timings.wr + timings.cl);
	}
	
	void SdramTimer::CheckCounter::updateIfLess(const CheckCounter& min)
	{
		IF(activate < min.activate) activate = min.activate;
		IF(precharge < min.precharge) precharge = min.precharge;
		IF(read < min.read) read = min.read;
		IF(write < min.write) write = min.write;
	}

	void SdramTimer::CheckCounter::dec()
	{
		if(!activate.empty()) IF(activate != 0) activate -= 1;
		if (!precharge.empty()) IF(precharge != 0) precharge -= 1;
		if (!read.empty()) IF(read != 0) read -= 1;
		if (!write.empty()) IF(write != 0) write -= 1;
	}
	
	void SdramTimer::CheckCounter::reg()
	{
		activate = gtry::reg(activate, 0);
		precharge = gtry::reg(precharge, 0);
		read = gtry::reg(read, 0);
		write = gtry::reg(write, 0);
	}
}

