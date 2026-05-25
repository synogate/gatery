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




BOOST_FIXTURE_TEST_CASE(crossport_readfirst_bram, TestWithDefaultDevice<Test_CrossPortOrdering>)
{
	using namespace gtry;

	readFirst = true;
	declareNoConflict = false;
	latency = 1;
	memType = MemType::MEDIUM;


	execute();
	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
}

BOOST_FIXTURE_TEST_CASE(crossport_writefirst_bram, TestWithDefaultDevice<Test_CrossPortOrdering>)
{
	using namespace gtry;

	readFirst = false;
	declareNoConflict = false;
	latency = 1;
	memType = MemType::MEDIUM;


	execute();
	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
}

BOOST_FIXTURE_TEST_CASE(crossport_dontcare_bram_1, TestWithDefaultDevice<Test_CrossPortOrdering>)
{
	using namespace gtry;

	readFirst = true;
	declareNoConflict = true;
	latency = 1;
	memType = MemType::MEDIUM;


	execute();
	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
	BOOST_TEST(exportContains(std::regex{"out_readPort_0_dataOut <= \\(others => 'X'\\);"}));
}

BOOST_FIXTURE_TEST_CASE(crossport_dontcare_bram_2, TestWithDefaultDevice<Test_CrossPortOrdering>)
{
	using namespace gtry;

	readFirst = false;
	declareNoConflict = true;
	latency = 1;
	memType = MemType::MEDIUM;


	execute();
	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
	BOOST_TEST(exportContains(std::regex{"out_readPort_1_dataOut <= \\(others => 'X'\\);"}));
}



BOOST_FIXTURE_TEST_CASE(crossport_readfirst_lutram, TestWithDefaultDevice<Test_CrossPortOrdering>)
{
	using namespace gtry;

	readFirst = true;
	declareNoConflict = false;
	latency = 0;
	memType = MemType::SMALL;


	execute();
	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
}

BOOST_FIXTURE_TEST_CASE(crossport_dontcare_lutram_1, TestWithDefaultDevice<Test_CrossPortOrdering>)
{
	using namespace gtry;

	readFirst = true;
	declareNoConflict = true;
	latency = 0;
	memType = MemType::SMALL;


	execute();
	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
}

BOOST_FIXTURE_TEST_CASE(crossport_dontcare_lutram_2, TestWithDefaultDevice<Test_CrossPortOrdering>)
{
	using namespace gtry;

	readFirst = false;
	declareNoConflict = true;
	latency = 0;
	memType = MemType::SMALL;


	execute();
	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
}







BOOST_FIXTURE_TEST_CASE(crossport_readfirst_bram_3cycle, TestWithDefaultDevice<Test_CrossPortOrdering>)
{
	using namespace gtry;

	readFirst = true;
	declareNoConflict = false;
	latency = 3;
	memType = MemType::MEDIUM;


	execute();
	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
}

BOOST_FIXTURE_TEST_CASE(crossport_writefirst_bram_3cycle, TestWithDefaultDevice<Test_CrossPortOrdering>)
{
	using namespace gtry;

	readFirst = false;
	declareNoConflict = false;
	latency = 3;
	memType = MemType::MEDIUM;


	execute();
	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
}

BOOST_FIXTURE_TEST_CASE(crossport_dontcare_bram_1_3cycle, TestWithDefaultDevice<Test_CrossPortOrdering>)
{
	using namespace gtry;

	readFirst = true;
	declareNoConflict = true;
	latency = 3;
	memType = MemType::MEDIUM;


	execute();
	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
}

BOOST_FIXTURE_TEST_CASE(crossport_dontcare_bram_2_3cycle, TestWithDefaultDevice<Test_CrossPortOrdering>)
{
	using namespace gtry;

	readFirst = false;
	declareNoConflict = true;
	latency = 3;
	memType = MemType::MEDIUM;


	execute();
	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
}



BOOST_FIXTURE_TEST_CASE(crossport_readfirst_lutram_3cycle, TestWithDefaultDevice<Test_CrossPortOrdering>)
{
	using namespace gtry;

	readFirst = true;
	declareNoConflict = false;
	latency = 3;
	memType = MemType::SMALL;


	execute();
	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
}

