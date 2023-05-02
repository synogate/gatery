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

#include <gatery/simulation/waveformFormats/VCDSink.h>
#include <gatery/simulation/Simulator.h>

#include <gatery/scl/stream/StreamArbiter.h>
#include <gatery/scl/stream/adaptWidth.h>
#include <gatery/scl/stream/Packet.h>
#include <gatery/scl/io/SpiMaster.h>

#include <gatery/debug/websocks/WebSocksInterface.h>

using namespace boost::unit_test;
using namespace gtry;

BOOST_FIXTURE_TEST_CASE(arbitrateInOrder_basic, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	scl::RvStream<UInt> in0;
	scl::RvStream<UInt> in1;

	*in0 = pinIn(8_b).setName("in0_data");
	valid(in0)= pinIn().setName("in0_valid");
	pinOut(ready(in0)).setName("in0_ready");

	*in1 = pinIn(8_b).setName("in1_data");
	valid(in1)= pinIn().setName("in1_valid");
	pinOut(ready(in1)).setName("in1_ready");

	scl::arbitrateInOrder uutObj{ in0, in1 };
	scl::RvStream<UInt>& uut = uutObj;
	pinOut(*uut).setName("out_data");
	pinOut(valid(uut)).setName("out_valid");
	ready(uut) = pinIn().setName("out_ready");

	addSimulationProcess([&]()->SimProcess {
		simu(ready(uut)) = '1';
		simu(valid(in0)) = '0';
		simu(valid(in1)) = '0';
		simu(*in0) = 0;
		simu(*in1) = 0;
		co_await AfterClk(clock);

		simu(valid(in0)) = '0';
		simu(valid(in1)) = '1';
		simu(*in1) = 1;
		co_await AfterClk(clock);

		simu(valid(in1)) = '0';
		simu(valid(in0)) = '1';
		simu(*in0) = 2;
		co_await AfterClk(clock);

		simu(valid(in1)) = '1';
		simu(valid(in0)) = '1';
		simu(*in0) = 3;
		simu(*in1) = 4;
		co_await AfterClk(clock);
		co_await AfterClk(clock);

		simu(valid(in1)) = '1';
		simu(valid(in0)) = '1';
		simu(*in0) = 5;
		simu(*in1) = 6;
		co_await AfterClk(clock);
		co_await AfterClk(clock);

		simu(valid(in0)) = '0';
		simu(valid(in1)) = '1';
		simu(*in1) = 7;
		co_await AfterClk(clock);

		simu(valid(in1)) = '0';
		simu(valid(in0)) = '0';
		simu(ready(uut)) = '0';
		co_await AfterClk(clock);

		simu(valid(in1)) = '0';
		simu(valid(in0)) = '1';
		simu(*in0) = 8;
		simu(ready(uut)) = '1';
		co_await AfterClk(clock);

		simu(valid(in1)) = '0';
		simu(valid(in0)) = '0';
		co_await AfterClk(clock);
	});

	addSimulationProcess([&]()->SimProcess {

		size_t counter = 1;
		while (true)
		{
			co_await OnClk(clock);
			if (simu(ready(uut)) && simu(valid(uut)))
			{
				BOOST_TEST(counter == simu(*uut));
				counter++;
			}
		}

	});

	//sim::VCDSink vcd{ design.getCircuit(), getSimulator(), "arbitrateInOrder_basic.vcd" };
   //vcd.addAllPins();
	//vcd.addAllNamedSignals();

	design.postprocess();
	//design.visualize("arbitrateInOrder_basic");

	runTicks(clock.getClk(), 16);
}

