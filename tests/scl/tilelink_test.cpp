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
#include <gatery/scl/synthesisTools/IntelQuartus.h>

#include <gatery/scl/tilelink/tilelink.h>
#include <gatery/scl/tilelink/TileLinkHub.h>
#include <gatery/scl/tilelink/TileLinkErrorResponder.h>

using namespace boost::unit_test;
using namespace gtry;
using namespace gtry::scl;

template<TileLinkSignal TLink = TileLinkUL>
class TileLinkSimuInitiator : public std::enable_shared_from_this<TileLinkSimuInitiator<TLink>>
{
public:
	TileLinkSimuInitiator(BitWidth addrWidth, BitWidth dataWidth, BitWidth sizeWidth, BitWidth sourceWidth, std::string prefix = "initiator")
	{
		tileLinkInit(m_link, addrWidth, dataWidth, sizeWidth, sourceWidth);
		pinIn(m_link, prefix);
	}

	void issueIdle()
	{
		simu(valid(m_link.a)) = 0;
		simu(ready(*m_link.d)) = 0;
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
		tileLinkInit(m_link, addrWidth, dataWidth, sizeWidth, sourceWidth);
		pinOut(m_link, prefix);
	}

	void issueIdle()
	{
		simu(ready(m_link.a)) = 0;
		simu(valid(*m_link.d)) = 0;
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
	auto target = std::make_shared<TileLinkSimuTarget<>>(4_b, 16_b, 4_b, 4_b);

	TileLinkDemux<TileLinkUL> demux;
	demux.attachSource(initiator->link());
	demux.attachSink(target->link(), 0);
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
	auto target = std::make_shared<TileLinkSimuTarget<>>(4_b, 16_b, 4_b, 4_b);

	TileLinkDemux<TileLinkUL> demux;
	demux.attachSource(initiator->link());
	demux.attachSink(target->link(), 0);
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
	auto target0 = std::make_shared<TileLinkSimuTarget<>>(4_b, 16_b, 4_b, 4_b, "target0");
	auto target1 = std::make_shared<TileLinkSimuTarget<>>(8_b, 16_b, 4_b, 4_b, "target1");
	auto target2 = std::make_shared<TileLinkSimuTarget<>>(2_b, 16_b, 4_b, 4_b, "target2");

	TileLinkDemux<TileLinkUL> demux;
	demux.attachSource(initiator->link());
	demux.attachSink(target0->link(), 0x000);
	demux.attachSink(target2->link(), 0x000);
	demux.attachSink(target1->link(), 0x100);
	demux.generate();

	addSimulationProcess([=]()->SimProcess {

		simu(valid(initiator->link().a)) = 0;
		simu(ready(*initiator->link().d)) = 1;
		simu(ready(target0->link().a)) = 1;
		simu(ready(target1->link().a)) = 1;
		simu(ready(target2->link().a)) = 1;
		simu(valid(*target0->link().d)) = 0;
		simu(valid(*target1->link().d)) = 0;
		simu(valid(*target2->link().d)) = 0;
		co_await WaitClk(clock);

		simu(valid(initiator->link().a)) = 1;
		initiator->issueCommand(TileLinkA::PutFullData, 0, 0, 1);
		co_await WaitClk(clock);
		BOOST_TEST(simu(valid(target0->link().a)) == 0);
		BOOST_TEST(simu(valid(target1->link().a)) == 0);
		BOOST_TEST(simu(valid(target2->link().a)) == 1);

		initiator->issueCommand(TileLinkA::PutFullData, 4, 0, 1);
		co_await WaitClk(clock);
		BOOST_TEST(simu(valid(target0->link().a)) == 1);
		BOOST_TEST(simu(valid(target1->link().a)) == 0);
		BOOST_TEST(simu(valid(target2->link().a)) == 0);

		initiator->issueCommand(TileLinkA::PutFullData, 16, 0, 1);
		co_await WaitClk(clock);
		BOOST_TEST(simu(valid(target0->link().a)) == 0);
		BOOST_TEST(simu(valid(target1->link().a)) == 0);
		BOOST_TEST(simu(valid(target2->link().a)) == 0);
		BOOST_TEST(simu(valid(*initiator->link().d)) == 1);
		BOOST_TEST(simu((*initiator->link().d)->error) == 1);

		initiator->issueCommand(TileLinkA::PutFullData, 256, 0, 1);
		co_await WaitClk(clock);
		BOOST_TEST(simu(valid(target0->link().a)) == 0);
		BOOST_TEST(simu(valid(target1->link().a)) == 1);
		BOOST_TEST(simu(valid(target2->link().a)) == 0);

		stopTest();
	});

	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
	runTicks(clock.getClk(), 16);
}

BOOST_FIXTURE_TEST_CASE(tilelink_errorResponder_test, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	auto initiator = std::make_shared<TileLinkSimuInitiator<>>(12_b, 16_b, 4_b, 4_b);

	scl::tileLinkErrorResponder(initiator->link());

	addSimulationProcess([=]()->SimProcess {

		auto& iniA = initiator->link().a;
		auto& iniD = *initiator->link().d;

		simu(valid(iniA)) = 0;
		simu(ready(iniD)) = 0;
		co_await WaitClk(clock);

		initiator->issueCommand(TileLinkA::PutFullData, 0, 0, 1);
		co_await WaitClk(clock);
		BOOST_TEST(simu(valid(iniD)) == 0);
		BOOST_TEST(simu(ready(iniA)) == 0);
		
		simu(valid(iniA)) = 1;
		co_await WaitClk(clock);
		BOOST_TEST(simu(valid(iniD)) == 1);
		BOOST_TEST(simu(ready(iniA)) == 0);
		BOOST_TEST(simu(iniD->opcode) == (size_t)TileLinkD::AccessAck);
		BOOST_TEST(simu(iniD->param) == 0);
		BOOST_TEST(simu(iniD->size) == 1);
		BOOST_TEST(simu(iniD->source) == 0);
		BOOST_TEST(simu(iniD->sink) == 0);

		simu(ready(iniD)) = 1;
		co_await WaitClk(clock);
		BOOST_TEST(simu(valid(iniD)) == 1);
		BOOST_TEST(simu(ready(iniA)) == 1);

		initiator->issueCommand(TileLinkA::Get, 0, 0, 1);
		co_await WaitClk(clock);
		BOOST_TEST(simu(valid(iniD)) == 1);
		BOOST_TEST(simu(ready(iniA)) == 1);
		BOOST_TEST(simu(iniD->opcode) == (size_t)TileLinkD::AccessAckData);
		BOOST_TEST(simu(iniD->param) == 0);
		BOOST_TEST(simu(iniD->size) == 1);
		BOOST_TEST(simu(iniD->source) == 0);
		BOOST_TEST(simu(iniD->sink) == 0);

		stopTest();
	});

	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
	runTicks(clock.getClk(), 16);
}

BOOST_FIXTURE_TEST_CASE(tilelink_errorResponder_burst_test, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	auto initiator = std::make_shared<TileLinkSimuInitiator<TileLinkUH>>(12_b, 16_b, 4_b, 4_b);
	scl::tileLinkErrorResponder(initiator->link());

	addSimulationProcess([=]()->SimProcess {

		auto& iniA = initiator->link().a;
		auto& iniD = *initiator->link().d;

		simu(valid(iniA)) = 0;
		simu(ready(iniD)) = 0;
		co_await WaitClk(clock);

		initiator->issueCommand(TileLinkA::PutFullData, 0, 0, 1);
		co_await WaitClk(clock);
		BOOST_TEST(simu(valid(iniD)) == 0);
		BOOST_TEST(simu(ready(iniA)) == 0);

		simu(valid(iniA)) = 1;
		co_await WaitClk(clock);
		BOOST_TEST(simu(valid(iniD)) == 1);
		BOOST_TEST(simu(ready(iniA)) == 0);
		BOOST_TEST(simu(iniD->opcode) == (size_t)TileLinkD::AccessAck);
		BOOST_TEST(simu(iniD->param) == 0);
		BOOST_TEST(simu(iniD->size) == 1);
		BOOST_TEST(simu(iniD->source) == 0);
		BOOST_TEST(simu(iniD->sink) == 0);
		BOOST_TEST(simu(iniD->error) == 1);

		simu(ready(iniD)) = 1;
		co_await WaitClk(clock);
		BOOST_TEST(simu(valid(iniD)) == 1);
		BOOST_TEST(simu(ready(iniA)) == 1);

		initiator->issueCommand(TileLinkA::Get, 0, 0, 1);
		co_await WaitClk(clock);
		BOOST_TEST(simu(valid(iniD)) == 1);
		BOOST_TEST(simu(ready(iniA)) == 1);
		BOOST_TEST(simu(iniD->opcode) == (size_t)TileLinkD::AccessAckData);
		BOOST_TEST(simu(iniD->param) == 0);
		BOOST_TEST(simu(iniD->size) == 1);
		BOOST_TEST(simu(iniD->source) == 0);
		BOOST_TEST(simu(iniD->sink) == 0);
		BOOST_TEST(simu(iniD->error) == 1);

		simu(ready(iniD)) = 0;
		initiator->issueCommand(TileLinkA::Get, 0, 0, 3);

		for (size_t i = 0; i < 5; ++i)
		{
			// no progress because ready low
			co_await WaitClk(clock);
			BOOST_TEST(simu(valid(iniD)) == 1);
			BOOST_TEST(simu(ready(iniA)) == 0);
			BOOST_TEST(simu(iniD->opcode) == (size_t)TileLinkD::AccessAckData);
			BOOST_TEST(simu(iniD->param) == 0);
			BOOST_TEST(simu(iniD->size) == 3);
			BOOST_TEST(simu(iniD->source) == 0);
			BOOST_TEST(simu(iniD->sink) == 0);
			BOOST_TEST(simu(iniD->error) == 0);
		}

		simu(ready(iniD)) = 1;
		co_await WaitFor(0);
		for (size_t i = 0; i < 4; ++i)
		{
			BOOST_TEST(simu(valid(iniD)) == 1);
			BOOST_TEST(simu(ready(iniA)) == (i == 3 ? 1 : 0));
			BOOST_TEST(simu(iniD->opcode) == (size_t)TileLinkD::AccessAckData);
			BOOST_TEST(simu(iniD->param) == 0);
			BOOST_TEST(simu(iniD->size) == 3);
			BOOST_TEST(simu(iniD->source) == 0);
			BOOST_TEST(simu(iniD->sink) == 0);
			BOOST_TEST(simu(iniD->error) == (i == 3 ? 1 : 0));
			co_await WaitClk(clock);
		}

		initiator->issueCommand(TileLinkA::Get, 0, 0, 2);
		co_await WaitFor(0);
		for (size_t i = 0; i < 2; ++i)
		{
			BOOST_TEST(simu(valid(iniD)) == 1);
			BOOST_TEST(simu(ready(iniA)) == i);
			BOOST_TEST(simu(iniD->opcode) == (size_t)TileLinkD::AccessAckData);
			BOOST_TEST(simu(iniD->param) == 0);
			BOOST_TEST(simu(iniD->size) == 2);
			BOOST_TEST(simu(iniD->source) == 0);
			BOOST_TEST(simu(iniD->sink) == 0);
			BOOST_TEST(simu(iniD->error) == i);
			co_await WaitClk(clock);
		}

		stopTest();
	});

	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
	runTicks(clock.getClk(), 24);
}

BOOST_FIXTURE_TEST_CASE(tilelink_mux_test, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	auto initiator0 = std::make_shared<TileLinkSimuInitiator<>>(12_b, 16_b, 4_b, 0_b, "initiator0");
	auto initiator1 = std::make_shared<TileLinkSimuInitiator<>>(12_b, 16_b, 4_b, 1_b, "initiator1");
	auto initiator2 = std::make_shared<TileLinkSimuInitiator<>>(12_b, 16_b, 4_b, 2_b, "initiator2");
	auto target = std::make_shared<TileLinkSimuTarget<>>(12_b, 16_b, 4_b, 4_b);

	TileLinkMux<TileLinkUL> mux;
	mux.attachSource(initiator0->link());
	mux.attachSource(initiator1->link());
	mux.attachSource(initiator2->link());
	target->link() <<= mux.generate();

	addSimulationProcess([=]()->SimProcess {

		initiator0->issueIdle();
		initiator1->issueIdle();
		initiator2->issueIdle();
		target->issueIdle();

		co_await WaitClk(clock);

		initiator0->issueCommand(TileLinkA::Get, 0, 0, 1, 0);
		initiator1->issueCommand(TileLinkA::Get, 0, 0, 1, 1);
		initiator2->issueCommand(TileLinkA::Get, 0, 0, 1, 2);
		co_await WaitClk(clock);
		BOOST_TEST(simu(ready(initiator0->link().a)) == 0);
		BOOST_TEST(simu(ready(initiator1->link().a)) == 0);
		BOOST_TEST(simu(ready(initiator2->link().a)) == 0);
		BOOST_TEST(simu(valid(target->link().a)) == 0);

		simu(valid(initiator2->link().a)) = 1;
		target->issueResponse(TileLinkD::AccessAckData, 0);
		co_await WaitClk(clock);
		BOOST_TEST(simu(ready(initiator0->link().a)) == 0);
		BOOST_TEST(simu(ready(initiator1->link().a)) == 0);
		BOOST_TEST(simu(ready(initiator2->link().a)) == 0);
		BOOST_TEST(simu(valid(target->link().a)) == 1);
		BOOST_TEST(simu(target->link().a->source) == 8 + 2);

		simu(ready(target->link().a)) = 1;
		co_await WaitClk(clock);
		BOOST_TEST(simu(ready(initiator0->link().a)) == 0);
		BOOST_TEST(simu(ready(initiator1->link().a)) == 0);
		BOOST_TEST(simu(ready(initiator2->link().a)) == 1);
		BOOST_TEST(simu(valid(target->link().a)) == 1);
		BOOST_TEST(simu(target->link().a->source) == 8 + 2);

		simu(valid(initiator2->link().a)) = 0;
		simu(valid(*target->link().d)) = 1;
		co_await WaitClk(clock);
		BOOST_TEST(simu(ready(*target->link().d)) == 0);
		BOOST_TEST(simu(valid(*initiator0->link().d)) == 0);
		BOOST_TEST(simu(valid(*initiator1->link().d)) == 0);
		BOOST_TEST(simu(valid(*initiator2->link().d)) == 1);

		simu(ready(*initiator2->link().d)) = 1;
		co_await WaitClk(clock);
		BOOST_TEST(simu(ready(*target->link().d)) == 1);
		BOOST_TEST(simu(valid(*initiator0->link().d)) == 0);
		BOOST_TEST(simu(valid(*initiator1->link().d)) == 0);
		BOOST_TEST(simu(valid(*initiator2->link().d)) == 1);

		simu(valid(*target->link().d)) = 0;
		co_await WaitClk(clock);

		simu(valid(initiator0->link().a)) = 1;
		simu(valid(initiator2->link().a)) = 1;
		co_await WaitClk(clock);
		BOOST_TEST(simu(ready(initiator0->link().a)) == 1);
		BOOST_TEST(simu(ready(initiator1->link().a)) == 0);
		BOOST_TEST(simu(ready(initiator2->link().a)) == 0);
		BOOST_TEST(simu(valid(target->link().a)) == 1);
		BOOST_TEST(simu(target->link().a->source) == 0);

		simu(valid(initiator0->link().a)) = 0;
		simu(valid(initiator2->link().a)) = 0;
		co_await WaitClk(clock);

		stopTest();
	});

	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
	runTicks(clock.getClk(), 16);
}

BOOST_FIXTURE_TEST_CASE(tilelink_hub_test, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	auto initiator0 = std::make_shared<TileLinkSimuInitiator<TileLinkUH>>(32_b, 32_b, 2_b, 2_b, "initiator0");
	auto initiator1 = std::make_shared<TileLinkSimuInitiator<TileLinkUH>>(32_b, 32_b, 2_b, 0_b, "initiator1");

	TileLinkHub<TileLinkUH> hub;
	hub.attachSource(initiator0->link());
	hub.attachSource(initiator1->link());

	auto target0 = std::make_shared<TileLinkSimuTarget<TileLinkUH>>(31_b, 32_b, 2_b, hub.sourceWidth(), "target0");
	auto target1 = std::make_shared<TileLinkSimuTarget<TileLinkUH>>(31_b, 32_b, 2_b, hub.sourceWidth(), "target1");
	hub.attachSink(target0->link(), 0x0000'0000);
	hub.attachSink(target1->link(), 0x8000'0000);

	hub.generate();

	addSimulationProcess([=]()->SimProcess {

		initiator0->issueIdle();
		initiator1->issueIdle();
		target0->issueIdle();
		target1->issueIdle();

		co_await WaitClk(clock);

		initiator0->issueCommand(TileLinkA::Get, 0, 0, 1, 0);
		simu(valid(initiator0->link().a)) = 1;
		initiator1->issueCommand(TileLinkA::Get, 0, 0, 3, 1);
		simu(valid(initiator1->link().a)) = 1;
		co_await WaitClk(clock);
		BOOST_TEST(simu(valid(target0->link().a)) == 1);
		BOOST_TEST(simu(valid(target1->link().a)) == 0);

		initiator0->issueCommand(TileLinkA::PutFullData, 0, 0, 3, 0);
		simu(ready(target0->link().a)) = 1;
		co_await WaitClk(clock);
		target0->issueResponse(TileLinkD::AccessAck, 0);
		co_await WaitClk(clock);
		simu(valid(initiator0->link().a)) = 0;
		simu(ready(target0->link().a)) = 0;

		co_await WaitClk(clock);
		simu(valid(*target0->link().d)) = 1;
		simu(ready(*initiator0->link().d)) = 1;

		BOOST_TEST(simu(valid(target0->link().a)) == 1);
		simu(ready(target0->link().a)) = 1;

		co_await WaitClk(clock);
		BOOST_TEST(simu(valid(*initiator0->link().d)) == 1);
		target0->issueResponse(TileLinkD::AccessAckData, 0xC0FFEE00u);

		simu(valid(*target0->link().d)) = 0;
		simu(valid(initiator1->link().a)) = 0;
		simu(ready(target0->link().a)) = 0;
		co_await WaitClk(clock);

		simu(valid(*target0->link().d)) = 1;
		simu(ready(*initiator1->link().d)) = 1;
		co_await WaitClk(clock);
		co_await WaitClk(clock);
		simu(valid(*target0->link().d)) = 0;


		co_await WaitClk(clock);
		stopTest();
	});

	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
	runTicks(clock.getClk(), 16);

#if 0
	vhdl::VHDLExport vhdl("test_export/tilelink_hub_test/tilelink_hub_test.vhd");
	vhdl.targetSynthesisTool(new IntelQuartus());
	vhdl.writeStandAloneProjectFile("tilelink_hub_test.qsf");
	vhdl.writeClocksFile("tilelink_hub_test.sdc");
	vhdl(design.getCircuit());
#endif
}

BOOST_FIXTURE_TEST_CASE(tilelink_memory_test, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	auto initiator = std::make_shared<TileLinkSimuInitiator<>>(12_b, 16_b, 2_b, 1_b);
	Memory<BVec> mem(1ull << 12, 16_b);
	mem <<= initiator->link();

	addSimulationProcess([=]()->SimProcess {
		initiator->issueIdle();
		simu(ready(*initiator->link().d)) = 1;
		co_await WaitClk(clock);
		BOOST_TEST(simu(ready(initiator->link().a)) == 1);

		simu(valid(initiator->link().a)) = 1;
		for (size_t i = 0; i < 16; ++i)
		{
			size_t tag = i % 2;
			size_t size = i % 3 == 0 ? 0 : 1;
			size_t data = ((0xCD ^ i) << 8) | i;

			initiator->issueCommand(TileLinkA::PutFullData, i * 2, data, size, tag);
			co_await WaitClk(clock);

			auto& d = *initiator->link().d;
			BOOST_TEST(simu(valid(d)) == 1);
			BOOST_TEST(simu(d->opcode) == (size_t)TileLinkD::AccessAck);
			BOOST_TEST(simu(d->source) == tag);
			BOOST_TEST(simu(d->size) == size);
			BOOST_TEST(simu(d->error) == 0);
		}
		simu(valid(initiator->link().a)) = 0;
		co_await WaitClk(clock);
		BOOST_TEST(simu(valid(*initiator->link().d)) == 0);

		simu(valid(initiator->link().a)) = 1;
		for (size_t i = 0; i < 16; ++i)
		{
			initiator->issueCommand(TileLinkA::Get, i * 2, 0, 1, i % 2);
			co_await WaitClk(clock);

			size_t tag = i % 2;
			size_t size = i % 3 == 0 ? 0 : 1;
			size_t data = ((0xCD ^ i) << 8) | i;

			auto& d = *initiator->link().d;
			BOOST_TEST(simu(valid(d)) == 1);
			BOOST_TEST(simu(d->opcode) == (size_t)TileLinkD::AccessAckData);
			BOOST_TEST(simu(d->source) == tag);
			BOOST_TEST(simu(d->error) == 0);

			auto&& dataSig = simu(d->data);
			if (size == 0)
			{
				BOOST_TEST(dataSig.defined() == 0xFF);
				BOOST_TEST(dataSig.value() == i);
			}
			else
			{
				BOOST_TEST(dataSig.allDefined());
				BOOST_TEST(dataSig == data);
			}
		}
		simu(valid(initiator->link().a)) = 0;

		co_await WaitClk(clock);
		stopTest();
	});

	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
	runTicks(clock.getClk(), 128);
}
