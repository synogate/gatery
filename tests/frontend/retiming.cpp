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

#include <gatery/frontend.h>
#include <gatery/hlim/RegisterRetiming.h>
#include <gatery/hlim/Subnet.h>
#include <gatery/hlim/coreNodes/Node_Signal.h>
#include <gatery/hlim/GraphTools.h>
#include <gatery/hlim/CNF.h>
#include <gatery/hlim/RegisterRetiming.h>

#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

#include <cstdint>

using namespace boost::unit_test;
using BoostUnitTestSimulationFixture = gtry::BoostUnitTestSimulationFixture;

void stripSignalNodes(gtry::hlim::Circuit &circuit)
{
	for (auto &node : circuit.getNodes()) {
		if (dynamic_cast<gtry::hlim::Node_Signal*>(node.get())) 
			node->bypassOutputToInput(0, 0);
	}
}

BOOST_FIXTURE_TEST_CASE(retiming_forward_counter_new, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::sim;
	using namespace gtry::utils;

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);




	UInt input = pinIn(32_b);

	UInt counter = 32_b;
	counter = counter + 1;
	counter = reg(counter, 0, {.allowRetimingForward=true});

	UInt output = counter | reg(input, 0, {.allowRetimingForward=true});

	stripSignalNodes(design.getCircuit());
	auto subnet = hlim::Subnet::all(design.getCircuit());
	design.getCircuit().optimizeSubnet(subnet);
	retimeForwardToOutput(design.getCircuit(), subnet, output.readPort(), {.ignoreRefs=true});

	auto outPin = pinOut(output);

	addSimulationProcess([=,this]()->SimProcess {
		simu(input) = 0;
		
		for (auto i : Range(32)) {
			BOOST_TEST(simu(outPin).value() == i);
			co_await AfterClk(clock);
		}

		stopTest();
	});

	//design.visualize("retiming_forward_counter_new_before");
	design.postprocess();

	//design.visualize("retiming_forward_counter_new");
	runTest(hlim::ClockRational(100, 1) / clock.getClk()->absoluteFrequency());
}

BOOST_FIXTURE_TEST_CASE(retiming_forward_counter_old, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::sim;
	using namespace gtry::utils;

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);




	UInt input = pinIn(32_b);
	UInt output = 32_b;

	UInt counter = 32_b;
	counter = reg(counter, 0, {.allowRetimingForward=true});
	counter = counter + 1;

	output = counter | reg(input, 0, {.allowRetimingForward=true});

	stripSignalNodes(design.getCircuit());
	auto subnet = hlim::Subnet::all(design.getCircuit());
	design.getCircuit().optimizeSubnet(subnet);
	retimeForwardToOutput(design.getCircuit(), subnet, output.readPort(), {.ignoreRefs=true});
	auto outPin = pinOut(output);

	addSimulationProcess([=,this]()->SimProcess {
		simu(input) = 0;
		
		for (auto i : Range(32)) {
			BOOST_TEST(simu(outPin).value() == i+1);
			co_await AfterClk(clock);
		}

		stopTest();
	});
	//design.visualize("retiming_counter_new_before");

	design.postprocess();

	//design.visualize("retiming_forward_counter_old");
	runTest(hlim::ClockRational(100, 1) / clock.getClk()->absoluteFrequency());
}

BOOST_FIXTURE_TEST_CASE(retiming_hint_simple, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::sim;
	using namespace gtry::utils;

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	UInt input = pinIn(32_b);

	PipeBalanceGroup pipeBalanceGroup;
	input = pipeBalanceGroup(input);


	UInt output = input;
	for ([[maybe_unused]] auto i : Range(3))
		output = pipestage(output);

	pinOut(output);

//design.visualize("before");
	design.postprocess();
//design.visualize("after");


	BOOST_TEST(pipeBalanceGroup.getNumPipeBalanceGroupStages() == 3);
}

BOOST_FIXTURE_TEST_CASE(retiming_hint_simple_reset, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::sim;
	using namespace gtry::utils;

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	UInt input = pinIn(32_b);

	PipeBalanceGroup pipeBalanceGroup;
	input = pipeBalanceGroup(input, 0);


	UInt output = input;
	for ([[maybe_unused]] auto i : Range(3))
		output = pipestage(output);

	pinOut(output);

//design.visualize("before");
	design.postprocess();
//design.visualize("after");


	BOOST_TEST(pipeBalanceGroup.getNumPipeBalanceGroupStages() == 3);
}

enum class TestEnum
{
	VAL1, VAL2
};

struct TestStruct
{
	gtry::Bit a;
	gtry::UInt b;
//	gtry::SInt c;
//	gtry::BVec d;
//	gtry::Enum<TestEnum> e;
};

BOOST_HANA_ADAPT_STRUCT(TestStruct, a, b);