BOOST_FIXTURE_TEST_CASE(arbitrateInOrder_fuzz, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	scl::RvStream<UInt> in0;
	scl::RvStream<UInt> in1;

	*in0 = pinIn(8_b).setName("in0_data");
	valid(in0) = pinIn().setName("in0_valid");
	pinOut(ready(in0)).setName("in0_ready");

	*in1 = pinIn(8_b).setName("in1_data");
	valid(in1) = pinIn().setName("in1_valid");
	pinOut(ready(in1)).setName("in1_ready");

	scl::arbitrateInOrder uutObj{ in0, in1 };
	scl::RvStream<UInt>& uut = uutObj;
	pinOut(*uut).setName("out_data");
	pinOut(valid(uut)).setName("out_valid");
	ready(uut) = pinIn().setName("out_ready");

	addSimulationProcess([&]()->SimProcess {
		simu(ready(uut)) = '1';
		simu(valid(in0)) = '0';
		simu(valid(in1)) = '0';

		std::mt19937 rng{ 10179 };
		size_t counter = 1;
		while(true)
		{
			co_await OnClk(clock);
			if (simu(ready(in0)))
			{
				if(rng() % 2 == 0)
				{
					simu(valid(in0)) = '1';
					simu(*in0) = counter++;
				}
				else
				{
					simu(valid(in0)) = '0';
				}

				if(rng() % 2 == 0)
				{
					simu(valid(in1)) = '1';
					simu(*in1) = counter++;
				}
				else
				{
					simu(valid(in1)) = '0';
				}
			}

			// chaos monkey
			simu(ready(uut)) = rng() % 8 != 0;
		}
	});


	addSimulationProcess([&]()->SimProcess {

		size_t counter = 1;
		while(true)
		{
			co_await OnClk(clock);
			if(simu(ready(uut)) && simu(valid(uut)))
			{
				BOOST_TEST(counter % 256 == simu(*uut));
				counter++;
			}
		}

	});

	//sim::VCDSink vcd{ design.getCircuit(), getSimulator(), "arbitrateInOrder_fuzz.vcd" };
	//vcd.addAllPins();
	//vcd.addAllNamedSignals();

	design.postprocess();
   // design.visualize("arbitrateInOrder_fuzz");

	runTicks(clock.getClk(), 256);
}

class StreamTransferFixture : public BoostUnitTestSimulationFixture
{
protected:
	void transfers(size_t numTransfers)
	{
		HCL_ASSERT(m_groups == 0);
		m_transfers = numTransfers;
	}
	void groups(size_t numGroups)
	{
		HCL_ASSERT(m_groups == 0);
		m_groups = numGroups;
	}

	Clock m_clock = Clock({ .absoluteFrequency = 100'000'000 });

	void simulateTransferTest(scl::StreamSignal auto& source, scl::StreamSignal auto& sink)
	{
		simulateBackPressure(sink);
		simulateSendData(source, m_groups++);
		simulateRecvData(sink);
	}

	template<scl::StreamSignal T>
	void simulateArbiterTestSink(T& sink)
	{
		simulateBackPressure(sink);
		simulateRecvData(sink);
	}

	template<scl::StreamSignal T>
	void simulateArbiterTestSource(T& source)
	{
		simulateSendData(source, m_groups++);
	}

	void In(scl::StreamSignal auto& stream, std::string prefix = "in")
	{
		pinIn(stream, prefix);
	}

	void Out(scl::StreamSignal auto& stream, std::string prefix = "out")
	{
		pinOut(stream, prefix);
	}

	void simulateBackPressure(scl::StreamSignal auto& stream)
	{
		auto recvClock = ClockScope::getClk();

		addSimulationProcess([&, recvClock]()->SimProcess {
			std::mt19937 rng{ std::random_device{}() };

			simu(ready(stream)) = '0';
			do
				co_await WaitStable();
			while (simu(valid(stream)) == '0');

			// todo: not good
			co_await WaitFor(Seconds{1,10} / recvClock.absoluteFrequency());

			while (true)
			{
				simu(ready(stream)) = rng() % 2 != 0;
				co_await AfterClk(recvClock);
			}
		});
	}

	void simulateSendData(scl::RvStream<UInt>& stream, size_t group)
	{
		addSimulationProcess([=, this, &stream]()->SimProcess {
			std::mt19937 rng{ std::random_device{}() };
			for(size_t i = 0; i < m_transfers; ++i)
			{
				simu(valid(stream)) = '0';
				simu(*stream).invalidate();

				while((rng() & 1) == 0)
					co_await AfterClk(m_clock);

				simu(valid(stream)) = '1';
				simu(*stream) = i + group * m_transfers;

				co_await scl::performTransferWait(stream, m_clock);
			}
			simu(valid(stream)) = '0';
			simu(*stream).invalidate();
		});
	}

