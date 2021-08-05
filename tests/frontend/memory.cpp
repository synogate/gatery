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
#include "frontend/pch.h"
#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

#include <cstdint>

using namespace boost::unit_test;
using UnitTestSimulationFixture = gtry::BoostUnitTestSimulationFixture;

BOOST_FIXTURE_TEST_CASE(async_ROM, UnitTestSimulationFixture)
{
    using namespace gtry;
    using namespace gtry::sim;
    using namespace gtry::utils;

    Clock clock(ClockConfig{}.setAbsoluteFrequency(100'000'000).setName("clock"));
	ClockScope clkScp(clock);

    std::vector<unsigned> contents;
    contents.resize(16);
    std::mt19937 rng{ 18055 };
    for (auto &e : contents)
        e = rng() % 16;

    Memory<BVec> rom(contents.size(), 4_b);
    rom.fillPowerOnState(createDefaultBitVectorState(16, 4, [&contents](std::size_t i, std::size_t *words){
        words[DefaultConfig::VALUE] = contents[i];
        words[DefaultConfig::DEFINED] = ~0ull;
    }));


    BVec addr = pinIn(4_b);
    auto output = pinOut(rom[addr]);


	addSimulationProcess([=,this,&contents]()->SimProcess {
        
        for (auto i : Range(16)) {
            simu(addr) = i;
			BOOST_TEST(simu(output).value() == contents[i]);
            co_await WaitClk(clock); // for waveforms
        }

        stopTest();
	});

	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
	runTest(hlim::ClockRational(100, 1) / clock.getClk()->getAbsoluteFrequency());
}



BOOST_FIXTURE_TEST_CASE(sync_ROM, UnitTestSimulationFixture)
{
    using namespace gtry;
    using namespace gtry::sim;
    using namespace gtry::utils;

    Clock clock(ClockConfig{}.setAbsoluteFrequency(100'000'000).setName("clock"));
	ClockScope clkScp(clock);

    std::vector<unsigned> contents;
    contents.resize(16);
    std::mt19937 rng{ 18055 };
    for (auto &e : contents)
        e = rng() % 16;

    Memory<BVec> rom(contents.size(), 4_b);
    rom.fillPowerOnState(createDefaultBitVectorState(16, 4, [&contents](std::size_t i, std::size_t *words){
        words[DefaultConfig::VALUE] = contents[i];
        words[DefaultConfig::DEFINED] = ~0ull;
    }));


    BVec addr = pinIn(4_b);
    auto output = pinOut(reg(rom[addr]));


	addSimulationProcess([=,this,&contents]()->SimProcess {
        
        for (auto i : Range(16)) {
            simu(addr) = i;
            co_await WaitClk(clock);
			BOOST_TEST(simu(output).value() == contents[i]);
        }

        stopTest();
	});

	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
	runTest(hlim::ClockRational(100, 1) / clock.getClk()->getAbsoluteFrequency());
}



BOOST_FIXTURE_TEST_CASE(async_mem, UnitTestSimulationFixture)
{
    using namespace gtry;
    using namespace gtry::sim;
    using namespace gtry::utils;

    Clock clock(ClockConfig{}.setAbsoluteFrequency(100'000'000).setName("clock"));
	ClockScope clkScp(clock);

    std::vector<unsigned> contents;
    contents.resize(16);
    std::mt19937 rng{ 18055 };
    for (auto &e : contents)
        e = rng() % 16;

    Memory<BVec> mem(contents.size(), 4_b);
    mem.noConflicts();

    BVec addr = pinIn(4_b);
    auto output = pinOut(mem[addr]);
    BVec input = pinIn(4_b);
    Bit wrEn = pinIn();
    IF (wrEn)
        mem[addr] = input;

	addSimulationProcess([=,this,&contents]()->SimProcess {

        simu(wrEn) = '0';
        co_await WaitClk(clock);

        simu(wrEn) = '1';
        for (auto i : Range(16)) {
            simu(addr) = i;
            simu(input) = contents[i];
            co_await WaitClk(clock);
        }
        simu(wrEn) = '0';

        for (auto i : Range(16)) {
            simu(addr) = i;
			BOOST_TEST(simu(output).value() == contents[i]);
            co_await WaitClk(clock);
        }        

        stopTest();
	});

	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
	runTest(hlim::ClockRational(100, 1) / clock.getClk()->getAbsoluteFrequency());
}