BOOST_FIXTURE_TEST_CASE(retiming_hint_struct, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::sim;
	using namespace gtry::utils;

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	TestStruct s_in;
	s_in.a = pinIn();
	s_in.b = pinIn(32_b);
//	s_in.c = (SInt)(UInt)pinIn(32_b);
//	s_in.d = (BVec)(UInt)pinIn(32_b);
//	s_in.e = Enum<TestEnum>((UInt)pinIn(1_b));

	PipeBalanceGroup pipeBalanceGroup;
	TestStruct s_out = pipeBalanceGroup(s_in);


	for ([[maybe_unused]] auto i : Range(3))
		s_out = pipestage(s_out);

	pinOut(s_out.a);
	pinOut(s_out.b);

	addSimulationProcess([=,this]()->SimProcess {
		simu(s_in.a) = false;
		simu(s_in.b) = 42;
		
		for ([[maybe_unused]] auto i : Range(2)) {
			BOOST_TEST(!simu(s_out.a).defined());
			BOOST_TEST(!simu(s_out.b).defined());

			co_await AfterClk(clock);
		}
		co_await AfterClk(clock);

		BOOST_TEST(simu(s_out.a).defined());
		BOOST_TEST(simu(s_out.a).value() == false);
		BOOST_TEST(simu(s_out.b).defined());
		BOOST_TEST(simu(s_out.b).value() == 42);

		stopTest();
	});	

//design.visualize("before");
	design.postprocess();
//design.visualize("after");

	BOOST_TEST(pipeBalanceGroup.getNumPipeBalanceGroupStages() == 3);

	runTest(hlim::ClockRational(100, 1) / clock.getClk()->absoluteFrequency());	
}


BOOST_FIXTURE_TEST_CASE(retiming_hint_struct_reset, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::sim;
	using namespace gtry::utils;

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	TestStruct s_in;
	s_in.a = pinIn();
	s_in.b = pinIn(32_b);

	TestStruct r;
	r.a = '1';
	r.b = "32b0";

	PipeBalanceGroup pipeBalanceGroup;
	TestStruct s_out = pipeBalanceGroup(s_in, r);


	for ([[maybe_unused]] auto i : Range(3))
		s_out = pipestage(s_out);

	pinOut(s_out.a);
	pinOut(s_out.b);



	addSimulationProcess([=,this]()->SimProcess {
		simu(s_in.a) = false;
		simu(s_in.b) = 42;
		
		for ([[maybe_unused]] auto i : Range(3)) {
			BOOST_TEST(simu(s_out.a).defined());
			BOOST_TEST(simu(s_out.a).value() == true);
			BOOST_TEST(simu(s_out.b).defined());
			BOOST_TEST(simu(s_out.b).value() == 0);

			co_await AfterClk(clock);
		}

		BOOST_TEST(simu(s_out.a).defined());
		BOOST_TEST(simu(s_out.a).value() == false);
		BOOST_TEST(simu(s_out.b).defined());
		BOOST_TEST(simu(s_out.b).value() == 42);

		stopTest();
	});

//design.visualize("before");
	design.postprocess();
//design.visualize("after");

	BOOST_TEST(pipeBalanceGroup.getNumPipeBalanceGroupStages() == 3);

	runTest(hlim::ClockRational(100, 1) / clock.getClk()->absoluteFrequency());

}



BOOST_FIXTURE_TEST_CASE(retiming_hint_branching, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::sim;
	using namespace gtry::utils;

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	UInt input1 = pinIn(32_b);
	UInt input2 = pinIn(32_b);

	PipeBalanceGroup pipeBalanceGroup;
	auto a = pipeBalanceGroup(input1);
	auto b = pipeBalanceGroup(input2);

	b = pipestage(b);

	UInt output = a + b;
	output = pipestage(output);

	pinOut(output);


	addSimulationProcess([=,this]()->SimProcess {
		simu(input1) = 1337;
		simu(input2) = 42;
		
		for ([[maybe_unused]] auto i : Range(1)) {
			BOOST_TEST(!simu(output).defined());

			co_await AfterClk(clock);
		}
		co_await AfterClk(clock);

		BOOST_TEST(simu(output).defined());
		BOOST_TEST(simu(output).value() == 1337+42);

		stopTest();
	});

//design.visualize("before");
	design.postprocess();
//design.visualize("after");

	BOOST_TEST(pipeBalanceGroup.getNumPipeBalanceGroupStages() == 2);


	runTest(hlim::ClockRational(100, 1) / clock.getClk()->absoluteFrequency());		
}

BOOST_FIXTURE_TEST_CASE(retiming_pipeinputgroup, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::sim;
	using namespace gtry::utils;

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	UInt input1 = pinIn(32_b);
	UInt input2 = pinIn(32_b);
	UInt a = input1;
	UInt b = input2;

	pipeinputgroup(a, b);

	b = pipestage(b);

	UInt output = a + b;
	output = pipestage(output);

	pinOut(output);


	addSimulationProcess([=, this]()->SimProcess {
		simu(input1) = 1337;
		simu(input2) = 42;

		for([[maybe_unused]] auto i : Range(1)) {
			BOOST_TEST(!simu(output).defined());

			co_await AfterClk(clock);
		}
		co_await AfterClk(clock);

		BOOST_TEST(simu(output).defined());
		BOOST_TEST(simu(output).value() == 1337 + 42);

		stopTest();
	});

//design.visualize("before");
	design.postprocess();
//design.visualize("after");

	runTest(hlim::ClockRational(100, 1) / clock.getClk()->absoluteFrequency());
}


BOOST_FIXTURE_TEST_CASE(retiming_hint_branching_reset, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::sim;
	using namespace gtry::utils;

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	UInt input1 = pinIn(32_b);
	UInt input2 = pinIn(32_b);

	PipeBalanceGroup pipeBalanceGroup;
	auto a = pipeBalanceGroup(input1, 0);
	auto b = pipeBalanceGroup(input2, 1);

	b = pipestage(b);

	UInt output = a + b;
	output = pipestage(output);

	pinOut(output);


	addSimulationProcess([=,this]()->SimProcess {
		simu(input1) = 1337;
		simu(input2) = 42;
		
		for ([[maybe_unused]] auto i : Range(2)) {
			BOOST_TEST(simu(output).defined());
			BOOST_TEST(simu(output).value() == 0+1);

			co_await AfterClk(clock);
		}

		BOOST_TEST(simu(output).defined());
		BOOST_TEST(simu(output).value() == 1337+42);

		stopTest();
	});	

//design.visualize("before");
	design.postprocess();
//design.visualize("after");

	BOOST_TEST(pipeBalanceGroup.getNumPipeBalanceGroupStages() == 2);


	runTest(hlim::ClockRational(100, 1) / clock.getClk()->absoluteFrequency());		
}