	template<class... Meta>
	void simulateSendData(scl::RvPacketStream<UInt, Meta...>& stream, size_t group)
	{
		addSimulationProcess([=, this, &stream]()->SimProcess {
			std::mt19937 rng{ std::random_device{}() };
			for(size_t i = 0; i < m_transfers;)
			{
				const size_t packetLen = std::min<size_t>(m_transfers - i, rng() % 5 + 1);
				for(size_t j = 0; j < packetLen; ++j)
				{
					simu(valid(stream)) = '0';
					simu(eop(stream)).invalidate();
					simu(*stream).invalidate();

					while((rng() & 1) == 0)
						co_await AfterClk(m_clock);

					simu(valid(stream)) = '1';
					simu(eop(stream)) = j == packetLen - 1;
					simu(*stream) = i + j + group * m_transfers;

					co_await scl::performTransferWait(stream, m_clock);
				}
				i += packetLen;
			}
			simu(valid(stream)) = '0';
			simu(*stream).invalidate();
		});
	}

	template<class... Meta>
	void simulateSendData(scl::RsPacketStream<UInt, Meta...>& stream, size_t group)
	{
		addSimulationProcess([=, this, &stream]()->SimProcess {
			std::mt19937 rng{ std::random_device{}() };
			for (size_t i = 0; i < m_transfers;)
			{
				simu(sop(stream)) = '0';
				simu(eop(stream)) = '0';
				simu(*stream).invalidate();

				while ((rng() & 1) == 0)
					co_await AfterClk(m_clock);

				const size_t packetLen = std::min<size_t>(m_transfers - i, rng() % 5 + 1);
				for (size_t j = 0; j < packetLen; ++j)
				{
					simu(sop(stream)) = j == 0;
					simu(eop(stream)) = j == packetLen - 1;
					simu(*stream) = i + j + group * m_transfers;

					co_await scl::performTransferWait(stream, m_clock);
				}
				i += packetLen;
			}

			simu(sop(stream)) = '0';
			simu(eop(stream)) = '0';
			simu(*stream).invalidate();
		});
	}

	template<scl::StreamSignal T>
	void simulateRecvData(const T& stream)
	{
		auto recvClock = ClockScope::getClk();

		auto myTransfer = pinOut(transfer(stream)).setName("simulateRecvData_transfer");

		addSimulationProcess([=, this, &stream]()->SimProcess {
			std::vector<size_t> expectedValue(m_groups);
			while(true)
			{
				co_await OnClk(recvClock);

				if(simu(myTransfer) == '1')
				{
					size_t data = simu(*(stream.operator ->()));
					BOOST_TEST(data / m_transfers < expectedValue.size());
					if (data / m_transfers < expectedValue.size())
					{
						BOOST_TEST(data % m_transfers == expectedValue[data / m_transfers]);
						expectedValue[data / m_transfers]++;
					}
				}

				if (std::ranges::all_of(expectedValue, [=, this](size_t val) { return val == m_transfers; }))
				{
					stopTest();
					co_await AfterClk(recvClock);
				}
			}
		});
	}

private:
	size_t m_groups = 0;
	size_t m_transfers = 16;
};

BOOST_FIXTURE_TEST_CASE(stream_transform, StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	{
		// const compile test
		const scl::VStream<UInt, scl::Eop> vs{ 5_b };
		auto res = vs.remove<scl::Eop>();
		auto rsr = vs.reduceTo<scl::Stream<UInt>>();
		auto vso = vs.transform(std::identity{});
	}

	scl::RvStream<UInt> in = scl::RvPacketStream<UInt, scl::Sop>{ 5_b }
								.remove<scl::Sop>()
								.reduceTo<scl::RvStream<UInt>>()
								.remove<scl::Eop>();
	In(in);

	struct Intermediate
	{
		UInt data;
		Bit test;
	};

	scl::RvStream<Intermediate> im = in
		.reduceTo<scl::RvStream<UInt>>()
		.transform([](const UInt& data) {
			return Intermediate{ data, '1' };
		});

	scl::RvStream<UInt> out = im.transform(&Intermediate::data);
	Out(out);

	simulateTransferTest(in, out);

	//recordVCD("stream_downstreamReg.vcd");
	design.postprocess();
	runTicks(m_clock.getClk(), 1024);
}