BOOST_FIXTURE_TEST_CASE(sync_mem, UnitTestSimulationFixture)
{
    using namespace gtry;
    using namespace gtry::sim;
    using namespace gtry::utils;

    Clock clock(ClockConfig{}.setAbsoluteFrequency(100'000'000).setName("clock"));
	ClockScope clkScp(clock);

    std::vector<unsigned> contents;
    contents.resize(16);
    std::mt19937 rng{ 18055 };
    for (auto &e : contents)
        e = rng() % 16;

    Memory<BVec> mem(contents.size(), 4_b);
    mem.noConflicts();

    BVec addr = pinIn(4_b);
    auto output = pinOut(reg(mem[addr]));
    BVec input = pinIn(4_b);
    Bit wrEn = pinIn();
    IF (wrEn)
        mem[addr] = input;

	addSimulationProcess([=,this,&contents]()->SimProcess {

        simu(wrEn) = '0';
        co_await WaitClk(clock);

        simu(wrEn) = '1';
        for (auto i : Range(16)) {
            simu(addr) = i;
            simu(input) = contents[i];
            co_await WaitClk(clock);
        }
        simu(wrEn) = '0';

        for (auto i : Range(16)) {
            simu(addr) = i;
            co_await WaitClk(clock);
			BOOST_TEST(simu(output).value() == contents[i]);
        }        

        stopTest();
	});

	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
	runTest(hlim::ClockRational(100, 1) / clock.getClk()->getAbsoluteFrequency());
}



BOOST_FIXTURE_TEST_CASE(async_mem_read_before_write, UnitTestSimulationFixture)
{
    using namespace gtry;
    using namespace gtry::sim;
    using namespace gtry::utils;

    Clock clock(ClockConfig{}.setAbsoluteFrequency(100'000'000).setName("clock"));
	ClockScope clkScp(clock);

    std::vector<unsigned> contents;
    contents.resize(16);
    std::mt19937 rng{ 18055 };
    for (auto &e : contents)
        e = rng() % 16;

    Memory<BVec> mem(contents.size(), 4_b);

    BVec rdAddr = pinIn(4_b);
    auto output = pinOut(mem[rdAddr]);

    BVec wrAddr = pinIn(4_b);
    BVec input = pinIn(4_b);
    Bit wrEn = pinIn();
    IF (wrEn)
        mem[wrAddr] = input;

	addSimulationProcess([=,this,&contents]()->SimProcess {

        simu(wrEn) = '0';
        co_await WaitClk(clock);

        simu(wrEn) = '1';
        for (auto i : Range(16)) {
            simu(wrAddr) = i;
            simu(input) = contents[i];
            co_await WaitClk(clock);
        }
        simu(wrEn) = '0';

        for (auto i : Range(16)) {

            bool doWrite = i % 2;
            bool writeSameAddr = i % 3;

            simu(wrEn) = doWrite;
            if (writeSameAddr)
                simu(wrAddr) = i;
            else
                simu(wrAddr) = 0;

            simu(input) = 0;

            simu(rdAddr) = i;

			BOOST_TEST(simu(output).value() == contents[i]);
            co_await WaitClk(clock);
        }        

        stopTest();
	});

	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
	runTest(hlim::ClockRational(100, 1) / clock.getClk()->getAbsoluteFrequency());
}



