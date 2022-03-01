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

#include <gatery/scl/StreamArbiter.h>


using namespace boost::unit_test;
using namespace gtry;

BOOST_FIXTURE_TEST_CASE(arbitrateInOrder_basic, BoostUnitTestSimulationFixture)
{
    Clock clock(ClockConfig{}.setAbsoluteFrequency(100'000'000).setName("clock"));
    ClockScope clkScp(clock);

    scl::Stream<UInt> in0;
    scl::Stream<UInt> in1;

    in0.value() = pinIn(8_b).setName("in0_data");
    in0.valid = pinIn().setName("in0_valid");
    in0.ready = Bit{};
    pinOut(*in0.ready).setName("in0_ready");

    in1.value() = pinIn(8_b).setName("in1_data");
    in1.valid = pinIn().setName("in1_valid");
    in1.ready = Bit{};
    pinOut(*in1.ready).setName("in1_ready");

    scl::arbitrateInOrder uut{ in0, in1 };
    pinOut(uut.value()).setName("out_data");
    pinOut(*uut.valid).setName("out_valid");
    *uut.ready = pinIn().setName("out_ready");

    addSimulationProcess([&]()->SimProcess {
        simu(*uut.ready) = 1;
        simu(*in0.valid) = 0;
        simu(*in1.valid) = 0;
        simu(in0.value()) = 0;
        simu(in1.value()) = 0;
        co_await WaitClk(clock);

        simu(*in0.valid) = 0;
        simu(*in1.valid) = 1;
        simu(in1.value()) = 1;
        co_await WaitClk(clock);

        simu(*in1.valid) = 0;
        simu(*in0.valid) = 1;
        simu(in0.value()) = 2;
        co_await WaitClk(clock);

        simu(*in1.valid) = 1;
        simu(*in0.valid) = 1;
        simu(in0.value()) = 3;
        simu(in1.value()) = 4;
        co_await WaitClk(clock);
        co_await WaitClk(clock);

        simu(*in1.valid) = 1;
        simu(*in0.valid) = 1;
        simu(in0.value()) = 5;
        simu(in1.value()) = 6;
        co_await WaitClk(clock);
        co_await WaitClk(clock);

        simu(*in0.valid) = 0;
        simu(*in1.valid) = 1;
        simu(in1.value()) = 7;
        co_await WaitClk(clock);

        simu(*in1.valid) = 0;
        simu(*in0.valid) = 0;
        simu(*uut.ready) = 0;
        co_await WaitClk(clock);

        simu(*in1.valid) = 0;
        simu(*in0.valid) = 1;
        simu(in0.value()) = 8;
        simu(*uut.ready) = 1;
        co_await WaitClk(clock);

        simu(*in1.valid) = 0;
        simu(*in0.valid) = 0;
        co_await WaitClk(clock);
    });

    addSimulationProcess([&]()->SimProcess {

        size_t counter = 1;
        while (true)
        {
            if (simu(*uut.ready) && simu(*uut.valid))
            {
                BOOST_TEST(counter == simu(uut.value()));
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
    Clock clock(ClockConfig{}.setAbsoluteFrequency(100'000'000).setName("clock"));
    ClockScope clkScp(clock);

    scl::Stream<UInt> in0;
    scl::Stream<UInt> in1;

    in0.value() = pinIn(8_b).setName("in0_data");
    in0.valid = pinIn().setName("in0_valid");
    in0.ready = Bit{};
    pinOut(*in0.ready).setName("in0_ready");

    in1.value() = pinIn(8_b).setName("in1_data");
    in1.valid = pinIn().setName("in1_valid");
    in1.ready = Bit{};
    pinOut(*in1.ready).setName("in1_ready");

    scl::arbitrateInOrder uut{ in0, in1 };
    pinOut(uut.value()).setName("out_data");
    pinOut(*uut.valid).setName("out_valid");
    *uut.ready = pinIn().setName("out_ready");

    addSimulationProcess([&]()->SimProcess {
        simu(*uut.ready) = 1;
        simu(*in0.valid) = 0;
        simu(*in1.valid) = 0;

        std::mt19937 rng{ 10179 };
        size_t counter = 1;
        bool wasReady = false;
        while (true)
        {
            if (wasReady)
            {
                if (rng() % 2 == 0)
                {
                    simu(*in0.valid) = 1;
                    simu(in0.value()) = counter++;
                }
                else
                {
                    simu(*in0.valid) = 0;
                }

                if (rng() % 2 == 0)
                {
                    simu(*in1.valid) = 1;
                    simu(in1.value()) = counter++;
                }
                else
                {
                    simu(*in1.valid) = 0;
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
        while (true)
        {
            if (simu(*uut.ready) && simu(*uut.valid))
            {
                BOOST_TEST(counter % 256 == simu(uut.value()));
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
