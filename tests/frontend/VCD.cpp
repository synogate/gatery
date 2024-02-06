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

#include <gatery/simulation/Simulator.h>

#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>
#include <random>

using namespace boost::unit_test;
using namespace gtry;
using namespace gtry::utils;
using BoostUnitTestSimulationFixture = gtry::BoostUnitTestSimulationFixture;

template<class BaseFixture>
class VCDTestFixture : public BaseFixture
{
	public:
		VCDTestFixture();

		bool VCDContains(const std::regex &regex);
		bool GTKWaveProjectFileContains(const std::regex &regex);
	protected:
		std::filesystem::path m_testDir;

		void prepRun() override;
};


template<class BaseFixture>
class IgnoreTapMessages : public BaseFixture
{
	public:
		virtual void onDebugMessage(const hlim::BaseNode* src, std::string msg) override { }
		virtual void onWarning(const hlim::BaseNode* src, std::string msg) override { }
		virtual void onAssert(const hlim::BaseNode* src, std::string msg) override { }
};


template<class BaseFixture>
VCDTestFixture<BaseFixture>::VCDTestFixture()
{
	const auto& testCase = boost::unit_test::framework::current_test_case();
	std::filesystem::path testCaseFile{ std::string{ testCase.p_file_name.begin(), testCase.p_file_name.end() } };
	m_testDir = std::filesystem::path{ "tmp" } / testCaseFile.stem() / testCase.p_name.get();

	std::error_code ignored;
	std::filesystem::remove_all(m_testDir, ignored);
	std::filesystem::create_directories(m_testDir);
}

template<class BaseFixture>
void VCDTestFixture<BaseFixture>::prepRun()
{
	BaseFixture::prepRun();
	BaseFixture::recordVCD((m_testDir / "test.vcd").string(), true);
}

template<class BaseFixture>
bool VCDTestFixture<BaseFixture>::VCDContains(const std::regex &regex)
{
	BaseFixture::m_vcdSink.reset();
	std::fstream file((m_testDir / "test.vcd").string(), std::fstream::in);
	BOOST_TEST((bool) file);
	std::stringstream buffer;
	buffer << file.rdbuf();

	return std::regex_search(buffer.str(), regex);
}

template<class BaseFixture>
bool VCDTestFixture<BaseFixture>::GTKWaveProjectFileContains(const std::regex &regex)
{
	BaseFixture::m_vcdSink.reset();
	std::fstream file((m_testDir / "test.vcd.gtkw").string(), std::fstream::in);
	BOOST_TEST((bool) file);
	std::stringstream buffer;
	buffer << file.rdbuf();

	return std::regex_search(buffer.str(), regex);
}



BOOST_AUTO_TEST_SUITE(VCD)


