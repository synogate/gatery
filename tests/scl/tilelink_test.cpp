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

#include <gatery/scl/synthesisTools/IntelQuartus.h>

#include <gatery/scl/stream/SimuHelpers.h>
#include <gatery/scl/tilelink/tilelink.h>
#include <gatery/scl/tilelink/TileLinkDMA.h>
#include <gatery/scl/tilelink/TileLinkHub.h>
#include <gatery/scl/tilelink/TileLinkErrorResponder.h>
#include <gatery/scl/tilelink/TileLinkFiFo.h>
#include <gatery/scl/tilelink/TileLinkStreamFetch.h>
#include <gatery/scl/tilelink/TileLinkMasterModel.h>
#include <gatery/scl/tilelink/TileLinkAdapter.h>

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
		simu(valid(m_link.a)) = '0';
		simu(ready(*m_link.d)) = '0';
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
		simu(ready(m_link.a)) = '0';
		simu(valid(*m_link.d)) = '0';
	}

	void issueResponse(TileLinkD::OpCode code, uint64_t data, bool error = false)
	{
		simu((*m_link.d)->opcode) = (uint64_t)code;
		simu((*m_link.d)->param) = 0;
		simu((*m_link.d)->size) = simu(m_link.a->size);
		simu((*m_link.d)->source) = simu(m_link.a->source);
		simu((*m_link.d)->sink) = 0;
		simu((*m_link.d)->error) = error;
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
		co_await AfterClk(clock);

		stopTest();
	});

	design.postprocess();
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

	addSimulationProcess([=, this]()->SimProcess {

		auto& iniA = initiator->link().a;
		auto& iniD = *initiator->link().d;
		auto& tgtA = target->link().a;
		auto& tgtD = *target->link().d;

		simu(valid(iniA)) = '0';
		simu(ready(iniD)) = '0';
		simu(ready(tgtA)) = '0';
		simu(valid(tgtD)) = '0';
		co_await AfterClk(clock);
		BOOST_TEST(simu(valid(tgtA)) == '0');

		initiator->issueCommand(TileLinkA::PutFullData, 0, 0, 1);
		co_await AfterClk(clock);
		BOOST_TEST(simu(valid(tgtA)) == '0');
		BOOST_TEST(simu(ready(iniA)) == '0');

		simu(valid(initiator->link().a)) = '1';
		co_await AfterClk(clock);
		BOOST_TEST(simu(valid(tgtA)) == '1');
		BOOST_TEST(simu(ready(iniA)) == '0');

		simu(ready(tgtA)) = '1';
		co_await AfterClk(clock);
		BOOST_TEST(simu(valid(tgtA)) == '1');
		BOOST_TEST(simu(ready(iniA)) == '1');

		stopTest();
	});

	design.postprocess();
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

	addSimulationProcess([=, this]()->SimProcess {

		auto& iniA = initiator->link().a;
		auto& iniD = *initiator->link().d;
		auto& tgtA = target->link().a;
		auto& tgtD = *target->link().d;

		simu(valid(iniA)) = '0';
		simu(ready(iniD)) = '0';
		simu(ready(tgtA)) = '0';
		simu(valid(tgtD)) = '0';
		co_await AfterClk(clock);
		BOOST_TEST(simu(valid(iniD)) == '0');
		BOOST_TEST(simu(ready(tgtD)) == '0');

		initiator->issueCommand(TileLinkA::PutFullData, 0, 0, 1);
		target->issueResponse(TileLinkD::AccessAck, 1337);
		co_await AfterClk(clock);
		BOOST_TEST(simu(valid(iniD)) == '0');
		BOOST_TEST(simu(ready(tgtD)) == '0');

		simu(valid(tgtD)) = '1';
		co_await AfterClk(clock);
		BOOST_TEST(simu(valid(iniD)) == '1');
		BOOST_TEST(simu(ready(tgtD)) == '0');

		simu(ready(iniD)) = '1';
		co_await AfterClk(clock);
		BOOST_TEST(simu(valid(iniD)) == '1');
		BOOST_TEST(simu(ready(tgtD)) == '1');

		stopTest();
	});

	design.postprocess();
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

	addSimulationProcess([=, this]()->SimProcess {

		simu(valid(initiator->link().a)) = '0';
		simu(ready(*initiator->link().d)) = '1';
		simu(ready(target0->link().a)) = '1';
		simu(ready(target1->link().a)) = '1';
		simu(ready(target2->link().a)) = '1';
		simu(valid(*target0->link().d)) = '0';
		simu(valid(*target1->link().d)) = '0';
		simu(valid(*target2->link().d)) = '0';
		co_await AfterClk(clock);

		simu(valid(initiator->link().a)) = '1';
		initiator->issueCommand(TileLinkA::PutFullData, 0, 0, 1);
		co_await AfterClk(clock);
		BOOST_TEST(simu(valid(target0->link().a)) == '0');
		BOOST_TEST(simu(valid(target1->link().a)) == '0');
		BOOST_TEST(simu(valid(target2->link().a)) == '1');

		initiator->issueCommand(TileLinkA::PutFullData, 4, 0, 1);
		co_await AfterClk(clock);
		BOOST_TEST(simu(valid(target0->link().a)) == '1');
		BOOST_TEST(simu(valid(target1->link().a)) == '0');
		BOOST_TEST(simu(valid(target2->link().a)) == '0');

		initiator->issueCommand(TileLinkA::PutFullData, 16, 0, 1);
		co_await AfterClk(clock);
		BOOST_TEST(simu(valid(target0->link().a)) == '0');
		BOOST_TEST(simu(valid(target1->link().a)) == '0');
		BOOST_TEST(simu(valid(target2->link().a)) == '0');
		BOOST_TEST(simu(valid(*initiator->link().d)) == '1');
		BOOST_TEST(simu((*initiator->link().d)->error) == '1');

		initiator->issueCommand(TileLinkA::PutFullData, 256, 0, 1);
		co_await AfterClk(clock);
		BOOST_TEST(simu(valid(target0->link().a)) == '0');
		BOOST_TEST(simu(valid(target1->link().a)) == '1');
		BOOST_TEST(simu(valid(target2->link().a)) == '0');

		stopTest();
	});

	design.postprocess();
	runTicks(clock.getClk(), 16);
}