BOOST_FIXTURE_TEST_CASE(retiming_hint_memory_rmw, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::sim;
	using namespace gtry::utils;

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	UInt addr = pinIn(4_b);
	UInt data = pinIn(32_b);
	Bit enable = pinIn();

	PipeBalanceGroup pipeBalanceGroup;
	addr = pipeBalanceGroup(addr);
	data = pipeBalanceGroup(data);
	enable = pipeBalanceGroup(enable);

	Memory<UInt> mem(16, 32_b);
	mem.setType(MemType::MEDIUM, 1);

	UInt rd = mem[addr];
	rd = pipestage(rd);

	IF (enable)
		mem[addr] = rd+data;

	pinOut(pipestage(rd));

//design.visualize("before");
	design.postprocess();
//design.visualize("after");

	BOOST_TEST(pipeBalanceGroup.getNumPipeBalanceGroupStages() == 2);
}


BOOST_FIXTURE_TEST_CASE(retiming_backward_fix_reset, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::sim;
	using namespace gtry::utils;

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	UInt addr = pinIn(4_b).setName("addr");
	UInt data = pinIn(32_b).setName("data");

	Memory<UInt> mem(16, 32_b);
	mem.setPowerOnStateZero();
	mem.setType(MemType::MEDIUM, 1);

	UInt rd = mem[addr];
	auto out = data ^ rd;

	out = reg(out, 0, {.allowRetimingBackward = true});

	pinOut(out).setName("out");

	UInt undefIn  = pinIn(32_b).setName("undefined_in");
	pinOut(reg(undefIn, 0)).setName("reference_out");


	addSimulationProcess([=,this]()->SimProcess {
		simu(addr).invalidate();
		simu(data).invalidate();

		BOOST_TEST(simu(out) == 0);

		co_await AfterClk(clock);

		BOOST_TEST(!simu(out).defined());

		simu(addr) = 0;
		simu(data) = 0;

		co_await AfterClk(clock);
		BOOST_TEST(simu(out) == 0x00);

		simu(data) = 0xFF;

		co_await AfterClk(clock);
		BOOST_TEST(simu(out) == 0xFF);

		stopTest();
	});		

	design.postprocess();

	runTest(hlim::ClockRational(100, 1) / clock.getClk()->absoluteFrequency());
}




BOOST_FIXTURE_TEST_CASE(retiming_enable_suggestion_spawner, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::sim;
	using namespace gtry::utils;

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	UInt input1 = pinIn(32_b).setName("input1");
	UInt input2 = pinIn(32_b).setName("input2");
	UInt a = input1;
	UInt b = input2;

	Bit enable = pinIn().setName("enable");
	ENIF (enable)
		pipeinputgroup(a, b);

	UInt output = a + b;
	output = pipestage(output);

	pinOut(output).setName("output");

	auto area = hlim::Subnet::all(design.getCircuit());
	auto enableCondition = hlim::suggestForwardRetimingEnableCondition(design.getCircuit(), area, output.readPort());
	hlim::Conjunction expectedEnableCondition;
	expectedEnableCondition.parseOutput(enable.readPort());
	BOOST_TEST(enableCondition.isEqualTo(expectedEnableCondition));
}



BOOST_FIXTURE_TEST_CASE(retiming_enable_suggestion_reg, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::sim;
	using namespace gtry::utils;

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	UInt input1 = pinIn(32_b).setName("input1");
	UInt input2 = pinIn(32_b).setName("input2");
	UInt a = input1;
	UInt b = input2;

	Bit enable = pinIn().setName("enable");
	ENIF (enable) {
		a = reg(a, {.allowRetimingForward = true});
		b = reg(b, {.allowRetimingForward = true});
	}

	UInt output = a + b;
	output = pipestage(output);

	pinOut(output).setName("output");

	auto area = hlim::Subnet::all(design.getCircuit());
	auto enableCondition = hlim::suggestForwardRetimingEnableCondition(design.getCircuit(), area, output.readPort());
	hlim::Conjunction expectedEnableCondition;
	expectedEnableCondition.parseOutput(enable.readPort());
	BOOST_TEST(enableCondition.isEqualTo(expectedEnableCondition));
}


BOOST_FIXTURE_TEST_CASE(retiming_enable_suggestion_mixed_conditions, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::sim;
	using namespace gtry::utils;

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	UInt input1 = pinIn(32_b).setName("input1");
	UInt input2 = pinIn(32_b).setName("input2");
	UInt a = input1;
	UInt b = input2;

	Bit ready = pinIn().setName("ready");
	Bit valid = pinIn().setName("valid");
	ENIF (ready)
		a = reg(a, {.allowRetimingForward = true});

	ENIF (ready & valid)
		b = reg(b, {.allowRetimingForward = true});

	UInt output = a + b;
	output = pipestage(output);

	pinOut(output).setName("output");

	auto area = hlim::Subnet::all(design.getCircuit());
	auto enableCondition = hlim::suggestForwardRetimingEnableCondition(design.getCircuit(), area, output.readPort());
	hlim::Conjunction expectedEnableCondition;
	expectedEnableCondition.parseOutput(ready.readPort());
	BOOST_TEST(enableCondition.isEqualTo(expectedEnableCondition));
}








