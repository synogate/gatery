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
#include <future>

#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

#include <gatery/scl/arch/colognechip/CPE.h>
#include <gatery/scl/arch/colognechip/io.h>
#include <gatery/scl/arch/colognechip/SpecialFunction.h>
#include <gatery/scl/synthesisTools/IntelQuartus.h>

#include <gatery/scl/stream/SimuHelpers.h>
#include <gatery/scl/tilelink/tilelink.h>
#include <gatery/scl/tilelink/TileLinkCache.h>
#include <gatery/scl/tilelink/TileLinkDMA.h>
#include <gatery/scl/tilelink/TileLinkHub.h>
#include <gatery/scl/tilelink/TileLinkInterconnect.h>
#include <gatery/scl/tilelink/TileLinkFiFo.h>
#include <gatery/scl/tilelink/TileLinkStreamFetch.h>
#include <gatery/scl/tilelink/TileLinkMasterModel.h>
#include <gatery/scl/tilelink/TileLinkAdapter.h>
#include <gatery/scl/tilelink/TileLinkSpiFlash.h>

using namespace boost::unit_test;
using namespace gtry;
using namespace gtry::scl;

template<TileLinkSignal TLink = TileLinkU>
class TileLinkSimuInitiator : public std::enable_shared_from_this<TileLinkSimuInitiator<TLink>>
{
public:
	TileLinkSimuInitiator(BitWidth addrWidth, BitWidth dataWidth, size_t burstLimitLog, BitWidth sourceWidth, std::string prefix = "initiator")
	{
		tileLinkInit(m_link, addrWidth, dataWidth, BitWidth::last(burstLimitLog), sourceWidth);
		m_link.cfg.burstLimitLog = burstLimitLog;
		pinIn(m_link, prefix);
	}

