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

#include "serdes.h"

//#include "OSERDESE2.h"

namespace gtry::scl {

Bit oserdes(UInt data) {
	Area serdesArea{"scl_oserdes"};
	auto scope = serdesArea.enter();

	UInt slow_data = reg(data);
	Bit slow_sync;
	slow_sync = reg(~slow_sync, '0');
	HCL_NAMED(slow_sync);

	Clock fastClk = ClockScope::getClk().deriveClock({
			.frequencyMultiplier = data.size(),
			.name = "fastClk"
		});

	// todo: This is not DDR!!
	
	UInt fast_data = slow_data.width();
	fast_data >>= 1;

	Bit fast_sync;
	fast_sync = fastClk(slow_sync);
	HCL_NAMED(fast_sync);

	IF(slow_sync != fast_sync)
		fast_data = slow_data;

	fast_data = fastClk(fast_data);
	HCL_NAMED(fast_data);

	Bit tx = fast_data.lsb();

	tx.setName("tx_before_override");

/*
	auto *oserdese2_master = DesignScope::createNode<OSERDESE2>(data.size());
	for (auto i : utils::Range(std::min<size_t>(8u, data.size())))
		oserdese2_master->setInput((OSERDESE2::Inputs) (OSERDESE2::IN_D1+i), data[i]);

	oserdese2_master->attachClock(fastClk.getClk(), OSERDESE2::CLK);
	oserdese2_master->attachClock(ClockScope::getClk().getClk(), OSERDESE2::CLKDIV);

	oserdese2_master->setInput(OSERDESE2::IN_OCE, '1');
	oserdese2_master->setInput(OSERDESE2::IN_T1, '0');
	oserdese2_master->setInput(OSERDESE2::IN_T2, '0');
	oserdese2_master->setInput(OSERDESE2::IN_T3, '0');
	oserdese2_master->setInput(OSERDESE2::IN_T4, '0');
	oserdese2_master->setInput(OSERDESE2::IN_TBYTEIN, '0');
	oserdese2_master->setInput(OSERDESE2::IN_TCE, '0');

	if (data.size() > 8) {
		auto *oserdese2_slave = DesignScope::createNode<OSERDESE2>(data.size());
		oserdese2_slave->setSlave();

		HCL_ASSERT(data.size() == 10);
		oserdese2_slave->setInput(OSERDESE2::IN_D3, data[8]);
		oserdese2_slave->setInput(OSERDESE2::IN_D4, data[9]);

		oserdese2_slave->attachClock(fastClk.getClk(), OSERDESE2::CLK);
		oserdese2_slave->attachClock(ClockScope::getClk().getClk(), OSERDESE2::CLKDIV);

		oserdese2_slave->setInput(OSERDESE2::IN_OCE, '1');
		oserdese2_slave->setInput(OSERDESE2::IN_T1, '0');
		oserdese2_slave->setInput(OSERDESE2::IN_T2, '0');
		oserdese2_slave->setInput(OSERDESE2::IN_T3, '0');
		oserdese2_slave->setInput(OSERDESE2::IN_T4, '0');
		oserdese2_slave->setInput(OSERDESE2::IN_TBYTEIN, '0');
		oserdese2_slave->setInput(OSERDESE2::IN_TCE, '0');


		oserdese2_slave->setInput(OSERDESE2::IN_SHIFTIN1, '0');
		oserdese2_slave->setInput(OSERDESE2::IN_SHIFTIN2, '0');

		Bit shift1 = oserdese2_slave->getOutput(OSERDESE2::OUT_SHIFTOUT1);
		Bit shift2 = oserdese2_slave->getOutput(OSERDESE2::OUT_SHIFTOUT2);
		oserdese2_master->setInput(OSERDESE2::IN_SHIFTIN1, shift1);
		oserdese2_master->setInput(OSERDESE2::IN_SHIFTIN2, shift2);
	} else {
		oserdese2_master->setInput(OSERDESE2::IN_SHIFTIN1, '0');
		oserdese2_master->setInput(OSERDESE2::IN_SHIFTIN2, '0');
	}


	tx.exportOverride(SignalReadPort(oserdese2_master));
*/
	tx.setName("tx");

	return tx;
}


}
