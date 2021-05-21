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

#include <gatery/scl/Fifo.h>

using namespace boost::unit_test;
using namespace gtry;


BOOST_FIXTURE_TEST_CASE(Fifo_basics, UnitTestSimulationFixture)
{
    Clock clock(ClockConfig{}.setAbsoluteFrequency(100'000'000).setName("clock"));
    ClockScope clkScp(clock);
 
    scl::Fifo fifo(16, BVec{ 8_b });
    
    BVec pushData = pinIn(8_b).setName("push_data");
    Bit push = pinIn().setName("push_valid");
    fifo.push(pushData, push);

    BVec popData = 8_b;
    Bit popReady = pinIn().setName("pop_ready");
    fifo.pop(popData, popReady);
    
    pinOut(popData).setName("pop_data");
    OutputPin empty = pinOut(fifo.empty()).setName("empty");
    OutputPin full = pinOut(fifo.full()).setName("full");

    OutputPin halfEmpty = pinOut(fifo.almostEmpty(8)).setName("half_empty");
    OutputPin halfFull = pinOut(fifo.almostFull(8)).setName("half_full");

    addSimulationProcess([=]()->SimProcess {
        simu(pushData) = 0;
        simu(push) = '0';
        simu(popReady) = '0';

        for(size_t i = 0; i < 5; ++i)
            co_await WaitClk(clock);

        BOOST_TEST(simu(empty) == 1);
        BOOST_TEST(simu(full) == 0);
        BOOST_TEST(simu(halfEmpty) == 1);
        BOOST_TEST(simu(halfFull) == 0);

        for (size_t i = 0; i < 16; ++i)
        {
            simu(push) = '1';
            simu(pushData) = i * 3;
            co_await WaitClk(clock);
        }
        simu(push) = '0';

        BOOST_TEST(simu(empty) == 0);
        BOOST_TEST(simu(full) == 1);
        BOOST_TEST(simu(halfEmpty) == 0);
        BOOST_TEST(simu(halfFull) == 1);

        for (size_t i = 0; i < 16; ++i)
        {
            BOOST_TEST(simu(popData) == i * 3);
            simu(popReady) = '1';
            co_await WaitClk(clock);
        }
        simu(popReady) = '0';
        co_await WaitClk(clock);

        uint64_t dummyDataPut = 0;
        simu(pushData) = dummyDataPut++ * 5;
        co_await WaitClk(clock);


        for (size_t i = 0; i < 40; ++i)
        {
            simu(push) = '1';
            simu(pushData) = i * 5;
            co_await WaitClk(clock);
            simu(popReady) = '1';
            BOOST_TEST(simu(popData) == i * 5);
            BOOST_TEST(simu(halfEmpty) == 1);
            BOOST_TEST(simu(halfFull) == 0);
        }

        simu(push) = '0';
        co_await WaitClk(clock);
        simu(popReady) = '0';
        co_await WaitClk(clock);


    });


    sim::VCDSink vcd{ design.getCircuit(), getSimulator(), "fifo.vcd" };
    vcd.addAllPins();
    vcd.addAllNamedSignals();

    design.getCircuit().postprocess(gtry::DefaultPostprocessing{});

    design.visualize("after");

    runTicks(clock.getClk(), 500);
}