BOOST_FIXTURE_TEST_CASE(crossport_writefirst_lutram_3cycle, TestWithDefaultDevice<Test_CrossPortOrdering>)
{
	using namespace gtry;

	readFirst = false;
	declareNoConflict = false;
	latency = 3;
	memType = MemType::SMALL;


	execute();
	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
}

BOOST_FIXTURE_TEST_CASE(crossport_dontcare_lutram_1_3cycle, TestWithDefaultDevice<Test_CrossPortOrdering>)
{
	using namespace gtry;

	readFirst = true;
	declareNoConflict = true;
	latency = 3;
	memType = MemType::SMALL;


	execute();
	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
}

BOOST_FIXTURE_TEST_CASE(crossport_dontcare_lutram_2_3cycle, TestWithDefaultDevice<Test_CrossPortOrdering>)
{
	using namespace gtry;

	readFirst = false;
	declareNoConflict = true;
	latency = 3;
	memType = MemType::SMALL;


	execute();
	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
}


BOOST_FIXTURE_TEST_CASE(maskedWrite_byteEnable_noConflict, TestWithDefaultDevice<Test_MaskedWrite>)
{
	using namespace gtry;
	numElements = 32;
	elementWidth = 32_b;
	maskGranularity = 8;
	latency = 1;
	memType = MemType::DONT_CARE;
	declareNoConflict = true;
	execute();
	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
	BOOST_TEST(exportContains(std::regex{"FOR i IN"}));
}

BOOST_FIXTURE_TEST_CASE(maskedWrite_byteEnable_readFirst, TestWithDefaultDevice<Test_MaskedWrite>)
{
	using namespace gtry;
	numElements = 32;
	elementWidth = 32_b;
	maskGranularity = 8;
	latency = 1;
	memType = MemType::DONT_CARE;
	declareNoConflict = false;
	readFirst = true;
	execute();
	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
	BOOST_TEST(exportContains(std::regex{"FOR i IN"}));
}
/*
BOOST_FIXTURE_TEST_CASE(maskedWrite_byteEnable_writeFirst, TestWithDefaultDevice<Test_MaskedWrite>)
{
	using namespace gtry;
	numElements = 32;
	elementWidth = 32_b;
	maskGranularity = 8;
	latency = 1;
	memType = MemType::DONT_CARE;
	declareNoConflict = false;
	readFirst = false;
	execute();
	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
	BOOST_TEST(exportContains(std::regex{"FOR i IN"}));
}
*/

BOOST_FIXTURE_TEST_CASE(maskedWrite_wide, TestWithDefaultDevice<Test_MaskedWrite>)
{
	using namespace gtry;
	numElements = 64;
	elementWidth = 64_b;
	maskGranularity = 16;
	latency = 1;
	memType = MemType::DONT_CARE;
	execute();
	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
	BOOST_TEST(exportContains(std::regex{"FOR i IN"}));
}

BOOST_FIXTURE_TEST_CASE(maskedWrite_lutram_noConflict, TestWithDefaultDevice<Test_MaskedWrite>)
{
	using namespace gtry;
	numElements = 8;
	elementWidth = 16_b;
	maskGranularity = 8;
	latency = 0;
	memType = MemType::SMALL;
	declareNoConflict = true;
	execute();
	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
	BOOST_TEST(exportContains(std::regex{"FOR i IN"}));
}

BOOST_FIXTURE_TEST_CASE(maskedWrite_lutram_readFirst, TestWithDefaultDevice<Test_MaskedWrite>)
{
	using namespace gtry;
	numElements = 8;
	elementWidth = 16_b;
	maskGranularity = 8;
	latency = 0;
	memType = MemType::SMALL;
	declareNoConflict = false;
	readFirst = true;
	execute();
	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
	BOOST_TEST(exportContains(std::regex{"FOR i IN"}));
}


