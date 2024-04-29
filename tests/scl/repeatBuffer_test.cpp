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

#include <gatery/scl/RepeatBuffer.h>
#include <gatery/scl/stream/StreamRepeatBuffer.h>

#include <vector>

using namespace boost::unit_test;
using namespace gtry;

BOOST_FIXTURE_TEST_CASE(RepeatBuffer_basic, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);
 

	scl::RvPacketStream<UInt> inputStream(32_b);
	pinIn(inputStream, "inputStream");
	UInt wrapAround = pinIn(10_b).setName("wrapAround");

	scl::RvPacketStream<UInt> outputStreamPreReg(32_b);

	scl::RepeatBuffer<UInt> repeatBuffer(1024, 32_b);
	repeatBuffer.wrapAround(wrapAround);

	ready(inputStream) = '1';
	IF (sop(inputStream))
		repeatBuffer.wrReset();

	IF (transfer(inputStream)) {
		repeatBuffer.wrPush(*inputStream);
		sim_assert(eop(inputStream) == repeatBuffer.wrIsLast()) << "eop of input stream should match wrap around of repeat buffer";
	}


	Bit emitPacket = pinIn().setName("emitPacket");

	Bit validLatch;
	validLatch = reg(validLatch | emitPacket, '0');

	valid(outputStreamPreReg) = validLatch;
	*outputStreamPreReg = repeatBuffer.rdPeek();
	eop(outputStreamPreReg) = repeatBuffer.rdIsLast();
	IF (eop(outputStreamPreReg))
		validLatch = '0';

	IF (transfer(outputStreamPreReg))
		repeatBuffer.rdPop();


	scl::RvPacketStream<UInt> outputStream(32_b);
	outputStream <<= regDownstream(std::move(outputStreamPreReg), { .allowRetimingBackward = true });
	pinOut(outputStream, "outputStream");


	addSimulationProcess([&,this]()->SimProcess {
		simu(ready(outputStream)) = '0';
		simu(valid(inputStream)) = '0';

		std::vector<size_t> data(128);
		simu(wrapAround) = data.size()-1;

		simu(emitPacket) = '0';


		co_await OnClk(clock);

		// fill up
		simu(valid(inputStream)) = '1';
		for (size_t i = 0; i < data.size(); i++) {
			data[i] = i;
			simu(*inputStream) = data[i];

			simu(eop(inputStream)) = i+1 == data.size();

			co_await performTransferWait(inputStream, clock);
		}
		simu(eop(inputStream)) = 'x';
		simu(valid(inputStream)) = '0';


		// read out couple of times
		for (size_t repeat = 0; repeat < 3; repeat++) {
			co_await OnClk(clock);
			simu(emitPacket) = '1';
			co_await OnClk(clock);
			simu(emitPacket) = '0';
			co_await OnClk(clock);
			co_await OnClk(clock);


			simu(ready(outputStream)) = '1';
			for (size_t i = 0; i < data.size(); i++) {
				co_await performTransferWait(outputStream, clock);
				BOOST_TEST(simu(eop(outputStream)) == (i+1 == data.size()));
				BOOST_TEST(simu(*outputStream) == data[i]);
			}
			simu(ready(outputStream)) = '0';
		}


		stopTest();
	});


	design.postprocess();

	runTest(hlim::ClockRational(20000, 1) / clock.getClk()->absoluteFrequency());
}

