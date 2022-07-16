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
		simu(ready(uut)) = 1;
		simu(valid(in0)) = 0;
		simu(valid(in1)) = 0;
		simu(*in0) = 0;
		simu(*in1) = 0;
		co_await WaitClk(clock);

		simu(valid(in0)) = 0;
		simu(valid(in1)) = 1;
		simu(*in1) = 1;
		co_await WaitClk(clock);

		simu(valid(in1)) = 0;
		simu(valid(in0)) = 1;
		simu(*in0) = 2;
		co_await WaitClk(clock);

		simu(valid(in1)) = 1;
		simu(valid(in0)) = 1;
		simu(*in0) = 3;
		simu(*in1) = 4;
		co_await WaitClk(clock);
		co_await WaitClk(clock);

		simu(valid(in1)) = 1;
		simu(valid(in0)) = 1;
		simu(*in0) = 5;
		simu(*in1) = 6;
		co_await WaitClk(clock);
		co_await WaitClk(clock);

		simu(valid(in0)) = 0;
		simu(valid(in1)) = 1;
		simu(*in1) = 7;
		co_await WaitClk(clock);

		simu(valid(in1)) = 0;
		simu(valid(in0)) = 0;
		simu(ready(uut)) = 0;
		co_await WaitClk(clock);

		simu(valid(in1)) = 0;
		simu(valid(in0)) = 1;
		simu(*in0) = 8;
		simu(ready(uut)) = 1;
		co_await WaitClk(clock);

		simu(valid(in1)) = 0;
		simu(valid(in0)) = 0;
		co_await WaitClk(clock);
	});

	addSimulationProcess([&]()->SimProcess {

		size_t counter = 1;
		while (true)
		{
			if (simu(ready(uut)) && simu(valid(uut)))
			{
				BOOST_TEST(counter == simu(*uut));
				counter++;
			}
			co_await WaitClk(clock);
		}

	});

	//sim::VCDSink vcd{ design.getCircuit(), getSimulator(), "arbitrateInOrder_basic.vcd" };
   //vcd.addAllPins();
	//vcd.addAllNamedSignals();

	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
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
		simu(ready(uut)) = 1;
		simu(valid(in0)) = 0;
		simu(valid(in1)) = 0;

		std::mt19937 rng{ 10179 };
		size_t counter = 1;
		bool wasReady = false;
		while(true)
		{
			if(wasReady)
			{
				if(rng() % 2 == 0)
				{
					simu(valid(in0)) = 1;
					simu(*in0) = counter++;
				}
				else
				{
					simu(valid(in0)) = 0;
				}

				if(rng() % 2 == 0)
				{
					simu(valid(in1)) = 1;
					simu(*in1) = counter++;
				}
				else
				{
					simu(valid(in1)) = 0;
				}
			}

			// chaos monkey
			simu(ready(uut)) = rng() % 8 != 0 ? 1 : 0;

			wasReady = simu(ready(in0)) != 0;

			co_await WaitClk(clock);
		}
	});


	addSimulationProcess([&]()->SimProcess {

		size_t counter = 1;
		while(true)
		{
			if(simu(ready(uut)) && simu(valid(uut)))
			{
				BOOST_TEST(counter % 256 == simu(*uut));
				counter++;
			}
			co_await WaitClk(clock);
		}

	});

	//sim::VCDSink vcd{ design.getCircuit(), getSimulator(), "arbitrateInOrder_fuzz.vcd" };
	//vcd.addAllPins();
	//vcd.addAllNamedSignals();

	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
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

	void simulateTransferTest(scl::RvStream<UInt>& source, scl::RvStream<UInt>& sink)
	{
		simulateBackPreassure(sink);
		simulateSendData(source, m_groups++);
		simulateRecvData(sink);
	}

	template<scl::StreamSignal T>
	void simulateArbiterTestSink(T& sink)
	{
		simulateBackPreassure(sink);
		simulateRecvData(sink);
	}

	template<scl::StreamSignal T>
	void simulateArbiterTestSource(T& source)
	{
		simulateSendData(source, m_groups++);
	}

	void In(scl::RvStream<UInt>& stream, std::string prefix = "in_")
	{
		gtry::pinOut(ready(stream)).setName(prefix + "ready");
		valid(stream) = gtry::pinIn().setName(prefix + "valid");
		*stream = gtry::pinIn(stream->width()).setName(prefix + "data");
	}

	void In(scl::RvPacketStream<UInt>& stream, std::string prefix = "in_")
	{
		gtry::pinOut(ready(stream)).setName(prefix + "ready");
		valid(stream) = gtry::pinIn().setName(prefix + "valid");
		eop(stream) = gtry::pinIn().setName(prefix + "eop");
		*stream = gtry::pinIn(stream->width()).setName(prefix + "data");
	}

	void Out(scl::RvStream<UInt>& stream, std::string prefix = "out_")
	{
		ready(stream) = gtry::pinIn().setName(prefix + "ready");
		gtry::pinOut(valid(stream)).setName(prefix + "valid");
		gtry::pinOut(*stream).setName(prefix + "data");
	}

	void Out(scl::RvPacketStream<UInt>& stream, std::string prefix = "out_")
	{
		ready(stream) = gtry::pinIn().setName(prefix + "ready");
		gtry::pinOut(valid(stream)).setName(prefix + "valid");
		gtry::pinOut(eop(stream)).setName(prefix + "eop");
		gtry::pinOut(*stream).setName(prefix + "data");
	}

	template<scl::StreamSignal T>
	void simulateBackPreassure(T& stream)
	{
		addSimulationProcess([&]()->SimProcess {
			std::mt19937 rng{ std::random_device{}() };

			simu(ready(stream)) = 0;
			while(simu(valid(stream)) == 0)
				co_await WaitClk(m_clock);

			while(true)
			{
				simu(ready(stream)) = rng() % 2;
				co_await WaitClk(m_clock);
			}
		});
	}

	void simulateSendData(scl::RvStream<UInt>& stream, size_t group)
	{
		addSimulationProcess([=, &stream]()->SimProcess {
			std::mt19937 rng{ std::random_device{}() };
			for(size_t i = 0; i < m_transfers; ++i)
			{
				simu(valid(stream)) = 0;
				simu(*stream).invalidate();

				while((rng() & 1) == 0)
					co_await WaitClk(m_clock);

				simu(valid(stream)) = 1;
				simu(*stream) = i + group * m_transfers;

				co_await WaitFor(0);
				while(simu(ready(stream)) == 0)  
				{
					co_await WaitClk(m_clock);
					co_await WaitFor(0);
				}

				co_await WaitClk(m_clock);
			}
			simu(valid(stream)) = 0;
			simu(*stream).invalidate();
		});
	}

	void simulateSendData(scl::RvPacketStream<UInt>& stream, size_t group)
	{
		addSimulationProcess([=, &stream]()->SimProcess {
			std::mt19937 rng{ std::random_device{}() };
			for(size_t i = 0; i < m_transfers;)
			{
				const size_t packetLen = std::min<size_t>(m_transfers - i, rng() % 5 + 1);
				for(size_t j = 0; j < packetLen; ++j)
				{
					simu(valid(stream)) = 0;
					simu(eop(stream)).invalidate();
					simu(*stream).invalidate();

					while((rng() & 1) == 0)
						co_await WaitClk(m_clock);

					simu(valid(stream)) = 1;
					simu(eop(stream)) = j == packetLen - 1 ? 1 : 0;
					simu(*stream) = i + j + group * m_transfers;

					co_await WaitFor(0);
					while(simu(ready(stream)) == 0)
					{
						co_await WaitClk(m_clock);
						co_await WaitFor(0);
					}

					co_await WaitClk(m_clock);
				}
				i += packetLen;
			}
			simu(valid(stream)) = 0;
			simu(*stream).invalidate();
		});
	}

	template<scl::StreamSignal T>
	void simulateRecvData(const T& stream)
	{
		addSimulationProcess([=, &stream]()->SimProcess {
			std::vector<size_t> expectedValue(m_groups);
			while(true)
			{
				co_await WaitFor(0);
				co_await WaitFor(0);
				if(simu(ready(stream)) == 1 &&
					simu(valid(stream)) == 1)
				{
					size_t data = simu(*(stream.operator ->()));
					BOOST_TEST(data / m_transfers < expectedValue.size());
					if (data / m_transfers < expectedValue.size())
					{
						BOOST_TEST(data % m_transfers == expectedValue[data / m_transfers]);
						expectedValue[data / m_transfers]++;
					}
				}
				co_await WaitClk(m_clock);

				if(std::ranges::all_of(expectedValue, [=](size_t val) { return val == m_transfers; }))
				{
					stopTest();
					co_await WaitClk(m_clock);
				}
			}
		});
	}

