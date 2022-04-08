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

    scl::Fifo<UInt> create(size_t depth, BitWidth width)
    {
        scl::Fifo<UInt> fifo{ depth, UInt{ width } };
        actualDepth = fifo.depth();

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

    UInt pushData;
    Bit push;

    UInt popData;
    Bit pop;

    Bit empty;
    Bit full;

    size_t actualDepth;

};

BOOST_FIXTURE_TEST_CASE(Fifo_basic, BoostUnitTestSimulationFixture)
{
    Clock clock(ClockConfig{}.setAbsoluteFrequency(100'000'000).setName("clock"));
    ClockScope clkScp(clock);
 
    FifoTest fifo{ clock };
    auto&& uut = fifo.create(16, 8_b);

    size_t actualDepth = fifo.actualDepth;

    OutputPin halfEmpty = pinOut(uut.almostEmpty(actualDepth/2)).setName("half_empty");
    OutputPin halfFull = pinOut(uut.almostFull(actualDepth/2)).setName("half_full");
    
    addSimulationProcess([=,this]()->SimProcess {
        simu(fifo.pushData) = 0;
        simu(fifo.push) = 0;
        simu(fifo.pop) = 0;

        for(size_t i = 0; i < 5; ++i)
            co_await WaitClk(clock);

        BOOST_TEST(simu(fifo.empty) == 1);
        BOOST_TEST(simu(fifo.full) == 0);
        BOOST_TEST(simu(halfEmpty) == 1);
        BOOST_TEST(simu(halfFull) == 0);

        for (size_t i = 0; i < actualDepth; ++i)
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

        for (size_t i = 0; i < actualDepth; ++i)
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

        for (size_t i = 0, count = 0; count < actualDepth/2; ++i)
        {
            bool doPush = i % 15 != 0;
            bool doPop = count > 0 && (i % 8 != 0);
            simu(fifo.push) = doPush;
            simu(fifo.pushData) = uint8_t(i * 5);
            simu(fifo.pop) = doPop;
            co_await WaitClk(clock);

            if (doPush) count++;
            if (doPop) count--;
        }

        simu(fifo.push) = '0';
        simu(fifo.pop) = '0';
        co_await WaitClk(clock);

        BOOST_TEST(simu(fifo.empty) == 0);
        BOOST_TEST(simu(fifo.full) == 0);
        BOOST_TEST(simu(halfEmpty) == 1);
        BOOST_TEST(simu(halfFull) == 1);

        for (size_t i = 0; i < actualDepth/2; ++i)
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

        stopTest();
    });

    addSimulationProcess(fifo);

    //sim::VCDSink vcd{ design.getCircuit(), getSimulator(), "fifo.vcd" };
    //vcd.addAllPins();
    //vcd.addAllNamedSignals();

    design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
    //design.visualize("after");

    runTest(hlim::ClockRational(20000, 1) / clock.getClk()->getAbsoluteFrequency());
}

BOOST_FIXTURE_TEST_CASE(Fifo_fuzz, BoostUnitTestSimulationFixture)
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