BOOST_FIXTURE_TEST_CASE(stream_downstreamReg, StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::RvStream<UInt> in{ .data = 5_b };
	In(in);

	scl::RvStream<UInt> out = in.regDownstream();
	Out(out);

	simulateTransferTest(in, out);

	//recordVCD("stream_downstreamReg.vcd");
	design.postprocess();
	runTicks(m_clock.getClk(), 1024);
}
BOOST_FIXTURE_TEST_CASE(stream_uptreamReg, StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::RvStream<UInt> in{ .data = 5_b };
	In(in);

	scl::RvStream<UInt> out = in.regReady();
	Out(out);

	simulateTransferTest(in, out);

	//recordVCD("stream_uptreamReg.vcd");
	design.postprocess();
	runTicks(m_clock.getClk(), 1024);
}
BOOST_FIXTURE_TEST_CASE(stream_reg, StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::RvStream<UInt> in { .data = 10_b };
	In(in);

	scl::RvStream<UInt> out = reg(in);
	Out(out);

	simulateTransferTest(in, out);

	//recordVCD("stream_reg.vcd");
	design.postprocess();
	runTicks(m_clock.getClk(), 1024);
}
BOOST_FIXTURE_TEST_CASE(stream_reg_chaining, StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::RvStream<UInt> in{ .data = 5_b };
	In(in);

	scl::RvStream<UInt> out = in.regDownstreamBlocking().regDownstreamBlocking().regDownstreamBlocking().regDownstream();
	Out(out);

	simulateTransferTest(in, out);

	//recordVCD("stream_reg_chaining.vcd");
	design.postprocess();
	runTicks(m_clock.getClk(), 1024);
}

BOOST_FIXTURE_TEST_CASE(stream_fifo, StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::RvStream<UInt> in{ .data = 10_b };
	In(in);

	scl::RvStream<UInt> out = in.fifo();
	Out(out);

	transfers(500);
	simulateTransferTest(in, out);

	//recordVCD("stream_fifo.vcd");
	design.postprocess();
	runTicks(m_clock.getClk(), 1024);
}
BOOST_FIXTURE_TEST_CASE(streamArbiter_low1, StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::RvStream<UInt> in{ .data = 10_b };
	In(in);

	scl::StreamArbiter<scl::RvStream<UInt>> arbiter;
	arbiter.attach(in);
	arbiter.generate();

	Out(arbiter.out());

	simulateArbiterTestSource(in);
	simulateArbiterTestSink(arbiter.out());

	//recordVCD("streamArbiter_low1.vcd");
	design.postprocess();
	runTicks(m_clock.getClk(), 1024);
}

BOOST_FIXTURE_TEST_CASE(streamArbiter_low4, StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::StreamArbiter<scl::RvStream<UInt>> arbiter;
	std::array<scl::RvStream<UInt>, 4> in;
	for(size_t i = 0; i < in.size(); ++i)
	{
		*in[i] = 10_b;
		In(in[i], "in" + std::to_string(i) + "_");
		simulateArbiterTestSource(in[i]);
		arbiter.attach(in[i]);
	}
	arbiter.generate();

	Out(arbiter.out());
	simulateArbiterTestSink(arbiter.out());

	//recordVCD("streamArbiter_low4.vcd");
	design.postprocess();
	runTicks(m_clock.getClk(), 1024);
}

BOOST_FIXTURE_TEST_CASE(streamArbiter_low4_packet, StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::StreamArbiter<scl::RvPacketStream<UInt>> arbiter;
	std::array<scl::RvPacketStream<UInt>, 4> in;
	for(size_t i = 0; i < in.size(); ++i)
	{
		*in[i] = 10_b;
		In(in[i], "in" + std::to_string(i) + "_");
		simulateArbiterTestSource(in[i]);
		arbiter.attach(in[i]);
	}
	arbiter.generate();

	Out(arbiter.out());
	simulateArbiterTestSink(arbiter.out());

	//recordVCD("streamArbiter_low4_packet.vcd");
	design.postprocess();
	runTicks(m_clock.getClk(), 1024);
}

