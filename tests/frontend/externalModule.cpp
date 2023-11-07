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


namespace {

boost::test_tools::assertion_result canExport(boost::unit_test::test_unit_id)
{
	return gtry::GHDLGlobalFixture::hasGHDL();
}

}

using namespace gtry;

BOOST_AUTO_TEST_SUITE(ExternalModuleTests, * precondition(canExport))

struct BigDut : public gtry::GHDLTestFixture
{
	public:
		void build() {
			ExternalModule dut("TestEntity", "work");

				dut.generic("generic_int") = 5;
				dut.generic("generic_natural") = (size_t)10;
				dut.generic("generic_real") = 2.0;
				dut.generic("generic_string") = "string";
				dut.generic("generic_boolean").setBoolean(true);
				dut.generic("generic_bit").setBit(true, PinType::BIT);
				dut.generic("generic_logic").setBit(true, PinType::STD_LOGIC);
				dut.generic("generic_ulogic").setBit(true, PinType::STD_ULOGIC);

				dut.generic("generic_bitvector").setBitVector(8, 42, PinType::BIT);
				dut.generic("generic_logic_vector").setBitVector(8, 42, PinType::STD_LOGIC);
				dut.generic("generic_ulogic_vector").setBitVector(8, 42, PinType::STD_ULOGIC);


				{
					Clock clockA({.absoluteFrequency=Seconds{100'000'000,1}, .name = "clock_A"});
					ClockScope scope(clockA);

					dut.clockIn("clock_port_a", "clock_port_a_reset");
					dut.in("in_bit", {.type = PinType::BIT}) = pinIn().setName("in_1");
					dut.in("in_stdlogic", {.type = PinType::STD_LOGIC}) = pinIn().setName("in_2");
					dut.in("in_stdulogic", {.type = PinType::STD_ULOGIC}) = pinIn().setName("in_3");
				}

				{
					Clock clockB = dut.clockOut("clock_port_b", {}, {.absoluteFrequency=Seconds{200'000'000,1}, .name = "clock_B"});
					ClockScope scope(clockB);

					dut.in("in_bit_vector", 10_b, {.type = PinType::BIT}) = (BVec)pinIn(10_b).setName("in_4");
					dut.in("in_stdlogic_vector", 10_b, {.type = PinType::STD_LOGIC}) = (BVec)pinIn(10_b).setName("in_5");
					dut.in("in_stdulogic_vector", 10_b, {.type = PinType::STD_ULOGIC}) = (BVec)pinIn(10_b).setName("in_6");

					pinOut(dut.out("out_bit", {.type = PinType::BIT})).setName("out_1");
					pinOut(dut.out("out_stdlogic", {.type = PinType::STD_LOGIC})).setName("out_2");
					pinOut(dut.out("out_stdulogic", {.type = PinType::STD_ULOGIC})).setName("out_3");

					pinOut(dut.out("out_bit_vector", 10_b, {.type = PinType::BIT})).setName("out_4");
					pinOut(dut.out("out_stdlogic_vector", 10_b, {.type = PinType::STD_LOGIC})).setName("out_5");
					pinOut(dut.out("out_stdulogic_vector", 10_b, {.type = PinType::STD_ULOGIC})).setName("out_6");
				}

				addCustomVHDL("TestEntity", R"Delim(
					LIBRARY ieee;
					USE ieee.std_logic_1164.ALL;
					USE ieee.numeric_std.all;

					ENTITY TestEntity IS 
						GENERIC (
							generic_int : integer;
							generic_natural : natural;
							generic_real : real;
							generic_string : string;
							generic_boolean : boolean;
							generic_bit : bit;
							generic_logic : std_logic;
							generic_ulogic : std_ulogic;
							generic_bitvector : bit_vector(7 downto 0);
							generic_logic_vector : std_logic_vector(7 downto 0);
							generic_ulogic_vector : std_ulogic_vector(7 downto 0)
						);
						PORT(
							clock_port_a : in std_logic;
							clock_port_a_reset : in std_logic;

							in_bit : in BIT;
							in_stdlogic : in STD_LOGIC;
							in_stdulogic : in STD_ULOGIC;
							
							clock_port_b : out STD_LOGIC;
							in_bit_vector : in BIT_VECTOR(9 downto 0);
							in_stdlogic_vector : in STD_LOGIC_VECTOR(9 downto 0);
							in_stdulogic_vector : in STD_ULOGIC_VECTOR(9 downto 0);

							out_bit : out BIT;
							out_stdlogic : out STD_LOGIC;
							out_stdulogic : out STD_ULOGIC;
							out_bit_vector : out BIT_VECTOR(9 downto 0);
							out_stdlogic_vector : out STD_LOGIC_VECTOR(9 downto 0);
							out_stdulogic_vector : out STD_ULOGIC_VECTOR(9 downto 0)
						);
					END TestEntity;

					ARCHITECTURE impl OF TestEntity IS 
					BEGIN
					END impl;
				)Delim");		
		}
};


