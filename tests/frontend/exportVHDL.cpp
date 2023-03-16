/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2022 Michael Offel, Andreas Ley

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


#include <gatery/frontend/GHDLTestFixture.h>

#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>


using namespace boost::unit_test;

boost::test_tools::assertion_result canExport(boost::unit_test::test_unit_id)
{
	return gtry::GHDLGlobalFixture::hasGHDL();
}


BOOST_AUTO_TEST_SUITE(Export, * precondition(canExport))

BOOST_FIXTURE_TEST_CASE(unconnectedInputs, gtry::GHDLTestFixture)
{
	using namespace gtry;

    {
        UInt undefined = 3_b;
        Bit comparison = undefined == 0;
        pinOut(comparison).setName("out");
    }


	testCompilation();
}


BOOST_FIXTURE_TEST_CASE(loopyInputs, gtry::GHDLTestFixture)
{
	using namespace gtry;

    {
        UInt undefined = 3_b;
		HCL_NAMED(undefined); // signal node creates a loop
        Bit comparison = undefined == 0;
        pinOut(comparison).setName("out");
    }


	testCompilation();
}


BOOST_FIXTURE_TEST_CASE(literalComparison, gtry::GHDLTestFixture)
{
	using namespace gtry;

    {
        UInt undefined = "3bXXX";
        Bit comparison = undefined == 0;
        pinOut(comparison).setName("out");
    }

	testCompilation();
}



BOOST_FIXTURE_TEST_CASE(readOutput, gtry::GHDLTestFixture)
{
	using namespace gtry;

    {
		Bit input = pinIn().setName("input");
		Bit output;
		Bit output2;
		{
			Area area("mainArea", true);

			{
				Area area("producingSubArea", true);
				output = input ^ '1';
			}
			{
				Area area("consumingSubArea", true);
				output2 = output ^ '1';
			}
		}

        pinOut(output).setName("out");
        pinOut(output2).setName("out2");
    }

	testCompilation();
}




BOOST_FIXTURE_TEST_CASE(readOutputLocal, gtry::GHDLTestFixture)
{
	using namespace gtry;

    {
		Bit input = pinIn().setName("input");
		Bit output;
		Bit output2;
		{
			Area area("mainArea", true);

			output = input ^ '1';
			output2 = output ^ '1';
		}

        pinOut(output).setName("out");
        pinOut(output2).setName("out2");
    }

	testCompilation();
	//DesignScope::visualize("test_outputLocal");
}


BOOST_FIXTURE_TEST_CASE(muxUndefined, gtry::GHDLTestFixture)
{
	using namespace gtry;

    {
		Bit input1 = pinIn().setName("input1");
		Bit input2 = pinIn().setName("input2");
		Bit output;

		Bit undefined = 'x';
		HCL_NAMED(undefined);

		IF (undefined)
			output = input1;
		ELSE
			output = input2;

        pinOut(output).setName("out");
    }

	testCompilation();
	//DesignScope::visualize("test_muxUndefined");
}


BOOST_FIXTURE_TEST_CASE(keepNamedSignalOrphans, gtry::GHDLTestFixture)
{
	using namespace gtry;

    {
		Bit input1 = pinIn().setName("input1");
		Bit input2 = pinIn().setName("input2");
		Bit output;

		input2 ^= '1';

		output = input1 ^ input2;

		Bit input1_ = input1;
		setName(input1_, "orphaned_1");
		Bit output_ = output;
		setName(output_, "orphaned_2");

        pinOut(output).setName("out");
    }

	//DesignScope::visualize("test_keepNamedSignalOrphans_before");
	testCompilation();
	//DesignScope::visualize("test_keepNamedSignalOrphans");
	BOOST_TEST(exportContains(std::regex{"orphaned_1"}));
	BOOST_TEST(exportContains(std::regex{"orphaned_2"}));
}