BOOST_FIXTURE_TEST_CASE(streamArbiter_rr5, StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::StreamArbiter<scl::RvStream<UInt>, scl::ArbiterPolicyRoundRobin> arbiter;
	std::array<scl::RvStream<UInt>, 5> in;
	for (size_t i = 0; i < in.size(); ++i)
	{
		*in[i] = 10_b;
		In(in[i], "in" + std::to_string(i) + "_");
		simulateArbiterTestSource(in[i]);
		arbiter.attach(in[i]);
	}
	arbiter.generate();

	Out(arbiter.out());
	simulateArbiterTestSink(arbiter.out());

	//recordVCD("streamArbiter_rr5.vcd");
	design.postprocess();
	runTicks(m_clock.getClk(), 1024);

	//dbg::vis();
}

BOOST_FIXTURE_TEST_CASE(streamArbiter_reg_rr5, StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::StreamArbiter<scl::RvStream<UInt>, scl::ArbiterPolicyReg<scl::ArbiterPolicyRoundRobin>> arbiter;
	std::array<scl::RvStream<UInt>, 5> in;
	for (size_t i = 0; i < in.size(); ++i)
	{
		*in[i] = 10_b;
		In(in[i], "in" + std::to_string(i) + "_");
		simulateArbiterTestSource(in[i]);
		arbiter.attach(in[i]);
	}
	arbiter.generate();

	Out(arbiter.out());
	simulateArbiterTestSink(arbiter.out());

	//recordVCD("streamArbiter_reg_rr5.vcd");
	design.postprocess();
	runTicks(m_clock.getClk(), 1024);
}

BOOST_FIXTURE_TEST_CASE(streamArbiter_rrb5, StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::StreamArbiter<scl::RvStream<UInt>, scl::ArbiterPolicyRoundRobinBubble> arbiter;
	std::array<scl::RvStream<UInt>, 5> in;
	for(size_t i = 0; i < in.size(); ++i)
	{
		*in[i] = 10_b;
		In(in[i], "in" + std::to_string(i) + "_");
		simulateArbiterTestSource(in[i]);
		arbiter.attach(in[i]);
	}
	arbiter.generate();

	Out(arbiter.out());
	simulateArbiterTestSink(arbiter.out());

	//recordVCD("streamArbiter_rrb5.vcd");
	design.postprocess();
	runTicks(m_clock.getClk(), 1024);
}

BOOST_FIXTURE_TEST_CASE(streamArbiter_rrb5_packet, StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::StreamArbiter<scl::RvPacketStream<UInt>, scl::ArbiterPolicyRoundRobinBubble> arbiter;
	std::array<scl::RvPacketStream<UInt>, 5> in;
	for(size_t i = 0; i < in.size(); ++i)
	{
		*in[i] = 10_b;
		In(in[i], "in" + std::to_string(i) + "_");
		simulateArbiterTestSource(in[i]);
		arbiter.attach(in[i]);
	}
	arbiter.generate();

	Out(arbiter.out());
	simulateArbiterTestSink(arbiter.out());

	//recordVCD("streamArbiter_rrb5_packet.vcd");
	design.postprocess();
	runTicks(m_clock.getClk(), 1024);
}

BOOST_FIXTURE_TEST_CASE(stream_extendWidth, StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	{
		// add valid no ready compile test
		scl::Stream<UInt> inT{ 4_b };
		auto outT = scl::extendWidth(inT, 8_b);
	}
	{
		// add valid compile test
		scl::Stream<UInt, scl::Ready> inT{ 4_b };
		auto outT = scl::extendWidth(inT, 8_b);
	}

	scl::RvStream<UInt> in{ 4_b };
	In(in);

	auto out = scl::extendWidth(in, 8_b);
	Out(out);

	// send data
	addSimulationProcess([=, this, &in]()->SimProcess {
		simu(valid(in)) = '0';
		simu(*in).invalidate();
		for (size_t i = 0; i < 4; ++i)
			co_await AfterClk(m_clock);

		for (size_t i = 0; i < 32; ++i)
		{
			for (size_t j = 0; j < 2; ++j)
			{
				simu(valid(in)) = '1';
				simu(*in) = (i >> (j * 4)) & 0xF;

				co_await scl::performTransferWait(in, m_clock);
			}
		}
		});

	transfers(32);
	groups(1);
	simulateBackPressure(out);
	simulateRecvData(out);

	design.postprocess();
	//dbg::vis();
	
	runTicks(m_clock.getClk(), 1024);
}


