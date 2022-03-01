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

    Clock clock(ClockConfig{}.setAbsoluteFrequency(100'000'000).setName("clock"));
	ClockScope clkScp(clock);




    UInt input = pinIn(32_b);

    UInt counter = 32_b;
	counter = counter + 1;
	counter = reg(counter, 0, {.allowRetimingForward=true});

	UInt output = counter | reg(input, 0, {.allowRetimingForward=true});

	stripSignalNodes(design.getCircuit());
	auto subnet = hlim::Subnet::all(design.getCircuit());
	design.getCircuit().optimizeSubnet(subnet);
	retimeForwardToOutput(design.getCircuit(), subnet, output.getReadPort(), {.ignoreRefs=true});

	auto outPin = pinOut(output);

	addSimulationProcess([=,this]()->SimProcess {
		simu(input) = 0;
        
        for (auto i : Range(32)) {
			BOOST_TEST(simu(outPin).value() == i);
            co_await WaitClk(clock);
        }

        stopTest();
	});

	//design.visualize("retiming_forward_counter_new_before");
	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});

	//design.visualize("retiming_forward_counter_new");
	runTest(hlim::ClockRational(100, 1) / clock.getClk()->getAbsoluteFrequency());
}

BOOST_FIXTURE_TEST_CASE(retiming_forward_counter_old, BoostUnitTestSimulationFixture)
{
    using namespace gtry;
    using namespace gtry::sim;
    using namespace gtry::utils;

    Clock clock(ClockConfig{}.setAbsoluteFrequency(100'000'000).setName("clock"));
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
	retimeForwardToOutput(design.getCircuit(), subnet, output.getReadPort(), {.ignoreRefs=true});
	auto outPin = pinOut(output);

	addSimulationProcess([=,this]()->SimProcess {
		simu(input) = 0;
        
        for (auto i : Range(32)) {
			BOOST_TEST(simu(outPin).value() == i+1);
            co_await WaitClk(clock);
        }

        stopTest();
	});
	//design.visualize("retiming_counter_new_before");

	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});

	//design.visualize("retiming_forward_counter_old");
	runTest(hlim::ClockRational(100, 1) / clock.getClk()->getAbsoluteFrequency());
}

BOOST_FIXTURE_TEST_CASE(retiming_hint_simple, BoostUnitTestSimulationFixture)
{
    using namespace gtry;
    using namespace gtry::sim;
    using namespace gtry::utils;

    Clock clock(ClockConfig{}.setAbsoluteFrequency(100'000'000).setName("clock"));
	ClockScope clkScp(clock);

    UInt input = pinIn(32_b);

    Pipeline pipeline;
	input = pipeline(input);


	UInt output = input;
	for (auto i : Range(3))
		output = regHint(output);

	pinOut(output);

//design.visualize("before");
	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
//design.visualize("after");


	BOOST_TEST(pipeline.getNumPipelineStages() == 3);
}

BOOST_FIXTURE_TEST_CASE(retiming_hint_simple_reset, BoostUnitTestSimulationFixture)
{
    using namespace gtry;
    using namespace gtry::sim;
    using namespace gtry::utils;

    Clock clock(ClockConfig{}.setAbsoluteFrequency(100'000'000).setName("clock"));
	ClockScope clkScp(clock);

    UInt input = pinIn(32_b);

    Pipeline pipeline;
	input = pipeline(input, 0);


	UInt output = input;
	for ([[maybe_unused]] auto i : Range(3))
		output = regHint(output);

	pinOut(output);

//design.visualize("before");
	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
//design.visualize("after");


	BOOST_TEST(pipeline.getNumPipelineStages() == 3);
}


struct TestStruct
{
	gtry::Bit a;
	gtry::UInt b;
};

BOOST_HANA_ADAPT_STRUCT(TestStruct, a, b);

BOOST_FIXTURE_TEST_CASE(retiming_hint_struct, BoostUnitTestSimulationFixture)
{
    using namespace gtry;
    using namespace gtry::sim;
    using namespace gtry::utils;

    Clock clock(ClockConfig{}.setAbsoluteFrequency(100'000'000).setName("clock"));
	ClockScope clkScp(clock);

	TestStruct s_in;
    s_in.a = pinIn();
    s_in.b = pinIn(32_b);

    Pipeline pipeline;
	TestStruct s_out = pipeline(s_in);


	for ([[maybe_unused]] auto i : Range(3))
		s_out = regHint(s_out);

	pinOut(s_out.a);
	pinOut(s_out.b);



	addSimulationProcess([=,this]()->SimProcess {
		simu(s_in.a) = false;
		simu(s_in.b) = 42;
        
	    for ([[maybe_unused]] auto i : Range(3)) {
			BOOST_TEST(!simu(s_out.a).defined());
			BOOST_TEST(!simu(s_out.b).defined());

			co_await WaitClk(clock);
		}

		BOOST_TEST(simu(s_out.a).defined());
		BOOST_TEST(simu(s_out.a).value() == false);
		BOOST_TEST(simu(s_out.b).defined());
		BOOST_TEST(simu(s_out.b).value() == 42);

        stopTest();
	});	

//design.visualize("before");
	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
//design.visualize("after");

	BOOST_TEST(pipeline.getNumPipelineStages() == 3);

	runTest(hlim::ClockRational(100, 1) / clock.getClk()->getAbsoluteFrequency());	
}


