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


using namespace boost::unit_test;
using namespace gtry;

BOOST_FIXTURE_TEST_CASE(arbitrateInOrder_basic, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	scl::Stream<UInt> in0;
	scl::Stream<UInt> in1;

	in0.data = pinIn(8_b).setName("in0_data");
	in0.valid = pinIn().setName("in0_valid");
	pinOut(*in0.ready).setName("in0_ready");

	in1.data = pinIn(8_b).setName("in1_data");
	in1.valid = pinIn().setName("in1_valid");
	pinOut(*in1.ready).setName("in1_ready");

	scl::arbitrateInOrder uut{ in0, in1 };
	pinOut(uut.data).setName("out_data");
	pinOut(uut.valid).setName("out_valid");
	*uut.ready = pinIn().setName("out_ready");

	addSimulationProcess([&]()->SimProcess {
		simu(*uut.ready) = 1;
		simu(in0.valid) = 0;
		simu(in1.valid) = 0;
		simu(in0.data) = 0;
		simu(in1.data) = 0;
		co_await WaitClk(clock);

		simu(in0.valid) = 0;
		simu(in1.valid) = 1;
		simu(in1.data) = 1;
		co_await WaitClk(clock);

		simu(in1.valid) = 0;
		simu(in0.valid) = 1;
		simu(in0.data) = 2;
		co_await WaitClk(clock);

		simu(in1.valid) = 1;
		simu(in0.valid) = 1;
		simu(in0.data) = 3;
		simu(in1.data) = 4;
		co_await WaitClk(clock);
		co_await WaitClk(clock);

		simu(in1.valid) = 1;
		simu(in0.valid) = 1;
		simu(in0.data) = 5;
		simu(in1.data) = 6;
		co_await WaitClk(clock);
		co_await WaitClk(clock);

		simu(in0.valid) = 0;
		simu(in1.valid) = 1;
		simu(in1.data) = 7;
		co_await WaitClk(clock);

		simu(in1.valid) = 0;
		simu(in0.valid) = 0;
		simu(*uut.ready) = 0;
		co_await WaitClk(clock);

		simu(in1.valid) = 0;
		simu(in0.valid) = 1;
		simu(in0.data) = 8;
		simu(*uut.ready) = 1;
		co_await WaitClk(clock);

		simu(in1.valid) = 0;
		simu(in0.valid) = 0;
		co_await WaitClk(clock);
	});

	addSimulationProcess([&]()->SimProcess {

		size_t counter = 1;
		while (true)
		{
			if (simu(*uut.ready) && simu(uut.valid))
			{
				BOOST_TEST(counter == simu(uut.data));
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

	scl::Stream<UInt> in0;
	scl::Stream<UInt> in1;

	in0.data = pinIn(8_b).setName("in0_data");
	in0.valid = pinIn().setName("in0_valid");
	pinOut(*in0.ready).setName("in0_ready");

	in1.data = pinIn(8_b).setName("in1_data");
	in1.valid = pinIn().setName("in1_valid");
	pinOut(*in1.ready).setName("in1_ready");

	scl::arbitrateInOrder uut{ in0, in1 };
	pinOut(uut.data).setName("out_data");
	pinOut(uut.valid).setName("out_valid");
	*uut.ready = pinIn().setName("out_ready");

	addSimulationProcess([&]()->SimProcess {
		simu(*uut.ready) = 1;
		simu(in0.valid) = 0;
		simu(in1.valid) = 0;

		std::mt19937 rng{ 10179 };
		size_t counter = 1;
		bool wasReady = false;
		while(true)
		{
			if(wasReady)
			{
				if(rng() % 2 == 0)
				{
					simu(in0.valid) = 1;
					simu(in0.data) = counter++;
				}
				else
				{
					simu(in0.valid) = 0;
				}

				if(rng() % 2 == 0)
				{
					simu(in1.valid) = 1;
					simu(in1.data) = counter++;
				}
				else
				{
					simu(in1.valid) = 0;
				}
			}

			// chaos monkey
			simu(*uut.ready) = rng() % 8 != 0 ? 1 : 0;

			wasReady = simu(*in0.ready) != 0;

			co_await WaitClk(clock);
		}
	});


	addSimulationProcess([&]()->SimProcess {

		size_t counter = 1;
		while(true)
		{
			if(simu(*uut.ready) && simu(uut.valid))
			{
				BOOST_TEST(counter % 256 == simu(uut.data));
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

	Clock m_clock = Clock({ .absoluteFrequency = 100'000'000 });

	void simulateTransferTest(scl::Stream<UInt>& source, scl::Stream<UInt>& sink)
	{
		simulateBackPreassure(sink);
		simulateSendData(source, 16);
		simulateRecvData(sink, 16);
	}

	void In(scl::Stream<UInt>& stream, std::string prefix = "in_")
	{
		stream.data = gtry::pinIn(8_b).setName(prefix + "data");
		stream.valid = gtry::pinIn().setName(prefix + "valid");
		gtry::pinOut(*stream.ready).setName(prefix + "ready");
	}

	void Out(scl::Stream<UInt>& stream, std::string prefix = "out_")
	{
		gtry::pinOut(stream.data).setName(prefix + "data");
		gtry::pinOut(stream.valid).setName(prefix + "valid");
		*stream.ready = gtry::pinIn().setName(prefix + "ready");
	}

private:
	void simulateBackPreassure(scl::Stream<UInt>& stream)
	{
		addSimulationProcess([&]()->SimProcess {
			std::mt19937 rng{ std::random_device{}() };

			simu(*stream.ready) = 0;
			while(simu(stream.valid) == 0)
				co_await WaitClk(m_clock);

			while(true)
			{
				simu(*stream.ready) = rng() % 2;
				co_await WaitClk(m_clock);
			}
		});
	}

	void simulateSendData(scl::Stream<UInt>& stream, size_t transfers)
	{
		UInt counter = stream->width() + 1;

		stream.valid = '0';
		stream.data = ConstUInt(stream.data.width());

		IF(counter.lsb() == '0')
		{
			stream.valid = counter < transfers * 2;
			stream.data = counter.upper(stream->width());
		}
		
		IF(*stream.ready)
			counter += 1;
		counter = reg(counter, 0);

		/*
		addSimulationProcess([&, transfers]()->SimProcess {
			std::mt19937 rng{ std::random_device{}() };
			for(size_t i = 0; i < transfers; ++i)
			{
				simu(stream.valid) = 0;
				simu(stream.data).invalidate();

				while((rng() & 1) == 0)
					co_await WaitClk(m_clock);

				simu(stream.valid) = 1;
				simu(stream.data) = i;
				co_await WaitClk(m_clock);

				while(simu(*stream.ready) == 0)
					co_await WaitClk(m_clock);
			}
		});
		*/
	}

	void simulateRecvData(scl::Stream<UInt>& stream, size_t transfers)
	{
		addSimulationProcess([&, transfers]()->SimProcess {
			size_t expectedValue = 0;
			while(true)
			{
				if(simu(*stream.ready) == 1 &&
					simu(stream.valid) == 1)
				{
					BOOST_TEST(simu(stream.data) == expectedValue);
					expectedValue++;
				}
				co_await WaitClk(m_clock);

				if(expectedValue == transfers)
				{
					stopTest();
					co_await WaitClk(m_clock);
				}
			}
		});
	}
};

BOOST_FIXTURE_TEST_CASE(stream_downstreamReg, StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::Stream<UInt> in{ .data = 5_b };
	//In(in);

	scl::Stream<UInt> out = in.regDownstream();
	Out(out);

	simulateTransferTest(in, out);

	//recordVCD("stream_downstreamReg.vcd");
	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
	runTicks(m_clock.getClk(), 1024);
}

BOOST_FIXTURE_TEST_CASE(stream_uptreamReg, StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::Stream<UInt> in{ .data = 5_b };
	//In(in);

	scl::Stream<UInt> out = in.regUpstream();
	Out(out);

	simulateTransferTest(in, out);

	//recordVCD("stream_uptreamReg.vcd");
	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
	runTicks(m_clock.getClk(), 1024);
}

BOOST_FIXTURE_TEST_CASE(stream_reg, StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::Stream<UInt> in { .data = 5_b };
	//In(in);

	scl::Stream<UInt> out = reg(in);
	Out(out);

	simulateTransferTest(in, out);

	recordVCD("stream_reg.vcd");
	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
	runTicks(m_clock.getClk(), 1024);
}

BOOST_FIXTURE_TEST_CASE(stream_reg_chaining, StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::Stream<UInt> in{ .data = 5_b };
	//In(in);

	scl::Stream<UInt> out = in.regDownstreamBlocking().regDownstreamBlocking().regDownstreamBlocking().regDownstream();
	Out(out);

	simulateTransferTest(in, out);

	recordVCD("stream_reg_chaining.vcd");
	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
	runTicks(m_clock.getClk(), 1024);
}