BOOST_FIXTURE_TEST_CASE(stream_reduceWidth, StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::RvStream<UInt> in{ 24_b };
	In(in);

	scl::RvStream<UInt> out = scl::reduceWidth(in, 8_b);
	Out(out);

	// send data
	addSimulationProcess([=, this, &in]()->SimProcess {
		simu(valid(in)) = '0';
		simu(*in).invalidate();

		for(size_t i = 0; i < 8; ++i)
		{
			simu(valid(in)) = '1';
			simu(*in) =
				((i * 3 + 0) << 0) |
				((i * 3 + 1) << 8) |
				((i * 3 + 2) << 16);

			co_await scl::performTransferWait(in, m_clock);
		}
	});

	transfers(8 * 3);
	groups(1);
	simulateBackPressure(out);
	simulateRecvData(out);

	design.postprocess();
	runTicks(m_clock.getClk(), 1024);
}

BOOST_FIXTURE_TEST_CASE(stream_reduceWidth_RvPacketStream, StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::RvPacketStream<UInt> in{ 24_b };
	In(in);

	auto out = scl::reduceWidth(in, 8_b);
	Out(out);

	// send data
	addSimulationProcess([=, this, &in]()->SimProcess {
		for(size_t i = 0; i < 8; ++i)
		{
			simu(valid(in)) = '1';
			simu(eop(in)) = i % 2 == 1;
			simu(*in) =
				((i * 3 + 0) << 0) |
				((i * 3 + 1) << 8) |
				((i * 3 + 2) << 16);

			co_await scl::performTransferWait(in, m_clock);
		}
	});

	transfers(8 * 3);
	groups(1);
	simulateBackPressure(out);
	simulateRecvData(out);

	design.postprocess();
	runTicks(m_clock.getClk(), 1024);
}

BOOST_FIXTURE_TEST_CASE(stream_eraseFirstBeat, StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::RvPacketStream<UInt> in{ 8_b };
	In(in);

	scl::RvPacketStream<UInt> out = scl::eraseBeat(in, 0, 1);
	Out(out);

	// send data
	addSimulationProcess([=, this, &in]()->SimProcess {
		simu(valid(in)) = '0';
		simu(*in).invalidate();
		co_await AfterClk(m_clock);

		for(size_t i = 0; i < 32; i += 4)
		{
			for(size_t j = 0; j < 5; ++j)
			{
				simu(valid(in)) = '1';
				simu(*in) = uint8_t(i + j - 1);
				simu(eop(in)) = j == 4;

				co_await scl::performTransferWait(in, m_clock);
			}
		}
	});

	transfers(32);
	groups(1);
	simulateBackPressure(out);
	simulateRecvData(out);

	design.postprocess();
	runTicks(m_clock.getClk(), 1024);
}

BOOST_FIXTURE_TEST_CASE(stream_eraseLastBeat, StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::RvPacketStream<UInt> in{ 8_b };
	In(in);

	scl::RvPacketStream<UInt> out = scl::eraseLastBeat(in);
	Out(out);

	// send data
	addSimulationProcess([=, this, &in]()->SimProcess {
		simu(valid(in)) = '0';
		simu(*in).invalidate();
		co_await AfterClk(m_clock);

		for(size_t i = 0; i < 32; i += 4)
		{
			for(size_t j = 0; j < 5; ++j)
			{
				simu(valid(in)) = '1';
				simu(*in) = uint8_t(i + j);
				simu(eop(in)) = j == 4;

				co_await scl::performTransferWait(in, m_clock);
			}
		}
	});

	transfers(32);
	groups(1);
	simulateBackPressure(out);
	simulateRecvData(out);

	design.postprocess();
	runTicks(m_clock.getClk(), 1024);
}

