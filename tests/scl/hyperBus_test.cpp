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

#include <gatery/scl/arch/colognechip/io.h>
#include <gatery/scl/arch/colognechip/SpecialFunction.h>
#include <gatery/scl/memory/HyperRAM.h>
#include <gatery/scl/riscv/EmbeddedSystemBuilder.h>

using namespace boost::unit_test;
using namespace gtry;
using namespace gtry::scl;

class HyperBusMasterTest : public BoostUnitTestSimulationFixture
{
public:

	void driveIdle()
	{
		simu(phy.csn) = 1;
		simu(phy.rwds.f).stopDriving();
		simu(phy.dq.f).stopDriving();
	}

	enum class CommandType
	{
		write = 0,
		read = 1,
	};

	enum class AddressSpace
	{
		memory = 0,
		reg = 1,
	};

	enum class BurstType
	{
		wrap = 0,
		linear = 1,
	};

	SimFunction<bool> driveCommand(const Clock& clk, uint32_t wordAddress, CommandType type, BurstType burst = BurstType::linear, AddressSpace space = AddressSpace::memory)
	{
		simu(phy.csn) = 0;

		uint64_t command = 
			uint64_t(type) << 47 | 
			uint64_t(space) << 46 | 
			uint64_t(burst) << 45 | 
			uint64_t(wordAddress >> 3) << 16 | 
			(wordAddress & 0b111);

		bool extraLatency;
		for (size_t i = 0; i < 6; ++i)
		{
			extraLatency = (simu(phy.rwds.f) == 3);
			simu(phy.dq.f) = (command >> (5 - i) * 8) & 0xFF;
			co_await OnClk(clk);
		}
		simu(phy.dq.f).stopDriving();
		co_return extraLatency;
	}

	SimProcess waitLatency(const Clock& clk, bool extraLatency)
	{
		size_t l = latency;
		if (extraLatency)
			l *= 2;
		l -= 1; // 1 cycle is part of command sequence
		l *= 2; // we are in ddr clock mode

		for (size_t i = 0; i < l; ++i)
			co_await OnClk(clk);
	}

	SimProcess executeWrite(const Clock& clk, uint32_t byteAddress, uint64_t data, size_t byteSize)
	{
		HCL_ASSERT(byteSize % 2 == 0);
		HCL_ASSERT(byteAddress % 2 == 0);

		bool extraLatecy = co_await driveCommand(clk, 0, CommandType::write);
		co_await waitLatency(clk, extraLatecy);

		for (size_t i = 0; i < byteSize; ++i)
		{
			simu(phy.rwds.f) = '1';
			simu(phy.dq.f) = (data >> i * 8) & 0xFF;
			co_await OnClk(clk);
		}

		driveIdle();
		co_await OnClk(clk);
		co_await OnClk(clk);
	}

	SimFunction<uint64_t> executeRead(const Clock& clk, uint32_t byteAddress, size_t byteSize)
	{
		HCL_ASSERT(byteAddress % 2 == 0);

		bool extraLatecy = co_await driveCommand(clk, 0, CommandType::read);
		co_await waitLatency(clk, extraLatecy);

		co_await OnClk(clk); // protocol
		co_await OnClk(clk); // tCKD

		uint64_t ret = 0;
		for (size_t i = 0; i < byteSize; ++i)
		{
			BOOST_TEST(simu(phy.rwds.f) == (i % 2 == 0));

			ret |= simu(phy.dq.f) << i * 8;
			co_await OnClk(clk);
		}

		driveIdle();
		for(size_t i = 0; i < 2 + byteSize % 2; ++i)
			co_await OnClk(clk);
		co_return ret;
	}

protected:
	scl::HyperBusPhy phy = { .csn = 1_b };
	size_t latency = 7;
};

BOOST_FIXTURE_TEST_CASE(HyperRAMreadTest, HyperBusMasterTest)
{
	Clock clock({ .absoluteFrequency = 100_mhz, .triggerEvent = ClockConfig::TriggerEvent::RISING_AND_FALLING });
	ClockScope clkScp(clock);

	pinIn(phy, "phy");
	latency = 3;
	scl::hyperRAMSimulationModel(phy, { .latencyDefault = latency });

	addSimulationProcess([&, this]()->SimProcess {
		driveIdle();
		//co_await OnClk(clock);
		co_await OnClk(clock);

		uint64_t ref_data = 0x0706050403020100ull;
		co_await executeWrite(clock, 0, ref_data, 8);

		for(size_t i = 0; i < 2; ++i)
		{
			uint64_t data = co_await executeRead(clock, 0, 8);
			BOOST_TEST(data == ref_data);
		}
		stopTest();
	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout(1us));
}

BOOST_FIXTURE_TEST_CASE(HyperRAM_soc_test_export, BoostUnitTestSimulationFixture)
{
	const std::string& testName = boost::unit_test::framework::current_test_case().p_name.get();

	Clock clk12{ ClockConfig{
		.absoluteFrequency = 12_mhz,
		.name = "CLK12M",
		.resetType = hlim::RegisterAttributes::ResetType::NONE
	} };

	Clock clock = scl::arch::colognechip::pll(clk12, { 
		.absoluteFrequency = 24_mhz, 
		.resetType = ClockConfig::ResetType::NONE,
	//	.resetType = ClockConfig::ResetType::SYNCHRONOUS,
	});
	ClockScope clkScp(clock);

	Reverse<scl::HyperBusPhy> phy = scl::gateMateHyperBusPhy();
	Reverse<scl::HyperBusController> controller = scl::hyperBusControllerLowLatency(move(phy));

	UInt latency = 3_b;
	Bit registerAccess;

	scl::TileLinkU link16 = scl::hyperBusTileLink(move(controller), { .burstLimit = 2, .latency = latency, .registerAccess = registerAccess });
	scl::TileLinkU link32 = scl::tileLinkDoubleWidth(move(link16));

	PackedMemoryMap mmap("dev");
	uartDevice(mmap, "uart");
	mapIn(mmap, latency, "latency");
	mapIn(mmap, registerAccess, "registerAccess");
	TileLinkU mmapBus = tileLinkRegDecouple(*toTileLinkUL(mmap, 32_b));

	TileLinkDemux<TileLinkU> dataBus;
	TileLinkU dataFetch = tileLinkInit<TileLinkU>(31_b, 32_b);
	dataBus.attachSource(dataFetch);
	dataBus.attachSink(move(mmapBus), 0x1000'0000);
	dataBus.attachSink(move((TileLinkU&)link32), 0x2000'0000);
	dataBus.generate();

	formatCppHeader("export/" + testName + "/firmware/gen_mmap.h", *mmap.getTree().physicalDescription, 0x1000'0000);
	riscv::embeddedRV32(move(dataFetch), "firmware.elf", {
		.makeDir = "export/" + testName + "/firmware",
		.stackByteSize = 4096,
		.dataBusOffset = 0x8000,
	});

	addSimulationProcess([&]()->SimProcess {
		co_return;
	});

	design.postprocess();

	std::filesystem::path cwd = std::filesystem::path{ "export" } / testName;
	std::future<int> flow;
	vhdl::VHDLExport vhdl(cwd / "top.vhd");
	if (std::filesystem::is_directory(cwd))
	{
		//vhdl.addTestbenchRecorder(*m_simulator, "tb", true);
		vhdl(design.getCircuit());

		flow = std::async([=]() {
			return std::system(("make -C " + cwd.string()).c_str());
			});
	}

	//BOOST_TEST(!runHitsTimeout(2ms));
}

