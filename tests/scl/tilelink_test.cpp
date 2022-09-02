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
	link.a->size = sizeWidth;
	link.a->source = sourceWidth;
	link.a->address = addrWidth;
	link.a->mask = dataWidth / 8;
	link.a->data = dataWidth;

	(*link.d)->data = dataWidth;
	(*link.d)->size = sizeWidth;
	(*link.d)->source = sourceWidth;
	(*link.d)->sink = sourceWidth;
}

template<TileLinkSignal TLink = TileLinkUL>
class TileLinkSimuInitiator : public std::enable_shared_from_this<TileLinkSimuInitiator<TLink>>
{
public:
	TileLinkSimuInitiator(BitWidth addrWidth, BitWidth dataWidth, BitWidth sizeWidth, BitWidth sourceWidth, std::string prefix = "initiator")
	{
		initTileLink(m_link, addrWidth, dataWidth, sizeWidth, sourceWidth);
		pinIn(m_link, prefix);
	}

	void issueCommand(TileLinkA::OpCode code, uint64_t address, uint64_t data, uint64_t size, uint64_t source = 0)
	{
		simu(m_link.a->opcode) = (uint64_t)code;
		simu(m_link.a->param) = 0;
		simu(m_link.a->size) = size;
		simu(m_link.a->source) = source;
		simu(m_link.a->address) = address;
		simu(m_link.a->data) = data;

		const uint64_t numBytes = 1ull << size;
		const uint64_t bytesPerBeat = m_link.a->mask.width().bits();

		uint64_t mask;
		if (numBytes >= bytesPerBeat)
			mask = gtry::utils::bitMaskRange(0, bytesPerBeat);
		else
			mask = gtry::utils::bitMaskRange(address & (numBytes - 1), numBytes);
		simu(m_link.a->mask) = mask;
	}

	TLink& link() { return m_link; }
private:
	TLink m_link;

};

template<TileLinkSignal TLink = TileLinkUL>
class TileLinkSimuTarget : public std::enable_shared_from_this<TileLinkSimuTarget<TLink>>
{
public:
	TileLinkSimuTarget(BitWidth addrWidth, BitWidth dataWidth, BitWidth sizeWidth, BitWidth sourceWidth, std::string prefix = "target")
	{
		initTileLink(m_link, addrWidth, dataWidth, sizeWidth, sourceWidth);
		pinOut(m_link, prefix);
	}

	void issueResponse(TileLinkD::OpCode code, uint64_t data, bool error = false)
	{
		simu((*m_link.d)->opcode) = (uint64_t)code;
		simu((*m_link.d)->param) = 0;
		simu((*m_link.d)->size) = simu(m_link.a->size);
		simu((*m_link.d)->source) = simu(m_link.a->source);
		simu((*m_link.d)->sink) = 0;
		simu((*m_link.d)->error) = error ? 1 : 0;
		simu((*m_link.d)->data) = data;
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
	downstream(uh);

	addSimulationProcess([&]()->SimProcess {
		co_await WaitClk(clock);

		stopTest();
	});

	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
	runTicks(clock.getClk(), 16);
}

BOOST_FIXTURE_TEST_CASE(tilelink_demux_chanA_test, BoostUnitTestSimulationFixture)
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

		auto& iniA = initiator->link().a;
		auto& iniD = *initiator->link().d;
		auto& tgtA = target->link().a;
		auto& tgtD = *target->link().d;

		simu(valid(iniA)) = 0;
		simu(ready(iniD)) = 0;
		simu(ready(tgtA)) = 0;
		simu(valid(tgtD)) = 0;
		co_await WaitClk(clock);
		BOOST_TEST(simu(valid(tgtA)) == 0);

		initiator->issueCommand(TileLinkA::PutFullData, 0, 0, 1);
		co_await WaitClk(clock);
		BOOST_TEST(simu(valid(tgtA)) == 0);
		BOOST_TEST(simu(ready(iniA)) == 0);