BOOST_FIXTURE_TEST_CASE(maskedWrite_blockram_noConflict, TestWithDefaultDevice<Test_MaskedWrite>)
{
	using namespace gtry;
	numElements = 512;
	elementWidth = 32_b;
	maskGranularity = 8;
	latency = 1;
	memType = MemType::MEDIUM;
	declareNoConflict = true;
	execute();
	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
	BOOST_TEST(exportContains(std::regex{"FOR i IN"}));
}

BOOST_FIXTURE_TEST_CASE(maskedWrite_blockram_readFirst, TestWithDefaultDevice<Test_MaskedWrite>)
{
	using namespace gtry;
	numElements = 512;
	elementWidth = 32_b;
	maskGranularity = 8;
	latency = 1;
	memType = MemType::MEDIUM;
	declareNoConflict = false;
	readFirst = true;
	execute();
	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
	BOOST_TEST(exportContains(std::regex{"FOR i IN"}));
}
/*
BOOST_FIXTURE_TEST_CASE(maskedWrite_blockram_writeFirst, TestWithDefaultDevice<Test_MaskedWrite>)
{
	using namespace gtry;
	numElements = 512;
	elementWidth = 32_b;
	maskGranularity = 8;
	latency = 1;
	memType = MemType::MEDIUM;
	declareNoConflict = false;
	readFirst = false;
	execute();
	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
	BOOST_TEST(exportContains(std::regex{"FOR i IN"}));
}
*/

BOOST_FIXTURE_TEST_CASE(crossport_fifo, gtry::GHDLTestFixture)
{
	using namespace gtry;

	Clock clock({
			.absoluteFrequency = {{125'000'000,1}},
			.memoryResetType = ClockConfig::ResetType::NONE,
	});
	HCL_NAMED(clock);
	ClockScope scp(clock);

	Bit wrEn = pinIn().setName("wrEn");
	UInt wrData = pinIn(8_b).setName("wrData");

	Bit rdEn = pinIn().setName("rdEn");

	scl::Fifo<UInt> fifo(8, 8_b);

	IF (wrEn)
		fifo.push(wrData);

	IF (rdEn)
		fifo.pop();

	UInt rdValue = fifo.peek();
	pinOut(rdValue).setName("rdValue");

	fifo.generate();

	addSimulationProcess([&,this]()->SimProcess {
		simu(wrEn) = '0';
		simu(rdEn) = '0';
		co_await AfterClk(clock);
		simu(wrEn) = '1';
		simu(wrData) = 42;
		co_await AfterClk(clock);
		co_await AfterClk(clock);

		stopTest();
	});

	runTest(Seconds{2000, 1} / clock.absoluteFrequency());

	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
}


#if 0

BOOST_FIXTURE_TEST_CASE(trueDualPort_lutram_noConflict, TestWithDefaultDevice<Test_TrueDualPort>)
{
	using namespace gtry;
	numElements = 16;
	elementWidth = 8_b;
	latency = 0;
	memType = MemType::SMALL;
	declareNoConflict = true;
	execute();
	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
}

BOOST_FIXTURE_TEST_CASE(trueDualPort_blockram_noConflict, TestWithDefaultDevice<Test_TrueDualPort>)
{
	using namespace gtry;
	numElements = 512;
	elementWidth = 16_b;
	latency = 1;
	memType = MemType::MEDIUM;
	declareNoConflict = true;
	execute();
	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
}

BOOST_FIXTURE_TEST_CASE(trueDualPort_blockram_noConflict_2cycle, TestWithDefaultDevice<Test_TrueDualPort>)
{
	using namespace gtry;
	numElements = 512;
	elementWidth = 16_b;
	latency = 2;
	memType = MemType::MEDIUM;
	declareNoConflict = true;
	execute();
	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
}

BOOST_FIXTURE_TEST_CASE(trueDualPort_dontcare, TestWithDefaultDevice<Test_TrueDualPort>)
{
	using namespace gtry;
	numElements = 32;
	elementWidth = 8_b;
	latency = 1;
	memType = MemType::DONT_CARE;
	declareNoConflict = true;
	execute();
	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
}