BOOST_FIXTURE_TEST_CASE(stream_insertFirstBeat, StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::RvPacketStream<UInt> in{ 8_b };
	In(in);

	UInt insertData = pinIn(8_b).setName("insertData");
	scl::RvPacketStream<UInt> out = scl::insertBeat(in, 0, insertData);
	Out(out);

	// send data
	addSimulationProcess([=, this, &in]()->SimProcess {
		simu(valid(in)) = '0';
		simu(*in).invalidate();
		co_await AfterClk(m_clock);

		for(size_t i = 0; i < 32; i += 4)
		{
			for(size_t j = 0; j < 3; ++j)
			{
				simu(valid(in)) = '1';
				simu(insertData) = i + j;
				simu(*in) = uint8_t(i + j + 1);
				simu(eop(in)) = j == 2;

				co_await scl::performTransferWait(in, m_clock);
			}
		}
	});

	transfers(32);
	groups(1);
	simulateBackPressure(out);
	simulateRecvData(out);

	design.postprocess();
	runTicks(m_clock.getClk(), 1024);
}

BOOST_FIXTURE_TEST_CASE(stream_addEopDeferred, StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::RvStream<UInt> in{ 8_b };
	In(in);

	Bit eop = pinIn().setName("eop");
	scl::RvPacketStream<UInt> out = scl::addEopDeferred(in, eop);
	Out(out);

	// generate eop insert signal
	addSimulationProcess([=, this, &in]()->SimProcess {
		
		simu(eop) = '0';
		while(true)
		{
			co_await WaitStable();
			while (simu(valid(in)) == '0')
			{
				co_await AfterClk(m_clock);
				co_await WaitStable();
			}
			while(simu(valid(in)) == '1')
			{
				co_await AfterClk(m_clock);
				co_await WaitStable();
			}
			co_await WaitFor(Seconds{1,10}/m_clock.absoluteFrequency());
			simu(eop) = '1';
			co_await AfterClk(m_clock);
			simu(eop) = '0';
		}

	});

	transfers(32);
	groups(1);
	simulateSendData(in, 0);
	simulateBackPressure(out);
	simulateRecvData(out);

	design.postprocess();
	runTicks(m_clock.getClk(), 1024);
}

BOOST_FIXTURE_TEST_CASE(stream_addPacketSignalsFromSize, StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::RvStream<UInt> in{ 8_b };
	In(in);

	UInt size = 4_b;
	size = reg(size, 1);
	scl::RvPacketStream<UInt, scl::Sop> out = scl::addPacketSignalsFromCount(in, size);

	IF(transfer(out) & eop(out))
		size += 1;

	Out(out);

	transfers(32);
	groups(1);
	simulateSendData(in, 0);
	simulateBackPressure(out);
	simulateRecvData(out);

	design.postprocess();
	runTicks(m_clock.getClk(), 1024);
}

BOOST_FIXTURE_TEST_CASE(spi_stream_test, StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::RvStream<UInt> in{ .data = 8_b };
	In(in);

	scl::RvStream<BVec> inBVec = in.transform([](const UInt& v) { return (BVec)v; });
	scl::RvStream<BVec> outBVec = scl::SpiMaster{}.pinTestLoop().clockDiv(3).generate(inBVec);

	scl::RvStream<UInt> out = outBVec.transform([](const BVec& v) { return (UInt)v; });
	Out(out);

	simulateTransferTest(in, out);

	design.postprocess();
	runTicks(m_clock.getClk(), 4096);
}

BOOST_FIXTURE_TEST_CASE(stream_stall, StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::RvStream<UInt> in{ .data = 5_b };
	In(in);

	Bit stallCondition = pinIn().setName("stall");
	scl::RvStream<UInt> out = stall(in, stallCondition);
	Out(out);

	addSimulationProcess([=, this, &out, &in]()->SimProcess {

		simu(stallCondition) = '0';

		do
			co_await OnClk(m_clock);
		while (simu(valid(out)) == '0');
		co_await AfterClk(m_clock);
		co_await AfterClk(m_clock);

		std::mt19937 rng{ std::random_device{}() };
		while (true)
		{
			if (rng() % 4 != 0)
			{
				simu(stallCondition) = '1';
				co_await WaitStable();
				BOOST_TEST(simu(valid(out)) == '0');
				BOOST_TEST(simu(ready(in)) == '0');
			}
			else
			{
				simu(stallCondition) = '0';
			}
			co_await AfterClk(m_clock);
		}

	});

	simulateTransferTest(in, out);

	design.postprocess();
	runTicks(m_clock.getClk(), 1024);
}