BOOST_FIXTURE_TEST_CASE(tilelink_errorResponder_test, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	auto initiator = std::make_shared<TileLinkSimuInitiator<>>(12_b, 16_b, 4_b, 4_b);

	scl::tileLinkErrorResponder(initiator->link());

	addSimulationProcess([=, this]()->SimProcess {

		auto& iniA = initiator->link().a;
		auto& iniD = *initiator->link().d;

		simu(valid(iniA)) = '0';
		simu(ready(iniD)) = '0';
		co_await AfterClk(clock);

		initiator->issueCommand(TileLinkA::PutFullData, 0, 0, 1);
		co_await AfterClk(clock);
		BOOST_TEST(simu(valid(iniD)) == '0');
		BOOST_TEST(simu(ready(iniA)) == '0');
		
		simu(valid(iniA)) = '1';
		co_await AfterClk(clock);
		BOOST_TEST(simu(valid(iniD)) == '1');
		BOOST_TEST(simu(ready(iniA)) == '0');
		BOOST_TEST(simu(iniD->opcode) == (size_t)TileLinkD::AccessAck);
		BOOST_TEST(simu(iniD->param) == 0);
		BOOST_TEST(simu(iniD->size) == 1);
		BOOST_TEST(simu(iniD->source) == 0);
		BOOST_TEST(simu(iniD->sink) == 0);

		simu(ready(iniD)) = '1';
		co_await AfterClk(clock);
		BOOST_TEST(simu(valid(iniD)) == '1');
		BOOST_TEST(simu(ready(iniA)) == '1');

		initiator->issueCommand(TileLinkA::Get, 0, 0, 1);
		co_await AfterClk(clock);
		BOOST_TEST(simu(valid(iniD)) == '1');
		BOOST_TEST(simu(ready(iniA)) == '1');
		BOOST_TEST(simu(iniD->opcode) == (size_t)TileLinkD::AccessAckData);
		BOOST_TEST(simu(iniD->param) == 0);
		BOOST_TEST(simu(iniD->size) == 1);
		BOOST_TEST(simu(iniD->source) == 0);
		BOOST_TEST(simu(iniD->sink) == 0);

		stopTest();
	});

	design.postprocess();
	runTicks(clock.getClk(), 16);
}