BOOST_FIXTURE_TEST_CASE(RepeatBuffer_RvStream, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);
 

	scl::RvStream<UInt> inputStream(32_b);
	pinIn(inputStream, "inputStream");
	UInt wrapAround = pinIn(6_b).setName("wrapAround");
	Bit emitPacket = pinIn().setName("emitPacket");

	scl::RvStream<UInt> outputStream(32_b);
	{
		auto inConnect = constructFrom(inputStream);
		inConnect <<= inputStream;
		scl::RvStream<UInt> outputStreamPreReg = std::move(inConnect) | scl::strm::repeatBuffer({ .minDepth = 128, .wrapAround = wrapAround, .releaseNextPacket = emitPacket });
		outputStream <<= regDownstream(std::move(outputStreamPreReg), { .allowRetimingBackward = true });
	}

	pinOut(outputStream, "outputStream");


	addSimulationProcess([&,this]()->SimProcess {
		simu(ready(outputStream)) = '0';
		simu(valid(inputStream)) = '0';

		std::vector<size_t> data(64);
		simu(wrapAround) = data.size()-1;
		simu(emitPacket) = '0';

		co_await OnClk(clock);

		// fill up
		simu(valid(inputStream)) = '1';
		for (size_t i = 0; i < data.size(); i++) {
			data[i] = i;
			simu(*inputStream) = data[i];
			co_await performTransferWait(inputStream, clock);
		}
		simu(valid(inputStream)) = '0';


		// read out couple of times
		for (size_t repeat = 0; repeat < 3; repeat++) {
			co_await OnClk(clock);
			simu(emitPacket) = '1';
			co_await OnClk(clock);
			simu(emitPacket) = '0';
			co_await OnClk(clock);
			co_await OnClk(clock);


			simu(ready(outputStream)) = '1';
			for (size_t i = 0; i < data.size(); i++) {
				co_await performTransferWait(outputStream, clock);
				BOOST_TEST(simu(*outputStream) == data[i]);
			}
			simu(ready(outputStream)) = '0';
		}

		stopTest();
	});


	design.postprocess();

	runTest(hlim::ClockRational(20000, 1) / clock.getClk()->absoluteFrequency());
}

BOOST_FIXTURE_TEST_CASE(RepeatBuffer_RvPacketStream, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);
 

	scl::RvPacketStream<UInt> inputStream(32_b);
	pinIn(inputStream, "inputStream");

	scl::RvPacketStream<UInt> outputStreamPreReg(32_b);
	Bit emitPacket = pinIn().setName("emitPacket");

	scl::RvPacketStream<UInt> outputStream(32_b);
	{
		auto inConnect = constructFrom(inputStream);
		inConnect <<= inputStream;
		scl::RvPacketStream<UInt> outputStreamPreReg = std::move(inConnect) | scl::strm::repeatBuffer({ .minDepth = 128, .releaseNextPacket = emitPacket, .setWarpAroundFromWrEop = true });
		outputStream <<= regDownstream(std::move(outputStreamPreReg), { .allowRetimingBackward = true });
	}	

	pinOut(outputStream, "outputStream");


	addSimulationProcess([&,this]()->SimProcess {
		simu(ready(outputStream)) = '0';
		simu(valid(inputStream)) = '0';

		std::vector<size_t> data(64);
		simu(emitPacket) = '0';

		co_await OnClk(clock);

		// fill up
		simu(valid(inputStream)) = '1';
		for (size_t i = 0; i < data.size(); i++) {
			data[i] = i;
			simu(*inputStream) = data[i];
			simu(eop(inputStream)) = i+1 == data.size();
			co_await performTransferWait(inputStream, clock);
		}
		simu(eop(inputStream)) = 'x';
		simu(valid(inputStream)) = '0';


		// read out couple of times
		for (size_t repeat = 0; repeat < 3; repeat++) {
			co_await OnClk(clock);
			simu(emitPacket) = '1';
			co_await OnClk(clock);
			simu(emitPacket) = '0';
			co_await OnClk(clock);
			co_await OnClk(clock);


			simu(ready(outputStream)) = '1';
			for (size_t i = 0; i < data.size(); i++) {
				co_await performTransferWait(outputStream, clock);
				BOOST_TEST(simu(eop(outputStream)) == (i+1 == data.size()));
				BOOST_TEST(simu(*outputStream) == data[i]);
			}
			simu(ready(outputStream)) = '0';
		}

		stopTest();
	});

	//design.visualize("before");
	design.postprocess();
	//design.visualize("after");

	runTest(hlim::ClockRational(20000, 1) / clock.getClk()->absoluteFrequency());
}