		simu(valid(initiator->link().a)) = 1;
		co_await WaitClk(clock);
		BOOST_TEST(simu(valid(tgtA)) == 1);
		BOOST_TEST(simu(ready(iniA)) == 0);

		simu(ready(tgtA)) = 1;
		co_await WaitClk(clock);
		BOOST_TEST(simu(valid(tgtA)) == 1);
		BOOST_TEST(simu(ready(iniA)) == 1);

		stopTest();
	});

	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
	runTicks(clock.getClk(), 16);
}

BOOST_FIXTURE_TEST_CASE(tilelink_demux_chanD_test, BoostUnitTestSimulationFixture)
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

		auto& iniA = initiator->link().a;
		auto& iniD = *initiator->link().d;
		auto& tgtA = target->link().a;
		auto& tgtD = *target->link().d;

		simu(valid(iniA)) = 0;
		simu(ready(iniD)) = 0;
		simu(ready(tgtA)) = 0;
		simu(valid(tgtD)) = 0;
		co_await WaitClk(clock);
		BOOST_TEST(simu(valid(iniD)) == 0);
		BOOST_TEST(simu(ready(tgtD)) == 0);

		initiator->issueCommand(TileLinkA::PutFullData, 0, 0, 1);
		target->issueResponse(TileLinkD::AccessAck, 1337);
		co_await WaitClk(clock);
		BOOST_TEST(simu(valid(iniD)) == 0);
		BOOST_TEST(simu(ready(tgtD)) == 0);

		simu(valid(tgtD)) = 1;
		co_await WaitClk(clock);
		BOOST_TEST(simu(valid(iniD)) == 1);
		BOOST_TEST(simu(ready(tgtD)) == 0);

		simu(ready(iniD)) = 1;
		co_await WaitClk(clock);
		BOOST_TEST(simu(valid(iniD)) == 1);
		BOOST_TEST(simu(ready(tgtD)) == 1);

		stopTest();
	});

	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
	runTicks(clock.getClk(), 16);
}

BOOST_FIXTURE_TEST_CASE(tilelink_demux_chanA_routing_test, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	auto initiator = std::make_shared<TileLinkSimuInitiator<>>(12_b, 16_b, 4_b, 4_b);
	auto target0 = std::make_shared<TileLinkSimuTarget<>>(12_b, 16_b, 4_b, 4_b, "target0");
	auto target1 = std::make_shared<TileLinkSimuTarget<>>(12_b, 16_b, 4_b, 4_b, "target1");

	TileLinkDemux<TileLinkUL> demux;
	demux.attachSource(initiator->link());
	demux.attachSink(target0->link(), 0x000, 4);
	demux.attachSink(target1->link(), 0x100, 8);
	demux.generate();

	addSimulationProcess([=]()->SimProcess {

		auto& iniA = initiator->link().a;
		auto& iniD = *initiator->link().d;
		auto& tgt0A = target0->link().a;
		auto& tgt0D = *target0->link().d;
		auto& tgt1A = target1->link().a;
		auto& tgt1D = *target1->link().d;

		simu(valid(iniA)) = 1;
		simu(ready(tgt0A)) = 1;
		simu(ready(tgt1A)) = 1;
		co_await WaitClk(clock);

		initiator->issueCommand(TileLinkA::PutFullData, 0, 0, 1);
		co_await WaitClk(clock);
		BOOST_TEST(simu(valid(tgt0A)) == 1);
		BOOST_TEST(simu(valid(tgt1A)) == 0);

		initiator->issueCommand(TileLinkA::PutFullData, 16, 0, 1);
		co_await WaitClk(clock);
		BOOST_TEST(simu(valid(tgt0A)) == 0);
		BOOST_TEST(simu(valid(tgt1A)) == 0);

		initiator->issueCommand(TileLinkA::PutFullData, 256, 0, 1);
		co_await WaitClk(clock);
		BOOST_TEST(simu(valid(tgt0A)) == 0);
		BOOST_TEST(simu(valid(tgt1A)) == 1);

		stopTest();
	});

	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
	runTicks(clock.getClk(), 16);
}