BOOST_FIXTURE_TEST_CASE(tilelink_errorResponder_burst_test, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	auto initiator = std::make_shared<TileLinkSimuInitiator<TileLinkUH>>(12_b, 16_b, 4_b, 4_b);
	scl::tileLinkErrorResponder(initiator->link());

	addSimulationProcess([=, this]()->SimProcess {

		auto& iniA = initiator->link().a;
		auto& iniD = *initiator->link().d;

		simu(valid(iniA)) = '0';
		simu(ready(iniD)) = '0';
		co_await AfterClk(clock);

		initiator->issueCommand(TileLinkA::PutFullData, 0, 0, 1);
		co_await AfterClk(clock);
		BOOST_TEST(simu(valid(iniD)) == '0');
		BOOST_TEST(simu(ready(iniA)) == '0');

		simu(valid(iniA)) = '1';
		co_await AfterClk(clock);
		BOOST_TEST(simu(valid(iniD)) == '1');
		BOOST_TEST(simu(ready(iniA)) == '0');
		BOOST_TEST(simu(iniD->opcode) == (size_t)TileLinkD::AccessAck);
		BOOST_TEST(simu(iniD->param) == 0);
		BOOST_TEST(simu(iniD->size) == 1);
		BOOST_TEST(simu(iniD->source) == 0);
		BOOST_TEST(simu(iniD->sink) == 0);
		BOOST_TEST(simu(iniD->error) == '1');

		simu(ready(iniD)) = '1';
		co_await AfterClk(clock);
		BOOST_TEST(simu(valid(iniD)) == '1');
		BOOST_TEST(simu(ready(iniA)) == '1');

		initiator->issueCommand(TileLinkA::Get, 0, 0, 1);
		co_await AfterClk(clock);
		BOOST_TEST(simu(valid(iniD)) == '1');
		BOOST_TEST(simu(ready(iniA)) == '1');
		BOOST_TEST(simu(iniD->opcode) == (size_t)TileLinkD::AccessAckData);
		BOOST_TEST(simu(iniD->param) == 0);
		BOOST_TEST(simu(iniD->size) == 1);
		BOOST_TEST(simu(iniD->source) == 0);
		BOOST_TEST(simu(iniD->sink) == 0);
		BOOST_TEST(simu(iniD->error) == '1');

		simu(ready(iniD)) = '0';
		initiator->issueCommand(TileLinkA::Get, 0, 0, 3);

		for (size_t i = 0; i < 5; ++i)
		{
			// no progress because ready low
			co_await AfterClk(clock);
			BOOST_TEST(simu(valid(iniD)) == '1');
			BOOST_TEST(simu(ready(iniA)) == '0');
			BOOST_TEST(simu(iniD->opcode) == (size_t)TileLinkD::AccessAckData);
			BOOST_TEST(simu(iniD->param) == 0);
			BOOST_TEST(simu(iniD->size) == 3);
			BOOST_TEST(simu(iniD->source) == 0);
			BOOST_TEST(simu(iniD->sink) == 0);
			BOOST_TEST(simu(iniD->error) == '0');
		}

		simu(ready(iniD)) = '1';
		co_await WaitStable();
		for (size_t i = 0; i < 4; ++i)
		{
			BOOST_TEST(simu(valid(iniD)) == '1');
			BOOST_TEST(simu(ready(iniA)) == (i == 3));
			BOOST_TEST(simu(iniD->opcode) == (size_t)TileLinkD::AccessAckData);
			BOOST_TEST(simu(iniD->param) == 0);
			BOOST_TEST(simu(iniD->size) == 3);
			BOOST_TEST(simu(iniD->source) == 0);
			BOOST_TEST(simu(iniD->sink) == 0);
			BOOST_TEST(simu(iniD->error) == (i == 3));
			co_await AfterClk(clock);
		}

		initiator->issueCommand(TileLinkA::Get, 0, 0, 2);
		co_await WaitStable();
		for (size_t i = 0; i < 2; ++i)
		{
			BOOST_TEST(simu(valid(iniD)) == '1');
			BOOST_TEST(simu(ready(iniA)) == (i != 0));
			BOOST_TEST(simu(iniD->opcode) == (size_t)TileLinkD::AccessAckData);
			BOOST_TEST(simu(iniD->param) == 0);
			BOOST_TEST(simu(iniD->size) == 2);
			BOOST_TEST(simu(iniD->source) == 0);
			BOOST_TEST(simu(iniD->sink) == 0);
			BOOST_TEST(simu(iniD->error) == (i != 0));
			co_await AfterClk(clock);
		}

		stopTest();
	});

	design.postprocess();
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

	addSimulationProcess([=, this]()->SimProcess {

		initiator0->issueIdle();
		initiator1->issueIdle();
		initiator2->issueIdle();
		target->issueIdle();

		co_await AfterClk(clock);

		initiator0->issueCommand(TileLinkA::Get, 0, 0, 1, 0);
		initiator1->issueCommand(TileLinkA::Get, 0, 0, 1, 1);
		initiator2->issueCommand(TileLinkA::Get, 0, 0, 1, 2);
		co_await AfterClk(clock);
		BOOST_TEST(simu(ready(initiator0->link().a)) == '0');
		BOOST_TEST(simu(ready(initiator1->link().a)) == '0');
		BOOST_TEST(simu(ready(initiator2->link().a)) == '0');
		BOOST_TEST(simu(valid(target->link().a)) == '0');

		simu(valid(initiator2->link().a)) = '1';
		co_await WaitFor(Seconds{1,10}/clock.absoluteFrequency());
		target->issueResponse(TileLinkD::AccessAckData, 0);
		co_await AfterClk(clock);
		BOOST_TEST(simu(ready(initiator0->link().a)) == '0');
		BOOST_TEST(simu(ready(initiator1->link().a)) == '0');
		BOOST_TEST(simu(ready(initiator2->link().a)) == '0');
		BOOST_TEST(simu(valid(target->link().a)) == '1');
		BOOST_TEST(simu(target->link().a->source) == 8 + 2);

		simu(ready(target->link().a)) = '1';
		co_await AfterClk(clock);
		BOOST_TEST(simu(ready(initiator0->link().a)) == '0');
		BOOST_TEST(simu(ready(initiator1->link().a)) == '0');
		BOOST_TEST(simu(ready(initiator2->link().a)) == '1');
		BOOST_TEST(simu(valid(target->link().a)) == '1');
		BOOST_TEST(simu(target->link().a->source) == 8 + 2);

		simu(valid(initiator2->link().a)) = '0';
		simu(valid(*target->link().d)) = '1';
		co_await AfterClk(clock);
		BOOST_TEST(simu(ready(*target->link().d)) == '0');
		BOOST_TEST(simu(valid(*initiator0->link().d)) == '0');
		BOOST_TEST(simu(valid(*initiator1->link().d)) == '0');
		BOOST_TEST(simu(valid(*initiator2->link().d)) == '1');

		simu(ready(*initiator2->link().d)) = '1';
		co_await AfterClk(clock);
		BOOST_TEST(simu(ready(*target->link().d)) == '1');
		BOOST_TEST(simu(valid(*initiator0->link().d)) == '0');
		BOOST_TEST(simu(valid(*initiator1->link().d)) == '0');
		BOOST_TEST(simu(valid(*initiator2->link().d)) == '1');

		simu(valid(*target->link().d)) = '0';
		co_await AfterClk(clock);

		simu(valid(initiator0->link().a)) = '1';
		simu(valid(initiator2->link().a)) = '1';
		co_await AfterClk(clock);
		BOOST_TEST(simu(ready(initiator0->link().a)) == '1');
		BOOST_TEST(simu(ready(initiator1->link().a)) == '0');
		BOOST_TEST(simu(ready(initiator2->link().a)) == '0');
		BOOST_TEST(simu(valid(target->link().a)) == '1');
		BOOST_TEST(simu(target->link().a->source) == 0);

		simu(valid(initiator0->link().a)) = '0';
		simu(valid(initiator2->link().a)) = '0';
		co_await AfterClk(clock);

		stopTest();
	});

	
	try {
		design.postprocess();
	} catch (...) {
		dbg::vis();
	}
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

	addSimulationProcess([=, this]()->SimProcess {

		initiator0->issueIdle();
		initiator1->issueIdle();
		target0->issueIdle();
		target1->issueIdle();

		co_await AfterClk(clock);

		initiator0->issueCommand(TileLinkA::Get, 0, 0, 1, 0);
		simu(valid(initiator0->link().a)) = '1';
		initiator1->issueCommand(TileLinkA::Get, 0, 0, 3, 1);
		simu(valid(initiator1->link().a)) = '1';
		co_await AfterClk(clock);
		BOOST_TEST(simu(valid(target0->link().a)) == '1');
		BOOST_TEST(simu(valid(target1->link().a)) == '0');

		initiator0->issueCommand(TileLinkA::PutFullData, 0, 0, 3, 0);
		simu(ready(target0->link().a)) = '1';
		co_await AfterClk(clock);
		target0->issueResponse(TileLinkD::AccessAck, 0);
		co_await AfterClk(clock);
		simu(valid(initiator0->link().a)) = '0';
		simu(ready(target0->link().a)) = '0';

		co_await AfterClk(clock);
		simu(valid(*target0->link().d)) = '1';
		simu(ready(*initiator0->link().d)) = '1';

		BOOST_TEST(simu(valid(target0->link().a)) == '1');
		simu(ready(target0->link().a)) = '1';

		co_await AfterClk(clock);
		BOOST_TEST(simu(valid(*initiator0->link().d)) == '1');
		target0->issueResponse(TileLinkD::AccessAckData, 0xC0FFEE00u);

		simu(valid(*target0->link().d)) = '0';
		simu(valid(initiator1->link().a)) = '0';
		simu(ready(target0->link().a)) = '0';
		co_await AfterClk(clock);

		simu(valid(*target0->link().d)) = '1';
		simu(ready(*initiator1->link().d)) = '1';
		co_await AfterClk(clock);
		co_await AfterClk(clock);
		simu(valid(*target0->link().d)) = '0';


		co_await AfterClk(clock);
		stopTest();
	});

	design.postprocess();
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
	setName(ready(*initiator->link().d), "DD");
	mem <<= initiator->link();
	setName(ready(*initiator->link().d), "DE");

	addSimulationProcess([=, this]()->SimProcess {
		initiator->issueIdle();
		simu(ready(*initiator->link().d)) = '1';
		co_await AfterClk(clock);
		BOOST_TEST(simu(ready(initiator->link().a)) == '1');

		simu(valid(initiator->link().a)) = '1';
		for (size_t i = 0; i < 16; ++i)
		{
			size_t tag = i % 2;
			size_t size = i % 3 == 0 ? 0 : 1;
			size_t data = ((0xCD ^ i) << 8) | i;

			initiator->issueCommand(TileLinkA::PutFullData, i * 2, data, size, tag);
			co_await AfterClk(clock);

			auto& d = *initiator->link().d;
			BOOST_TEST(simu(valid(d)) == '1');
			BOOST_TEST(simu(d->opcode) == (size_t)TileLinkD::AccessAck);
			BOOST_TEST(simu(d->source) == tag);
			BOOST_TEST(simu(d->size) == size);
			BOOST_TEST(simu(d->error) == '0');
		}
		simu(valid(initiator->link().a)) = '0';
		co_await AfterClk(clock);
		BOOST_TEST(simu(valid(*initiator->link().d)) == '0');

		simu(valid(initiator->link().a)) = '1';
		for (size_t i = 0; i < 16; ++i)
		{
			initiator->issueCommand(TileLinkA::Get, i * 2, 0, 1, i % 2);
			co_await AfterClk(clock);

			size_t tag = i % 2;
			size_t size = i % 3 == 0 ? 0 : 1;
			size_t data = ((0xCD ^ i) << 8) | i;

			auto& d = *initiator->link().d;
			BOOST_TEST(simu(valid(d)) == '1');
			BOOST_TEST(simu(d->opcode) == (size_t)TileLinkD::AccessAckData);
			BOOST_TEST(simu(d->source) == tag);
			BOOST_TEST(simu(d->error) == '0');

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
		simu(valid(initiator->link().a)) = '0';
		co_await AfterClk(clock);

		simu(ready(*initiator->link().d)) = '0';
		co_await AfterClk(clock);
		BOOST_TEST(simu(ready(initiator->link().a)) == '1');

		initiator->issueCommand(TileLinkA::Get, 0, 0, 1, 1);
		simu(valid(initiator->link().a)) = '1';
		co_await AfterClk(clock);
		BOOST_TEST(simu(ready(initiator->link().a)) == '1');
		BOOST_TEST(simu(valid(*initiator->link().d)) == '1');

		initiator->issueCommand(TileLinkA::Get, 2, 0, 0, 0);
		for (size_t i = 0; i < 5; ++i)
		{
			co_await AfterClk(clock);
			simu(valid(initiator->link().a)) = '0';
			BOOST_TEST(simu(ready(initiator->link().a)) == '0');
			BOOST_TEST(simu(valid(*initiator->link().d)) == '1');
			BOOST_TEST(simu((*initiator->link().d)->source) == 1);
		}

		simu(ready(*initiator->link().d)) = '1';
		co_await AfterClk(clock);
		simu(ready(*initiator->link().d)) = '0';

		for (size_t i = 0; i < 5; ++i)
		{
			co_await AfterClk(clock);
			BOOST_TEST(simu(ready(initiator->link().a)) == '0');
			BOOST_TEST(simu(valid(*initiator->link().d)) == '1');
			BOOST_TEST(simu((*initiator->link().d)->source) == 0);
		}

		simu(ready(*initiator->link().d)) = '1';
		co_await AfterClk(clock);
		BOOST_TEST(simu(valid(*initiator->link().d)) == '0');

		co_await AfterClk(clock);
		stopTest();
	});

	design.postprocess();
	runTicks(clock.getClk(), 128);
}

BOOST_FIXTURE_TEST_CASE(tilelink_stream_fetch_test, BoostUnitTestSimulationFixture)
{
	Clock clock({ 
		.absoluteFrequency = 100'000'000,
		.memoryResetType = ClockConfig::ResetType::NONE
	});
	ClockScope clkScp(clock);

	std::vector<uint8_t> memData;
	for (uint16_t i = 0; i < 512; ++i)
		memData.push_back((uint8_t)i);

	Memory<BVec> mem(256, 16_b);
	mem.fillPowerOnState(sim::createDefaultBitVectorState(memData.size()*8, memData.data()));

	RvStream<TileLinkStreamFetch::Command> cmd;
	cmd->address = mem.addressWidth();
	cmd->beats = 4_b;
	pinIn(cmd, "cmd");

	RvStream<BVec> data{ 16_b };
	pinOut(data, "data");
	TileLinkUB memUB = TileLinkStreamFetch{}.generate(cmd, data, 1_b);
	mem <<= (TileLinkUL&) memUB;

	std::array<std::array<size_t, 2>, 3> testCases = {
		std::array<size_t, 2>{ 16, 4 },
		std::array<size_t, 2>{ 32, 1 },
		std::array<size_t, 2>{ 40, 8 },
	};

	addSimulationProcess([&]()->SimProcess {
		for (const auto& tc : testCases)
		{
			simu(cmd->address) = tc[0];
			simu(cmd->beats) = tc[1];
			co_await performTransfer(cmd, clock);
		}
	});

	addSimulationProcess([&]()->SimProcess {
		fork(scl::strm::readyDriverRNG(data, clock, 80));

		for(const auto& tc : testCases)
			for (size_t i = 0; i < tc[1]; ++i)
			{
				const size_t expectedByte = tc[0] + i * 2;
				const size_t expectedWord = ((expectedByte + 1) << 8) | expectedByte;
				auto a = co_await scl::strm::receivePacket(data, clock);
				BOOST_TEST(a.asUint64(data->width()) == expectedWord);
			}
		
		co_await OnClk(clock);
		BOOST_TEST(simu(valid(data)) == '0');
		stopTest();
	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 1, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(tilelink_burst_stream_fetch_test, BoostUnitTestSimulationFixture)
{
	Clock clock({ 
		.absoluteFrequency = 100'000'000,
		.memoryResetType = ClockConfig::ResetType::NONE
		});
	ClockScope clkScp(clock);

	std::vector<uint8_t> memData;
	for (uint16_t i = 0; i < 512; ++i)
		memData.push_back((uint8_t)i);

	Memory<BVec> mem(256, 16_b);
	mem.fillPowerOnState(sim::createDefaultBitVectorState(memData.size()*8, memData.data()));

	RvStream<TileLinkStreamFetch::Command> cmd;
	cmd->address = mem.addressWidth();
	cmd->beats = 5_b;
	pinIn(cmd, "cmd");

	RvStream<BVec> data{ 16_b };
	pinOut(data, "data");

	TileLinkUB fetcher = TileLinkStreamFetch{}.enableBursts(64).generate(cmd, data, 0_b); //maximum burst size is 4 beats ->64 bits

	TileLinkUL memTL = tileLinkInit<TileLinkUL>(fetcher.a->address.width(), fetcher.a->data.width(), fetcher.a->source.width() + 5_b);
	mem <<= memTL;
	tileLinkAddBurst(memTL, fetcher.a->size.width()) <<= fetcher;

	std::array<std::array<size_t, 2>, 3> testCases = {
		std::array<size_t, 2>{ 16, 4 },
		std::array<size_t, 2>{ 32, 8 },
		std::array<size_t, 2>{ 0, 16 },
	};

	addSimulationProcess([&]()->SimProcess {
		for (const auto& tc : testCases)
		{
			simu(cmd->address) = tc[0];
			simu(cmd->beats) = tc[1];
			co_await performTransfer(cmd, clock);
		}
		});

	addSimulationProcess([&]()->SimProcess {
		fork(scl::strm::readyDriverRNG(data, clock, 80));

		for(const auto& tc : testCases)
			for (size_t i = 0; i < tc[1]; ++i)
			{
				const size_t expectedByte = tc[0] + i * 2;
				const size_t expectedWord = ((expectedByte + 1) << 8) | expectedByte;
				auto a = co_await scl::strm::receivePacket(data, clock);
				BOOST_TEST(a.asUint64(data->width()) == expectedWord);
			}

		co_await OnClk(clock);
		BOOST_TEST(simu(valid(data)) == '0');
		stopTest();
		});

	if (false) recordVCD("dut.vcd");
	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 5, 1'000'000 }));
}

class LinkTest : public ClockedTest
{
public:
	LinkTest() : link(linkModel.getLink())
	{
	}

	scl::TileLinkMasterModel linkModel;
	scl::TileLinkUB& link;
};

BOOST_FIXTURE_TEST_CASE(tilelink_addburst_test, LinkTest)
{
	Memory<BVec> mem(256, 8_b);
	
	scl::TileLinkUL memLink;
	scl::tileLinkInit(memLink, 8_b, 8_b, 0_b, 6_b);
	mem <<= memLink;

	scl::TileLinkUB memBurst = scl::tileLinkAddBurst(memLink, 2_b);

	linkModel.init("m", 8_b, 8_b, 2_b);
	memBurst <<= link;
	timeout({ 1, 1'000'000 });

	addSimulationProcess([&]()->SimProcess {
		
		co_await OnClk(clock());

		fork(linkModel.put(0, 2, 0xDDCCBBAA, clock()));
		
		auto [val, def, err] = co_await linkModel.get(0, 2, clock());
		BOOST_TEST(!err);
		BOOST_TEST((val & def) == 0xDDCCBBAA);

		co_await OnClk(clock());
		stopTest();
	});
}

BOOST_FIXTURE_TEST_CASE(tilelink_addburst2_test, LinkTest)
{
	Memory<BVec> mem(256, 16_b);

	scl::TileLinkUL memLink;
	scl::tileLinkInit(memLink, 8_b, 16_b, 1_b, 12_b);
	mem <<= memLink;

	scl::TileLinkUB memBurst = scl::tileLinkAddBurst(memLink, 3_b);

	linkModel.init("m", 8_b, 16_b, 3_b, 5_b);
	memBurst <<= link;
	timeout({ 1, 1'000'000 });

	addSimulationProcess([&]()->SimProcess {

		co_await OnClk(clock());

		fork(linkModel.put(0, 2, 0xDDCCBBAA, clock()));
		fork(linkModel.put(4, 1, 0xFFEE, clock()));
		fork(linkModel.put(6, 0, 0x11, clock()));
		fork(linkModel.put(7, 0, 0x22, clock()));

		{
			auto [val, def, err] = co_await linkModel.get(0, 3, clock());
			BOOST_TEST(!err);
			BOOST_TEST((val & def) == 0x2211FFEEDDCCBBAA);
		}

		{
			auto [val, def, err] = co_await linkModel.get(0, 2, clock());
			BOOST_TEST(!err);
			BOOST_TEST((val & def) == 0xDDCCBBAA);
		}

		{
			auto [val, def, err] = co_await linkModel.get(2, 1, clock());
			BOOST_TEST(!err);
			BOOST_TEST((val & def) == 0xDDCC);
		}

		{
			auto [val, def, err] = co_await linkModel.get(1, 0, clock());
			BOOST_TEST(!err);
			BOOST_TEST((val & def) == 0xBB);
		}

		co_await OnClk(clock());
		stopTest();
	});
}

BOOST_FIXTURE_TEST_CASE(tilelink_doublewidth_test, LinkTest)
{
	Memory<BVec> mem(256, 8_b);

	scl::TileLinkUL memLink;
	scl::tileLinkInit(memLink, 8_b, 8_b, 0_b, 6_b);
	mem <<= memLink;

	scl::TileLinkUB memBurst = scl::tileLinkAddBurst(memLink, 2_b);
	scl::TileLinkUB memDouble = scl::tileLinkDoubleWidth(memBurst);

	linkModel.init("m", 8_b, 16_b, 2_b);
	memDouble <<= link;
	timeout({ 1, 1'000'000 });

	addSimulationProcess([&]()->SimProcess {
		co_await OnClk(clock());

		fork(linkModel.put(0, 2, 0xDDCCBBAA, clock()));
		fork(linkModel.put(4, 1, 0xFFEE, clock()));
		fork(linkModel.put(6, 0, 0x11, clock()));
		fork(linkModel.put(7, 0, 0x22, clock()));

		{
			auto [val, def, err] = co_await linkModel.get(0, 3, clock());
			BOOST_TEST(!err);
			BOOST_TEST((val & def) == 0x2211FFEEDDCCBBAA);
		}

		{
			auto [val, def, err] = co_await linkModel.get(0, 2, clock());
			BOOST_TEST(!err);
			BOOST_TEST((val & def) == 0xDDCCBBAA);
		}

		{
			auto [val, def, err] = co_await linkModel.get(2, 1, clock());
			BOOST_TEST(!err);
			BOOST_TEST((val & def) == 0xDDCC);
		}

		{
			auto [val, def, err] = co_await linkModel.get(1, 0, clock());
			BOOST_TEST(!err);
			BOOST_TEST((val & def) == 0xBB);
		}

		co_await OnClk(clock());
		stopTest();
	});
}

BOOST_FIXTURE_TEST_CASE(tilelink_fifo_test, LinkTest)
{
	Memory<BVec> mem(256, 8_b);

	auto memLink = scl::tileLinkInit<scl::TileLinkUL>(8_b, 8_b, 2_b);
	mem <<= memLink;

	auto memFifo = scl::tileLinkFifo(memLink);

	linkModel.init("m", 8_b, 8_b, 0_b, 2_b);
	memFifo.a <<= link.a;
	*link.d <<= *memFifo.d;

	addSimulationProcess([&]()->SimProcess {
		co_await OnClk(clock());

		for (size_t i = 0; i < 4; ++i)
			fork(linkModel.put(i, 0, i + 16, clock()));

		for (size_t i = 0; i < 4; ++i)
		{
			auto [val, def, err] = co_await linkModel.get(i, 0, clock());
			BOOST_TEST(!err);
			BOOST_TEST((val & def) == i + 16);
		}

		co_await OnClk(clock());
		stopTest();
	});
}

BOOST_FIXTURE_TEST_CASE(tilelink_FromStream_test, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000, .memoryResetType = ClockConfig::ResetType::NONE });
	ClockScope clkScp(clock);

	scl::RvStream<scl::AxiToStreamCmd> cmd{ {
			.startAddress = 8_b,
			.endAddress = 8_b,
			.bytesPerBurst = 16
	} };
	pinIn(cmd, "cmd");

	scl::RvStream<BVec> data{ 16_b };
	pinIn(data, "data");

	auto link = scl::tileLinkInit<scl::TileLinkUB>(8_b, 16_b, 0_b, 3_b);
	pinOut(link, "link");

	scl::tileLinkFromStream(move(cmd), move(data), move(link));

	addSimulationProcess([&]()->SimProcess {
		simu(valid(cmd)) = '0';
		co_await OnClk(clock);

		for (size_t i = 1; i < 4; ++i)
		{
			simu(cmd->startAddress) = i * 16;
			simu(cmd->endAddress) = i * 32;
			co_await performTransfer(cmd, clock);
		}
	});

	addSimulationProcess([&]()->SimProcess {
		simu(valid(data)) = '0';

		for(size_t i = 0;; ++i)
		{
			simu(*data) = i;
			co_await performTransfer(data, clock);
		}
	});

	addSimulationProcess([&]()->SimProcess {
		fork(scl::strm::readyDriverRNG(link.a, clock, 80));

		for (size_t i = 0; i < 6*8; ++i)
		{
			co_await performTransferWait(link.a, clock);
			BOOST_TEST(simu(link.a->data) == i);
		}
		co_await OnClk(clock);
		BOOST_TEST(!simu(valid(link.a)));

		stopTest();
	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 1, 1'000'000 }));
}