BOOST_FIXTURE_TEST_CASE(async_mem_write_before_read, UnitTestSimulationFixture)
{
    using namespace gtry;
    using namespace gtry::sim;
    using namespace gtry::utils;

    Clock clock(ClockConfig{}.setAbsoluteFrequency(100'000'000).setName("clock"));
	ClockScope clkScp(clock);

    std::vector<unsigned> contents;
    contents.resize(16);
    std::mt19937 rng{ 18055 };
    for (auto &e : contents)
        e = rng() % 16;

    Memory<BVec> mem(contents.size(), 4_b);

    BVec rdAddr = pinIn(4_b);

    BVec wrAddr = pinIn(4_b);
    BVec input = pinIn(4_b);
    Bit wrEn = pinIn();
    IF (wrEn)
        mem[wrAddr] = input;

    auto output = pinOut(mem[rdAddr]);

	addSimulationProcess([=,this,&contents]()->SimProcess {

        simu(wrEn) = '0';
        co_await WaitClk(clock);

        simu(wrEn) = '1';
        for (auto i : Range(16)) {
            simu(wrAddr) = i;
            simu(input) = contents[i];
            co_await WaitClk(clock);
        }
        simu(wrEn) = '0';

        for (auto i : Range(16)) {

            bool doWrite = i % 2;
            bool writeSameAddr = i % 3;

            simu(wrEn) = doWrite;
            if (writeSameAddr)
                simu(wrAddr) = i;
            else
                simu(wrAddr) = 0;

            simu(input) = 0;

            simu(rdAddr) = i;

            if (doWrite && writeSameAddr)
                BOOST_TEST(simu(output).value() == 0);
            else
			    BOOST_TEST(simu(output).value() == contents[i]);
            co_await WaitClk(clock);
        }        

        stopTest();
	});

	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
	runTest(hlim::ClockRational(100, 1) / clock.getClk()->getAbsoluteFrequency());
}

BOOST_FIXTURE_TEST_CASE(async_mem_read_modify_write, UnitTestSimulationFixture)
{
    using namespace gtry;
    using namespace gtry::sim;
    using namespace gtry::utils;

    Clock clock(ClockConfig{}.setAbsoluteFrequency(100'000'000).setName("clock"));
	ClockScope clkScp(clock);

    std::vector<unsigned> contents;
    contents.resize(4, 0);
    std::mt19937 rng{ 18055 };

    Memory<BVec> mem(contents.size(), 32_b);
    mem.setType(MemType::LUTRAM);
    mem.setPowerOnStateZero();

    BVec addr = pinIn(4_b);
    BVec output;
    Bit wrEn = pinIn();
    {
        BVec elem = mem[addr];
        output = reg(elem);

        IF (wrEn)
            mem[addr] = elem + 1;
    }
    pinOut(output);

	addSimulationProcess([=,this,&contents,&rng]()->SimProcess {

        simu(wrEn) = '0';
        co_await WaitClk(clock);

        std::uniform_real_distribution<float> zeroOne(0.0f, 1.0f);
        std::uniform_int_distribution<unsigned> randomAddr(0, 3);        

        size_t collisions = 0;

        bool lastWasWrite = false;
        unsigned lastAddr = 0;
        for ([[maybe_unused]] auto i : Range(10000)) {
            bool doInc = zeroOne(rng) > 0.1f;
            unsigned incAddr = randomAddr(rng);
            simu(wrEn) = doInc;
            simu(addr) = incAddr;
            if (doInc)
                contents[incAddr]++;

            if (lastWasWrite && lastAddr == incAddr)
                collisions++;

            lastWasWrite = doInc;
            lastAddr = incAddr;
            co_await WaitClk(clock);
        }

        BOOST_TEST(collisions > 1000u);

        simu(wrEn) = '0';

        for (auto i : Range(4)) {
            simu(addr) = i;
            co_await WaitClk(clock);
            BOOST_TEST(simu(output).value() == contents[i]);
        }        

        stopTest();
	});


	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});

	runTest(hlim::ClockRational(20000, 1) / clock.getClk()->getAbsoluteFrequency());
}