BOOST_FIXTURE_TEST_CASE(retiming_hint_struct_reset, BoostUnitTestSimulationFixture)
{
    using namespace gtry;
    using namespace gtry::sim;
    using namespace gtry::utils;

    Clock clock(ClockConfig{}.setAbsoluteFrequency(100'000'000).setName("clock"));
	ClockScope clkScp(clock);

	TestStruct s_in;
    s_in.a = pinIn();
    s_in.b = pinIn(32_b);

	TestStruct r;
	r.a = '1';
	r.b = "32b0";

    Pipeline pipeline;
	TestStruct s_out = pipeline(s_in, r);


    for ([[maybe_unused]] auto i : Range(3))
        s_out = regHint(s_out);

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

			co_await WaitClk(clock);
		}

		BOOST_TEST(simu(s_out.a).defined());
		BOOST_TEST(simu(s_out.a).value() == false);
		BOOST_TEST(simu(s_out.b).defined());
		BOOST_TEST(simu(s_out.b).value() == 42);

        stopTest();
	});

//design.visualize("before");
	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
//design.visualize("after");

	BOOST_TEST(pipeline.getNumPipelineStages() == 3);

	runTest(hlim::ClockRational(100, 1) / clock.getClk()->getAbsoluteFrequency());

}



BOOST_FIXTURE_TEST_CASE(retiming_hint_branching, BoostUnitTestSimulationFixture)
{
    using namespace gtry;
    using namespace gtry::sim;
    using namespace gtry::utils;

    Clock clock(ClockConfig{}.setAbsoluteFrequency(100'000'000).setName("clock"));
	ClockScope clkScp(clock);

    UInt input1 = pinIn(32_b);
    UInt input2 = pinIn(32_b);

    Pipeline pipeline;
	auto a = pipeline(input1);
	auto b = pipeline(input2);

	b = regHint(b);

	UInt output = a + b;
	output = regHint(output);

	pinOut(output);


	addSimulationProcess([=,this]()->SimProcess {
		simu(input1) = 1337;
		simu(input2) = 42;
        
	    for ([[maybe_unused]] auto i : Range(2)) {
			BOOST_TEST(!simu(output).defined());

			co_await WaitClk(clock);
		}

		BOOST_TEST(simu(output).defined());
		BOOST_TEST(simu(output).value() == 1337+42);

        stopTest();
	});

//design.visualize("before");
	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
//design.visualize("after");

	BOOST_TEST(pipeline.getNumPipelineStages() == 2);


	runTest(hlim::ClockRational(100, 1) / clock.getClk()->getAbsoluteFrequency());		
}


BOOST_FIXTURE_TEST_CASE(retiming_hint_branching_reset, BoostUnitTestSimulationFixture)
{
    using namespace gtry;
    using namespace gtry::sim;
    using namespace gtry::utils;

    Clock clock(ClockConfig{}.setAbsoluteFrequency(100'000'000).setName("clock"));
	ClockScope clkScp(clock);

    UInt input1 = pinIn(32_b);
    UInt input2 = pinIn(32_b);

    Pipeline pipeline;
	auto a = pipeline(input1, 0);
	auto b = pipeline(input2, 1);

	b = regHint(b);

	UInt output = a + b;
	output = regHint(output);

	pinOut(output);


	addSimulationProcess([=,this]()->SimProcess {
		simu(input1) = 1337;
		simu(input2) = 42;
        
	    for ([[maybe_unused]] auto i : Range(2)) {
			BOOST_TEST(simu(output).defined());
			BOOST_TEST(simu(output).value() == 0+1);

			co_await WaitClk(clock);
		}

		BOOST_TEST(simu(output).defined());
		BOOST_TEST(simu(output).value() == 1337+42);

        stopTest();
	});	

//design.visualize("before");
	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
//design.visualize("after");

	BOOST_TEST(pipeline.getNumPipelineStages() == 2);


	runTest(hlim::ClockRational(100, 1) / clock.getClk()->getAbsoluteFrequency());		
}


BOOST_FIXTURE_TEST_CASE(retiming_hint_memory_rmw, BoostUnitTestSimulationFixture)
{
    using namespace gtry;
    using namespace gtry::sim;
    using namespace gtry::utils;

    Clock clock(ClockConfig{}.setAbsoluteFrequency(100'000'000).setName("clock"));
	ClockScope clkScp(clock);

    UInt addr = pinIn(4_b);
    UInt data = pinIn(32_b);
	Bit enable = pinIn();

    Pipeline pipeline;
	addr = pipeline(addr);
	data = pipeline(data);
	enable = pipeline(enable);

	Memory<UInt> mem(16, 32_b);
	mem.setType(MemType::MEDIUM, 1);

	UInt rd = mem[addr];
	rd = regHint(rd);

	IF (enable)
		mem[addr] = rd+data;

	pinOut(regHint(rd));

//design.visualize("before");
	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
//design.visualize("after");

	BOOST_TEST(pipeline.getNumPipelineStages() == 2);
}