BOOST_FIXTURE_TEST_CASE(retiming_pipeline_enable, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::sim;
	using namespace gtry::utils;

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	UInt input1 = pinIn(32_b).setName("input1");
	UInt input2 = pinIn(32_b).setName("input2");
	UInt a = input1;
	UInt b = input2;

	Bit enable = pinIn().setName("enable");
	ENIF (enable)
		pipeinputgroup(a, b);

	UInt output = a + b;
	output = pipestage(output);

	pinOut(output).setName("output");

	addSimulationProcess([=, this]()->SimProcess {
		simu(input1) = 1337;
		simu(input2) = 42;
		simu(enable) = '1';


		BOOST_TEST(!simu(output).defined());

		co_await AfterClk(clock);

		BOOST_TEST(simu(output).defined());
		BOOST_TEST(simu(output).value() == 1337 + 42);

		simu(input1) = 265367647;
		simu(input2) = 48423;
		simu(enable) = '0';

		co_await AfterClk(clock);

		BOOST_TEST(simu(output).value() == 1337 + 42);

		stopTest();
	});

	//design.visualize("before");
	design.postprocess();

	runTest(hlim::ClockRational(100, 1) / clock.getClk()->absoluteFrequency());
}

BOOST_FIXTURE_TEST_CASE(retiming_pipeline_partial_enable, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::sim;
	using namespace gtry::utils;

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	UInt input1 = pinIn(32_b).setName("input1");
	UInt input2 = pinIn(32_b).setName("input2");
	UInt a = input1;
	UInt b = input2;

	Bit ready = pinIn().setName("ready");
	Bit valid = pinIn().setName("valid");
	Bit v = valid;

	pipeinputgroup(a, b, v);

	ENIF (ready & v)
		a = reg(a, {.allowRetimingForward = true});
		
	ENIF (ready)
		b = reg(b, {.allowRetimingForward = true});

	UInt output = a + b;
	output = pipestage(output);

	pinOut(output).setName("output");

	addSimulationProcess([=, this]()->SimProcess {
		simu(input1) = 1337;
		simu(input2) = 42;
		simu(ready) = '1';
		simu(valid) = '1';


		BOOST_TEST(!simu(output).defined());

		co_await AfterClk(clock);

		BOOST_TEST(simu(output) == 1337 + 42);

		simu(input1) = 265367647;
		simu(input2) = 48423;
		simu(ready) = '0';

		co_await AfterClk(clock);
		BOOST_TEST(simu(output) == 1337 + 42);
		co_await AfterClk(clock);
		BOOST_TEST(simu(output) == 1337 + 42);
		co_await AfterClk(clock);
		BOOST_TEST(simu(output) == 1337 + 42);

		simu(valid) = '0';
		simu(ready) = '1';

		co_await AfterClk(clock);
		BOOST_TEST(simu(output) == 1337 + 48423);
		co_await AfterClk(clock);
		BOOST_TEST(simu(output) == 1337 + 48423);
		co_await AfterClk(clock);
		BOOST_TEST(simu(output) == 1337 + 48423);

		stopTest();
	});

	//design.visualize("before");
	design.postprocess();

	runTest(hlim::ClockRational(100, 1) / clock.getClk()->absoluteFrequency());
}


BOOST_FIXTURE_TEST_CASE(retiming_pipeline_statemachine, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::sim;
	using namespace gtry::utils;

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	UInt input = pinIn(10_b).setName("input");
	UInt data = input;

	Bit ready = pinIn().setName("ready");
	Bit in_valid = pinIn().setName("valid");
	Bit valid = in_valid;
	ENIF (ready) {
		PipeBalanceGroup grp;
		data = grp(data);
		valid = grp(valid, '0');
	}


	UInt output;
	ENIF (ready & valid) {
		UInt counter = 10_b;
		counter = reg(counter+1, 0);
		output = counter + data;
	}

	output = pipestage(output);

	pinOut(output).setName("output");
	pinOut(valid).setName("output_valid");

	addSimulationProcess([=, this]()->SimProcess {
		simu(input) = 1000;
		simu(ready) = '0';
		simu(in_valid) = '0';


		BOOST_TEST(!simu(output).defined());

		co_await AfterClk(clock);

		BOOST_TEST(!simu(output).defined());

		simu(ready) = '0';
		simu(in_valid) = '1';

		co_await AfterClk(clock);
		BOOST_TEST(!simu(output).defined());
		co_await AfterClk(clock);
		BOOST_TEST(!simu(output).defined());
		co_await AfterClk(clock);
		BOOST_TEST(!simu(output).defined());

		simu(ready) = '1';
		simu(in_valid) = '0';

		co_await AfterClk(clock);
		co_await AfterClk(clock);
		co_await AfterClk(clock);
		co_await AfterClk(clock);
		BOOST_TEST(simu(output) == 1000+0);

		simu(ready) = '1';
		simu(in_valid) = '1';

		co_await AfterClk(clock);
		BOOST_TEST(simu(output) == 1000+0);
		co_await AfterClk(clock);
		BOOST_TEST(simu(output) == 1000+1);
		co_await AfterClk(clock);
		BOOST_TEST(simu(output) == 1000+2);
		co_await AfterClk(clock);
		BOOST_TEST(simu(output) == 1000+3);

		stopTest();
	});

	design.postprocess();

	runTest(hlim::ClockRational(100, 1) / clock.getClk()->absoluteFrequency());
}



