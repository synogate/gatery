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

#include <thread>


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

BOOST_FIXTURE_TEST_CASE(zeroBitEqualsZeroBits, gtry::GHDLTestFixture)
{
	using namespace gtry;

    {
        UInt a = pinIn(0_b);
        UInt b = 0_b;
        Bit comparison = a == b;
        pinOut(comparison).setName("out");
	}
    {
        UInt a = 0_b;
        UInt b = 0_b;
        Bit comparison = a == b;
        pinOut(comparison).setName("out2");
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







BOOST_FIXTURE_TEST_CASE(GenericMemoryExport_Depth1_Sync_1_no_reset, Test_GenericMemoryExport)
{
	using namespace gtry;

	registerResetType = memoryResetType = gtry::Clock::ResetType::SYNCHRONOUS;	
	latency = 1;
	depth = 1;

	execute();

	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
}

BOOST_FIXTURE_TEST_CASE(GenericMemoryExport_Depth1_Sync_1_w_reset, Test_GenericMemoryExport)
{
	using namespace gtry;

	registerResetType = memoryResetType = gtry::Clock::ResetType::SYNCHRONOUS;	
	latency = 1;
	latencyRegResetValue = 0;
	depth = 1;

	execute();

	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
	BOOST_TEST(exportContains(std::regex{"PROCESS\\(sysclk\\)"}));
	BOOST_TEST(exportContains(std::regex{"IF \\(reset = '1'\\) THEN"}));
}

BOOST_FIXTURE_TEST_CASE(GenericMemoryExport_Depth1_Async_1_w_reset, Test_GenericMemoryExport)
{
	using namespace gtry;

	registerResetType = memoryResetType = gtry::Clock::ResetType::ASYNCHRONOUS;	
	latency = 1;
	latencyRegResetValue = 0;
	depth = 1;

	execute();

	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
	BOOST_TEST(exportContains(std::regex{"PROCESS\\(sysclk\\)"}));
	BOOST_TEST(exportContains(std::regex{"PROCESS\\(sysclk, reset\\)"}));
	BOOST_TEST(exportContains(std::regex{"IF \\(reset = '1'\\) THEN"}));
}

BOOST_FIXTURE_TEST_CASE(GenericMemoryExport_Depth1_Sync_16_no_reset, Test_GenericMemoryExport)
{
	using namespace gtry;

	registerResetType = memoryResetType = gtry::Clock::ResetType::SYNCHRONOUS;	
	latency = 16;
	depth = 1;

	execute();

	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
}

BOOST_FIXTURE_TEST_CASE(GenericMemoryExport_Depth1_Sync_16_w_reset, Test_GenericMemoryExport)
{
	using namespace gtry;

	registerResetType = memoryResetType = gtry::Clock::ResetType::SYNCHRONOUS;	
	latency = 16;
	latencyRegResetValue = 0;
	depth = 1;

	execute();

	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
	BOOST_TEST(exportContains(std::regex{"PROCESS\\(sysclk\\)"}));
	BOOST_TEST(exportContains(std::regex{"IF \\(reset = '1'\\) THEN"}));
}

BOOST_FIXTURE_TEST_CASE(GenericMemoryExport_Depth1_Async_16_w_reset, Test_GenericMemoryExport)
{
	using namespace gtry;

	registerResetType = memoryResetType = gtry::Clock::ResetType::ASYNCHRONOUS;	
	latency = 16;
	latencyRegResetValue = 0;
	depth = 1;

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



BOOST_FIXTURE_TEST_CASE(exportOverrideConstant, gtry::GHDLTestFixture)
{
	using namespace gtry;

    {
		Bit input1 = pinIn().setName("input1");
		Bit input2 = pinIn().setName("input2");

		Bit input2C = '1';
		input2C.exportOverride(input2);

		Bit output = input1 | input2C;
        pinOut(output).setName("output");
    }

	testCompilation();

	BOOST_TEST(exportContains(std::regex{"output <= \\(input1 or input2\\)"}));
}


BOOST_FIXTURE_TEST_CASE(signalNamesDontPropagateIntoSubEntities, gtry::GHDLTestFixture)
{
	using namespace gtry;

    {
		Bit input1 = pinIn().setName("input1");
		Bit input2 = pinIn().setName("input2");

		HCL_NAMED(input1);
		HCL_NAMED(input2);

		Bit output;
		{
			Area subArea("sub", true);

			output = input1 ^ input2;
		}

        pinOut(output).setName("output");
    }

	design.visualize("before");
	testCompilation();
	design.visualize("after");

	BOOST_TEST(!exportContains(std::regex{" <= \\(in_input1 xor in_input2\\);"}));
}

BOOST_FIXTURE_TEST_CASE(signalNamesDontPropagateIntoSubEntitiesMultiLevel, gtry::GHDLTestFixture)
{
	using namespace gtry;

    {
		Bit input1 = pinIn().setName("input1");
		Bit input2 = pinIn().setName("input2");

		HCL_NAMED(input1);
		HCL_NAMED(input2);

		Bit output;
		{
			Area subArea1("sub1", true);
			Area subArea2("sub2", true);
			Area subArea3("sub3", true);
			setName(input1, "I1");
			setName(input2, "I2");

			output = input1 ^ input2;
		}

        pinOut(output).setName("output");
    }

	testCompilation();

	BOOST_TEST(!exportContains(std::regex{" <= \\(in_input1 xor in_input2\\);"}));
}



BOOST_FIXTURE_TEST_CASE(noRewriteWithoutChange, gtry::GHDLTestFixture)
{
	using namespace gtry;

	Bit in = pinIn().setName("in");
	pinOut(in).setName("out");

    {
		vhdl::VHDLExport vhdl("design.vhdl", false);
		vhdl.writeProjectFile("projectFile.txt");
		vhdl.writeStandAloneProjectFile("standAloneProjectFile.txt");		
		vhdl.writeConstraintsFile("constraints.txt");
		vhdl.writeClocksFile("clocks.txt");
		vhdl(design.getCircuit());
    }

	auto writeTime1 = std::filesystem::last_write_time("design.vhdl");

	using namespace std::chrono;
	std::this_thread::sleep_for(1s);

    {
		vhdl::VHDLExport vhdl("design.vhdl", false);
		vhdl.writeProjectFile("projectFile.txt");
		vhdl.writeStandAloneProjectFile("standAloneProjectFile.txt");		
		vhdl.writeConstraintsFile("constraints.txt");
		vhdl.writeClocksFile("clocks.txt");
		vhdl(design.getCircuit());
    }

	auto writeTime2 = std::filesystem::last_write_time("design.vhdl");

	bool fileWasRewritten = writeTime2 != writeTime1;
	BOOST_TEST(!fileWasRewritten);
}



BOOST_FIXTURE_TEST_CASE(oneFilePerPartition, gtry::GHDLTestFixture)
{
	using namespace gtry;

	Bit in = pinIn().setName("in");

	{
		Area area1("area1", true);
		area1.setPartition(true);
		Area area2("area2", true);
		in ^= pinIn().setName("in2");
	}

	{
		Area area3("area3", true);
		area3.setPartition(true);
		in ^= pinIn().setName("in3");
	}

	pinOut(in).setName("out");

    {
		vhdl::VHDLExport vhdl("design.vhdl", false);
		vhdl.outputMode(vhdl::OutputMode::FILE_PER_PARTITION);
		vhdl.writeProjectFile("projectFile.txt");
		vhdl.writeStandAloneProjectFile("standAloneProjectFile.txt");		
		vhdl.writeConstraintsFile("constraints.txt");
		vhdl.writeClocksFile("clocks.txt");
		vhdl(design.getCircuit());
    }

	BOOST_TEST(std::filesystem::exists("area1.vhd"));
	BOOST_TEST(!std::filesystem::exists("area2.vhd"));
	BOOST_TEST(std::filesystem::exists("area3.vhd"));
}

BOOST_FIXTURE_TEST_CASE(oneFilePerPartitionWithComponentInstantiation, gtry::GHDLTestFixture)
{
	using namespace gtry;

	Bit in = pinIn().setName("in");

	{
		Area area1("area1", true);
		area1.setPartition(true);
		area1.useComponentInstantiation(true);
		Area area2("area2", true);
		in ^= pinIn().setName("in2");
	}

	{
		Area area3("area3", true);
		area3.setPartition(true);
		in ^= pinIn().setName("in3");
	}

	pinOut(in).setName("out");


	vhdlOutputMode = vhdl::OutputMode::FILE_PER_PARTITION;
	
	testCompilation();

	BOOST_TEST(exportContains(std::regex{"COMPONENT"}));
}


BOOST_FIXTURE_TEST_CASE(oneFilePerPartitionWithComponentInstantiationWithAttributes, gtry::GHDLTestFixture)
{
	using namespace gtry;

	Bit in = pinIn().setName("in");

	{
		Area area1("area1", true);
		area1.setPartition(true);
		area1.useComponentInstantiation(true);
		area1.groupAttributes().userDefinedVendorAttributes["all"]["black_box"] = {.type = "string", .value = "\"yes\""};
		Area area2("area2", true);
		in ^= pinIn().setName("in2");
	}

	{
		Area area3("area3", true);
		area3.setPartition(true);
		in ^= pinIn().setName("in3");
	}

	pinOut(in).setName("out");


	vhdlOutputMode = vhdl::OutputMode::FILE_PER_PARTITION;
	
	testCompilation();

	BOOST_TEST(exportContains(std::regex{"ATTRIBUTE black_box OF .* : COMPONENT IS \"yes\";"}));
}


BOOST_FIXTURE_TEST_CASE(signalAttributes, gtry::GHDLTestFixture)
{
	using namespace gtry;

	{
		Bit input1 = pinIn().setName("input1");
		Bit input2 = pinIn().setName("input2");

		Bit input = input1 ^ input2;

		setName(input, "input_xor");

		SignalAttributes attrib;
		attrib.userDefinedVendorAttributes["all"]["something"] = {.type = "string", .value = "\"maybe\""};
		attribute(input, attrib);

		pinOut(input).setName("output");
	}

	{
		Bit input = pinIn().setName("inputSingle");

		setName(input, "input_single");

		SignalAttributes attrib;
		attrib.userDefinedVendorAttributes["all"]["something"] = {.type = "string", .value = "\"maybe_single\""};
		attribute(input, attrib);

		pinOut(input).setName("outputSingle");
	}	
	{
		Bit input = '0';

		setName(input, "input_const");

		SignalAttributes attrib;
		attrib.userDefinedVendorAttributes["all"]["something"] = {.type = "string", .value = "\"maybe_const\""};
		attribute(input, attrib);

		pinOut(input).setName("outputConst");
	}	

	{
		Bit input1 = pinIn().setName("input1_no_signal");
		Bit input2 = pinIn().setName("input2_no_signal");

		Bit input = input1 ^ input2;


		SignalAttributes attrib;
		attrib.userDefinedVendorAttributes["all"]["something"] = {.type = "string", .value = "\"maybe_no_signal\""};
		attribute(input, attrib);

		pinOut(input).setName("output_no_signal");
	}

	{
		Bit input = pinIn().setName("inputSingle_no_signal");

		SignalAttributes attrib;
		attrib.userDefinedVendorAttributes["all"]["something"] = {.type = "string", .value = "\"maybe_single_no_signal\""};
		attribute(input, attrib);

		pinOut(input).setName("outputSingle_no_signal");
	}	
	{
		Bit input = '0';

		SignalAttributes attrib;
		attrib.userDefinedVendorAttributes["all"]["something"] = {.type = "string", .value = "\"maybe_const_no_signal\""};
		attribute(input, attrib);

		pinOut(input).setName("outputConst_no_signal");
	}
//	design.visualize("before");
	testCompilation();
//	design.visualize("after");

	BOOST_TEST(exportContains(std::regex{"maybe"}));
	BOOST_TEST(exportContains(std::regex{"maybe_single"}));
	BOOST_TEST(exportContains(std::regex{"maybe_const"}));
/*
	BOOST_TEST(exportContains(std::regex{"maybe_no_signal"}));
	BOOST_TEST(exportContains(std::regex{"maybe_single_no_signal"}));
	BOOST_TEST(exportContains(std::regex{"maybe_const_no_signal"}));
*/
}



BOOST_FIXTURE_TEST_CASE(simulationOnlyPins, gtry::GHDLTestFixture)
{
	using namespace gtry;

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);
	{
		Bit input1 = pinIn().setName("input1");
		Bit input2 = pinIn().setName("input2");

		Bit output = input1 ^ input2;
		pinOut(output).setName("output");

		Area subarea("subArea", true);

		Bit simulationOnlyInput = pinIn({.simulationOnlyPin = true}).setName("simulationOnlyInput");
		Bit simProcessDriver = input1 & input2 & simulationOnlyInput;

		pinOut(simProcessDriver, {.simulationOnlyPin = true}).setName("simulationOnlyOutput");


		Bit inHelper = simulationOnlyInput;
		inHelper.exportOverride(input2);
		Bit output2 = input1 ^ inHelper;
		pinOut(output2).setName("output2");


		addSimulationProcess([=,this]()->SimProcess {

			// Just read and write some stuff including the simulation pins to force everything into the recorded test bench

			co_await OnClk(clock);

			simu(input1) = '1';
			simu(input2) = '0';
			simu(simulationOnlyInput) = '0';

			co_await OnClk(clock);

			BOOST_TEST(simu(output) == '1');
			BOOST_TEST(simu(output2) == '1');
			BOOST_TEST(simu(simProcessDriver) == '0');

			stopTest();
		});

	}

	runTest(hlim::ClockRational(200, 1) / clock.getClk()->absoluteFrequency());	

	BOOST_TEST(exportContains(std::regex{"input1"}));
	BOOST_TEST(exportContains(std::regex{"input2"}));
	BOOST_TEST(exportContains(std::regex{"output"}));
	BOOST_TEST(exportContains(std::regex{"output2"}));

	BOOST_TEST(!exportContains(std::regex{"simulationOnlyInput"}));
	BOOST_TEST(!exportContains(std::regex{"simulationOnlyOutput"}));
}


BOOST_FIXTURE_TEST_CASE(tryExportSimulationOnlyPins, gtry::GHDLTestFixture)
{
	using namespace gtry;


	{
		Bit input1 = pinIn().setName("input1");
		Bit input2 = pinIn({.simulationOnlyPin = true}).setName("input2");

		Bit output = input1 ^ input2;
		pinOut(output).setName("output");
	}


	BOOST_CHECK_THROW(testCompilation(), gtry::utils::DesignError);
}


BOOST_FIXTURE_TEST_CASE(testReadingInputPins, gtry::GHDLTestFixture)
{
	using namespace gtry;

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);
	{
		Bit input1 = pinIn().setName("input1");
		Bit input2 = pinIn().setName("input2");

		Bit output = input1 ^ input2;
		pinOut(output).setName("output");

		addSimulationProcess([=,this]()->SimProcess {

			co_await OnClk(clock);

			simu(input1) = '1';
			simu(input2) = '0';

			co_await OnClk(clock);

			BOOST_TEST(simu(output) == '1');
			BOOST_TEST(simu(input2) == '0');

			stopTest();
		});

	}

	runTest(hlim::ClockRational(200, 1) / clock.getClk()->absoluteFrequency());	
}


BOOST_FIXTURE_TEST_CASE(constantRewireCorrectlyFolds, gtry::GHDLTestFixture)
{
	using namespace gtry;

	UInt mask = 4_b;
	{
		Bit enable = pinIn().setName("input");
		pinOut(enable & mask[0]).setName("output");
	}
	mask = oext(0); // must be here to place a referenced signal node between the unused constant node and the oext-rewire, preventing constant folding into the rewire
	
	testCompilation();
	BOOST_TEST(exportContains(std::regex{"output <= input"}));
}





BOOST_FIXTURE_TEST_CASE(foldBinaryMuxesToCase, gtry::GHDLTestFixture)
{
	using namespace gtry;

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	std::mt19937 rng(267);
	std::uniform_int_distribution<unsigned> random(0, 1000);
	std::vector<size_t> table(10);
	for (auto &e : table) e = random(rng);


	UInt output = 32_b;
	output = dontCare(output);

	UInt selector = pinIn(4_b).setName("selector");
	for (size_t i = 0; i < table.size(); i++)
		IF (selector == i)
			output = table[i];

	pinOut(output).setName("output");


	addSimulationProcess([=,this]()->SimProcess {

		co_await OnClk(clock);

		for (size_t i = 0; i < 16; i++) {
			simu(selector) = i;
			co_await OnClk(clock);

			if (i < table.size())
				BOOST_TEST(simu(output) == table[i]);
			else
				BOOST_TEST(simu(output).defined() == 0);
		}

		stopTest();
	});

	design.getCircuit().shuffleNodes();
	
	//design.visualize("before");
	testCompilation();
	//design.visualize("after");
	BOOST_TEST(exportContains(std::regex{"CASE UNSIGNED\\(selector\\) IS"}));
}






BOOST_FIXTURE_TEST_CASE(ZeroBitDisconnect, gtry::GHDLTestFixture)
{
	using namespace gtry;

	Clock clock({ .absoluteFrequency = 10'000 });
	ClockScope clockScope(clock);

	Bit in = pinIn().setName("in");
	UInt muxSelect = 0_b;

	Bit out = mux(muxSelect, { in });

	pinOut(out).setName("out");

	addSimulationProcess([=, this]()->SimProcess {
		std::mt19937 rng = std::mt19937{ 1337 };
		
		for ([[maybe_unused]] auto i : gtry::utils::Range(100)) {
			bool v = rng() & 1;
			simu(in) = v;
			co_await AfterClk(clock);
			BOOST_TEST(simu(out) == v);
		}

		stopTest();
	});


	runTest({ 1,1 });
}


BOOST_FIXTURE_TEST_CASE(testCarryAddSingleLine, gtry::GHDLTestFixture)
{
	using namespace gtry;

	Clock clock({ .absoluteFrequency = 10'000 });
	ClockScope clockScope(clock);

	UInt a = pinIn(8_b).setName("a");
	UInt b = pinIn(8_b).setName("b");
	Bit c = pinIn().setName("carry");

	UInt out = addC(a, b, c);

	pinOut(out).setName("out");

	//design.visualize("before");
	testCompilation();
	//design.visualize("after");
	//BOOST_TEST(exportContains(std::regex{"CASE UNSIGNED\\(selector\\) IS"}));
}

BOOST_FIXTURE_TEST_CASE(tristateBit, gtry::GHDLTestFixture)
{
	using namespace gtry;

	Clock clock({ .absoluteFrequency = 10'000 });
	ClockScope clockScope(clock);
	
	UInt value = pinIn(10_b).setName("value");
	Bit enable = pinIn().setName("enable");
	UInt readback;
	{
		Area area("area", true);
		readback = tristatePin(value, enable).setName("tristatePin");
		readback = readback + 1;
	}
	pinOut(readback).setName("readback");

	//design.visualize("before");
	testCompilation();
	//design.visualize("after");
	BOOST_TEST(exportContains(std::regex{"\\(UNSIGNED\\(tristatePin\\) \\+ \"0000000001\"\\);"}));
}


BOOST_FIXTURE_TEST_CASE(tristateBitIntoSubEntity, gtry::GHDLTestFixture)
{
	using namespace gtry;

	Clock clock({ .absoluteFrequency = 10'000 });
	ClockScope clockScope(clock);
	
	UInt value = pinIn(10_b).setName("value");
	Bit enable = pinIn().setName("enable");
	UInt readback;
	readback = tristatePin(value, enable).setName("tristatePin");
	{
		Area area("area", true);
		readback = readback + 1;
	}
	pinOut(readback).setName("readback");

	//design.visualize("before");
	testCompilation();
	//design.visualize("after");
	BOOST_TEST(exportContains(std::regex{"s_tristatePin_2 <= UNSIGNED\\(tristatePin\\);"}));
}

BOOST_FIXTURE_TEST_CASE(tristateBitIntoParent, gtry::GHDLTestFixture)
{
	using namespace gtry;

	Clock clock({ .absoluteFrequency = 10'000 });
	ClockScope clockScope(clock);
	
	UInt value = pinIn(10_b).setName("value");
	Bit enable = pinIn().setName("enable");
	UInt readback;
	{
		Area area("area", true);
		readback = tristatePin(value, enable).setName("tristatePin");
	}
	readback = readback + 1;
	pinOut(readback).setName("readback");

	//design.visualize("before");
	testCompilation();
	//design.visualize("after");
	BOOST_TEST(exportContains(std::regex{"out_tristatePin <= UNSIGNED\\(tristatePin_2\\);"}));
}



BOOST_FIXTURE_TEST_CASE(IgnoreSimulationOnlyPins, gtry::GHDLTestFixture)
{
	using namespace gtry;

	Clock clock({ .absoluteFrequency = 10'000 });
	ClockScope clockScope(clock);

	Bit in = pinIn().setName("in");

	Bit out;

	{
		Area area("magic", true);

		ExternalModule dut("TestEntity", "work");
		{
			dut.in("in_bit", {.type = PinType::STD_LOGIC}) = in;
			out = dut.out("out_bit", {.type = PinType::STD_LOGIC});
		}

		auto simProcessIn = pinOut(in, { .simulationOnlyPin = true });
		auto simOverride = pinIn({ .simulationOnlyPin = true });

		out.simulationOverride((Bit)simOverride);

		DesignScope::get()->getCircuit().addSimulationProcess([=]() -> SimProcess {
			while (true) {
				ReadSignalList allInputs;
				simu(simOverride) = !(bool)simu(simProcessIn);

				co_await allInputs.anyInputChange();
			}
			co_return;
		});		
	}

	pinOut(out).setName("out");


	addCustomVHDL("TestEntity", R"Delim(

		LIBRARY ieee;
		USE ieee.std_logic_1164.ALL;
		USE ieee.numeric_std.all;

		ENTITY TestEntity IS 
			PORT(
				in_bit : in STD_LOGIC;
				out_bit : out STD_LOGIC
			);
		END TestEntity;

		ARCHITECTURE impl OF TestEntity IS 
		BEGIN
			do_stuff : PROCESS (all)
			begin
				out_bit <= not(in_bit);
			end PROCESS;
		END impl;

	)Delim");	

	addSimulationProcess([=, this]()->SimProcess {
		std::mt19937 rng = std::mt19937{ 1337 };
		
		for ([[maybe_unused]] auto i : gtry::utils::Range(100)) {
			bool v = rng() & 1;
			simu(in) = v;
			co_await AfterClk(clock);
			BOOST_TEST(simu(out) == !v);
		}

		stopTest();
	});


	runTest({ 1,1 });
}


BOOST_AUTO_TEST_SUITE_END()
