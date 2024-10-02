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
#include "scl/pch.h"

#include "MappingTests_IO.h"
#include "MappingTests_Memory.h"

#include <gatery/frontend/GHDLTestFixture.h>
#include <gatery/scl/utils/GlobalBuffer.h>

#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>


using namespace boost::unit_test;


namespace {

template<class Fixture>
struct TestWithDefaultDevice : public Fixture
{
	TestWithDefaultDevice() {

	}
};

boost::test_tools::assertion_result canExport(boost::unit_test::test_unit_id)
{
	return gtry::GHDLGlobalFixture::hasGHDL();
}

}


BOOST_AUTO_TEST_SUITE(GenericTechMapping, * precondition(canExport))






BOOST_FIXTURE_TEST_CASE(scl_ddr, TestWithDefaultDevice<Test_ODDR>)
{
	execute();
}



BOOST_FIXTURE_TEST_CASE(scl_ddr_for_clock, TestWithDefaultDevice<Test_ODDR_ForClock>)
{
	execute();
}




BOOST_FIXTURE_TEST_CASE(histogram_noAddress, TestWithDefaultDevice<Test_Histogram>)
{
	using namespace gtry;
	numBuckets = 1;
	bucketWidth = 8_b;
	execute();
	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
}




BOOST_FIXTURE_TEST_CASE(lutram_1, TestWithDefaultDevice<Test_Histogram>)
{
	using namespace gtry;
	numBuckets = 4;
	bucketWidth = 8_b;
	execute();
	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
}

BOOST_FIXTURE_TEST_CASE(lutram_1_noEnable, TestWithDefaultDevice<Test_Histogram>)
{
	using namespace gtry;
	numBuckets = 4;
	bucketWidth = 8_b;
	forceNoEnable = true;
	execute();
	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
}


BOOST_FIXTURE_TEST_CASE(lutram_1_resetLogic, TestWithDefaultDevice<Test_Histogram>)
{
	using namespace gtry;
	numBuckets = 4;
	bucketWidth = 8_b;
	forceMemoryResetLogic = true;
	execute();
	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
}

BOOST_FIXTURE_TEST_CASE(lutram_1_resetLogic_noEnable, TestWithDefaultDevice<Test_Histogram>)
{
	using namespace gtry;
	numBuckets = 4;
	bucketWidth = 8_b;
	forceMemoryResetLogic = true;
	forceNoEnable = true;
	execute();
	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
}

BOOST_FIXTURE_TEST_CASE(lutram_2, TestWithDefaultDevice<Test_Histogram>)
{
	using namespace gtry;
	numBuckets = 32;
	bucketWidth = 8_b;
	execute();
	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
}

BOOST_FIXTURE_TEST_CASE(blockram_1, TestWithDefaultDevice<Test_Histogram>)
{
	using namespace gtry;
	numBuckets = 512;
	bucketWidth = 8_b;
	execute();
	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
}

BOOST_FIXTURE_TEST_CASE(blockram_2, TestWithDefaultDevice<Test_Histogram>)
{
	using namespace gtry;
	numBuckets = 512;
	iterationFactor = 4;
	bucketWidth = 32_b;
	execute();
	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
}

BOOST_FIXTURE_TEST_CASE(blockram_cascade, TestWithDefaultDevice<Test_MemoryCascade>)
{
	using namespace gtry;
	execute();
	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
}

BOOST_FIXTURE_TEST_CASE(external_high_latency, TestWithDefaultDevice<Test_Histogram>)
{
	using namespace gtry;
	numBuckets = 128;
	iterationFactor = 10;
	bucketWidth = 16_b;
	highLatencyExternal = true;
	execute();
	BOOST_TEST(exportContains(std::regex{"rd_address : OUT STD_LOGIC_VECTOR[\\S\\s]*rd_readdata : IN STD_LOGIC_VECTOR[\\S\\s]*wr_address : OUT STD_LOGIC_VECTOR[\\S\\s]*wr_writedata : OUT STD_LOGIC_VECTOR[\\S\\s]*wr_write"}));
	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
}


BOOST_FIXTURE_TEST_CASE(readEnable, TestWithDefaultDevice<Test_ReadEnable>)
{
	using namespace gtry;
	execute();
}

BOOST_FIXTURE_TEST_CASE(readEnable_bram_2Cycle, TestWithDefaultDevice<Test_ReadEnable>)
{
	using namespace gtry;
	twoCycleLatencyBRam = true;
	execute();
}

BOOST_AUTO_TEST_SUITE_END()