BOOST_FIXTURE_TEST_CASE(tapInGTKWaveProjectFiles, VCDTestFixture<BoostUnitTestSimulationFixture>)
{
	using namespace gtry;

	Clock clock({ .absoluteFrequency = 10'000 });

	UInt counter = 8_b;
	counter = reg(counter+1, 0);
	HCL_NAMED(counter);
	pinOut(counter);

	UInt unused = counter ^ 1;
	HCL_NAMED(unused);
	tap(unused);


	addSimulationProcess([clock,this]()->SimProcess {

		for ([[maybe_unused]] auto i : gtry::utils::Range(50)) {
			co_await AfterClk(clock);
		}

		stopTest();
	});

	design.postprocess();

	runTicks(clock.getClk(), 100000);

	BOOST_TEST(VCDContains(std::regex{"unused"}));
	BOOST_TEST(GTKWaveProjectFileContains(std::regex{"unused"}));
}

/*
enum MyEnum {
	ENUM_VALUE_A,
	ENUM_VALUE_B,
};

BOOST_FIXTURE_TEST_CASE(EnumFilterInGTKWaveProjectFiles, VCDTestFixture<BoostUnitTestSimulationFixture>)
{
	using namespace gtry;

	Clock clock({ .absoluteFrequency = 10'000 });


	Enum<MyEnum> myEnum;
	myEnum = Enum<MyEnum>(UInt(pinIn(1_b).setName("input")));

	HCL_NAMED(myEnum);
	tap(myEnum);

	addSimulationProcess([clock,this]()->SimProcess {

		for ([[maybe_unused]] auto i : gtry::utils::Range(50)) {
			co_await AfterClk(clock);
		}

		stopTest();
	});

	design.postprocess();

	runTicks(clock.getClk(), 100000);

	BOOST_TEST(VCDContains(std::regex{"myEnum"}));
	BOOST_TEST(GTKWaveProjectFileContains(std::regex{"myEnum"}));
	BOOST_TEST(GTKWaveProjectFileContains(std::regex{"MyEnum"}));
}
*/

BOOST_FIXTURE_TEST_CASE(testMessagesInVCD, IgnoreTapMessages<VCDTestFixture<BoostUnitTestSimulationFixture>>)
{
	using namespace gtry;

	Clock clock({ .absoluteFrequency = 10'000 });

	Bit b = pinIn();
	sim_assert(b) << "Something bad has happened: b is " << b;

	addSimulationProcess([=,this]()->SimProcess {
		simu(b) = true;

		co_await AfterClk(clock);
		co_await AfterClk(clock);

		simu(b) = false;

		co_await AfterClk(clock);

		stopTest();
	});

	design.postprocess();

	runTicks(clock.getClk(), 100000);

	BOOST_TEST(VCDContains(std::regex{"Something.*bad.*has.*happened:.*b.*is.*0"}));
}




BOOST_FIXTURE_TEST_CASE(testMemoryInVCD, VCDTestFixture<BoostUnitTestSimulationFixture>)
{
	using namespace gtry;
	using namespace gtry::sim;
	using namespace gtry::utils;

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	std::vector<size_t> contents;
	contents.resize(16);
	std::mt19937 rng{ 18055 };
	for (auto &e : contents)
		e = rng() % 16;

	Memory<UInt> mem(contents.size(), 4_b);
	mem.noConflicts();
	mem.setName("MyMemory");

	UInt addr = pinIn(4_b).setName("addr");
	auto output = pinOut(mem[addr]).setName("output");
	UInt input = pinIn(4_b).setName("input");
	Bit wrEn = pinIn().setName("wrEn");
	IF (wrEn)
		mem[addr] = input;

	addSimulationProcess([=,this,&contents,&clock]()->SimProcess {

		simu(wrEn) = '0';
		co_await AfterClk(clock);

		simu(wrEn) = '1';
		for (auto i : Range(16)) {
			simu(addr) = i;
			simu(input) = contents[i];
			co_await AfterClk(clock);
		}
		simu(wrEn) = '0';

		for (auto i : Range(16)) {
			simu(addr) = i;
			co_await WaitStable();
			BOOST_TEST(simu(output) == contents[i]);
			co_await AfterClk(clock);
		}		

		stopTest();
	});

	design.postprocess();
	runTest(hlim::ClockRational(100, 1) / clock.getClk()->absoluteFrequency());

	BOOST_TEST(VCDContains(std::regex{"MyMemory"}));
	BOOST_TEST(VCDContains(std::regex{"addr_0015"}));
}




BOOST_FIXTURE_TEST_CASE(testMultiSignalsSameDriver, VCDTestFixture<BoostUnitTestSimulationFixture>)
{
	using namespace gtry;
	using namespace gtry::sim;
	using namespace gtry::utils;

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	UInt input = pinIn(4_b).setName("input1");
	UInt input2 = pinIn(4_b).setName("input2");
	HCL_NAMED(input);
	UInt dummy = input;
	HCL_NAMED(dummy);
	tap(dummy);

	UInt output = input ^ input2;

	UInt outDummy = output;
	HCL_NAMED(outDummy);
	tap(outDummy);

	HCL_NAMED(output);
	pinOut(output).setName("out");

	UInt outDummyAfter = output;
	HCL_NAMED(outDummyAfter);
	tap(outDummyAfter);

	addSimulationProcess([=,this]()->SimProcess {

		co_await AfterClk(clock);
		co_await AfterClk(clock);
		stopTest();
	});

	design.postprocess();
	runTest(hlim::ClockRational(100, 1) / clock.getClk()->absoluteFrequency());

	BOOST_TEST(VCDContains(std::regex{"\\$var wire 4 . input \\$end"}));
	BOOST_TEST(VCDContains(std::regex{"\\$var wire 4 . dummy \\$end"}));
	BOOST_TEST(VCDContains(std::regex{"\\$var wire 4 . output \\$end"}));
	BOOST_TEST(VCDContains(std::regex{"\\$var wire 4 . outDummy \\$end"}));
	BOOST_TEST(VCDContains(std::regex{"\\$var wire 4 . outDummyAfter \\$end"}));
}





BOOST_AUTO_TEST_SUITE_END()
