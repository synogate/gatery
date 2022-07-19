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
#include "scl/pch.h"
#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

#include <gatery/debug/websocks/WebSocksInterface.h>

#include <gatery/scl/tilelink/tilelink.h>
#include <gatery/scl/tilelink/TileLinkDemux.h>

using namespace boost::unit_test;
using namespace gtry;
using namespace gtry::scl;

template<TileLinkSignal TLink>
void initTileLink(TLink& link, BitWidth addrWidth, BitWidth dataWidth, BitWidth sizeWidth, BitWidth sourceWidth)
{
	*link.a = dataWidth;
	link.chanA().size = sizeWidth;
	link.chanA().source = sourceWidth;
	link.chanA().address = addrWidth;
	byteEnable(link.a) = dataWidth / 8;

	*link.d = dataWidth;
	link.chanD().size = sizeWidth;
	link.chanD().source = sourceWidth;
	link.chanD().sink = sourceWidth;
}

template<TileLinkSignal TLink = TileLinkUL>
class TileLinkSimuInitiator : public std::enable_shared_from_this<TileLinkSimuInitiator<TLink>>
{
public:
	TileLinkSimuInitiator(BitWidth addrWidth, BitWidth dataWidth, BitWidth sizeWidth, BitWidth sourceWidth, std::string prefix = "initiator_")
	{
		initTileLink(m_link, addrWidth, dataWidth, sizeWidth, sourceWidth);

		pinIn(m_link.a, prefix + "A");
		pinOut(m_link.d, prefix + "D");
	}

	void issueCommand(TileLinkA::OpCode code, uint64_t address, uint64_t data, uint64_t size, uint64_t source = 0)
	{
		simu(m_link.chanA().opcode) = (uint64_t)code;
		simu(m_link.chanA().param) = 0;
		simu(m_link.chanA().size) = size;
		simu(m_link.chanA().source) = source;
		simu(m_link.chanA().address) = address;
		simu(*m_link.a) = data;

		const uint64_t numBytes = 1ull << size;
		const uint64_t bytesPerBeat = byteEnable(m_link.a).width().bits();

		uint64_t mask;
		if (numBytes >= bytesPerBeat)
			mask = gtry::utils::bitMaskRange(0, bytesPerBeat);
		else
			mask = gtry::utils::bitMaskRange(address & (numBytes - 1), numBytes);
		simu(byteEnable(m_link.a)) = mask;
	}

	TLink& link() { return m_link; }
private:
	TLink m_link;

};

template<TileLinkSignal TLink = TileLinkUL>
class TileLinkSimuTarget : public std::enable_shared_from_this<TileLinkSimuTarget<TLink>>
{
public:
	TileLinkSimuTarget(BitWidth addrWidth, BitWidth dataWidth, BitWidth sizeWidth, BitWidth sourceWidth, std::string prefix = "target_")
	{
		initTileLink(m_link, addrWidth, dataWidth, sizeWidth, sourceWidth);

		pinOut(m_link.a, prefix + "A");
		pinIn(m_link.d, prefix + "D");
	}

	

	TLink& link() { return m_link; }
private:
	TLink m_link;

};

BOOST_FIXTURE_TEST_CASE(tilelink_dummy_test, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	TileLinkUH uh;
	TileLinkUL ul;

	addSimulationProcess([&]()->SimProcess {
		co_await WaitClk(clock);

		stopTest();
	});

	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
	runTicks(clock.getClk(), 16);
}

BOOST_FIXTURE_TEST_CASE(tilelink_demux_test, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	auto initiator = std::make_shared<TileLinkSimuInitiator<>>(8_b, 16_b, 4_b, 4_b);
	auto target = std::make_shared<TileLinkSimuTarget<>>(8_b, 16_b, 4_b, 4_b);

	TileLinkDemux<TileLinkUL> demux;
	demux.attachSource(initiator->link());
	demux.attachSink(target->link(), 0, 4);
	demux.generate();

	addSimulationProcess([=]()->SimProcess {
		simu(ready(initiator->link().d)) = 0;
		initiator->issueCommand(TileLinkA::PutFullData, 0, 0, 1);

		co_await WaitClk(clock);

		stopTest();
	});

	dbg::vis();
	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
	runTicks(clock.getClk(), 16);
}