private:
	size_t m_groups = 0;
	size_t m_transfers = 16;
};

BOOST_FIXTURE_TEST_CASE(stream_downstreamReg, StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::RvStream<UInt> in{ .data = 5_b };
	In(in);

	scl::RvStream<UInt> out = in.regDownstream();
	Out(out);

	simulateTransferTest(in, out);

	//recordVCD("stream_downstreamReg.vcd");
	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
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
	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
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
	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
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
	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
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
	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
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
	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
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
	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
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
	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
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
	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
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
	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
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
	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
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
	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
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
	addSimulationProcess([=, &in]()->SimProcess {
		simu(valid(in)) = 0;
		simu(*in).invalidate();
		for (size_t i = 0; i < 4; ++i)
			co_await WaitClk(m_clock);

		for (size_t i = 0; i < 32; ++i)
		{
			for (size_t j = 0; j < 2; ++j)
			{
				simu(valid(in)) = 1;
				simu(*in) = (i >> (j * 4)) & 0xF;

				co_await WaitFor(0);
				while (simu(ready(in)) == 0)
					co_await WaitClk(m_clock);
				co_await WaitClk(m_clock);
			}
		}
		});

	transfers(32);
	groups(1);
	simulateBackPreassure(out);
	simulateRecvData(out);

	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
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
	addSimulationProcess([=, &in]()->SimProcess {
		simu(valid(in)) = 0;
		simu(*in).invalidate();

		for(size_t i = 0; i < 8; ++i)
		{
			simu(valid(in)) = 1;
			simu(*in) =
				((i * 3 + 0) << 0) |
				((i * 3 + 1) << 8) |
				((i * 3 + 2) << 16);
			co_await WaitFor(0);
			while(simu(ready(in)) == 0)
				co_await WaitClk(m_clock);
			co_await WaitClk(m_clock);
		}
	});

	transfers(8 * 3);
	groups(1);
	simulateBackPreassure(out);
	simulateRecvData(out);

	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
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
	addSimulationProcess([=, &in]()->SimProcess {
		for(size_t i = 0; i < 8; ++i)
		{
			simu(valid(in)) = 1;
			simu(eop(in)) = i % 2 == 1;
			simu(*in) =
				((i * 3 + 0) << 0) |
				((i * 3 + 1) << 8) |
				((i * 3 + 2) << 16);

			co_await WaitFor(0);
			while(simu(ready(in)) == 0)
				co_await WaitClk(m_clock);
			co_await WaitClk(m_clock);
		}
	});

	transfers(8 * 3);
	groups(1);
	simulateBackPreassure(out);
	simulateRecvData(out);

	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
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
	addSimulationProcess([=, &in]()->SimProcess {
		simu(valid(in)) = 0;
		simu(*in).invalidate();
		co_await WaitClk(m_clock);

		for(size_t i = 0; i < 32; i += 4)
		{
			for(size_t j = 0; j < 5; ++j)
			{
				simu(valid(in)) = 1;
				simu(*in) = uint8_t(i + j - 1);
				simu(eop(in)) = j == 4 ? 1 : 0;

				co_await WaitFor(0);
				while(simu(ready(in)) == 0)
					co_await WaitClk(m_clock);
				co_await WaitClk(m_clock);
			}
		}
	});

	transfers(32);
	groups(1);
	simulateBackPreassure(out);
	simulateRecvData(out);

	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
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
	addSimulationProcess([=, &in]()->SimProcess {
		simu(valid(in)) = 0;
		simu(*in).invalidate();
		co_await WaitClk(m_clock);

		for(size_t i = 0; i < 32; i += 4)
		{
			for(size_t j = 0; j < 5; ++j)
			{
				simu(valid(in)) = 1;
				simu(*in) = uint8_t(i + j);
				simu(eop(in)) = j == 4 ? 1 : 0;

				co_await WaitFor(0);
				while(simu(ready(in)) == 0)
					co_await WaitClk(m_clock);
				co_await WaitClk(m_clock);
			}
		}
	});

	transfers(32);
	groups(1);
	simulateBackPreassure(out);
	simulateRecvData(out);

	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
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
	addSimulationProcess([=, &in]()->SimProcess {
		simu(valid(in)) = 0;
		simu(*in).invalidate();
		co_await WaitClk(m_clock);

		for(size_t i = 0; i < 32; i += 4)
		{
			for(size_t j = 0; j < 3; ++j)
			{
				simu(valid(in)) = 1;
				simu(insertData) = i + j;
				simu(*in) = uint8_t(i + j + 1);
				simu(eop(in)) = j == 2 ? 1 : 0;

				co_await WaitFor(0);
				while(simu(ready(in)) == 0)
					co_await WaitClk(m_clock);
				co_await WaitClk(m_clock);
			}
		}
	});

	transfers(32);
	groups(1);
	simulateBackPreassure(out);
	simulateRecvData(out);

	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
	runTicks(m_clock.getClk(), 1024);
}
#if 0

BOOST_FIXTURE_TEST_CASE(stream_makePacketStreamDeffered, StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::Stream<UInt> in;
	in.data = 8_b;
	In(in);

	Bit eop = pinIn().setName("eop");
	scl::Stream<scl::Packet<UInt>> out = scl::makePacketStream(in, eop, true);
	Out(out);

	// send data
	addSimulationProcess([=, &in]()->SimProcess {
		
		simu(eop) = 0;
		while(true)
		{
			while(simu(valid(in)) == 0)
			{
				co_await WaitClk(m_clock);
				co_await WaitFor(0);
			}
			while(simu(valid(in)) == 1)
			{
				co_await WaitClk(m_clock);
				co_await WaitFor(0);
			}
			simu(eop) = 1;
			co_await WaitClk(m_clock);
			simu(eop) = 0;
		}

	});

	transfers(32);
	groups(1);
	simulateSendData(in, 0);
	simulateBackPreassure(out);
	simulateRecvData(out);

	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
	runTicks(m_clock.getClk(), 1024);
}

#endif