BOOST_FIXTURE_TEST_CASE(retiming_pipeline_memory, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::sim;
	using namespace gtry::utils;

	Clock clock({ .absoluteFrequency = 100'000'00, .memoryResetType = ClockConfig::ResetType::NONE });
	ClockScope clkScp(clock);

	UInt input = pinIn(10_b).setName("input");
	UInt addr = pinIn(4_b).setName("addr");
	UInt data = input;
	UInt mem_addr = addr;

	Bit ready = pinIn().setName("ready");
	Bit in_valid = pinIn().setName("valid");
	Bit valid = in_valid;
	ENIF (ready) {
		PipeBalanceGroup grp;
		data = grp(data);
		mem_addr = grp(mem_addr);
		valid = grp(valid, '0');
	}


	Memory<UInt> memory(16, 10_b);
	memory.setType(MemType::DONT_CARE, 1);
	memory.initZero();

	UInt output;
	ENIF (ready & valid) {
		UInt val = memory[mem_addr];
		memory[mem_addr] = val+1;
		output = val + data;
	}

	output = pipestage(output);

	pinOut(output).setName("output");
	pinOut(valid).setName("output_valid");

	addSimulationProcess([=, this]()->SimProcess {
		simu(input) = 1000;
		simu(addr) = 0;
		simu(ready) = '0';
		simu(in_valid) = '0';


		BOOST_TEST(!simu(output).defined());

		co_await AfterClk(clock);

		BOOST_TEST(!simu(output).defined());

		simu(ready) = '0';
		simu(in_valid) = '1';

		co_await AfterClk(clock);
		BOOST_TEST(!simu(output).defined());
		co_await AfterClk(clock);
		BOOST_TEST(!simu(output).defined());
		co_await AfterClk(clock);
		BOOST_TEST(!simu(output).defined());

		simu(ready) = '1';
		simu(in_valid) = '0';

		co_await AfterClk(clock);
		co_await AfterClk(clock);
		co_await AfterClk(clock);
		co_await AfterClk(clock);
		BOOST_TEST(simu(output) == 1000+0);

		simu(ready) = '1';
		simu(in_valid) = '1';

		co_await AfterClk(clock);
		BOOST_TEST(simu(output) == 1000+0);
		co_await AfterClk(clock);
		BOOST_TEST(simu(output) == 1000+1);
		co_await AfterClk(clock);
		BOOST_TEST(simu(output) == 1000+2);
		co_await AfterClk(clock);
		BOOST_TEST(simu(output) == 1000+3);

		simu(addr) = 1;
		simu(input) = 500;

		co_await AfterClk(clock);
		BOOST_TEST(simu(output) == 500+0);
		co_await AfterClk(clock);
		BOOST_TEST(simu(output) == 500+1);
		co_await AfterClk(clock);
		BOOST_TEST(simu(output) == 500+2);
		co_await AfterClk(clock);
		BOOST_TEST(simu(output) == 500+3);

		simu(ready) = '0';
		co_await AfterClk(clock);
		co_await AfterClk(clock);
		co_await AfterClk(clock);

		simu(ready) = '1';
		co_await AfterClk(clock);
		BOOST_TEST(simu(output) == 500+4);
		co_await AfterClk(clock);
		BOOST_TEST(simu(output) == 500+5);
		co_await AfterClk(clock);
		BOOST_TEST(simu(output) == 500+6);
		co_await AfterClk(clock);
		BOOST_TEST(simu(output) == 500+7);


		stopTest();
	});

	//design.visualize("before");
	design.postprocess();

	runTest(hlim::ClockRational(100, 1) / clock.getClk()->absoluteFrequency());
}




BOOST_FIXTURE_TEST_CASE(retiming_pipeline_memory_cond_write, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::sim;
	using namespace gtry::utils;

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	UInt input = pinIn(10_b).setName("input");
	Bit wrEn = pinIn().setName("wrEn");
	UInt addr = pinIn(4_b).setName("addr");
	UInt data = input;
	UInt mem_addr = addr;
	Bit writeEnable = wrEn;

	Bit ready = pinIn().setName("ready");
	Bit in_valid = pinIn().setName("valid");
	Bit valid = in_valid;
	ENIF (ready) {
		PipeBalanceGroup grp;
		data = grp(data);
		mem_addr = grp(mem_addr);
		writeEnable = grp(writeEnable);
		valid = grp(valid, '0');
	}


	Memory<UInt> memory(16, 10_b);
	memory.setType(MemType::DONT_CARE, 1);
	memory.initZero();

	UInt output;
	ENIF (ready & valid) {
		UInt val = memory[mem_addr];
		IF (writeEnable)
			memory[mem_addr] = val+1;
		output = val + data;
	}

	output = pipestage(output);

	pinOut(output).setName("output");
	pinOut(valid).setName("output_valid");

	addSimulationProcess([=, this]()->SimProcess {
		simu(input) = 1000;
		simu(addr) = 0;
		simu(ready) = '0';
		simu(wrEn) = '1';
		simu(in_valid) = '0';


		BOOST_TEST(!simu(output).defined());

		co_await AfterClk(clock);

		BOOST_TEST(!simu(output).defined());

		simu(ready) = '0';
		simu(in_valid) = '1';

		co_await AfterClk(clock);
		BOOST_TEST(!simu(output).defined());
		co_await AfterClk(clock);
		BOOST_TEST(!simu(output).defined());
		co_await AfterClk(clock);
		BOOST_TEST(!simu(output).defined());

		simu(ready) = '1';
		simu(in_valid) = '0';

		co_await AfterClk(clock);
		co_await AfterClk(clock);
		co_await AfterClk(clock);
		co_await AfterClk(clock);
		BOOST_TEST(simu(output) == 1000+0);

		simu(ready) = '1';
		simu(in_valid) = '1';

		co_await AfterClk(clock);
		BOOST_TEST(simu(output) == 1000+0);
		co_await AfterClk(clock);
		BOOST_TEST(simu(output) == 1000+1);
		co_await AfterClk(clock);
		BOOST_TEST(simu(output) == 1000+2);
		co_await AfterClk(clock);
		BOOST_TEST(simu(output) == 1000+3);

		simu(wrEn) = '0';
		co_await AfterClk(clock);
		BOOST_TEST(simu(output) == 1000+4);
		co_await AfterClk(clock);
		BOOST_TEST(simu(output) == 1000+4);
		co_await AfterClk(clock);
		BOOST_TEST(simu(output) == 1000+4);

		simu(wrEn) = '1';

		simu(addr) = 1;
		simu(input) = 500;

		co_await AfterClk(clock);
		BOOST_TEST(simu(output) == 500+0);
		co_await AfterClk(clock);
		BOOST_TEST(simu(output) == 500+1);
		co_await AfterClk(clock);
		BOOST_TEST(simu(output) == 500+2);
		co_await AfterClk(clock);
		BOOST_TEST(simu(output) == 500+3);


		stopTest();
	});

	design.postprocess();

	runTest(hlim::ClockRational(100, 1) / clock.getClk()->absoluteFrequency());
}