BOOST_FIXTURE_TEST_CASE(ReqAckSync_1_10, StreamTransferFixture)
{
	Clock outClk({ .absoluteFrequency = 10'000'000 });
	HCL_NAMED(outClk);
	ClockScope clkScp(m_clock);

	scl::RvStream<UInt> in{ .data = 5_b };
	In(in);
	simulateSendData(in, 0);
	groups(1);

	scl::RvStream<UInt> out = gtry::scl::synchronizeStreamReqAck(in, m_clock, outClk);
	{
		ClockScope clock(outClk);
		Out(out);

		simulateBackPressure(out);
		simulateRecvData(out);
	}

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 50, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(ReqAckSync_1_1, StreamTransferFixture)
{
	Clock outClk({ .absoluteFrequency = 100'000'000 });
	HCL_NAMED(outClk);
	ClockScope clkScp(m_clock);

	scl::RvStream<UInt> in{ .data = 5_b };
	In(in);
	simulateSendData(in, 0);
	groups(1);

	scl::RvStream<UInt> out = gtry::scl::synchronizeStreamReqAck(in, m_clock, outClk);
	{
		ClockScope clock(outClk);
		Out(out);

		simulateBackPressure(out);
		simulateRecvData(out);
	}

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 50, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(ReqAckSync_10_1, StreamTransferFixture)
{
	Clock outClk({ .absoluteFrequency = 1000'000'000 });
	HCL_NAMED(outClk);
	ClockScope clkScp(m_clock);

	scl::RvStream<UInt> in{ .data = 5_b };
	In(in);
	simulateSendData(in, 0);
	groups(1);

	scl::RvStream<UInt> out = gtry::scl::synchronizeStreamReqAck(in, m_clock, outClk);
	{
		ClockScope clock(outClk);
		Out(out);

		simulateBackPressure(out);
		simulateRecvData(out);
	}

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 50, 1'000'000 }));
}


BOOST_FIXTURE_TEST_CASE(TransactionalFifo_StoreForwardStream, StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::RvPacketStream<UInt, scl::Error> in = { 16_b };
	scl::RvPacketStream<UInt> out = scl::storeForwardFifo(in, 32);

	error(in) = '0';

	In(in);
	Out(out);
	transfers(1000);
	simulateTransferTest(in, out);

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 50, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(TransactionalFifo_StoreForwardStream_PayloadOnly, StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::TransactionalFifo<UInt> fifo{ 32, 16_b };

	scl::RvPacketStream<UInt, scl::Error> in = { 16_b };
	In(in);
	error(in) = '0';
	fifo <<= in;

	scl::RvPacketStream<UInt> out = { 16_b };
	out <<= fifo;
	Out(out);

	fifo.generate();

	simulateTransferTest(in, out);

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 50, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(TransactionalFifo_StoreForwardStream_sopeop, StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::RsPacketStream<UInt> in = { 16_b };
	scl::RsPacketStream<UInt> out = scl::storeForwardFifo(in, 32);

	In(in);
	Out(out);
	transfers(1000);
	simulateTransferTest(in, out);

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 50, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(TransactionalFifoCDCSafe, StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::RvPacketStream<UInt> in = { 16_b };
	scl::RvPacketStream<UInt> out;
	

	scl::TransactionalFifo fifo(32, scl::PacketStream<UInt>{ in->width() });
	
	In(in);

	fifo <<= in;

	simulateSendData(in, 0);
	transfers(100);
	groups(1);

	Clock outClk({ .absoluteFrequency = 100'000'000 });
	HCL_NAMED(outClk);
	{
		ClockScope clock(outClk);
		out <<= fifo;
		Out(out);
		fifo.generate();

		simulateBackPressure(out);
		simulateRecvData(out);
	}

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 50, 1'000'000 }));
}