BOOST_FIXTURE_TEST_CASE(CompilationTest, BigDut)
{
	
	build();
	testCompilation();
	

	BOOST_TEST(exportContains(std::regex{"clock_A : IN STD_LOGIC"})); // We have clock_A
	BOOST_TEST(exportContains(std::regex{"reset : IN STD_LOGIC"}));   // and a reset
	BOOST_TEST(!exportContains(std::regex{"clock_B : IN STD_LOGIC"})); // We dno't have clock_B created by TestEntity

	BOOST_TEST(exportContains(std::regex{":\\s*entity\\s+work.TestEntity"})); // We instantiate the test entity 
}


BOOST_FIXTURE_TEST_CASE(CompilationTest_deep_hierarchy, BigDut)
{
	{
		Area area1("area1", true);
		Area area2("area2", true);
		build();
	}
	testCompilation();
	

	BOOST_TEST(exportContains(std::regex{"clock_A : IN STD_LOGIC"})); // We have clock_A
	BOOST_TEST(exportContains(std::regex{"reset : IN STD_LOGIC"}));   // and a reset
	BOOST_TEST(!exportContains(std::regex{"clock_B : IN STD_LOGIC"})); // We dno't have clock_B created by TestEntity

	BOOST_TEST(exportContains(std::regex{":\\s*entity\\s+work.TestEntity"})); // We instantiate the test entity 
}



struct BiDirDut : public gtry::GHDLTestFixture
{
	public:
		void build() {

			ExternalModule dut("TestEntity", "work");
			{
				Clock clockA({.absoluteFrequency=Seconds{100'000'000,1}, .name = "clock_A"});
				ClockScope scope(clockA);

				dut.clockIn("clock_port_a", "clock_port_a_reset");
				dut.in("in_bit", {.type = PinType::BIT}) = pinIn().setName("in_1");
				dut.inoutPin("inout_bit", "pin_inout_bit", {.type = PinType::STD_LOGIC});
				dut.inoutPin("inout_bvec", "pin_inout_bvec", 10_b, {.type = PinType::STD_LOGIC});
			}

			addCustomVHDL("TestEntity", R"Delim(

				LIBRARY ieee;
				USE ieee.std_logic_1164.ALL;
				USE ieee.numeric_std.all;

				ENTITY TestEntity IS 
					PORT(
						clock_port_a : in std_logic;
						clock_port_a_reset : in std_logic;

						in_bit : in BIT;
						inout_bit : inout STD_LOGIC;
						inout_bvec : inout STD_LOGIC_VECTOR(9 downto 0)
					);
				END TestEntity;

				ARCHITECTURE impl OF TestEntity IS 
				BEGIN
				END impl;

			)Delim");
		}
};


BOOST_FIXTURE_TEST_CASE(BidirTest, BiDirDut)
{
	build();
	
	testCompilation();

	//design.visualize("test");
	

	BOOST_TEST(exportContains(std::regex{"clock_A : IN STD_LOGIC"})); // We have clock_A
	BOOST_TEST(exportContains(std::regex{"reset : IN STD_LOGIC"}));   // and a reset
	BOOST_TEST(exportContains(std::regex{":\\s*entity\\s+work.TestEntity"})); // We instantiate the test entity 
}



BOOST_FIXTURE_TEST_CASE(BidirTest_deep_hierarchy, BiDirDut)
{
	{
		Area area1("area1", true);
		Area area2("area2", true);
		build();
	}
	
	testCompilation();

	//design.visualize("test");
	

	BOOST_TEST(exportContains(std::regex{"clock_A : IN STD_LOGIC"})); // We have clock_A
	BOOST_TEST(exportContains(std::regex{"reset : IN STD_LOGIC"}));   // and a reset
	BOOST_TEST(exportContains(std::regex{":\\s*entity\\s+work.TestEntity"})); // We instantiate the test entity 
}


BOOST_AUTO_TEST_SUITE_END()