BOOST_FIXTURE_TEST_CASE(retiming_pipeline_memory_independent_sides, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::sim;
	using namespace gtry::utils;

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	UInt input = pinIn(10_b).setName("input");
	Bit wrEn = pinIn().setName("wrEn");
	UInt wrAddr = pinIn(4_b).setName("wrAddr");
	UInt data = input;
	UInt mem_wr_addr = wrAddr;
	Bit writeEnable = wrEn;

	Bit ready1 = pinIn().setName("ready1");
	Bit in_valid1 = pinIn().setName("valid1");


	UInt rdAddr = pinIn(4_b).setName("rdAddr");
	UInt mem_rd_addr = rdAddr;

	Bit ready2 = pinIn().setName("ready2");
	Bit in_valid2 = pinIn().setName("valid2");




	Memory<UInt> memory(16, 10_b);
	memory.setType(MemType::DONT_CARE, 1);
	memory.initZero();
	memory.allowArbitraryPortRetiming();


	std::optional<PipeBalanceGroup> grp1;

	Bit valid1 = in_valid1;
	ENIF (ready1) {
		grp1.emplace();
		data = (*grp1)(data);
		mem_wr_addr = (*grp1)(mem_wr_addr);
		writeEnable = (*grp1)(writeEnable);
		valid1 = (*grp1)(valid1, '0');
	}

	ENIF (ready1 & valid1) {
		IF (writeEnable)
			memory[mem_wr_addr] = data;
	}



	std::optional<PipeBalanceGroup> grp2;

	Bit valid2 = in_valid2;
	ENIF (ready2) {
		grp2.emplace();
		mem_rd_addr = (*grp2)(mem_rd_addr);
		valid2 = (*grp2)(valid2, '0');
	}

	UInt output;
	ENIF (ready2 & valid2) {
		output = memory[mem_rd_addr];
	}	

	output = pipestage(output);

	pinOut(output).setName("output");
	pinOut(valid1).setName("output_valid1");
	pinOut(valid2).setName("output_valid2");

	design.postprocess();

	// only testing successfull postprocessing/retiming

	BOOST_TEST(grp1->getNumPipeBalanceGroupStages() == 0);
	BOOST_TEST(grp2->getNumPipeBalanceGroupStages() == 1);

}



BOOST_FIXTURE_TEST_CASE(retiming_pipeline_in_export_override_branch, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::sim;
	using namespace gtry::utils;

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	UInt input1 = pinIn(32_b).setName("input1");
	UInt input2 = pinIn(32_b).setName("input2");
	UInt a = input1;
	UInt b = input2;

	Bit enable = pinIn().setName("enable");
	ENIF (enable)
		pipeinputgroup(a, b);

	UInt output = a + b;

	UInt exportOutput = pipestage(a) + b;
	HCL_NAMED(exportOutput);
	output.exportOverride(exportOutput);

	pinOut(output).setName("output");

	addSimulationProcess([=, this]()->SimProcess {
		simu(input1) = 1337;
		simu(input2) = 42;
		simu(enable) = '1';


		BOOST_TEST(!simu(output).defined());

		co_await AfterClk(clock);

		BOOST_TEST(simu(output).defined());
		BOOST_TEST(simu(output).value() == 1337 + 42);

		simu(input1) = 265367647;
		simu(input2) = 48423;
		simu(enable) = '0';

		co_await AfterClk(clock);

		BOOST_TEST(simu(output).value() == 1337 + 42);

		stopTest();
	});

	//design.visualize("before");
	design.postprocess();

	runTest(hlim::ClockRational(100, 1) / clock.getClk()->absoluteFrequency());
}