	TileLinkSimuInitiator(BitWidth addrWidth, BitWidth dataWidth, BitWidth sizeW, BitWidth sourceWidth, std::string prefix = "initiator") :
		TileLinkSimuInitiator(addrWidth, dataWidth, sizeW.last(), sourceWidth, prefix)
	{
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

template<TileLinkSignal TLink = TileLinkU>
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

	TileLinkU uh;
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

	TileLinkDemux<TileLinkU> demux;
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

	TileLinkDemux<TileLinkU> demux;
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

	TileLinkDemux<TileLinkU> demux;
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

BOOST_FIXTURE_TEST_CASE(tilelink_addressdecoder_chanA_test, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	auto initiator = std::make_shared<TileLinkSimuInitiator<>>(4_b, 16_b, 1_b, 4_b);
	auto target = std::make_shared<TileLinkSimuTarget<>>(4_b, 16_b, 4_b, 4_b);

	TileLinkAddressDecoder{}
		(0, move(target->link()))
		.generate() <<= initiator->link();

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

BOOST_FIXTURE_TEST_CASE(tilelink_addressdecoder_chanD_test, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	auto initiator = std::make_shared<TileLinkSimuInitiator<>>(4_b, 16_b, 1_b, 4_b);
	auto target = std::make_shared<TileLinkSimuTarget<>>(4_b, 16_b, 4_b, 4_b);

	TileLinkAddressDecoder{}
		(0, move(target->link()))
		.generate() <<= initiator->link();

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

BOOST_FIXTURE_TEST_CASE(tilelink_addressdecoder_chanA_routing_test, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	auto initiator = std::make_shared<TileLinkSimuInitiator<>>(9_b, 16_b, 1_b, 4_b);
	auto target0 = std::make_shared<TileLinkSimuTarget<>>(4_b, 16_b, 4_b, 4_b, "target0");
	auto target1 = std::make_shared<TileLinkSimuTarget<>>(8_b, 16_b, 4_b, 4_b, "target1");
	auto target2 = std::make_shared<TileLinkSimuTarget<>>(2_b, 16_b, 4_b, 4_b, "target2");

	TileLinkAddressDecoder{}
		(0x000, move(target0->link()))
		(0x000, move(target2->link()))
		(0x100, move(target1->link()))
		.generate() <<= initiator->link();

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
		co_await AfterClk(clock);
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

	auto initiator = std::make_shared<TileLinkSimuInitiator<>>(12_b, 16_b, 1, 4_b);
	scl::tileLinkErrorResponder(initiator->link().cfg) <<= initiator->link();

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

	auto initiator = std::make_shared<TileLinkSimuInitiator<TileLinkU>>(12_b, 16_b, 3, 4_b);
	scl::tileLinkErrorResponder(initiator->link().cfg) <<= initiator->link();

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

	TileLinkMux<TileLinkU> mux;
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

BOOST_FIXTURE_TEST_CASE(tilelink_arbiter_test, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	auto initiator0 = std::make_shared<TileLinkSimuInitiator<>>(12_b, 16_b, 1_b, 2_b, "initiator0");
	auto initiator1 = std::make_shared<TileLinkSimuInitiator<>>(12_b, 16_b, 1_b, 2_b, "initiator1");
	auto initiator2 = std::make_shared<TileLinkSimuInitiator<>>(12_b, 16_b, 1_b, 2_b, "initiator2");
	auto target = std::make_shared<TileLinkSimuTarget<>>(12_b, 16_b, 4_b, 4_b);

	TileLinkArbiter arbiter(move(target->link()), 2_b);
	arbiter.out() <<= initiator0->link();
	arbiter.out() <<= initiator1->link();
	arbiter.out() <<= initiator2->link();
	arbiter.generate();

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
		co_await WaitFor(Seconds{ 1,10 } / clock.absoluteFrequency());
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
	}
	catch (...) {
		dbg::vis();
	}
	runTicks(clock.getClk(), 1000);
}
BOOST_FIXTURE_TEST_CASE(tilelink_hub_test, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	auto initiator0 = std::make_shared<TileLinkSimuInitiator<TileLinkU>>(32_b, 32_b, 2_b, 2_b, "initiator0");
	auto initiator1 = std::make_shared<TileLinkSimuInitiator<TileLinkU>>(32_b, 32_b, 2_b, 0_b, "initiator1");

	TileLinkHub<TileLinkU> hub;
	hub.attachSource(initiator0->link());
	hub.attachSource(initiator1->link());

	auto target0 = std::make_shared<TileLinkSimuTarget<TileLinkU>>(31_b, 32_b, 2_b, hub.sourceWidth(), "target0");
	auto target1 = std::make_shared<TileLinkSimuTarget<TileLinkU>>(31_b, 32_b, 2_b, hub.sourceWidth(), "target1");
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
	TileLinkU memUB = TileLinkStreamFetch{}.generate(cmd, data, 1_b);
	mem <<= (TileLinkU&) memUB;

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

	TileLinkU fetcher = TileLinkStreamFetch{}.enableBursts(64).generate(cmd, data, 0_b); //maximum burst size is 4 beats ->64 bits

	TileLinkU memTL = tileLinkInit<TileLinkU>(fetcher.a->address.width(), fetcher.a->data.width(), fetcher.a->source.width() + 5_b);
	mem <<= memTL;
	tileLinkAddBurst(move(memTL), fetcher.a->size.width()) <<= fetcher;

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
	scl::TileLinkU& link;
};

BOOST_FIXTURE_TEST_CASE(tilelink_addburst_test, LinkTest)
{
	Memory<BVec> mem(256, 8_b);
	
	scl::TileLinkU memLink;
	scl::tileLinkInit(memLink, 8_b, 8_b, 0_b, 6_b);
	mem <<= memLink;

	scl::TileLinkU memBurst = scl::tileLinkAddBurst(move(memLink), 2_b);

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

	scl::TileLinkU memLink;
	scl::tileLinkInit(memLink, 8_b, 16_b, 1_b, 12_b);
	mem <<= memLink;

	scl::TileLinkU memBurst = scl::tileLinkAddBurst(move(memLink), 3_b);

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
	linkModel.init("m", 8_b, 16_b, 2_b);

	tileLinkMemory(8_b, 8_b, 6_b)
		| scl::tileLinkAddBurst(3)
		| scl::tileLinkDoubleWidth()
		<<= link;

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

	auto memLink = scl::tileLinkInit<scl::TileLinkU>(8_b, 8_b, 2_b);
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

	auto link = scl::tileLinkInit<scl::TileLinkU>(8_b, 16_b, 0_b, 3_b);
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

class TileLinkInstructionCacheTest : public BoostUnitTestSimulationFixture
{
public:
	TileLinkInstructionCacheTest()
	{
		memoryState.resize(128 * 16);
		for (size_t i = 0; i < memoryState.size() / 16; ++i)
		{
			memoryState.insert(sim::DefaultConfig::VALUE, i * 16, 16, rng() & 0xFFFF);
			memoryState.insert(sim::DefaultConfig::DEFINED, i * 16, 16, 0xFFFF);
		}

		mm.probability(0.9f, 0.9f);
	}

	SimProcess performCpuAccessTest()
	{
		co_await OnClk(clock);

		size_t openRequests = 0;
		size_t addr = 0;
		while (true)
		{
			while (openRequests == 2)
				co_await OnClk(clock);

			while (issueWrites && rng() % 2 == 0)
			{
				size_t wrAddr = rng();
				wrAddr %= memoryState.size() / 8;
				wrAddr &= ~1;

				size_t wrData = rng() & 0xFFFF;
				memoryState.insert(sim::DefaultConfig::VALUE, wrAddr * 8, 16, wrData);
				memoryState.insert(sim::DefaultConfig::DEFINED, wrAddr * 8, 16, 0xFFFF);

				fork(mm.put(wrAddr, 1, wrData, clock));
			}

			addr += 2;
			if (rng() % 5 == 0)
				addr -= 6; // branch back (loop)
			if (rng() % 11 == 0)
				addr = rng(); // branch random (call)
			addr %= memoryState.size() / 8;
			addr &= ~1;

			openRequests++;
			fork([&]()->SimProcess {
				size_t value = memoryState.extract(sim::DefaultConfig::VALUE, addr * 8, 16);
				size_t defined = memoryState.extract(sim::DefaultConfig::DEFINED, addr * 8, 16);

				auto [val, def, err] = co_await mm.get(addr, 1, clock);
				BOOST_TEST(!err);
				BOOST_TEST(val == value);
				BOOST_TEST(def == defined);
				openRequests--;
				checkedRequests++;
				});
		}
	}

	SimProcess checkProgress(size_t cyclesBetweenChecks)
	{
		size_t lastCheckedRequests = 0;
		while (true)
		{
			for (size_t i = 0; i < cyclesBetweenChecks; ++i)
				co_await OnClk(clock);

			BOOST_TEST(lastCheckedRequests != checkedRequests);
			lastCheckedRequests = checkedRequests;
		}
	}
protected:
	Clock clock = Clock({ .absoluteFrequency = 100'000'000, .memoryResetType = ClockConfig::ResetType::NONE });
	std::mt19937_64 rng = std::mt19937_64{ std::random_device{}() };
	gtry::sim::DefaultBitVectorState memoryState;
	scl::TileLinkMasterModel mm;
	size_t checkedRequests = 0;
	bool issueWrites = false;

	scl::TileLinkU generateTestMemory()
	{
		Memory<BVec> mem(memoryState.size() / 16, 16_b);
		mem.fillPowerOnState(memoryState);
		auto link = tileLinkInit<TileLinkU>(8_b, 16_b, 6_b);
		mem <<= link;

		return move(link) | scl::tileLinkAddBurst(2_b) | name("mem");
	}


};

BOOST_FIXTURE_TEST_CASE(tilelink_cache_dmrob_test, TileLinkInstructionCacheTest)
{
	{
		ClockScope clkScp(clock);
		scl::TileLinkU memory = generateTestMemory();
		scl::TileLinkU cached = scl::tileLinkCacheDirectMappedReadOnlyBurst(move((scl::TileLinkU&)memory), 4_b);
		mm.initAndDrive("tl", move(cached));
	}

	addSimulationProcess(std::bind(&TileLinkInstructionCacheTest::performCpuAccessTest, this));
	addSimulationProcess(std::bind(&TileLinkInstructionCacheTest::checkProgress, this, 8));

	design.postprocess();
	runHitsTimeout({ 10, 1'000'000 });
}

BOOST_FIXTURE_TEST_CASE(tilelink_cache_dmv_test, TileLinkInstructionCacheTest)
{
	issueWrites = true;
	{
		ClockScope clkScp(clock);
		scl::TileLinkU memory = generateTestMemory();
		scl::TileLinkU cached = scl::tileLinkCacheDirectMappedVictim(move((scl::TileLinkU&)memory), 4_b);
		mm.initAndDrive("tl", move(cached));
	}

	addSimulationProcess(std::bind(&TileLinkInstructionCacheTest::performCpuAccessTest, this));
	addSimulationProcess(std::bind(&TileLinkInstructionCacheTest::checkProgress, this, 128));

	design.postprocess();
	runHitsTimeout({ 10, 1'000'000 });
}

BOOST_FIXTURE_TEST_CASE(tilelink_spiflash_test, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000, .memoryResetType = ClockConfig::ResetType::NONE });
	ClockScope clkScp(clock);

	scl::SpiPort spiPort = scl::SpiPort::Create(8_b, true);
	pinOut(spiPort, "spi");

	// most basic simulation model
	spiPort.inEn.resetValue('0');
	UInt inDelay = pinIn(2_b).setName("inDelay");
	valid(*spiPort.in) = delay(spiPort.inEn, inDelay);
	**spiPort.in = 0;

	scl::TileLinkU tlPort = scl::tileLinkSpiFlash(spiPort, { 
		.sourceW = 1_b, .sizeW = 3_b,
		.preloadCommandByteTimeout = hlim::ClockRational{ 1, 1'000'000 },
	});

	scl::TileLinkMasterModel mm;
	mm.initAndDrive("tl", move((TileLinkU&)tlPort));

	addSimulationProcess([&]()->SimProcess {
		co_await OnClk(clock);
		for (size_t d = 0; d < 3; d += 2)
		{
			bool putErr = co_await mm.put(0, d, 0, clock);
			BOOST_TEST(putErr);
			simu(inDelay) = d;

			for (size_t i = 0; i < 5; ++i)
			{
				auto [val, def, err] = co_await mm.get(72211 * i, i, clock);
				BOOST_TEST(!err);
				BOOST_TEST(val == 0);

				if(i == 3) // wait to test command preloading
					for(size_t w = 0; w < 8; ++w)
						co_await OnClk(clock);
			}

			co_await OnClk(clock);
		}

		for (size_t w = 0; w < 8; ++w)
			co_await OnClk(clock);
		stopTest();
	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 5, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(tilelink_spiflash_dual_ddr_test, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000, .memoryResetType = ClockConfig::ResetType::NONE });
	ClockScope clkScp(clock);

	scl::SpiPort spiPort = scl::SpiPort::Create(4_b, true);
	pinOut(spiPort, "spi");

	// most basic simulation model
	valid(*spiPort.in) = reg(spiPort.inEn, '0');
	**spiPort.in = 0;

	scl::TileLinkU tlPort = scl::tileLinkSpiFlash(spiPort, {
		.sourceW = 1_b, .sizeW = 3_b
	});

	scl::TileLinkMasterModel mm;
	mm.initAndDrive("tl", move((TileLinkU&)tlPort));

	addSimulationProcess([&]()->SimProcess {
		co_await OnClk(clock);

		for (size_t i = 0; i < 5; ++i)
		{
			auto [val, def, err] = co_await mm.get(72211 * i, i, clock);
			BOOST_TEST(!err);
			BOOST_TEST(val == 0);
		}

		for (size_t w = 0; w < 8; ++w)
			co_await OnClk(clock);

		stopTest();
	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 2, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(tilelink_spiflash_dual_sdr_test, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000, .memoryResetType = ClockConfig::ResetType::NONE });
	ClockScope clkScp(clock);

	scl::SpiPort spiPort = scl::SpiPort::Create(2_b, false);
	pinOut(spiPort, "spi");

	// most basic simulation model
	valid(*spiPort.in) = reg(spiPort.inEn, '0');
	**spiPort.in = 0;

	scl::TileLinkU tlPort = scl::tileLinkSpiFlash(spiPort, {
			.sourceW = 1_b, .sizeW = 3_b
		});

	scl::TileLinkMasterModel mm;
	mm.initAndDrive("tl", move((TileLinkU&)tlPort));

	addSimulationProcess([&]()->SimProcess {
		co_await OnClk(clock);

		for (size_t i = 0; i < 5; ++i)
		{
			auto [val, def, err] = co_await mm.get(72211 * i, i, clock);
			BOOST_TEST(!err);
			BOOST_TEST(val == 0);
		}

		for (size_t w = 0; w < 8; ++w)
			co_await OnClk(clock);

		stopTest();
	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 3, 1'000'000 }));
}

namespace gtry::scl
{
	void spiPortGateMate(SpiPort& port, std::string pinPrefix = "SPI_", bool useLatch = true)
	{
		using namespace arch::colognechip;
		HCL_DESIGNCHECK(!port.ddrMode);

		//pinOut(scl::Counter{2_b}.value().msb(), "FLASH_CLK");
		//return;

		CC_OBUF clkBuf;
		clkBuf.pin(pinPrefix + "CLK");
		CC_ODDR clkDDR{clkBuf};
		clkDDR.clockInversion(true);
		clkDDR.D0() = '0';
		clkDDR.D1() = port.clkEn;

		CC_OBUF csnBuf;
		csnBuf.pin(pinPrefix + "CSn");
		csnBuf.regOut(true);
		csnBuf.O() = reg(!port.cs.lsb());

		auto captureInputOnHalfCycle = [=](Bit in) {
			if (useLatch)
			{
				// the latch gives us slightly more margin on the output
				// we could also use the register path in case latch makes problems
				CC_DLT inLatch;
				inLatch.G() = ClockScope::getClk().clkSignal();
				inLatch.D() = in;
				return inLatch.Q();
			}
			else
			{
				CC_DFF inDff;
				inDff.clkInv(true);
				inDff.D() = in;
				return inDff.Q();
			}
		};

		**port.in = undefined(**port.in);
		if(port.out->width() == 1_b)
		{
			// there is a desdicated input and output pin in 1 bit mode
			CC_OBUF outBuf;
			outBuf.pin(pinPrefix + "DAT0");
			outBuf.regOut(true);
			outBuf.O() = reg(port.out->lsb());

			CC_IBUF inBuf;
			inBuf.pin(pinPrefix + "DAT1");
			inBuf.regIn(true);
			inBuf.schmittTrigger(true);

			(**port.in).lsb() = captureInputOnHalfCycle(inBuf.I());
		}
		else
		{
			// in all other modes pins are bidirectional
			for (size_t i = 0; i < port.out->size(); ++i)
			{
				arch::colognechip::CC_IOBUF buf;
				buf.regIn(true);
				buf.regOut(true);
				buf.pin(pinPrefix + "DAT" + std::to_string(i));
				buf.O() = reg(port.out->at(i));
				buf.disable() = reg(!valid(port.out), '1');
				buf.schmittTrigger(true);
				(**port.in)[i] = captureInputOnHalfCycle(buf.I());
			}
		}

		valid(*port.in) = reg(port.inEn, '0');
		**port.in = gtry::simOverride(**port.in, ConstBVec(0, (**port.in).width()));
	}
}

BOOST_FIXTURE_TEST_CASE(tilelink_spiflash_sdr_gatemate_test, BoostUnitTestSimulationFixture, *boost::unit_test::disabled())
{
	{
		std::ofstream f{ "export/tilelink_spiflash_sdr_gatemate_test/flash_content.hex", std::ios::binary };
		for (size_t i = 0; i < 4096; ++i)
			f.put((char)i);
	}

#if 1
	Clock clock({ 
		.absoluteFrequency = 12'000'000, 
		.name = "CLK12M", 
		.resetType = ClockConfig::ResetType::SYNCHRONOUS,
		.memoryResetType = ClockConfig::ResetType::NONE,
		.resetActive = ClockConfig::ResetActive::LOW,
	});

	Bit synchronizedReset = scl::synchronizeRelease(arch::colognechip::usr_rstn(), ClockScope::getClk(), clock, ClockConfig::ResetActive::LOW);
	clock.reset(synchronizedReset);
#else
	Clock clockIn({
		.absoluteFrequency = 12'000'000,
		.name = "CLK12M",
		.resetType = ClockConfig::ResetType::NONE,
	});
	ClockScope pinScope{ (clockIn) };
	Clock clock = arch::colognechip::globalReset(
		arch::colognechip::pll({ 
			.absoluteFrequency = 6'000'000,
		})
	);
#endif
	ClockScope clkScp(clock);

	scl::SpiPort spiPort = scl::SpiPort::Create(4_b, false);
	spiPortGateMate(spiPort, "FLASH_");

#if 1
	scl::TileLinkU tlPort = scl::tileLinkSpiFlash(spiPort, {
		.sourceW = 1_b, .sizeW = 3_b,
		//.preloadCommandByteTimeout = hlim::ClockRational{1, 1},
	});

	{ // test driver
		enum class State {
			wait,
			request,
			response
		};
		Reg<Enum<State>> state{ State::wait };
	
		IF(state.current() == State::wait)
		{
			IF(scl::Counter{ 21_b }.isLast())
				state = State::request;
		}

		scl::Counter addrCounter{ 12_b };

		tlPort.a->setupGet(zext(addrCounter.value()) | 1048576);
		valid(tlPort.a) = '0';
		ready(*tlPort.d) = '1';

		IF(state.current() == State::request)
		{
			valid(tlPort.a) = '1';
			IF(transfer(tlPort.a))
				state = State::response;
		}

		Bit resultCheck;
		IF(state.current() == State::response)
		{
			IF(transfer(*tlPort.d))
			{
				state = State::wait;
				resultCheck = (*tlPort.d)->data == addrCounter.value().lower(8_b);
				addrCounter.inc();
			}
		}
		resultCheck = reg(resultCheck, Bit{ '0' }) | name("resultCheck");
		pinOut(resultCheck, "UART0_TX");
	}
#endif

	design.postprocess();

	vhdl::VHDLExport vhdl("export/tilelink_spiflash_sdr_gatemate_test/tilelink_spiflash_sdr_gatemate_test.vhd");
	vhdl(design.getCircuit());

	auto flowTask = std::async([]() {
		return std::system("make -C export/tilelink_spiflash_sdr_gatemate_test");
	});

	runHitsTimeout({ 4, 1'000'000 });

	flowTask.wait();
}
