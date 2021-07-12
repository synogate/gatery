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

#include <queue>

using namespace boost::unit_test;
using namespace gtry;

struct FifoTest
{
    FifoTest(Clock& clk) : clk(clk) {}

    scl::Fifo<BVec> create(size_t depth, BitWidth width)
    {
        scl::Fifo<BVec> fifo{ depth, BVec{ width } };
        pushData = width;
        popData = width;
        fifo.push(pushData, push);
        fifo.pop(popData, pop);
        
        push = pinIn().setName("push_valid");
        pushData = pinIn(width).setName("push_data");
        pop = pinIn().setName("pop_ready");
        pinOut(popData).setName("pop_data");

        empty = fifo.empty();
        full = fifo.full();
        pinOut(empty).setName("empty");
        pinOut(full).setName("full");

        return fifo;
    }

    SimProcess operator() ()
    {
        std::queue<uint8_t> model;

        while (true)
        {
            if (simu(empty) == 0)
                BOOST_TEST(!model.empty());
            if (simu(full) == 1)
                BOOST_TEST(!model.empty());

            if (simu(push) && !simu(full))
                model.push(uint8_t(simu(pushData)));

            if (!simu(empty))
            {
                uint8_t peekValue = (uint8_t)simu(popData);
                BOOST_TEST(!model.empty());
                if(!model.empty())
                    BOOST_TEST(peekValue == model.front());
            }

            if (simu(pop) && !simu(empty))
            {
                if(!model.empty())
                    model.pop();
            }

            co_await WaitClk(clk);
        }
    }

    Clock clk;

    BVec pushData;
    Bit push;

    BVec popData;
    Bit pop;

    Bit empty;
    Bit full;

};

BOOST_FIXTURE_TEST_CASE(Fifo_basic, UnitTestSimulationFixture)
{
    Clock clock(ClockConfig{}.setAbsoluteFrequency(100'000'000).setName("clock"));
    ClockScope clkScp(clock);
 
    FifoTest fifo{ clock };
    auto&& uut = fifo.create(16, 8_b);

    OutputPin halfEmpty = pinOut(uut.almostEmpty(8)).setName("half_empty");
    OutputPin halfFull = pinOut(uut.almostFull(8)).setName("half_full");
    
    addSimulationProcess([&]()->SimProcess {
        simu(fifo.pushData) = 0;
        simu(fifo.push) = 0;
        simu(fifo.pop) = 0;

        for(size_t i = 0; i < 5; ++i)
            co_await WaitClk(clock);

        BOOST_TEST(simu(fifo.empty) == 1);
        BOOST_TEST(simu(fifo.full) == 0);
        BOOST_TEST(simu(halfEmpty) == 1);
        BOOST_TEST(simu(halfFull) == 0);

        for (size_t i = 0; i < 16; ++i)
        {
            simu(fifo.push) = '1';
            simu(fifo.pushData) = i * 3;
            co_await WaitClk(clock);
        }
        simu(fifo.push) = '0';
        co_await WaitClk(clock);

        BOOST_TEST(simu(fifo.empty) == 0);
        BOOST_TEST(simu(fifo.full) == 1);
        BOOST_TEST(simu(halfEmpty) == 0);
        BOOST_TEST(simu(halfFull) == 1);

        for (size_t i = 0; i < 16; ++i)
        {
            simu(fifo.pop) = '1';
            co_await WaitClk(clock);
        }

        simu(fifo.pop) = '0';
        co_await WaitClk(clock);

        BOOST_TEST(simu(fifo.empty) == 1);
        BOOST_TEST(simu(fifo.full) == 0);
        BOOST_TEST(simu(halfEmpty) == 1);
        BOOST_TEST(simu(halfFull) == 0);

        for (size_t i = 0; i < 128; ++i)
        {
            simu(fifo.push) = i % 15 != 0 ? 1 : 0;
            simu(fifo.pushData) = uint8_t(i * 5);
            co_await WaitClk(clock);
            simu(fifo.pop) = i % 8 != 0 ? 1 : 0;
        }

        simu(fifo.push) = '0';
        simu(fifo.pop) = '0';
        co_await WaitClk(clock);

        BOOST_TEST(simu(fifo.empty) == 0);
        BOOST_TEST(simu(fifo.full) == 0);
        BOOST_TEST(simu(halfEmpty) == 0);
        BOOST_TEST(simu(halfFull) == 1);

        for (size_t i = 0; i < 8; ++i)
        {
            simu(fifo.pop) = '1';
            co_await WaitClk(clock);
        }

        simu(fifo.pop) = '0';
        co_await WaitClk(clock);

        BOOST_TEST(simu(fifo.empty) == 1);
        BOOST_TEST(simu(fifo.full) == 0);
        BOOST_TEST(simu(halfEmpty) == 1);
        BOOST_TEST(simu(halfFull) == 0);

    });

    addSimulationProcess(fifo);

    //sim::VCDSink vcd{ design.getCircuit(), getSimulator(), "fifo.vcd" };
    //vcd.addAllPins();
    //vcd.addAllNamedSignals();

    design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
    //design.visualize("after");

    runTicks(clock.getClk(), 190);
}

BOOST_FIXTURE_TEST_CASE(Fifo_fuzz, UnitTestSimulationFixture)
{
    Clock clock(ClockConfig{}.setAbsoluteFrequency(100'000'000).setName("clock"));
    ClockScope clkScp(clock);

    FifoTest fifo{ clock };
    fifo.create(16, 8_b);

    addSimulationProcess([&]()->SimProcess {
        simu(fifo.pushData) = 0;

        std::mt19937 rng{ 12524 };
        while (true)
        {
            if (!simu(fifo.full) && (rng() % 2 == 0))
            {
                simu(fifo.push) = 1;
                simu(fifo.pushData) = uint8_t(rng());
            }
            else
            {
                simu(fifo.push) = 0;
            }

            if (!simu(fifo.empty) && (rng() % 2 == 0))
            {
                simu(fifo.pop) = 1;
            }
            else
            {
                simu(fifo.pop) = 0;
            }

            co_await WaitClk(clock);
        }
    });

    addSimulationProcess(fifo);

    //sim::VCDSink vcd{ design.getCircuit(), getSimulator(), "fifo_fuzz.vcd" };
    //vcd.addAllPins();
    //vcd.addAllNamedSignals();

    design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
    //design.visualize("fifo_fuzz");

    runTicks(clock.getClk(), 2048);
}