BOOST_FIXTURE_TEST_CASE(retiming_pipeline_retime_export_override, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::sim;
	using namespace gtry::utils;

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	UInt input1 = pinIn(32_b).setName("input1");
	UInt input2 = pinIn(32_b).setName("input2");
	UInt a = input1;
	UInt b = input2;

	Bit enable = pinIn().setName("enable");
	ENIF (enable)
		pipeinputgroup(a, b);

	UInt output = a + b;

	UInt exportOutput = a + b + 10; // Not used in simulation
	HCL_NAMED(exportOutput);
	output.exportOverride(exportOutput);

	output = pipestage(output);

	pinOut(output).setName("output");

	addSimulationProcess([=, this]()->SimProcess {
		simu(input1) = 1337;
		simu(input2) = 42;
		simu(enable) = '1';


		BOOST_TEST(!simu(output).defined());

		co_await AfterClk(clock);

		BOOST_TEST(simu(output).defined());
		BOOST_TEST(simu(output).value() == 1337 + 42);

		simu(input1) = 265367647;
		simu(input2) = 48423;
		simu(enable) = '0';

		co_await AfterClk(clock);

		BOOST_TEST(simu(output).value() == 1337 + 42);

		stopTest();
	});

	//design.visualize("before");
	design.postprocess();

	runTest(hlim::ClockRational(100, 1) / clock.getClk()->absoluteFrequency());
}




BOOST_FIXTURE_TEST_CASE(retiming_pipeline_retime_over_black_box, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::sim;
	using namespace gtry::utils;

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);
	
	UInt input1 = pinIn(32_b).setName("input1");
	UInt input2 = pinIn(32_b).setName("input2");
	Bit enable = pinIn().setName("enable");
	UInt output;
	{

		UInt a = input1;
		UInt b = input2;

		ENIF (enable)
			pipeinputgroup(a, b);

		output = a + b;


		ExternalModule fluxCapacitor{ "FLUX_CAPACITOR", "UNISIM", "vcomponents" };
		fluxCapacitor.in("a", a.width()) = (BVec) a;
		fluxCapacitor.in("b", b.width()) = (BVec) b;
		UInt exportOutput = (UInt) fluxCapacitor.out("O", a.width());

		HCL_NAMED(exportOutput);
		output.exportOverride(exportOutput);

		output = pipestage(output);

		pinOut(output).setName("output");
	}

	addSimulationProcess([=, this]()->SimProcess {
		simu(input1) = 1337;
		simu(input2) = 42;
		simu(enable) = '1';


		BOOST_TEST(!simu(output).defined());

		co_await AfterClk(clock);

		BOOST_TEST(simu(output).defined());
		BOOST_TEST(simu(output).value() == 1337 + 42);

		simu(input1) = 265367647;
		simu(input2) = 48423;
		simu(enable) = '0';

		co_await AfterClk(clock);

		BOOST_TEST(simu(output).value() == 1337 + 42);

		stopTest();
	});

	//design.visualize("before");
	design.postprocess();

	runTest(hlim::ClockRational(100, 1) / clock.getClk()->absoluteFrequency());
}





BOOST_FIXTURE_TEST_CASE(retiming_pipeline_negativeRegister, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::sim;
	using namespace gtry::utils;

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);
	
	UInt input1 = pinIn(32_b).setName("input1");
	UInt input2 = pinIn(32_b).setName("input2");
	Bit enable = pinIn().setName("enable");
	UInt output;
	{

		UInt a = input1;
		UInt b = input2;

		ENIF (enable)
			pipeinputgroup(a, b);

		output = a + b;


		auto [a_prev, enable_a] = negativeReg(a);
		auto [b_prev, enable_b] = negativeReg(b);

		ExternalModule fluxCapacitor{ "FLUX_CAPACITOR", "UNISIM", "vcomponents" };
		fluxCapacitor.in("a", a.width()) = (BVec) a_prev;
		fluxCapacitor.in("b", b.width()) = (BVec) b_prev;
		fluxCapacitor.in("enable_a") = enable_a;
		fluxCapacitor.in("enable_b") = enable_b;
		UInt exportOutput = (UInt) fluxCapacitor.out("O", a.width());

		HCL_NAMED(exportOutput);
		output.exportOverride(exportOutput);

		pinOut(output).setName("output");
	}

	addSimulationProcess([=, this]()->SimProcess {
		simu(input1) = 1337;
		simu(input2) = 42;
		simu(enable) = '1';


		BOOST_TEST(!simu(output).defined());

		co_await AfterClk(clock);

		BOOST_TEST(simu(output).defined());
		BOOST_TEST(simu(output).value() == 1337 + 42);

		simu(input1) = 265367647;
		simu(input2) = 48423;
		simu(enable) = '0';

		co_await AfterClk(clock);
		co_await AfterClk(clock);
		co_await AfterClk(clock);

		BOOST_TEST(simu(output).value() == 1337 + 42);

		stopTest();
	});

	//design.visualize("before");
	design.postprocess();


	auto *fluxCapacitor = design.getCircuit().findFirstNodeByName("FLUX_CAPACITOR");
	BOOST_REQUIRE(fluxCapacitor);
	BOOST_TEST(fluxCapacitor->getNonSignalDriver(0).node == input1.readPort().node);
	BOOST_TEST(fluxCapacitor->getNonSignalDriver(1).node == input2.readPort().node);
	BOOST_TEST(fluxCapacitor->getNonSignalDriver(2).node == enable.readPort().node);
	BOOST_TEST(fluxCapacitor->getNonSignalDriver(3).node == enable.readPort().node);

	runTest(hlim::ClockRational(100, 1) / clock.getClk()->absoluteFrequency());
}



