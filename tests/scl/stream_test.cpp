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

#include <gatery/scl/synthesisTools/XilinxVivado.h>
#include <gatery/scl/stream/strm.h>
#include <gatery/scl/io/SpiMaster.h> 

#include <gatery/scl/sim/SimulationSequencer.h>
#include <gatery/scl/stream/SimuHelpers.h>

using namespace boost::unit_test;
using namespace gtry;

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
			co_await WaitFor(Seconds{ 1,10 } / recvClock.absoluteFrequency());

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
			for (size_t i = 0; i < m_transfers; ++i)
			{
				simu(valid(stream)) = '0';
				simu(*stream).invalidate();

				while ((rng() & 1) == 0)
					co_await AfterClk(m_clock);

				simu(valid(stream)) = '1';
				simu(*stream) = i + group * m_transfers;

				co_await performTransferWait(stream, m_clock);
			}
			simu(valid(stream)) = '0';
			simu(*stream).invalidate();
			});
	}

	template<class... Meta>
	void simulateSendData(scl::Stream<UInt, Meta...>& stream, size_t group)
	{
		addSimulationProcess([=, this, &stream]()->SimProcess {
			if constexpr (std::remove_reference_t<decltype(stream)>::template has<scl::Error>())
				simu(error(stream)) = '0';

			std::mt19937 rng{ std::random_device{}() };
			for (size_t i = 0; i < m_transfers;)
			{
				const size_t packetLen = std::min<size_t>(m_transfers - i, rng() % 5 + 1);
				co_await sendDataPacket(stream, group, i, packetLen, rng() & rng());
				i += packetLen;
			}
			simu(valid(stream)) = '0';
			simu(*stream).invalidate();
		});
	}

	template<class... Meta>
	SimProcess sendDataPacket(scl::Stream<UInt, Meta...>& stream, size_t group, size_t packetOffset, size_t packetLen, uint64_t invalidBeats = 0)
	{
		using StreamT = scl::Stream<UInt, Meta...>;
		constexpr bool hasValid = StreamT::template has<scl::Valid>();
		for (size_t j = 0; j < packetLen; ++j)
		{
			simu(eop(stream)).invalidate();
			simu(*stream).invalidate();

			if constexpr (hasValid)
			{
				simu(valid(stream)) = '0';
				for (;(invalidBeats & 1) != 0; invalidBeats >>= 1)
					co_await AfterClk(m_clock);
				invalidBeats >>= 1;
				simu(valid(stream)) = '1';
			}
			else
			{
				simu(sop(stream)) = j == 0;
			}

			simu(eop(stream)) = j == packetLen - 1;
			simu(*stream) = packetOffset + j + group * m_transfers;

			co_await performTransferWait(stream, m_clock);
		}

		if (!hasValid)
			simu(sop(stream)) = '0';
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

					co_await performTransferWait(stream, m_clock);
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
			while (true)
			{
				co_await OnClk(recvClock);

				if (simu(myTransfer) == '1')
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

	scl::RvStream<UInt> out = scl::strm::regDownstream(move(in));
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

	scl::RvStream<UInt> out = scl::strm::regReady(move(in));
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

	scl::RvStream<UInt> out = regDecouple(in);
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

	scl::RvStream<UInt> out = scl::strm::regDownstream(scl::strm::regDownstreamBlocking(scl::strm::regDownstreamBlocking(scl::strm::regDownstreamBlocking(move(in)))));
	Out(out);

	simulateTransferTest(in, out);

	//recordVCD("stream_reg_chaining.vcd");
	design.postprocess();
	runTicks(m_clock.getClk(), 1024);
}

BOOST_FIXTURE_TEST_CASE(stream_fifo_0, StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::RvStream<UInt> in{ .data = 10_b };
	In(in);

	scl::RvStream<UInt> out = scl::strm::fifo(move(in), 16, scl::FifoLatency(0));
	Out(out);

	transfers(500);
	simulateTransferTest(in, out);

	//recordVCD("stream_fifo.vcd");
	design.postprocess();
	runTicks(m_clock.getClk(), 1024);
}

BOOST_FIXTURE_TEST_CASE(stream_fifo_1, StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::RvStream<UInt> in{ .data = 10_b };
	In(in);

	scl::RvStream<UInt> out = scl::strm::fifo(move(in), 16, scl::FifoLatency(1));
	Out(out);

	transfers(500);
	simulateTransferTest(in, out);

	//recordVCD("stream_fifo.vcd");
	design.postprocess();
	runTicks(m_clock.getClk(), 1024);
}

BOOST_FIXTURE_TEST_CASE(stream_fifo_2, StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::RvStream<UInt> in{ .data = 10_b };
	In(in);

	scl::RvStream<UInt> out = scl::strm::fifo(move(in), 16, scl::FifoLatency(2));
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
		In(in[i], "in" + std::to_string(i));
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

BOOST_FIXTURE_TEST_CASE(streamArbiter_rrs5_packet, StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::StreamArbiter<scl::RvPacketStream<UInt>, scl::ArbiterPolicyRoundRobinStrict> arbiter;
	std::array<scl::RvPacketStream<UInt>, 5> in;
	for (size_t i = 0; i < in.size(); ++i)
	{
		*in[i] = 10_b;
		In(in[i], "in" + std::to_string(i));
		simulateArbiterTestSource(in[i]);
		arbiter.attach(in[i]);
	}
	arbiter.generate();

	Out(arbiter.out());
	simulateArbiterTestSink(arbiter.out());

	design.postprocess();
	runTicks(m_clock.getClk(), 1024);
}

BOOST_FIXTURE_TEST_CASE(stream_extendWidth, StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	{
		// add valid no ready compile test
		scl::Stream<UInt> inT{ 4_b };
		auto outT = scl::strm::extendWidth(move(inT), 8_b);
	}
	{
		// add valid compile test
		scl::Stream<UInt, scl::Ready> inT{ 4_b };
		auto outT = scl::strm::extendWidth(move(inT), 8_b);
	}

	scl::RvStream<UInt> in{ 4_b };
	In(in);

	auto out = scl::strm::extendWidth(move(in), 8_b);
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

				co_await performTransferWait(in, m_clock);
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

	scl::RvStream<UInt> out = scl::strm::reduceWidth(move(in), 8_b);
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

			co_await performTransferWait(in, m_clock);
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

	auto out = scl::strm::reduceWidth(move(in), 8_b);
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

			co_await performTransferWait(in, m_clock);
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

	scl::RvPacketStream<UInt> out = scl::strm::eraseBeat(move(in), 0, 1);
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

				co_await performTransferWait(in, m_clock);
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

	scl::RvPacketStream<UInt> out = eraseLastBeat(in);
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

				co_await performTransferWait(in, m_clock);
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
	scl::RvPacketStream<UInt> out = scl::strm::insertBeat(move(in), 0, insertData);
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

				co_await performTransferWait(in, m_clock);
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
	scl::RvPacketStream<UInt> out = addEopDeferred(in, eop);
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
	scl::RvPacketStream<UInt, scl::Sop> out = addPacketSignalsFromCount(in, size);

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
	scl::RvStream<UInt> out = scl::strm::stall(move(in), stallCondition);
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

	scl::RvStream<UInt> out = gtry::scl::strm::synchronizeStreamReqAck(in, m_clock, outClk);
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

	scl::RvStream<UInt> out = gtry::scl::strm::synchronizeStreamReqAck(in, m_clock, outClk);
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

	scl::RvStream<UInt> out = gtry::scl::strm::synchronizeStreamReqAck(in, m_clock, outClk);
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
	scl::RvPacketStream<UInt> out = storeForwardFifo(in, 32);
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
	scl::strm::pushStoreForward(fifo, move(in));

	scl::RvStream<UInt> out = scl::strm::pop(fifo);
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
	scl::Stream out = storeForwardFifo(in, 32);

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
	pushStoreForward(fifo, in);

	simulateSendData(in, 0);
	transfers(100);
	groups(1);

	Clock outClk({ .absoluteFrequency = 100'000'000 });
	HCL_NAMED(outClk);
	{
		ClockScope clock(outClk);
		out = scl::strm::pop(fifo);
		Out(out);
		fifo.generate();

		simulateBackPressure(out);
		simulateRecvData(out);
	}

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 50, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(addReadyAndFailOnBackpressure_test, StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::VPacketStream<UInt, scl::Error> in = { 16_b };
	scl::RvPacketStream<UInt, scl::Error> out = addReadyAndFailOnBackpressure(in);

	In(in);
	Out(out);
	groups(1);

	addSimulationProcess([=, this, &out, &in]()->SimProcess {
		simu(error(in)) = '0';
		simu(ready(out)) = '1';

		// simple packet passthrough test
		fork(sendDataPacket(in, 0, 0, 3));
		do co_await performTransferWait(out, m_clock);
		while(simu(eop(out)) == '0');
		BOOST_TEST(simu(error(out)) == '0');

		// simple error passthrough test
		fork(sendDataPacket(in, 0, 0, 3));
		simu(error(in)) = '1';
		do co_await performTransferWait(out, m_clock);
		while (simu(eop(out)) == '0');
		BOOST_TEST(simu(error(out)) == '1');
		simu(error(in)) = '0';

		// one beat not ready
		fork(sendDataPacket(in, 0, 0, 3));
		co_await OnClk(m_clock);
		simu(ready(out)) = '0';
		co_await OnClk(m_clock);
		simu(ready(out)) = '1';
		co_await OnClk(m_clock);
		BOOST_TEST(simu(eop(out)) == '1');
		BOOST_TEST(simu(error(out)) == '1');

		// next packet after error should be valid
		fork(sendDataPacket(in, 0, 0, 3));
		do co_await performTransferWait(out, m_clock);
		while (simu(eop(out)) == '0');
		BOOST_TEST(simu(error(out)) == '0');

		// eop beat not ready and bubble for generated eop
		fork(sendDataPacket(in, 0, 0, 3));
		co_await OnClk(m_clock);
		co_await OnClk(m_clock);
		simu(ready(out)) = '0';
		co_await OnClk(m_clock);
		co_await OnClk(m_clock);
		co_await OnClk(m_clock);
		simu(ready(out)) = '1';
		co_await OnClk(m_clock);
		BOOST_TEST(simu(valid(out)) == '1');
		BOOST_TEST(simu(eop(out)) == '1');
		BOOST_TEST(simu(error(out)) == '1');

		// next packet after error should be valid
		fork(sendDataPacket(in, 0, 0, 3));
		do co_await performTransferWait(out, m_clock);
		while (simu(eop(out)) == '0');
		BOOST_TEST(simu(error(out)) == '0');

		// eop beat not ready and NO bubble for generated eop
		fork(sendDataPacket(in, 0, 0, 3));
		co_await OnClk(m_clock);
		co_await OnClk(m_clock);
		simu(ready(out)) = '0';
		co_await OnClk(m_clock);
		co_await OnClk(m_clock);
		co_await OnClk(m_clock);
		simu(ready(out)) = '1';

		fork(sendDataPacket(in, 0, 0, 3));
		do co_await performTransferWait(out, m_clock);
		while (simu(eop(out)) == '0');
		BOOST_TEST(simu(error(out)) == '1');


		stopTest();

	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 50, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(addReadyAndFailOnBackpressure_sop_test, StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::SPacketStream<UInt, scl::Error> in = { 16_b };
	scl::RsPacketStream<UInt, scl::Error> out = addReadyAndFailOnBackpressure(in);

	In(in);
	Out(out);
	groups(1);

	addSimulationProcess([=, this, &out, &in]()->SimProcess {
		simu(error(in)) = '0';
		simu(ready(out)) = '1';

		// simple packet passthrough test
		fork(sendDataPacket(in, 0, 0, 3));
		do co_await performTransferWait(out, m_clock);
		while (simu(eop(out)) == '0');
		BOOST_TEST(simu(error(out)) == '0');

		// simple error passthrough test
		fork(sendDataPacket(in, 0, 0, 3));
		simu(error(in)) = '1';
		do co_await performTransferWait(out, m_clock);
		while (simu(eop(out)) == '0');
		BOOST_TEST(simu(error(out)) == '1');
		simu(error(in)) = '0';

		// one beat not ready
		fork(sendDataPacket(in, 0, 0, 3));
		co_await OnClk(m_clock);
		simu(ready(out)) = '0';
		co_await OnClk(m_clock);
		simu(ready(out)) = '1';
		co_await OnClk(m_clock);
		BOOST_TEST(simu(eop(out)) == '1');
		BOOST_TEST(simu(error(out)) == '1');

		// next packet after error should be valid
		fork(sendDataPacket(in, 0, 0, 3));
		do co_await performTransferWait(out, m_clock);
		while (simu(eop(out)) == '0');
		BOOST_TEST(simu(error(out)) == '0');

		// eop beat not ready and bubble for generated eop
		fork(sendDataPacket(in, 0, 6, 3));
		co_await OnClk(m_clock);
		co_await OnClk(m_clock);
		simu(ready(out)) = '0';
		co_await OnClk(m_clock);
		co_await OnClk(m_clock);
		co_await OnClk(m_clock);
		simu(ready(out)) = '1';
		co_await OnClk(m_clock);
		BOOST_TEST(simu(eop(out)) == '1');
		BOOST_TEST(simu(error(out)) == '1');

		// next packet after error should be valid
		fork(sendDataPacket(in, 0, 0, 3));
		do co_await performTransferWait(out, m_clock);
		while (simu(eop(out)) == '0');
		BOOST_TEST(simu(error(out)) == '0');

		// eop beat not ready and NO bubble for generated eop
		fork(sendDataPacket(in, 0, 0, 3));
		co_await OnClk(m_clock);
		co_await OnClk(m_clock);
		simu(ready(out)) = '0';
		co_await OnClk(m_clock);
		co_await OnClk(m_clock);
		co_await OnClk(m_clock);
		simu(ready(out)) = '1';

		fork(sendDataPacket(in, 0, 0, 3));
		do co_await performTransferWait(out, m_clock);
		while (simu(eop(out)) == '0');
		BOOST_TEST(simu(error(out)) == '1');


		stopTest();

		});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 50, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(store_forward_fifo_fuzz, BoostUnitTestSimulationFixture)
{
	Clock clk = Clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScope(clk);

	std::mt19937 rng{ 12524 };

	scl::RvPacketStream<BVec, scl::Error> inPacketStream{ 8_b };
	auto outPacketStream = storeForwardFifo(inPacketStream, 16);
	pinIn(inPacketStream, "inPacketStream");
	pinOut(outPacketStream, "outPacketStream");

	std::uniform_int_distribution<size_t> rngSize{ 1, 16 };
	std::queue<scl::strm::SimPacket> packets;

	// insert packets
	addSimulationProcess([&, this]()->SimProcess {
		std::vector<uint8_t> data;
		for (size_t i = 0; i < 256; ++i)
		{
			data.resize(rngSize(rng));
			for (uint8_t& it : data)
				it = (uint8_t)rng();

			scl::strm::SimPacket packet{ data };
			packet.error(rng() % 2 + '0');
			packet.invalidBeats(rng() & rng());
			co_await sendPacket(inPacketStream, packet, clk);

			if (packet.error() == '0')
				packets.push(packet);
		}

		while (!packets.empty())
			co_await OnClk(clk);
		stopTest();
	});

	// receive packets
	addSimulationProcess([&, this]()->SimProcess {
		fork(readyDriverRNG(outPacketStream, clk, 50));
		while (true)
		{
			scl::strm::SimPacket packet = co_await receivePacket(outPacketStream, clk);
			BOOST_TEST(!packets.empty());
			if (!packets.empty())
			{
				BOOST_TEST(packets.front().payload == packet.payload);
				packets.pop();
			}
		}
	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 50, 1'000'000 }));
}
 

BOOST_FIXTURE_TEST_CASE(streamBroadcaster, BoostUnitTestSimulationFixture)
{
	Clock clk = Clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScope(clk);

	std::mt19937 rng{ 12524 };

	scl::RvStream<BVec> inStream{ 8_b };
	scl::RvStream<BVec> outStream1{ 8_b };
	scl::RvStream<BVec> outStream2{ 8_b };

	{
		scl::StreamBroadcaster inBCaster(inStream);

		outStream1 <<= inBCaster;
		outStream2 <<= inBCaster;
	}

	pinIn(inStream, "inStream");
	pinOut(outStream1, "outStream1");
	pinOut(outStream2, "outStream2");

	addSimulationProcess([&, this]()->SimProcess {

		std::vector<std::uint8_t> allData;
		allData.resize(1024);
		for (auto &b : allData)
			b = rng();

		size_t numDone = 0;

		auto checkOutput = [&](scl::RvStream<BVec> &stream)->SimProcess {

			fork([&]()->SimProcess {
				while (true) {
					simu(ready(stream)) = (rng() & 1) == 1;
					co_await OnClk(clk);
				}
			});

			for (auto &b : allData) {
				co_await performTransferWait(stream, clk);
				BOOST_TEST(simu(*stream) == b);
			}
			numDone++;
		};

		fork([&]()->SimProcess {
			while (true) {
				simu(valid(inStream)) = (rng() & 1) == 1;
				co_await OnClk(clk);
			}
		});

		fork(checkOutput(outStream1));
		fork(checkOutput(outStream2));

		for (auto &b : allData) {
			simu(*inStream) = b;
			co_await performTransferWait(inStream, clk);
		}

		while (numDone != 2)
			co_await OnClk(clk);

		stopTest();
	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 10000, 100'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(streamShiftLeft_test, BoostUnitTestSimulationFixture)
{
	Clock clk = Clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScope(clk);

	UInt shift = pinIn(4_b).setName("shift");

	scl::RvPacketStream<BVec, scl::Empty> in{ 16_b };
	empty(in) = 1_b;

	auto out = streamShiftLeft(in, shift);
	pinIn(in, "in");
	pinOut(out, "out");

	// insert packets
	addSimulationProcess([&, this]()->SimProcess {
		fork(readyDriverRNG(out, clk, 50));

		for(size_t e = 0; e < empty(in).width().count(); ++e)
		{
			scl::strm::SimPacket packet{ 
				e ? 0xFF0000FFFF0000ull : 0xFFFF0000FFFF0000ull,
				e ? 56_b : 64_b 
			};

			for (size_t i = 0; i < shift.width().count(); ++i)
			{
				simu(shift) = i;
				fork(sendPacket(in, packet, clk));

				scl::strm::SimPacket outPacket = co_await receivePacket(out, clk);
				BOOST_TEST(outPacket.payload.size() == packet.payload.size() + i);
			}
		}

		co_await OnClk(clk);
		stopTest();
	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 50, 1'000'000 }));
}

struct stream_shiftRight_test : BoostUnitTestSimulationFixture
{
	size_t streamShift = 12;
	BitWidth packetSize = 64_b;
	BitWidth streamW = 16_b;

	std::optional<uint8_t> backpressureReadyPercentage;

	bool hasAnticipatedEnd() { return (packetSize.value % streamW.value) <= streamShift; }

	void execute() {
		std::mt19937_64 mt(std::random_device{}());
		Clock clk = Clock({ .absoluteFrequency = 100'000'000 });
		ClockScope clkScope(clk);

		UInt shift = streamShift;

		scl::RvPacketStream<BVec, scl::EmptyBits> in{ streamW };
		emptyBits(in) = 4_b;

		auto out = streamShiftRight(move(in), shift);
		pinIn(in, "in");
		pinOut(out, "out");

		uint64_t payload = mt() & packetSize.mask();
		scl::strm::SimPacket inputPacket(payload, packetSize);

		size_t iterations = 4;

		addSimulationProcess(
			[&, this]()->SimProcess {
				if(backpressureReadyPercentage)
					return scl::strm::readyDriverRNG(out, clk, *backpressureReadyPercentage);
				else
					return scl::strm::readyDriver(out, clk);
			}
		);

		addSimulationProcess(
			[&, this]()->SimProcess {
				scl::SimulationSequencer seq;
				for (size_t i = 0; i < iterations; i++)
				{
					fork(scl::strm::sendPacket(in, inputPacket, clk, seq));
				}

				for (size_t i = 0; i < iterations; i++)
				{
					scl::strm::SimPacket outputPacket = co_await scl::strm::receivePacket(out, clk);
					BOOST_TEST(outputPacket.payload == scl::strm::SimPacket(payload >> streamShift, packetSize - streamShift).payload);
				}
				co_await OnClk(clk);
				co_await OnClk(clk);
				co_await OnClk(clk);
				stopTest();
			}
		);

		if(false) recordVCD("stream_shiftRight.vcd");
		design.postprocess();
		//design.visualize("stream_shiftRight");
		BOOST_TEST(!runHitsTimeout({ 50, 1'000'000 }));
	}
};

BOOST_FIXTURE_TEST_CASE(stream_shiftRight_test_poc, stream_shiftRight_test)
{
	streamShift = 4;
	packetSize = 64_b;
	streamW = 16_b;

	execute();
}
BOOST_FIXTURE_TEST_CASE(stream_shiftRight_test_anticipated_end, stream_shiftRight_test)
{
	streamShift = 12;
	packetSize = 60_b;
	streamW = 16_b;
	BOOST_TEST(hasAnticipatedEnd());
	execute();
}

BOOST_FIXTURE_TEST_CASE(stream_shiftRight_test_no_anticipated_end, stream_shiftRight_test)
{
	streamShift = 11;
	packetSize = 60_b;
	streamW = 16_b;
	BOOST_TEST(!hasAnticipatedEnd());
	execute();
}

BOOST_FIXTURE_TEST_CASE(stream_shiftRight_test_anticipated_end_chaos_light, stream_shiftRight_test)
{
	streamShift = 12;
	packetSize = 60_b;
	streamW = 16_b;
	backpressureReadyPercentage = 50;
	BOOST_TEST(hasAnticipatedEnd());
	execute();
}

BOOST_FIXTURE_TEST_CASE(stream_shiftRight_test_one_beat_packet, stream_shiftRight_test)
{
	streamShift = 4;
	packetSize = 12_b;
	streamW = 16_b;
	backpressureReadyPercentage = 50;
	execute();
}

BOOST_FIXTURE_TEST_CASE(stream_shiftRight_dynamic, BoostUnitTestSimulationFixture)
{
	std::mt19937_64 mt(std::random_device{}());
	Clock clk = Clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScope(clk);

	BitWidth packetSize = 60_b;
	BitWidth streamW = 16_b;
	size_t backpressureReadyPercentage = 100;

	UInt shift = 3_b;
	pinIn(shift, "shift");

	scl::RvPacketStream<BVec, scl::EmptyBits> in{ streamW };
	emptyBits(in) = 4_b;
	pinIn(in, "in");

	UInt shiftStable = scl::capture(shift, valid(in) & sop(in));

	auto out = streamShiftRight(move(in), shiftStable);
	pinOut(out, "out");

	uint64_t payload = mt() & packetSize.mask();
	scl::strm::SimPacket inputPacket(payload, packetSize);

	size_t iterations = 4;

	addSimulationProcess(
		[&, this]()->SimProcess {
				return scl::strm::readyDriverRNG(out, clk, backpressureReadyPercentage);
		}
	);

	
	addSimulationProcess([&, this]()->SimProcess {
		size_t shiftAmt = 1;
		for (size_t i = 0; i < iterations; i++)
		{
			simu(shift) = shiftAmt++;
			co_await scl::strm::sendPacket(in, inputPacket, clk);
		}
		
		simu(shift) = shiftAmt--;
		for (size_t i = 0; i < iterations; i++)
		{
			simu(shift) = shiftAmt--;
			co_await scl::strm::sendPacket(in, inputPacket, clk);
		}
	});	

	addSimulationProcess(
		[&, this]()->SimProcess {
			size_t shiftAmt = 1;
			for (size_t i = 0; i < iterations; i++)
			{
				scl::strm::SimPacket outputPacket = co_await scl::strm::receivePacket(out, clk);
				BOOST_TEST(outputPacket.payload == scl::strm::SimPacket(payload >> shiftAmt, packetSize - shiftAmt).payload /*, "going up shift by " + std::to_string(shiftAmt) + " broken"*/);
				shiftAmt++;
			}
			shiftAmt--;
			for (size_t i = 0; i < iterations; i++)
			{
				scl::strm::SimPacket outputPacket = co_await scl::strm::receivePacket(out, clk);
				BOOST_TEST(outputPacket.payload == scl::strm::SimPacket(payload >> shiftAmt, packetSize - shiftAmt).payload, "going down shift by " + std::to_string(shiftAmt) + " broken");
				shiftAmt--;
			}
			co_await OnClk(clk);
			co_await OnClk(clk);
			co_await OnClk(clk);
			stopTest();
		}
	);

	if(false) recordVCD("stream_shiftRight.vcd");
	design.postprocess();
	//design.visualize("stream_shiftRight");
	BOOST_TEST(!runHitsTimeout({ 50, 1'000'000 }));
}

struct stream_shiftRight_better_fixture : BoostUnitTestSimulationFixture
{
	std::mt19937 rng;
	BitWidth streamW = 16_b;
	size_t numPackets = 10;
	size_t waitBetweenPackets = 5;
	std::function<size_t()> getPacketSize = [](){ return 64; };
	std::function<size_t()> getShiftAmt = [](){ return 4; };
	std::function<uint64_t()> getInvalidMask = [](){ return 0; };

	std::optional<uint8_t> backpressureReadyPercentage;
	Seconds simuTime = { 50, 1'000'000 };

	void execute() {
		rng.seed(3080);
		Clock clk = Clock({ .absoluteFrequency = 100'000'000 });
		ClockScope clkScope(clk);

		UInt shiftAmt = BitWidth::count(streamW.bits());
		pinIn(shiftAmt, "shiftAmt");

		scl::RvPacketStream<BVec, scl::EmptyBits> in{ streamW };
		emptyBits(in) = BitWidth::count(streamW.bits());
		pinIn(in, "in");

		auto out = streamShiftRight(move(in), shiftAmt);
		pinOut(out, "out");

		std::vector<std::tuple<sim::DefaultBitVectorState, size_t>> testCases(numPackets);
		for (auto& testCase : testCases) {
			std::get<size_t>(testCase) = getShiftAmt();
			auto& dbv = std::get<sim::DefaultBitVectorState>(testCase);
			auto size = getPacketSize();
			while(size <= std::get<size_t>(testCase))
				size = getPacketSize(); // very inefficient
			dbv = sim::createRandomDefaultBitVectorState(size, rng);
			dbv.setRange(sim::DefaultConfig::DEFINED, 0, dbv.size()); //not necessary but nice for visual debugging
		}

		addSimulationProcess(
			[&, this]()->SimProcess {
				if(backpressureReadyPercentage)
					return scl::strm::readyDriverRNG(out, clk, *backpressureReadyPercentage);
				else
					return scl::strm::readyDriver(out, clk);
			}
		);

		//send data
		addSimulationProcess(
			[&, this]()->SimProcess {
				for(const auto& testCase : testCases){
					const auto& dbv = std::get<sim::DefaultBitVectorState>(testCase);
					size_t packetSize = dbv.size();
					size_t shiftAmt = std::get<size_t>(testCase);
					sim::SimulationContext::current()->onDebugMessage(nullptr, 
						std::to_string(dbv.size()) + 
						" >> " + 
						std::to_string(std::get<size_t>(testCase)) + 
						": " + 
						std::to_string((streamW.bits() - (packetSize - shiftAmt) % streamW.bits())%streamW.bits()) +
						" emptyBits"
					);
					co_await scl::strm::sendPacket(in, scl::strm::SimPacket(dbv).invalidBeats(getInvalidMask()), clk);
					for (size_t i = 0; i < waitBetweenPackets; i++)
						co_await OnClk(clk);
				}
			}
		);

		//send shift on sop of data, then invalidate
		addSimulationProcess(
			[&, this]()->SimProcess {
				for (const auto& testCase: testCases){
					simu(shiftAmt) = std::get<size_t>(testCase);
					do{
						co_await performTransferWait(in, clk);
						simu(shiftAmt).invalidate();
					} while (!(simuValid(in) == '1' && simuReady(in) == '1' && simuEop(in) == '1'));
				}
			}
		);

		//receive packets and check
		addSimulationProcess(
			[&, this]()->SimProcess {
				for (const auto& testCase: testCases)
				{
					scl::strm::SimPacket outputPacket = co_await scl::strm::receivePacket(out, clk);
					auto receivedDbv = outputPacket.payload;
					const auto& sentDbv = std::get<sim::DefaultBitVectorState>(testCase);
					const auto& shift = std::get<size_t>(testCase);
					auto expectedDbv = sentDbv.extract(shift, sentDbv.size() - shift);
					BOOST_TEST(receivedDbv == expectedDbv);
					if (receivedDbv != expectedDbv) {
						sim::SimulationContext::current()->onAssert(nullptr, "The packet finishing at this beat is wrong");
					}
				}
				co_await OnClk(clk);
				co_await OnClk(clk);
				co_await OnClk(clk);
				stopTest();
			}
		);

		design.postprocess();
		BOOST_TEST(!runHitsTimeout(simuTime));
	}
};

BOOST_FIXTURE_TEST_CASE(stream_shift_right_better_noargs, stream_shiftRight_better_fixture) {
	//streamW = 16_b;
	//numPackets = 10;
	//waitBetweenPackets = 5;
	//getPacketSize = [](){ return 64; };
	//getShiftAmt = [](){ return 4; };
	//backpressureReadyPercentage;
	execute();
}

BOOST_FIXTURE_TEST_CASE(stream_shift_right_better_increasing, stream_shiftRight_better_fixture) {
	//streamW = 16_b;
	//numPackets = 10;
	waitBetweenPackets = 0;
	//getPacketSize = [](){ return 64; };
	size_t currentShift = 0;
	getShiftAmt = [&](){ return currentShift++; };
	backpressureReadyPercentage = 50;
	getInvalidMask = [&]() { return rng(); }; //0 to 15
	execute();
}

BOOST_FIXTURE_TEST_CASE(stream_shift_right_better_decreasing, stream_shiftRight_better_fixture) {
	//streamW = 16_b;
	//numPackets = 10;
	waitBetweenPackets = 0;
	//getPacketSize = [](){ return 64; };
	size_t currentShift = 10;
	getShiftAmt = [&](){ return currentShift--; };
	backpressureReadyPercentage = 50;
	getInvalidMask = [&]() { return rng(); }; //0 to 15
	execute();
}

BOOST_FIXTURE_TEST_CASE(stream_shift_right_better_random, stream_shiftRight_better_fixture) {
	//streamW = 16_b;
	numPackets = 1000;
	waitBetweenPackets = 0;
	getPacketSize = [&](){ return (rng() & 0x1F) + 1; };//1 to 64
	getShiftAmt = [&]() { return rng() & 0xF; }; //0 to 15
	getInvalidMask = [&]() { return rng(); };
	backpressureReadyPercentage = 50;
	simuTime = { 1000, 1'000'000 };
	execute();
}

BOOST_FIXTURE_TEST_CASE(stream_shift_right_better_rework_playground, stream_shiftRight_better_fixture) {
	//streamW = 16_b;
	numPackets = 10;
	waitBetweenPackets = 0;
	size_t offset = 0;
	//size_t packetSize = 12;
	getPacketSize = [&]() { return 20; };//return (rng() & 0x3F) + 1; };//1 to 64
	getShiftAmt = [&]() { return offset++; };// rng() & 0xF;}; //0 to 15

	backpressureReadyPercentage = 50;
	simuTime = { 1, 1'000'000 };
	execute();
}

BOOST_FIXTURE_TEST_CASE(stream_shift_right_better_one_beat, stream_shiftRight_better_fixture) {
	//streamW = 16_b;
	//numPackets = 10;
	waitBetweenPackets = 0;
	getPacketSize = [](){ return 12; };
	getShiftAmt = [](){ return 4; };
	backpressureReadyPercentage = 50;
	getInvalidMask = [&]() { return rng(); }; //0 to 15
	execute();
}

BOOST_FIXTURE_TEST_CASE(stream_shift_right_better_shift_0, stream_shiftRight_better_fixture) {
	//streamW = 16_b;
	numPackets = 10;
	waitBetweenPackets = 0;
	size_t packetSize = 20;
	getPacketSize = [&](){ return packetSize++; };
	getShiftAmt = [](){ return 0; };
	backpressureReadyPercentage = 50;
	getInvalidMask = [&]() { return rng(); }; //0 to 15
	execute();
}

BOOST_FIXTURE_TEST_CASE(stream_shift_right_better_shift_0_and_one_beat, stream_shiftRight_better_fixture) {
	//streamW = 16_b;
	numPackets = 20;
	waitBetweenPackets = 0;
	size_t packetSize = 0;
	getPacketSize = [&](){ return (packetSize++ % streamW.bits()) + 1; };
	getShiftAmt = [](){ return 0; };
	backpressureReadyPercentage = 50;
	getInvalidMask = [&]() { return rng(); }; //0 to 15
	execute();
}

BOOST_FIXTURE_TEST_CASE(streamInsert_test, BoostUnitTestSimulationFixture)
{
	Clock clk = Clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScope(clk);

	std::mt19937 rng{ 12524 };

	scl::RvStream<UInt> inOffset{ 8_b };
	scl::RvPacketStream<BVec> inBase{ 16_b };
	scl::RvPacketStream<BVec> inInsert{ 16_b };
	auto out = scl::strm::insert(move(inBase), std::move(inInsert), std::move(inOffset));

	pinIn(inOffset, "inOffset");
	pinIn(inBase, "inBase");
	pinIn(inInsert, "inInsert");
	pinOut(out, "out");

	// insert packets
	addSimulationProcess([&, this]()->SimProcess {
		simu(ready(out)) = '1';

		for (size_t i = 0; i < 32 / 4 + 1; ++i)
		{
			fork(sendPacket(inBase, scl::strm::SimPacket{ 0x76543210ull, 32_b }, clk));
			fork(sendPacket(inInsert, scl::strm::SimPacket{ 0xfedcba98, 32_b }, clk));
			fork(sendPacket(inOffset, scl::strm::SimPacket{ i * 4, 8_b }, clk));
		
			scl::strm::SimPacket packet = co_await receivePacket(out, clk);
			co_await OnClk(clk);
		}

		co_await OnClk(clk);
		stopTest();
	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 1, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(streamInsert_fuzz_test, BoostUnitTestSimulationFixture)
{
	Clock clk = Clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScope(clk);

	std::mt19937_64 rng{ 12524 };

	scl::RvStream<UInt> inOffset{ 8_b };
	scl::RvPacketStream<BVec, scl::EmptyBits> inBase{ 8_b };
	scl::RvPacketStream<BVec, scl::EmptyBits> inInsert{ inBase->width() };

	for (scl::RvPacketStream<BVec, scl::EmptyBits>& in : { std::ref(inBase), std::ref(inInsert) })
		in.template get<scl::EmptyBits>().emptyBits = BitWidth::count(inBase->width().bits());

	auto out = scl::strm::insert(move(inBase), std::move(inInsert), std::move(inOffset));

	pinIn(inOffset, "inOffset");
	pinIn(inBase, "inBase");
	pinIn(inInsert, "inInsert");
	pinOut(out, "out");

	// insert packets
	addSimulationProcess([&, this]()->SimProcess {
		fork(readyDriverRNG(out, clk, 90));

		for (size_t i = 0; i < 128; ++i)
		{
			uint64_t baseW = rng() % 64 + 1;
			uint64_t baseData = rng() & gtry::utils::bitMaskRange(0, baseW);
			uint64_t insertW = baseW != 64 ? rng() % (64 - baseW) : 0;
			uint64_t insertData = rng() & gtry::utils::bitMaskRange(0, insertW);
			uint64_t insertOffset = rng() % (baseW + 1);

			fork(sendPacket(inBase, scl::strm::SimPacket{ baseData, BitWidth(baseW) }, clk));
			if (insertW)
			{
				fork(sendPacket(inInsert, scl::strm::SimPacket{ insertData, BitWidth{insertW} }, clk));
				fork(sendPacket(inOffset, scl::strm::SimPacket{ insertOffset, 8_b }, clk));
			}

			scl::strm::SimPacket packet = co_await receivePacket(out, clk);

			uint64_t expected = 
				(baseData & gtry::utils::bitMaskRange(0, insertOffset)) |
				(insertData << insertOffset) |
				((baseData & ~gtry::utils::bitMaskRange(0, insertOffset)) << insertW);

			uint64_t received = packet.asUint64(BitWidth(baseW + insertW));

			BOOST_TEST(packet.payload.size() == baseW + insertW);
			BOOST_TEST(received == expected);

			if (packet.payload.size() != baseW + insertW)
				break;
			if (received != expected)
				break;

			if(rng() % 16 == 0)
				co_await OnClk(clk);
		}

		co_await OnClk(clk);
		stopTest();
	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 10, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(streamDropPacket_test, BoostUnitTestSimulationFixture)
{
	Clock clk = Clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScope(clk);

	std::mt19937_64 rng{ 12524 };

	Bit drop = pinIn().setName("drop");
	scl::RvPacketStream<BVec> in{ 8_b };
	pinIn(in, "in");

	auto out = scl::strm::dropPacket(move(in), drop);
	pinOut(out, "out");

	std::queue<scl::strm::SimPacket> expected;
	bool expectedComplete = false;
	
	addSimulationProcess([&, this]()->SimProcess {

		for (size_t i = 0; i < 32; ++i)
		{
			BitWidth packetLen = (rng() % 4 + 1) * in->width();
			scl::strm::SimPacket packet{
				gtry::utils::bitfieldExtract(rng(), 0, packetLen.bits()),
				packetLen
			};

			if (rng() % 2)
			{
				expected.push(packet);
				simu(drop) = '0';
			}
			else
			{
				simu(drop) = '1';
			}

			co_await sendPacket(in, packet, clk);
		}
		expectedComplete = true;
	});
	
	addSimulationProcess([&, this]()->SimProcess {
		fork(readyDriverRNG(out, clk, 70));

		while (!expected.empty() || !expectedComplete)
		{
			scl::strm::SimPacket packet = co_await receivePacket(out, clk);
			BOOST_TEST(!expected.empty());
			if(!expected.empty())
			{
				BOOST_TEST(packet.payload == expected.front().payload);
				expected.pop();
			}
		}

		co_await OnClk(clk);
		stopTest();
	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 4, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(fifoPopCompile_test, BoostUnitTestSimulationFixture)
{
	Clock clk = Clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScope(clk);

	{
		scl::Fifo<BVec> fifo{ 4, 8_b };
		scl::RvStream<BVec> fifoPopPort = scl::strm::pop(fifo);
		fifo.generate();
	}

	{
		scl::Stream<BVec, scl::Empty> templateStream{ 8_b };
		empty(templateStream) = 1_b;

		scl::Fifo<scl::Stream<BVec, scl::Empty>> fifo{ 4, templateStream };
		scl::RvStream<BVec, scl::Empty> fifoPopPort = pop(fifo);
		fifo.generate();
	}
}

BOOST_FIXTURE_TEST_CASE(serialPushParallelPop_fuzzTest, BoostUnitTestSimulationFixture)
{
	Clock clk = Clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScope(clk);

	scl::RvStream<UInt> in(16_b);
	pinIn(in, "in");

	Vector out = scl::strm::serialPushParallelPopBuffer(move(in), 3);
	pinOut(out, "out");

	std::set<size_t> inputs;
	size_t numToSend = 512;
	

	/*input valid driver*/
	addSimulationProcess([&, this]() -> SimProcess {
		std::mt19937 gen(10317);
		while (true) {
			simu(valid(in)) = (gen() & 0x1) == 0 ? '0' : '1';
			co_await OnClk(clk);
		}
	});

	/*input data driver*/
	addSimulationProcess([&, this]() -> SimProcess {
		scl::SimulationSequencer seq;
		for (size_t i = 0; i < numToSend; i++)
		{
			simu(*in) = i;
			co_await performTransferWait(in, clk);
			BOOST_TEST((inputs.find(i & 0xFFFF) == inputs.end()));
			inputs.insert(i & 0xFFFF);
		}
	});


	size_t total = 0;
	unsigned int seed = 12;
	std::mt19937 gen(seed);
	/*output transfers driver*/
	for (auto& outStrm : out) {
		gen.seed(seed++);
		addSimulationProcess([&, this]() -> SimProcess {
			while (true) {
				simu(ready(outStrm)) = (gen() & 0b11) == 0 ? '1' : '0';
				co_await OnClk(clk);
				if (simu(ready(outStrm)) == '1' && simu(valid(outStrm)) == '1') {
					total++;
					BOOST_TEST((inputs.find(simu(*outStrm)) != inputs.end()));
					inputs.erase(simu(*outStrm));
				}

				if (total == numToSend) {
					BOOST_TEST((inputs.size() == 0));
					stopTest();
				}
			}
			});
	}

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 100, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(serialPushParallelPop_popLast, BoostUnitTestSimulationFixture)
{
	Clock clk = Clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScope(clk);

	scl::RvStream<UInt> in(16_b);
	pinIn(in, "in");

	auto out = scl::strm::serialPushParallelPopBuffer(move(in), 3);
	pinOut(out, "out");

	std::set<size_t> inputs;
	size_t numToSend = 512;


	/*input valid driver*/
	addSimulationProcess([&, this]() -> SimProcess {
		std::mt19937 gen(10317);
		while (true) {
			simu(valid(in)) = '1';//(gen() & 0x1) == 0 ? '0' : '1';
			co_await OnClk(clk);
		}
		});

	/*input data driver*/
	addSimulationProcess([&, this]() -> SimProcess {
		scl::SimulationSequencer seq;
		for (size_t i = 0; i < numToSend; i++)
		{
			simu(*in) = i;
			co_await performTransferWait(in, clk);
			BOOST_TEST((inputs.find(i & 0xFFFF) == inputs.end()));
			inputs.insert(i & 0xFFFF);
		}
		});


	size_t total = 0;
	unsigned int seed = 12;
	std::mt19937 gen(seed);
	/*output transfers driver*/
	for (auto& outStrm : out) {
		gen.seed(seed++);
		addSimulationProcess([&, this]() -> SimProcess {
			simu(ready(outStrm)) = '0'; //(gen() & 0b11) == 0 ? '1' : '0';
			simu(ready(out.back())) = '1'; //(gen() & 0b11) == 0 ? '1' : '0';
			while (true) {

				co_await OnClk(clk);
				if (simu(ready(outStrm)) == '1' && simu(valid(outStrm)) == '1') {
					total++;
					BOOST_TEST((inputs.find(simu(*outStrm)) != inputs.end()));
					inputs.erase(simu(*outStrm));
				}

				if (total == numToSend) {
					BOOST_TEST((inputs.size() == 0));
					stopTest();
				}
			}
			});
	}

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 100, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(delay_test_0, BoostUnitTestSimulationFixture)
{

	size_t delay = 0;
	Clock clk = Clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScope(clk);


	scl::RvStream<UInt> in(16_b);
	pinIn(in, "in");

	scl::StreamBroadcaster InHandle(move(in));

	scl::RvStream<UInt> out = InHandle.bcastTo() | scl::strm::delay(delay);
	pinOut(out, "out");

	size_t numToSend = 16;
	std::map<size_t, gtry::Seconds> inputs;

	/*input valid driver*/
	addSimulationProcess([&, this]() -> SimProcess {
		std::mt19937 gen(10317);
		while (true) {
			simu(valid(in)) = (gen() & 0x1) == 0 ? '0' : '1';
			co_await OnClk(clk);
		}
	});

	/*input data driver*/
	addSimulationProcess([&, this]() -> SimProcess {
		scl::SimulationSequencer seq;
		for (size_t i = 0; i < numToSend; i++)
		{
			simu(*in) = i;
			co_await performTransferWait(in, clk);
			BOOST_TEST((inputs.find(i & 0xFFFF) == inputs.end()));
			inputs[i & 0xFFFF] = getCurrentSimulationTime();
		}
	});


	size_t total = 0;
	std::mt19937 gen(50);
	/*output transfers driver*/
	addSimulationProcess([&, this]() -> SimProcess {
		simu(ready(out)) = (gen() & 0b11) == 0 ? '1' : '0';
		while (true) {
			co_await OnClk(clk);
			if (simu(ready(out)) == '1' && simu(valid(out)) == '1') {
				total++;
				BOOST_TEST((inputs.find(simu(*out)) != inputs.end()));
				BOOST_TEST((toNanoseconds(getCurrentSimulationTime()) - toNanoseconds(inputs[simu(*out)])) >= toNanoseconds(gtry::Seconds{ delay, 100'000'000 })); // make this work
				inputs.erase(simu(*out));
			}

			if (total == numToSend) {
				BOOST_TEST((inputs.size() == 0));
				stopTest();
			}
		}
		});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 2, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(delay_test_1, BoostUnitTestSimulationFixture)
{

	size_t delay = 1;
	Clock clk = Clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScope(clk);


	scl::RvStream<UInt> in(16_b);
	pinIn(in, "in");

	scl::StreamBroadcaster InHandle(move(in));

	scl::RvStream<UInt> out = InHandle.bcastTo() | scl::strm::delay(delay);
	pinOut(out, "out");

	size_t numToSend = 16;
	std::map<size_t, gtry::Seconds> inputs;

	/*input valid driver*/
	addSimulationProcess([&, this]() -> SimProcess {
		std::mt19937 gen(10317);
		while (true) {
			simu(valid(in)) = (gen() & 0x1) == 0 ? '0' : '1';
			co_await OnClk(clk);
		}
		});

	/*input data driver*/
	addSimulationProcess([&, this]() -> SimProcess {
		scl::SimulationSequencer seq;
		for (size_t i = 0; i < numToSend; i++)
		{
			simu(*in) = i;
			co_await performTransferWait(in, clk);
			BOOST_TEST((inputs.find(i & 0xFFFF) == inputs.end()));
			inputs[i & 0xFFFF] = getCurrentSimulationTime();
		}
		});


	size_t total = 0;
	std::mt19937 gen(50);
	/*output transfers driver*/
	addSimulationProcess([&, this]() -> SimProcess {
		simu(ready(out)) = (gen() & 0b11) == 0 ? '1' : '0';
		while (true) {
			co_await OnClk(clk);
			if (simu(ready(out)) == '1' && simu(valid(out)) == '1') {
				total++;
				BOOST_TEST((inputs.find(simu(*out)) != inputs.end()));
				BOOST_TEST((toNanoseconds(getCurrentSimulationTime()) - toNanoseconds(inputs[simu(*out)])) >= toNanoseconds(gtry::Seconds{ delay, 100'000'000 })); // make this work
				inputs.erase(simu(*out));
			}

			if (total == numToSend) {
				BOOST_TEST((inputs.size() == 0));
				stopTest();
			}
		}
		});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 2, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(delay_test_2, BoostUnitTestSimulationFixture)
{

	size_t delay = 2;
	Clock clk = Clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScope(clk);


	scl::RvStream<UInt> in(16_b);
	pinIn(in, "in");

	scl::StreamBroadcaster InHandle(move(in));

	scl::RvStream<UInt> out = InHandle.bcastTo() | scl::strm::delay(delay);
	pinOut(out, "out");

	size_t numToSend = 16;
	std::map<size_t, gtry::Seconds> inputs;

	/*input valid driver*/
	addSimulationProcess([&, this]() -> SimProcess {
		std::mt19937 gen(10317);
		while (true) {
			simu(valid(in)) = (gen() & 0x1) == 0 ? '0' : '1';
			co_await OnClk(clk);
		}
		});

	/*input data driver*/
	addSimulationProcess([&, this]() -> SimProcess {
		scl::SimulationSequencer seq;
		for (size_t i = 0; i < numToSend; i++)
		{
			simu(*in) = i;
			co_await performTransferWait(in, clk);
			BOOST_TEST((inputs.find(i & 0xFFFF) == inputs.end()));
			inputs[i & 0xFFFF] = getCurrentSimulationTime();
		}
		});


	size_t total = 0;
	std::mt19937 gen(50);
	/*output transfers driver*/
	addSimulationProcess([&, this]() -> SimProcess {
		simu(ready(out)) = (gen() & 0b11) == 0 ? '1' : '0';
		while (true) {
			co_await OnClk(clk);
			if (simu(ready(out)) == '1' && simu(valid(out)) == '1') {
				total++;
				BOOST_TEST((inputs.find(simu(*out)) != inputs.end()));
				BOOST_TEST((toNanoseconds(getCurrentSimulationTime()) - toNanoseconds(inputs[simu(*out)])) >= toNanoseconds(gtry::Seconds{ delay, 100'000'000 })); // make this work
				inputs.erase(simu(*out));
			}

			if (total == numToSend) {
				BOOST_TEST((inputs.size() == 0));
				stopTest();
			}
		}
		});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 2, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(delay_test_10, BoostUnitTestSimulationFixture)
{

	size_t delay = 10;
	Clock clk = Clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScope(clk);


	scl::RvStream<UInt> in(16_b);
	pinIn(in, "in");

	scl::StreamBroadcaster InHandle(move(in));

	scl::RvStream<UInt> out = InHandle.bcastTo() | scl::strm::delay(delay);
	pinOut(out, "out");

	size_t numToSend = 16;
	std::map<size_t, gtry::Seconds> inputs;

	/*input valid driver*/
	addSimulationProcess([&, this]() -> SimProcess {
		std::mt19937 gen(10317);
		while (true) {
			simu(valid(in)) = (gen() & 0x1) == 0 ? '0' : '1';
			co_await OnClk(clk);
		}
		});

	/*input data driver*/
	addSimulationProcess([&, this]() -> SimProcess {
		scl::SimulationSequencer seq;
		for (size_t i = 0; i < numToSend; i++)
		{
			simu(*in) = i;
			co_await performTransferWait(in, clk);
			BOOST_TEST((inputs.find(i & 0xFFFF) == inputs.end()));
			inputs[i & 0xFFFF] = getCurrentSimulationTime();
		}
		});


	size_t total = 0;
	std::mt19937 gen(50);
	/*output transfers driver*/
	addSimulationProcess([&, this]() -> SimProcess {
		simu(ready(out)) = (gen() & 0b11) == 0 ? '1' : '0';
		while (true) {
			co_await OnClk(clk);
			if (simu(ready(out)) == '1' && simu(valid(out)) == '1') {
				total++;
				BOOST_TEST((inputs.find(simu(*out)) != inputs.end()));
				BOOST_TEST((toNanoseconds(getCurrentSimulationTime()) - toNanoseconds(inputs[simu(*out)])) >= toNanoseconds(gtry::Seconds{ delay, 100'000'000 })); // make this work
				inputs.erase(simu(*out));
			}

			if (total == numToSend) {
				BOOST_TEST((inputs.size() == 0));
				stopTest();
			}
		}
		});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 2, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(stream_credit_fifo, BoostUnitTestSimulationFixture)
{
	Clock clk = Clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScope(clk);

	scl::RvStream<UInt> in{ .data = 10_b };
	pinIn(in, "in");

	addSimulationProcess([&, this]() -> SimProcess {
		for (size_t i = 0;; i++)
			co_await scl::strm::sendBeat(in, i, clk);
	});

	auto out = delayAutoPipeline(move(in), 8);
	pinOut(out, "out");

	addSimulationProcess([&, this]() -> SimProcess {
		fork(scl::strm::readyDriverRNG(out, clk, 80));

		for (size_t i = 0; i < 24; ++i)
		{
			co_await scl::strm::performTransferWait(out, clk);
			BOOST_TEST(simu(*out) == i);
		}
		stopTest();
	});

	design.postprocess();
	if (true) {
		m_vhdlExport.emplace("export/stream_credit_fifo.vhd");
		m_vhdlExport->targetSynthesisTool(new gtry::XilinxVivado());
		(*m_vhdlExport)(design.getCircuit());
	}

	BOOST_TEST(!runHitsTimeout({ 1, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(stream_demux_test, BoostUnitTestSimulationFixture)
{
	Clock clk = Clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScope(clk);

	scl::RvStream<UInt> in{ .data = 10_b };
	pinIn(in, "in");
	UInt select = pinIn(1_b).setName("select");

	addSimulationProcess([&, this]() -> SimProcess {
		for (size_t s = 0; s < select.width().count(); ++s)
		{
			simu(select) = s;

			for (size_t i = 0; i < 4; i++)
				co_await scl::strm::sendBeat(in, i, clk);
		}
	});

	scl::StreamDemux<scl::RvStream<UInt>> demux{ move(in), select };
	Vector out = demux.out();
	pinOut(out, "out");

	addSimulationProcess([&, this]() -> SimProcess {
		uint32_t seed = 0;
		for (scl::RvStream<UInt>& o : out)
			fork(scl::strm::readyDriverRNG(o, clk, 80, seed++));

		for (scl::RvStream<UInt>& o : out)
		{
			for (size_t i = 0; i < 4; ++i)
			{
				co_await scl::strm::performTransferWait(o, clk);
				BOOST_TEST(simu(*o) == i);
			}
		}
		stopTest();
	});

	BOOST_TEST(!runHitsTimeout({ 1, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(credit_broadcaster_test, BoostUnitTestSimulationFixture)
{
	Clock clk = Clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScope(clk);

	scl::RvStream<UInt> in0{ .data = 4_b };
	auto in0WithCredits = scl::strm::creditStream(move(in0)); HCL_NAMED(in0WithCredits);
	pinIn(in0WithCredits, "in0WithCredits");
	Bit& in0_credits = *in0WithCredits.template get<scl::strm::Credit>().increment;

	scl::StreamBroadcaster caster(move(in0WithCredits));

	auto out0WithCredits = caster.bcastTo();
	pinOut(out0WithCredits, "out0WithCredits");
	auto out1WithCredits = caster.bcastTo();
	pinOut(out1WithCredits, "out1WithCredits");

	Bit& out0_credits = *out0WithCredits.template get<scl::strm::Credit>().increment;
	Bit& out1_credits = *out1WithCredits.template get<scl::strm::Credit>().increment;

	std::mt19937 mt(9384);
	//check that the downstream signals are being properly forward broadcasted to all sinks
	addSimulationProcess([&, this]() -> SimProcess {	
		while (true) {
			bool sentSomething = mt() & 1;
			size_t randomData = mt();
			simu(valid(in0WithCredits)) = sentSomething? '1':'0';
			if (sentSomething)
				simu(*in0WithCredits) = randomData;
			co_await WaitFor(Seconds{ 0,1 });
			BOOST_TEST(simu(valid(out0WithCredits)) ==  (sentSomething? '1':'0'));
			BOOST_TEST(simu(valid(out1WithCredits)) ==  (sentSomething? '1':'0'));
			if(sentSomething)
				BOOST_TEST(simu(*out1WithCredits) == (randomData & 0xF));
			co_await AfterClk(clk);
		}
	});

	//out0 will be granted 32 of credits immediately, then no more credits anymore
	const size_t testCredits = 32;
	addSimulationProcess([&, this]() -> SimProcess {
		simu(out0_credits) = '1';
		for (size_t i = 0; i < testCredits; i++)
			co_await OnClk(clk);
		simu(out0_credits) = '0';
	});

	//out1 will grant 64 credits with randomly sized gaps
	addSimulationProcess([&, this]() -> SimProcess {
		simu(out1_credits) = '0';
		for (size_t i = 0; i < 64; i++) {
			for (size_t j = 0; j < (mt() & 7); j++)
				co_await OnClk(clk);
			simu(out1_credits) = '1';
			co_await OnClk(clk);
			simu(out1_credits) = '0';
		}
	});
	
	//check that we got exactly 32 credits
	addSimulationProcess([&, this]() -> SimProcess {
		size_t receivedCredits = 0;
		while (receivedCredits < testCredits) {
			if (simu(in0_credits) == '1') {
				receivedCredits++;
			}
			co_await OnClk(clk);
		}
		for (size_t i = 0; i < 64; i++)
		{
			BOOST_TEST(simu(in0_credits) == '0');
			co_await OnClk(clk);
		}
		stopTest();
		});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 10, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(stream_allowance_stall_test, BoostUnitTestSimulationFixture)
{
	Clock clk({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clk);

	const size_t allowance = 15;

	BitWidth allowanceW = 8_b;

	Bit allow;
	pinIn(allow, "allow");

	scl::RvStream<Bit> in('0');
	pinIn(in, "in");

	auto out = move(in) | scl::strm::allowanceStall(allow, allowanceW);

	pinOut(out, "out");

	addSimulationProcess([&, this]()->SimProcess { return scl::strm::readyDriverRNG(out, clk, 50); });

	std::mt19937 rng(647489);
	addSimulationProcess([&, this]()->SimProcess {
		while (true) {
			co_await scl::strm::sendBeat(in, '1', clk);
			size_t wait = rng() & 0xF;
			for (size_t i = 0; i < wait; i++)
				co_await OnClk(clk);
		}
	}); 

	addSimulationProcess([&, this]()->SimProcess {
		simu(allow) = '0';
		for (size_t i = 0; i < allowance; i++)
		{
			simu(allow) = '1';
			co_await OnClk(clk);
			simu(allow) = '0';
			size_t wait = rng() & 0x3;
			for (size_t i = 0; i < wait; i++)
				co_await OnClk(clk);
		}
	});

	size_t received = 0;
	addSimulationProcess([&, this]()->SimProcess {
		while (received < allowance) {
			co_await scl::strm::performTransferWait(out, clk);
			received++;
		}
	}); 

	addSimulationProcess([&, this]()->SimProcess {
		for (size_t i = 0; i < 200; i++){
			co_await OnClk(clk);
		}
		BOOST_TEST(received == allowance);
		stopTest();
	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 100, 1'000'000 }));
}



BOOST_FIXTURE_TEST_CASE(stream_allowance_initial_allowance, BoostUnitTestSimulationFixture)
{
	Clock clk({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clk);

	const size_t initialAllowance = 15;

	BitWidth allowanceW = 8_b;

	Bit allow;
	pinIn(allow, "allow");

	scl::RvStream<Bit> in('0');
	pinIn(in, "in");

	auto out = move(in) | scl::strm::allowanceStall(allow, allowanceW, initialAllowance);

	pinOut(out, "out");

	addSimulationProcess([&, this]()->SimProcess { return scl::strm::readyDriver(out, clk); });

	std::mt19937 rng(647489);
	addSimulationProcess([&, this]()->SimProcess {
		while (true) {
			co_await scl::strm::sendBeat(in, '1', clk);
		}
		});

	size_t received = 0;
	addSimulationProcess([&, this]()->SimProcess {
		while (received < initialAllowance) {
			co_await scl::strm::performTransferWait(out, clk);
			received++;
		}
		}); 

	addSimulationProcess([&, this]()->SimProcess {
		for (size_t i = 0; i < 200; i++){
			co_await OnClk(clk);
		}
		BOOST_TEST(received == initialAllowance);
		stopTest();
		});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 100, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(sequencer_fuzz_test, BoostUnitTestSimulationFixture)
{
	Clock clk({ .absoluteFrequency = 100'000'000, .memoryResetType = ClockConfig::ResetType::NONE });
	ClockScope clkScp(clk);

	BitWidth txidW = 6_b;

	scl::RvStream<UInt, scl::TxId> in{ txidW };
	txid(in) = txidW;
	pinIn(in, "in");

	scl::RvStream<UInt, scl::TxId> out = move(in) | scl::strm::sequencer();
	pinOut(out, "out");

	std::vector<size_t> unusedTxId(txidW.count());
	std::iota(unusedTxId.begin(), unusedTxId.end(), 0);

	addSimulationProcess([&, this]()->SimProcess {
		std::mt19937 rng{ 5434 };
		while (true)
		{
			while (unusedTxId.size() <= rng() % (txidW.count() / 2))
				co_await OnClk(clk);

			std::vector<size_t>::iterator it = unusedTxId.begin() + (rng() % unusedTxId.size());
			simu(*in) = *it;
			simu(txid(in)) = *it;
			unusedTxId.erase(it);
			co_await scl::strm::performTransfer(in, clk);
		}
	});

	addSimulationProcess([&, this]()->SimProcess {
		fork(scl::strm::readyDriverRNG(out, clk, 50));

		for (size_t i = 0;; i++)
		{
			co_await scl::strm::performTransferWait(out, clk);
			size_t id = simu(txid(out));
			BOOST_TEST(id == i % txidW.count());
			BOOST_TEST(id == simu(*out));
			BOOST_TEST((std::find(unusedTxId.begin(), unusedTxId.end(), id) == unusedTxId.end()));
			unusedTxId.push_back(id);
		}
	});

	design.postprocess();
	runHitsTimeout({ 800, 1'000'000 });
}

BOOST_FIXTURE_TEST_CASE(sequencer_fill_test, BoostUnitTestSimulationFixture)
{
	Clock clk({ .absoluteFrequency = 100'000'000, .memoryResetType = ClockConfig::ResetType::NONE });
	ClockScope clkScp(clk);

	BitWidth txidW = 4_b;

	scl::RvStream<UInt, scl::TxId> in{ txidW };
	txid(in) = txidW;
	pinIn(in, "in");

	scl::RvStream<UInt, scl::TxId> out = move(in) | scl::strm::sequencer();
	pinOut(out, "out");

	addSimulationProcess([&, this]()->SimProcess {
		for (size_t t = 0; t < 2; ++t)
		{
			// send every txid except for the first one
			for (size_t i = 1; i < txidW.count(); i++)
			{
				size_t prime = t ? 6089 : 2539;
				size_t id = i * prime;
				simu(*in) = id;
				simu(txid(in)) = id;
				co_await scl::strm::performTransfer(in, clk);
			}

			// send missing txid
			simu(*in) = 0;
			simu(txid(in)) = 0;
			co_await scl::strm::performTransfer(in, clk);

			// wait for flush
			co_await scl::strm::performTransferWait(out, clk);
			while (simu(valid(out)) == '1')
				co_await OnClk(clk);
		}
	});

	addSimulationProcess([&, this]()->SimProcess {
		simu(ready(out)) = '1';
		for (size_t i = 0; i < txidW.count(); i++)
		{
			co_await scl::strm::performTransferWait(out, clk);
			size_t id = simu(txid(out));
			BOOST_TEST(id == i);
			BOOST_TEST(id == simu(*out));
		}

		simu(ready(out)) = '0';

		// wait until all txids are sent again
		for (size_t i = 0; i < txidW.count(); i++)
			co_await performTransferWait(in, clk);
		while (simu(valid(out)) == '0')
			co_await OnClk(clk);
		co_await OnClk(clk);

		simu(ready(out)) = '1';
		for (size_t i = 0; i < txidW.count(); i++)
		{
			co_await scl::strm::performTransferWait(out, clk);
			size_t id = simu(txid(out));
			BOOST_TEST(id == i);
			BOOST_TEST(id == simu(*out));
		}
		stopTest();
	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 1, 1'000'000 }));
}
