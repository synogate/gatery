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

using namespace boost::unit_test;
using namespace gtry;
using namespace gtry::utils;
using BoostUnitTestSimulationFixture = gtry::BoostUnitTestSimulationFixture;


BOOST_FIXTURE_TEST_CASE(SimProc_Basics, BoostUnitTestSimulationFixture)
{
    using namespace gtry;



    unsigned baudRate = 19200;
    Clock clock({ .absoluteFrequency = baudRate*5 });
    std::vector<std::uint8_t> dataStream;
    {
        ClockScope clkScp(clock);
        auto rx = pinIn().setName("inRx");
        sim_tap((Bit)rx);

        gtry::scl::UART uart;
        uart.baudRate = baudRate;


        auto stream = uart.receive(rx);

        auto outData = pinOut(stream.data).setName("outData");
        auto outValid = pinOut(stream.valid).setName("outValid");
        auto outReady = pinIn().setName("outReady");
        stream.ready = outReady;



        auto sending = pinIn().setName("sending");
        sim_tap((Bit)sending);

        addSimulationProcess([rx, &clock, &dataStream, sending, baudRate]()->SimProcess{
            dataStream.clear();
            simu(rx) = '1';
            simu(sending) = '0';

            co_await WaitFor(Seconds(2)/clock.getAbsoluteFrequency());
            while (true) {
                std::uint8_t data = rand();
                dataStream.push_back(data);


                simu(sending) = '1';
                simu(rx) = '0'; // start bit
                co_await WaitFor(Seconds(1,baudRate));
                for ([[maybe_unused]]  auto i : gtry::utils::Range(8)) {
                    simu(rx) = data & 1; // data bit
                    data >>= 1;
                    co_await WaitFor(Seconds(1,baudRate));
                }
                simu(rx) = '1'; // stop bit
                co_await WaitFor(Seconds(1,baudRate));
                simu(sending) = '0';

                co_await WaitFor(Seconds(rand() % 100)/clock.getAbsoluteFrequency());
            }
        });

        addSimulationProcess([=, &clock, &dataStream]()->SimProcess{
            simu(outReady) = false;
            co_await WaitFor(Seconds(1,2)/clock.getAbsoluteFrequency());

            simu(outReady) = true;

            size_t readIdx = 0;
            while (true) {
                while (!simu(outValid))
                    co_await WaitClk(clock);

                size_t data = simu(outData);
                BOOST_TEST(readIdx < dataStream.size());
                if (readIdx < dataStream.size())
                    BOOST_TEST(data == dataStream[readIdx]);

                readIdx++;

                co_await WaitClk(clock);
            }
        });

    }

    design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
    runTicks(clock.getClk(), 500);
}