struct Test_GenericMemoryExport : public gtry::GHDLTestFixture
{
	size_t latency = 16;
	gtry::BitWidth width = gtry::BitWidth(8);
	size_t depth = 32;

	gtry::Clock::ResetType registerResetType = gtry::Clock::ResetType::SYNCHRONOUS;
	gtry::Clock::ResetType memoryResetType = gtry::Clock::ResetType::SYNCHRONOUS;
	std::optional<size_t> latencyRegResetValue;
	
	void execute() {
		using namespace gtry;
		using namespace gtry::sim;
		using namespace gtry::utils;

		Clock clock({ .absoluteFrequency = 100'000'000, .resetType = registerResetType, .memoryResetType = memoryResetType });
		ClockScope clkScp(clock);

		Memory<UInt> mem(depth, width);
		mem.setType(MemType::DONT_CARE, latency);
		mem.noConflicts();

		UInt addr = pinIn(8_b).setName("addr");
		UInt output = mem[addr];
		for ([[maybe_unused]] auto i : Range(latency)) {
			if (latencyRegResetValue)
				output = reg(output, *latencyRegResetValue, {.allowRetimingBackward=true});
			else
				output = reg(output, {.allowRetimingBackward=true});
		}
		pinOut(output).setName("output");
		UInt input = pinIn(width).setName("input");
		Bit wrEn = pinIn().setName("wrEn");
		IF (wrEn)
			mem[addr] = input;

		addSimulationProcess([=,this]()->SimProcess {

			std::vector<std::optional<size_t>> contents;
			contents.resize(depth);
			std::mt19937 rng{ 18055 };

			simu(wrEn) = '0';
			co_await OnClk(clock);

			size_t testsInFlight = 0;

			for ([[maybe_unused]] auto i : Range(100)) {
				if (rng() & 1) {
					size_t idx = rng() % depth;
					size_t newValue = rng() % (1 << width.value);
					auto oldValue = contents[idx];

					simu(wrEn) = '1';
					simu(addr) = idx;
					simu(input) = newValue;
					contents[idx] = newValue;

					fork([=,this, &testsInFlight]()->SimProcess {
						testsInFlight++;
						for ([[maybe_unused]] auto i : Range(latency+1))
							co_await OnClk(clock);
						auto v = simu(output);
						if (!oldValue)
							BOOST_TEST(!v.allDefined());
						else
							BOOST_TEST(v == *oldValue);
						testsInFlight--;
					});
				} else {
					simu(wrEn) = '0';
				}
				co_await OnClk(clock);
			}

			while (testsInFlight)
				co_await OnClk(clock);

			stopTest();
		});

		design.postprocess();
		runTest(hlim::ClockRational(200, 1) / clock.getClk()->absoluteFrequency());	
	}
};


BOOST_FIXTURE_TEST_CASE(GenericMemoryExport_Sync_1_no_reset, Test_GenericMemoryExport)
{
	using namespace gtry;

	registerResetType = memoryResetType = gtry::Clock::ResetType::SYNCHRONOUS;	
	latency = 1;

	execute();

	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
}

BOOST_FIXTURE_TEST_CASE(GenericMemoryExport_Sync_1_w_reset, Test_GenericMemoryExport)
{
	using namespace gtry;

	registerResetType = memoryResetType = gtry::Clock::ResetType::SYNCHRONOUS;	
	latency = 1;
	latencyRegResetValue = 0;

	execute();

	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
	BOOST_TEST(exportContains(std::regex{"PROCESS\\(sysclk\\)"}));
	BOOST_TEST(exportContains(std::regex{"IF \\(reset = '1'\\) THEN"}));
}

BOOST_FIXTURE_TEST_CASE(GenericMemoryExport_Async_1_w_reset, Test_GenericMemoryExport)
{
	using namespace gtry;

	registerResetType = memoryResetType = gtry::Clock::ResetType::ASYNCHRONOUS;	
	latency = 1;
	latencyRegResetValue = 0;

	execute();

	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
	BOOST_TEST(exportContains(std::regex{"PROCESS\\(sysclk\\)"}));
	BOOST_TEST(exportContains(std::regex{"PROCESS\\(sysclk, reset\\)"}));
	BOOST_TEST(exportContains(std::regex{"IF \\(reset = '1'\\) THEN"}));
}

