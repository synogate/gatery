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
		~VCDTestFixture();


		bool VCDContains(const std::regex &regex);
		bool GTKWaveProjectFileContains(const std::regex &regex);
	protected:
		std::filesystem::path m_cwd;

		void prepRun() override;
};




template<class BaseFixture>
VCDTestFixture<BaseFixture>::VCDTestFixture()
{
	m_cwd = std::filesystem::current_path();

	std::filesystem::path tmpDir("tmp/");
	std::error_code ignored;
	std::filesystem::remove_all(tmpDir, ignored);
	std::filesystem::create_directories(tmpDir);
    std::filesystem::current_path(tmpDir);
}


template<class BaseFixture>
VCDTestFixture<BaseFixture>::~VCDTestFixture()
{
	std::filesystem::current_path(m_cwd);
	//std::filesystem::remove_all("tmp/");
}

template<class BaseFixture>
void VCDTestFixture<BaseFixture>::prepRun()
{
	BaseFixture::prepRun();
	BaseFixture::recordVCD("test.vcd");
}

template<class BaseFixture>
bool VCDTestFixture<BaseFixture>::VCDContains(const std::regex &regex)
{
	BaseFixture::m_vcdSink.reset();
	std::fstream file("test.vcd", std::fstream::in);
	BOOST_TEST((bool) file);
	std::stringstream buffer;
	buffer << file.rdbuf();

	return std::regex_search(buffer.str(), regex);
}

template<class BaseFixture>
bool VCDTestFixture<BaseFixture>::GTKWaveProjectFileContains(const std::regex &regex)
{
	BaseFixture::m_vcdSink.reset();
	std::fstream file("test.vcd.gtkw", std::fstream::in);
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
	pinIn(myEnum).setName("myEnum");

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

BOOST_AUTO_TEST_SUITE_END()