BOOST_FIXTURE_TEST_CASE(retiming_pipeline_multipleNegativeRegister, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::sim;
	using namespace gtry::utils;

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);
	
	UInt input1 = pinIn(32_b).setName("input1");
	UInt input2 = pinIn(32_b).setName("input2");
	Bit enable = pinIn().setName("enable");
	UInt output;
	{

		UInt a = input1;
		UInt b = input2;

		ENIF (enable)
			pipeinputgroup(a, b);

		output = a + b;


		auto [a_prev, enable_a] = negativeReg(a);
		auto [a_prevPrev, enable_a2] = negativeReg(a_prev);
		auto [b_prev, enable_b] = negativeReg(b);

		ExternalModule fluxCapacitor{ "FLUX_CAPACITOR", "UNISIM", "vcomponents" };
		fluxCapacitor.in("a", a.width()) = (BVec) a_prevPrev;
		fluxCapacitor.in("b", b.width()) = (BVec) b_prev;
		fluxCapacitor.in("enable_a") = enable_a;
		fluxCapacitor.in("enable_a2") = enable_a2;
		fluxCapacitor.in("enable_b") = enable_b;
		UInt exportOutput = (UInt) fluxCapacitor.out("O", a.width());

		HCL_NAMED(exportOutput);
		output.exportOverride(exportOutput);

		pinOut(output).setName("output");
	}

	addSimulationProcess([=, this]()->SimProcess {
		simu(input1) = 1337;
		simu(input2) = 42;
		simu(enable) = '1';


		BOOST_TEST(!simu(output).defined());

		co_await AfterClk(clock);

		BOOST_TEST(simu(output).defined());
		BOOST_TEST(simu(output).value() == 1337 + 42);

		co_await AfterClk(clock);

		simu(input1) = 265367647;
		simu(input2) = 48423;
		simu(enable) = '0';

		co_await AfterClk(clock);
		co_await AfterClk(clock);
		co_await AfterClk(clock);

		BOOST_TEST(simu(output).value() == 1337 + 42);

		stopTest();
	});

	design.visualize("before");
	design.postprocess();
	design.visualize("after");

	auto *fluxCapacitor = design.getCircuit().findFirstNodeByName("FLUX_CAPACITOR");
	BOOST_REQUIRE(fluxCapacitor);
	BOOST_TEST(fluxCapacitor->getNonSignalDriver(0).node == input1.readPort().node);
	BOOST_TEST(fluxCapacitor->getNonSignalDriver(1).node != input2.readPort().node); // Pointing to register
	BOOST_TEST(fluxCapacitor->getNonSignalDriver(2).node == enable.readPort().node);
	BOOST_TEST(fluxCapacitor->getNonSignalDriver(3).node == enable.readPort().node);
	BOOST_TEST(fluxCapacitor->getNonSignalDriver(4).node == enable.readPort().node);

	runTest(hlim::ClockRational(100, 1) / clock.getClk()->absoluteFrequency());
}

BOOST_FIXTURE_TEST_CASE(retiming_pipeline_multipleNegativeRegister_in_ready_valid_stream, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::sim;
	using namespace gtry::utils;

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);
	
	UInt input1 = pinIn(32_b).setName("input1");
	UInt input2 = pinIn(32_b).setName("input2");
	Bit ready = pinIn().setName("ready");
	Bit valid = pinIn().setName("valid");
	UInt output = dontCare(UInt(32_b));
	{

		UInt a = input1;
		UInt b = input2;

		ENIF (ready)
			pipeinputgroup(a, b);


		IF (retimingBlocker(ready) & valid) {

			output = a + b;


			auto [a_prev, enable_a] = negativeReg(a);
			auto [a_prevPrev, enable_a2] = negativeReg(a_prev);
			auto [b_prev, enable_b] = negativeReg(b);

			ExternalModule fluxCapacitor{ "FLUX_CAPACITOR", "UNISIM", "vcomponents" };
			fluxCapacitor.in("a", a.width()) = (BVec) a_prevPrev;
			fluxCapacitor.in("b", b.width()) = (BVec) b_prev;
			fluxCapacitor.in("enable_a") = enable_a;
			fluxCapacitor.in("enable_a2") = enable_a2;
			fluxCapacitor.in("enable_b") = enable_b;
			UInt exportOutput = (UInt) fluxCapacitor.out("O", a.width());

			HCL_NAMED(exportOutput);
			output.exportOverride(exportOutput);
		}

		pinOut(output).setName("output");
	}

	addSimulationProcess([=, this]()->SimProcess {
		simu(input1) = 1337;
		simu(input2) = 42;
		simu(ready) = '1';
		simu(valid) = '1';


		BOOST_TEST(!simu(output).defined());

		co_await AfterClk(clock);

		BOOST_TEST(simu(output).defined());
		BOOST_TEST(simu(output).value() == 1337 + 42);

		co_await AfterClk(clock);

		simu(input1) = 265367647;
		simu(input2) = 48423;
		simu(ready) = '0';

		co_await AfterClk(clock);
		co_await AfterClk(clock);
		co_await AfterClk(clock);

		simu(ready) = '1';

		co_await WaitStable();

		BOOST_TEST(simu(output).value() == 1337 + 42);

		stopTest();
	});

	design.visualize("before");
	design.postprocess();
	design.visualize("after");

	auto *fluxCapacitor = design.getCircuit().findFirstNodeByName("FLUX_CAPACITOR");
	BOOST_REQUIRE(fluxCapacitor);
	BOOST_TEST(fluxCapacitor->getNonSignalDriver(0).node == input1.readPort().node);
	BOOST_TEST(fluxCapacitor->getNonSignalDriver(1).node != input2.readPort().node); // Pointing to register

	runTest(hlim::ClockRational(100, 1) / clock.getClk()->absoluteFrequency());
}