BOOST_FIXTURE_TEST_CASE(trueDualPort_maskedWrite_byteEnable_noConflict, TestWithDefaultDevice<Test_TrueDualPortMaskedWrite>)
{
	using namespace gtry;
	numElements = 32;
	elementWidth = 32_b;
	maskGranularity = 8;
	latency = 1;
	memType = MemType::DONT_CARE;
	declareNoConflict = true;
	execute();
	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
	BOOST_TEST(exportContains(std::regex{"FOR i IN"}));
}

BOOST_FIXTURE_TEST_CASE(trueDualPort_maskedWrite_wide, TestWithDefaultDevice<Test_TrueDualPortMaskedWrite>)
{
	using namespace gtry;
	numElements = 64;
	elementWidth = 64_b;
	maskGranularity = 16;
	latency = 1;
	memType = MemType::DONT_CARE;
	declareNoConflict = true;
	execute();
	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
	BOOST_TEST(exportContains(std::regex{"FOR i IN"}));
}

BOOST_FIXTURE_TEST_CASE(trueDualPort_maskedWrite_lutram_noConflict, TestWithDefaultDevice<Test_TrueDualPortMaskedWrite>)
{
	using namespace gtry;
	numElements = 8;
	elementWidth = 16_b;
	maskGranularity = 8;
	latency = 0;
	memType = MemType::SMALL;
	declareNoConflict = true;
	execute();
	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
	BOOST_TEST(exportContains(std::regex{"FOR i IN"}));
}

BOOST_FIXTURE_TEST_CASE(trueDualPort_maskedWrite_blockram_noConflict, TestWithDefaultDevice<Test_TrueDualPortMaskedWrite>)
{
	using namespace gtry;
	numElements = 512;
	elementWidth = 32_b;
	maskGranularity = 8;
	latency = 1;
	memType = MemType::MEDIUM;
	declareNoConflict = true;
	execute();
	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
	BOOST_TEST(exportContains(std::regex{"FOR i IN"}));
}



BOOST_FIXTURE_TEST_CASE(trueDualPort_rmw_maskedWrite_byteEnable, TestWithDefaultDevice<Test_TrueDualPortRMWMaskedWrite>)
{
	using namespace gtry;
	numElements = 32;
	elementWidth = 32_b;
	maskGranularity = 8;
	latency = 1;
	memType = MemType::DONT_CARE;
	declareNoConflict = true;
	execute();
	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
	BOOST_TEST(exportContains(std::regex{"FOR i IN"}));
}

BOOST_FIXTURE_TEST_CASE(trueDualPort_rmw_maskedWrite_lutram, TestWithDefaultDevice<Test_TrueDualPortRMWMaskedWrite>)
{
	using namespace gtry;
	numElements = 16;
	elementWidth = 16_b;
	maskGranularity = 8;
	latency = 0;
	memType = MemType::SMALL;
	declareNoConflict = true;
	execute();
	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
	BOOST_TEST(exportContains(std::regex{"FOR i IN"}));
}

BOOST_FIXTURE_TEST_CASE(trueDualPort_rmw_maskedWrite_blockram, TestWithDefaultDevice<Test_TrueDualPortRMWMaskedWrite>)
{
	using namespace gtry;
	numElements = 512;
	elementWidth = 32_b;
	maskGranularity = 8;
	latency = 1;
	memType = MemType::MEDIUM;
	declareNoConflict = true;
	execute();
	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
	BOOST_TEST(exportContains(std::regex{"FOR i IN"}));
}

BOOST_FIXTURE_TEST_CASE(trueDualPort_rmw_maskedWrite_blockram_2cycle, TestWithDefaultDevice<Test_TrueDualPortRMWMaskedWrite>)
{
	using namespace gtry;
	numElements = 512;
	elementWidth = 32_b;
	maskGranularity = 8;
	latency = 2;
	memType = MemType::MEDIUM;
	declareNoConflict = true;
	execute();
	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
	BOOST_TEST(exportContains(std::regex{"FOR i IN"}));
}

#endif

BOOST_AUTO_TEST_SUITE_END()
