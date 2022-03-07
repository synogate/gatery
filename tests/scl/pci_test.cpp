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

#include <gatery/scl/io/pci.h>

#include <queue>

using namespace boost::unit_test;
using namespace gtry;

struct TbReq
{
    bool read = false;
    bool write = false;
    uint32_t address = 0;
    uint32_t data = 0;
};

struct TlpBuilder
{
    uint64_t hdr;

    TlpBuilder& length(size_t words) { return insert(words, 0, 10); }
    TlpBuilder& data(bool val = true) { return insert(val, 30); }
    TlpBuilder& requester(size_t bus, size_t dev, size_t func) {
        insert(func, 48, 3);
        insert(dev, 48 + 3, 5);
        return insert(bus, 56, 8);
    }
    TlpBuilder& tag(size_t val) { return insert(val, 40, 8); }

    operator uint64_t () const { return hdr; }

    TlpBuilder& insert(uint64_t val, size_t offset, size_t width) 
    { 
        hdr = gtry::utils::bitfieldInsert(hdr, offset, width, val);
        return *this;
    }

    TlpBuilder& insert(bool val, size_t offset)
    {
        return insert(val ? 1 : 0, offset, 1);
    }
};

BOOST_FIXTURE_TEST_CASE(pci_AvmmBridge_basic, BoostUnitTestSimulationFixture)
{
    Clock clock(ClockConfig{}.setAbsoluteFrequency(100'000'000).setName("clock"));
    ClockScope clkScp(clock);

    scl::Stream<scl::pci::Tlp> in;
    in.valid = pinIn().setName("in_valid");

    UInt inHeader = pinIn(64_b).setName("in_header");
    UInt inAddress = pinIn(32_b).setName("in_address");
    in.data.header = cat(inAddress, inHeader);
    in.data.data = pinIn(32_b).setName("in_data");
    in.ready = Bit{};
    pinOut(*in.ready).setName("in_ready");

    scl::pci::PciId completerId;
    completerId.bus = 1;
    completerId.dev = 2;
    completerId.func = 3;

    scl::AvalonMM avmm;
    avmm.ready = Bit{};
    avmm.read = Bit{};
    avmm.write = Bit{};
    avmm.writeData = 32_b;
    avmm.readData = 32_b;
    avmm.readDataValid = Bit{};
    avmm.maximumPendingReadTransactions = 4;

    scl::pci::AvmmBridge uut{ in, avmm, completerId };
    scl::Stream<scl::pci::Tlp>& out = uut.tx();
    out.ready = pinIn().setName("out_ready");
    pinOut(*out.valid).setName("out_valid");
    pinOut(out.data.header).setName("out_header");
    pinOut(out.data.data).setName("out_data");

    Memory<UInt> testMem{ 1 << 16, avmm.writeData->getWidth() };
    IF(*avmm.write & *avmm.ready)
        testMem[avmm.address(0, 16_b)] = *avmm.writeData;
    *avmm.readData = testMem[avmm.address(0, 16_b)];
    *avmm.readDataValid = *avmm.read;
    for (size_t i = 0; i < 6; ++i)
    {
        *avmm.readData = reg(*avmm.readData, {.allowRetimingBackward=true});
        *avmm.readDataValid = reg(*avmm.readDataValid, '0', {.allowRetimingBackward=true});
    }
    *avmm.ready = pinIn().setName("avmm_ready_sim");

    setName(avmm, "avmm");
    //avmm.pinOut("avmm");

    // stream in bfm
    std::queue<TbReq> reqQueue;
    std::queue<TlpBuilder> cplQueue;


    addSimulationProcess([&]()->SimProcess {
        simu(*avmm.ready) = 1;

        co_await WaitClk(clock);

        for (uint32_t i = 0; i < 16; ++i)
            reqQueue.push(TbReq{ .write = true, .address = i * 4, .data = i });

        for (uint32_t i = 0; i < 16; ++i)
            reqQueue.push(TbReq{ .read = true, .address = i * 4 });


    });


    addSimulationProcess([&]()->SimProcess {

        std::mt19937 rng{ 18057 };
        uint8_t tag = 0xAA;

        simu(*in.valid) = 0;
        while (true)
        {
            if (simu(*in.ready) != 0)
            {
                if (reqQueue.empty())
                {
                    simu(*in.valid) = 0;
                }
                else
                {
                    const TbReq req = reqQueue.front();
                    reqQueue.pop();
                    simu(inAddress) = req.address;
                    simu(in.data.data) = req.data;

                    auto tlp = TlpBuilder{}.length(1);
                    tlp.requester(rng(), rng(), rng());
                    tlp.tag(tag++);

                    if (req.read)
                    {
                        simu(*in.valid) = 1;
                        simu(inHeader) = tlp;
                        cplQueue.push(tlp);
                    }
                    else if (req.write)
                    {
                        simu(*in.valid) = 1;
                        simu(inHeader) = tlp.data();
                    }
                    else
                    {
                        simu(*in.valid) = 0;
                    }
                }
            }
            co_await WaitClk(clock);
        }
    });

    // stream out bfm
    addSimulationProcess([&]()->SimProcess {
        simu(*out.ready) = 1;

        while (true)
        {
            if (simu(*out.ready) & simu(*out.valid))
            {
                BOOST_TEST(!cplQueue.empty());

                if (!cplQueue.empty())
                {
                    uint64_t reqTlp = cplQueue.front();
                    cplQueue.pop();

                    auto resTlpState = simu(out.data.header).eval();
                    uint64_t hdr = resTlpState.extractNonStraddling(sim::DefaultConfig::VALUE, 0, 64);
                    uint64_t dst = resTlpState.extractNonStraddling(sim::DefaultConfig::VALUE, 64, 32);
                    uint64_t data = simu(out.data.data);

                    BOOST_TEST(gtry::utils::bitfieldExtract(hdr, 24, 8) == 0x4A);

                }
            }
            co_await WaitClk(clock);
        }


    });


    //sim::VCDSink vcd{ design.getCircuit(), getSimulator(), "pci_AvmmBridge_basic.vcd" };
    //vcd.addAllPins();
    //vcd.addAllNamedSignals();

    design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
    //design.visualize("pci_AvmmBridge_basic");

    runTicks(clock.getClk(), 190);
}
