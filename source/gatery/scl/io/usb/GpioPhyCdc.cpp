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
#include "GpioPhyCdc.h"

#include <gatery/scl/arch/colognechip/SpecialFunction.h>
#include <gatery/scl/stream/utils.h>
#include <gatery/scl/stream/Packet.h>

namespace gtry::scl::usb
{
	GpioPhyCdc::GpioPhyCdc()
	{
	}

	Bit GpioPhyCdc::setup(OpMode mode)
	{
		if(!m_clkSlow)
			m_clkSlow = arch::colognechip::clkdivRegChain(ClockScope::getClk(), 1);

		Bit ret = GpioPhy::setup(mode);

		auto ent = m_area.enter();
		Clock& phyClk = GpioPhy::clock();

		{ // rx side
			const PhyRxStream& phy = GpioPhy::rx();
			Stream rx = strm::createVStream(phy.data, phy.valid) |
				strm::attach(Sop{ phy.sop }) |
				strm::attach(Ready{}) |
				name("phyrx") |
				strm::regReady() |
				strm::synchronizeStreamReqAck(phyClk, *m_clkSlow, { .outStages = 1 }) |
				strm::removeReady();

			m_rx.data = *rx;
			m_rx.valid = valid(rx);
			m_rx.sop = sop(rx);

			Bit success =
				scl::synchronizeEvent(phy.eop & !phy.error, phyClk, *m_clkSlow, { .outStages = 2 }) |
				name("rx_success");
			Bit fail =
				scl::synchronizeEvent(phy.eop & phy.error, phyClk, *m_clkSlow, { .outStages = 2 }) |
				name("rx_fail");

			m_rx.eop = success | fail;
			m_rx.error = fail;

			m_rx = name(m_rx, "phy2func");
		}
		{ // status
			// status is used for line state idle detection and therefore considered not sensitive to cdc errors
			m_status =
				synchronize(GpioPhy::status(), phyClk, *m_clkSlow, { .outStages = 2 }) |
				name("status");
		}

		return ret;
	}

	void GpioPhyCdc::generateTx(Bit& en, Bit& p, Bit& n)
	{
		RvPacketStream<UInt, Error> func2phy;
		{
			auto ent = m_area.enter();

			Clock clkPhy = ClockScope::getClk();
			ClockScope csSlow{ *m_clkSlow };
			Stream tx = strm::createVStream(m_tx.data, m_tx.valid) |
				strm::attach(Eop{ '0' }) |
				strm::attach(Error{ m_tx.error }) |
				strm::attach(Ready{}) |
				name("tx");
			m_tx.ready = ready(tx);

			func2phy = move(tx) |
				strm::addEopDeferred(edgeFalling(m_tx.valid)) |
				strm::synchronizeStreamReqAck(*m_clkSlow, clkPhy, { .outStages = 1 }) |
				clockScope(clkPhy) |
				strm::regReady() |
				name("func2phy");
		}
		GpioPhy::generateTx(en, p, n, move(func2phy));
	}

	Clock& GpioPhyCdc::clock()
	{
		return *m_clkSlow;
	}

	const PhyRxStatus& GpioPhyCdc::status() const
	{
		return m_status;
	}

	PhyTxStream& GpioPhyCdc::tx()
	{
		return m_tx;
	}

	PhyRxStream& GpioPhyCdc::rx()
	{
		return m_rx;
	}
}