BOOST_FIXTURE_TEST_CASE(sync_mem_read_modify_write, UnitTestSimulationFixture)
{
    using namespace gtry;
    using namespace gtry::sim;
    using namespace gtry::utils;

    Clock clock(ClockConfig{}.setAbsoluteFrequency(100'000'000).setName("clock"));
	ClockScope clkScp(clock);

    std::vector<unsigned> contents;
    contents.resize(4, 0);
    std::mt19937 rng{ 18055 };

    Memory<BVec> mem(contents.size(), 32_b);
    mem.setType(MemType::BRAM);
    mem.setPowerOnStateZero();

    BVec addr = pinIn(4_b);
    BVec output;
    Bit wrEn = pinIn();
    {
        BVec elem = mem[addr];
        output = reg(elem);

        IF (wrEn)
            mem[addr] = elem + 1;
    }
    pinOut(output);

	addSimulationProcess([=,this,&contents,&rng]()->SimProcess {

        simu(wrEn) = '0';
        co_await WaitClk(clock);

        std::uniform_real_distribution<float> zeroOne(0.0f, 1.0f);
        std::uniform_int_distribution<unsigned> randomAddr(0, 3);        

        size_t collisions = 0;

        bool lastWasWrite = false;
        size_t lastAddr = 0;
        for ([[maybe_unused]] auto i : Range(10000)) {
            bool doInc = zeroOne(rng) > 0.1f;
            size_t incAddr = randomAddr(rng);
            simu(wrEn) = doInc;
            simu(addr) = incAddr;
            if (doInc)
                contents[incAddr]++;

            if (lastWasWrite && lastAddr == incAddr)
                collisions++;

            lastWasWrite = doInc;
            lastAddr = incAddr;
            co_await WaitClk(clock);
        }

        BOOST_TEST(collisions > 1000u, "Too few collisions to verify correct RMW behavior");

        simu(wrEn) = '0';

        for (auto i : Range(4)) {
            simu(addr) = i;
            co_await WaitClk(clock);
            BOOST_TEST(simu(output).value() == contents[i]);
        }        

        stopTest();
	});

    design.visualize("before");
	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
    design.visualize("after");
	runTest(hlim::ClockRational(20000, 1) / clock.getClk()->getAbsoluteFrequency());
}





BOOST_FIXTURE_TEST_CASE(sync_mem_read_modify_write_multiple_reads, UnitTestSimulationFixture)
{
    using namespace gtry;
    using namespace gtry::sim;
    using namespace gtry::utils;

    Clock clock(ClockConfig{}.setAbsoluteFrequency(100'000'000).setName("clock"));
	ClockScope clkScp(clock);

    std::vector<unsigned> contents;
    contents.resize(4, 0);
    std::mt19937 rng{ 18055 };

    Memory<BVec> mem(contents.size(), 32_b);
    mem.setType(MemType::BRAM);
    mem.setPowerOnStateZero();

    BVec addr = pinIn(4_b).setName("rmw_addr");
    BVec rd_addr = pinIn(4_b).setName("rd_addr");
    Bit wrEn = pinIn().setName("wr_en");
    BVec readOutputBefore = reg(mem[rd_addr]);
    pinOut(readOutputBefore).setName("readOutputBefore");
    {
        BVec elem = mem[addr];
        IF (wrEn)
            mem[addr] = elem + 1;
    }
    BVec readOutputAfter = reg(mem[rd_addr]);
    pinOut(readOutputAfter).setName("readOutputAfter");

	addSimulationProcess([=,this,&contents,&rng]()->SimProcess {

        simu(wrEn) = '0';
        co_await WaitClk(clock);

        std::uniform_real_distribution<float> zeroOne(0.0f, 1.0f);
        std::uniform_int_distribution<unsigned> randomAddr(0, 3);        

        unsigned collisions = 0;

        bool lastWasWrite = false;
        unsigned lastAddr = 0;
        for ([[maybe_unused]] auto i : Range(100000)) {

            unsigned read_addr = randomAddr(rng);
            simu(rd_addr) = read_addr;
            unsigned expectedReadContentBefore = contents[read_addr];

            bool doInc = zeroOne(rng) > 0.1f;
            unsigned incAddr = randomAddr(rng);
            simu(wrEn) = doInc;
            simu(addr) = incAddr;
            if (doInc)
                contents[incAddr]++;

            if (lastWasWrite && lastAddr == incAddr)
                collisions++;

            unsigned expectedReadContentAfter = contents[read_addr];

            co_await WaitClk(clock);

            unsigned actualReadContentBefore = simu(readOutputBefore).value();
            BOOST_TEST(actualReadContentBefore == expectedReadContentBefore, 
                "Read-port (before RMW) yields " << actualReadContentBefore << " but expected " << expectedReadContentBefore << ". Read-port address: " << read_addr << " RMW address: " << incAddr << " last clock cycle RMW addr: " << lastAddr);
            BOOST_TEST(simu(readOutputAfter).value() == expectedReadContentAfter);

            lastWasWrite = doInc;
            lastAddr = incAddr;
        }

        BOOST_TEST(collisions > 1000, "Too few collisions to verify correct RMW behavior");

        stopTest();
	});

    design.visualize("before");
	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
    design.visualize("after");
	runTest(hlim::ClockRational(200000, 1) / clock.getClk()->getAbsoluteFrequency());
}