BOOST_FIXTURE_TEST_CASE(GenericMemoryExport_Sync_16_no_reset, Test_GenericMemoryExport)
{
	using namespace gtry;

	registerResetType = memoryResetType = gtry::Clock::ResetType::SYNCHRONOUS;	
	latency = 16;

	execute();

	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
}

BOOST_FIXTURE_TEST_CASE(GenericMemoryExport_Sync_16_w_reset, Test_GenericMemoryExport)
{
	using namespace gtry;

	registerResetType = memoryResetType = gtry::Clock::ResetType::SYNCHRONOUS;	
	latency = 16;
	latencyRegResetValue = 0;

	execute();

	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
	BOOST_TEST(exportContains(std::regex{"PROCESS\\(sysclk\\)"}));
	BOOST_TEST(exportContains(std::regex{"IF \\(reset = '1'\\) THEN"}));
}

BOOST_FIXTURE_TEST_CASE(GenericMemoryExport_Async_16_w_reset, Test_GenericMemoryExport)
{
	using namespace gtry;

	registerResetType = memoryResetType = gtry::Clock::ResetType::ASYNCHRONOUS;	
	latency = 16;
	latencyRegResetValue = 0;

	execute();

	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
	BOOST_TEST(exportContains(std::regex{"PROCESS\\(sysclk\\)"}));
	BOOST_TEST(exportContains(std::regex{"PROCESS\\(sysclk, reset\\)"}));
	BOOST_TEST(exportContains(std::regex{"IF \\(reset = '1'\\) THEN"}));
}






BOOST_FIXTURE_TEST_CASE(unusedNamedSignalVanishes, gtry::GHDLTestFixture)
{
	using namespace gtry;

    {
		Bit input1 = pinIn().setName("input1");
		Bit input2 = pinIn().setName("input2");
		Bit input3 = pinIn().setName("input3");
		Bit output;

		input2 ^= '1';

		output = input1 ^ input2;
        pinOut(output).setName("out");



		Bit unused = input1 ^ input3;
		HCL_NAMED(unused);
    }

	testCompilation();
	BOOST_TEST(!exportContains(std::regex{"unused"}));
}

BOOST_FIXTURE_TEST_CASE(unusedTappedSignalRemains, gtry::GHDLTestFixture)
{
	using namespace gtry;

    {
		Bit input1 = pinIn().setName("input1");
		Bit input2 = pinIn().setName("input2");
		Bit input3 = pinIn().setName("input3");
		Bit output;

		input2 ^= '1';

		output = input1 ^ input2;
        pinOut(output).setName("out");

		Bit unused = input1 ^ input3;
		HCL_NAMED(unused);
		tap(unused);
    }

	testCompilation();

	BOOST_TEST(exportContains(std::regex{"s_unused <= "}));
}


BOOST_FIXTURE_TEST_CASE(signalTapForcesVariableToSignal, gtry::GHDLTestFixture)
{
	using namespace gtry;

    {
		Bit input1 = pinIn().setName("input1");
		Bit input2 = pinIn().setName("input2");
		Bit input3 = pinIn().setName("input3");
		Bit output;

		input2 ^= '1';

		Bit intermediate = input1 ^ input2;
		HCL_NAMED(intermediate);
		tap(intermediate);

		output = intermediate ^ input3;
        pinOut(output).setName("out");
    }

	testCompilation();

	BOOST_TEST(exportContains(std::regex{"SIGNAL s_intermediate : STD_LOGIC;"}));
	BOOST_TEST(exportContains(std::regex{"<= \\(s_intermediate xor input3\\)"}));
}


BOOST_AUTO_TEST_SUITE_END()
