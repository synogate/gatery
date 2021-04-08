/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Synogate GbR

    Gatery is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    Gatery is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

#include <hcl/frontend.h>
#include <hcl/simulation/UnitTestSimulationFixture.h>

#include <hcl/stl/io/uart.h>

#include <iostream>

using namespace boost::unit_test;
using namespace hcl::core::frontend;
using namespace hcl::utils;
using UnitTestSimulationFixture = hcl::core::frontend::UnitTestSimulationFixture;


BOOST_FIXTURE_TEST_CASE(SimProc_Basics, UnitTestSimulationFixture)
{
    using namespace hcl::core::frontend;



    unsigned baudRate = 19200;
    Clock clock(ClockConfig{}.setAbsoluteFrequency(baudRate*5).setName("clock"));
    std::vector<std::uint8_t> dataStream;
    {
        ClockScope clkScp(clock);
        auto rx = pinIn().setName("inRx");
        sim_tap((Bit)rx);

        hcl::stl::UART uart;
        uart.baudRate = baudRate;


        auto stream = uart.recieve(rx);

        auto outData = pinOut(stream.data).setName("outData");
        auto outValid = pinOut(stream.valid).setName("outValid");
        auto outReady = pinIn().setName("outReady");
        stream.ready = outReady;



        auto sending = pinIn().setName("sending");
        sim_tap((Bit)sending);

        addSimulationProcess([rx, &clock, &dataStream, sending, baudRate]()->SimProcess{
            dataStream.clear();
            sim(rx) = '1';
            sim(sending) = '0';

            co_await WaitFor(Seconds(2)/clock.getAbsoluteFrequency());
            while (true) {
                std::uint8_t data = rand();
                dataStream.push_back(data);


                sim(sending) = '1';
                sim(rx) = '0'; // start bit
                co_await WaitFor(Seconds(1,baudRate));
                for (auto i : hcl::utils::Range(8)) {
                    sim(rx) = data & 1; // data bit
                    data >>= 1;
                    co_await WaitFor(Seconds(1,baudRate));
                }
                sim(rx) = '1'; // stop bit
                co_await WaitFor(Seconds(1,baudRate));
                sim(sending) = '0';

                co_await WaitFor(Seconds(rand() % 100)/clock.getAbsoluteFrequency());
            }
        });

        addSimulationProcess([=, &clock, &dataStream]()->SimProcess{
            sim(outReady) = false;
            co_await WaitFor(Seconds(1,2)/clock.getAbsoluteFrequency());

            sim(outReady) = true;

            size_t readIdx = 0;
            while (true) {
                while (!sim(outValid))
                    co_await WaitClk(clock);

                std::uint8_t data = sim(outData);
                BOOST_TEST(readIdx < dataStream.size());
                if (readIdx < dataStream.size())
                    BOOST_TEST(data == dataStream[readIdx]);

                readIdx++;

                co_await WaitClk(clock);
            }
        });

    }

    design.getCircuit().optimize(3);
    runTicks(clock.getClk(), 500);
}