BOOST_FIXTURE_TEST_CASE(sync_mem_read_modify_write_on_wrEn, UnitTestSimulationFixture)
{
    using namespace gtry;
    using namespace gtry::sim;
    using namespace gtry::utils;

    Clock clock(ClockConfig{}.setAbsoluteFrequency(100'000'000).setName("clock"));
	ClockScope clkScp(clock);

    std::vector<unsigned> contents;
    contents.resize(4, 0);
    std::mt19937 rng{ 18055 };

    Memory<BVec> mem(contents.size(), 32_b);
    mem.setType(MemType::BRAM);
    mem.setPowerOnStateZero();

    BVec addr = pinIn(4_b).setName("rmw_addr");
    Bit shuffler = pinIn().setName("shuffler");

    BVec rd_addr = pinIn(4_b).setName("rd_addr");
    BVec readOutputBefore = reg(mem[rd_addr]);
    pinOut(readOutputBefore).setName("readOutputBefore");
    {
        BVec elem = mem[addr];
        Bit doWrite = elem[0] ^ shuffler;
        elem += 1;
        IF (doWrite)
            mem[addr] = elem;
    }


	addSimulationProcess([=,this,&contents,&rng]()->SimProcess {

        std::uniform_real_distribution<float> zeroOne(0.0f, 1.0f);
        std::uniform_int_distribution<unsigned> randomAddr(0, 3);        

        unsigned collisions = 0;

        bool lastWasWrite = false;
        unsigned lastAddr = 0;
        for ([[maybe_unused]] auto i : Range(100000)) {

            unsigned read_addr = randomAddr(rng);
            simu(rd_addr) = read_addr;
            unsigned expectedReadContentBefore = contents[read_addr];

            bool shfl = zeroOne(rng) > 0.5f;
            unsigned incAddr = randomAddr(rng);
            simu(shuffler) = shfl;
            simu(addr) = incAddr;

            bool doWrite = (contents[incAddr] & 1) ^ shfl;
            if (doWrite)
                contents[incAddr]++;

            if (lastWasWrite && lastAddr == incAddr)
                collisions++;

            co_await WaitClk(clock);

            unsigned actualReadContentBefore = simu(readOutputBefore).value();
            BOOST_TEST(actualReadContentBefore == expectedReadContentBefore, 
                "Read-port (before RMW) yields " << actualReadContentBefore << " but expected " << expectedReadContentBefore << ". Read-port address: " << read_addr << " RMW address: " << incAddr << " last clock cycle RMW addr: " << lastAddr);

            lastWasWrite = doWrite;
            lastAddr = incAddr;
        }

        BOOST_TEST(collisions > 1000, "Too few collisions to verify correct RMW behavior");

        stopTest();
	});

    design.visualize("before");
	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
    design.visualize("after");
	runTest(hlim::ClockRational(200000, 1) / clock.getClk()->getAbsoluteFrequency());
}
