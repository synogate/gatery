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

#include <gatery/scl/synthesisTools/IntelQuartusTestFixture.h>
#include <gatery/scl/arch/intel/IntelDevice.h>

#include <gatery/scl/stream/strm.h>


#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>


using namespace boost::unit_test;

boost::test_tools::assertion_result canRunQuartus(boost::unit_test::test_unit_id)
{
	return gtry::IntelQuartusGlobalFixture::hasIntelQuartus();
}


namespace {

template<class Fixture>
struct TestWithCycloneDevice : public Fixture
{
	TestWithCycloneDevice() {
		auto device = std::make_unique<gtry::scl::IntelDevice>();
		device->setupCyclone10();
		Fixture::design.setTargetTechnology(std::move(device));
	}
};

}



BOOST_AUTO_TEST_SUITE(IntelQuartusTests, * precondition(canRunQuartus))

BOOST_FIXTURE_TEST_CASE(fifoLutram, TestWithCycloneDevice<gtry::IntelQuartusTestFixture>)
{
	using namespace gtry;

	Clock clock({
		.absoluteFrequency = {{500'000'000, 1}},
	});
	ClockScope clockScope(clock);

	scl::RvStream<BVec> inputStream = { 8_b };
	pinIn(inputStream, "inputStream");

	auto outputStream = move(inputStream) | scl::strm::regDecouple() | scl::strm::fifo(8) | scl::strm::regDecouple();
	pinOut(outputStream, "outputStream");

	testCompilation();
	BOOST_TEST(exportContains(std::regex{"altera_mf.altera_mf_components.altdpram"}));
	BOOST_TEST(exportContains(std::regex{"ram_block_type => \"MLAB\""}));
	BOOST_TEST(!exportContains(std::regex{"out_conflict_bypass_mux"}));
	BOOST_TEST(exportContains(std::regex{"read_during_write_mode_mixed_ports => \"DONT_CARE\""}));
	
	BOOST_TEST(timingMet(clock));
	BOOST_TEST(getFitterResourceUtilization("scl_fifo0").ALMsForMemory.inclChildren == 10.0);
	BOOST_TEST(getFitterResourceUtilization("scl_fifo0").M20Ks == 0);
	BOOST_TEST(getFitterResourceUtilization("scl_fifo0").ALMsNeeded.inclChildren < 20.0);
	BOOST_TEST(getFitterResourceUtilization("scl_fifo0").dedicatedLogicRegisters.inclChildren < 20.0);
}

BOOST_FIXTURE_TEST_CASE(fifoLutramSingleCycle, TestWithCycloneDevice<gtry::IntelQuartusTestFixture>)
{
	using namespace gtry;


	Clock clock({
		.absoluteFrequency = {{500'000'000, 1}},
	});
	ClockScope clockScope(clock);

	scl::RvStream<BVec> inputStream = { 8_b };
	pinIn(inputStream, "inputStream");

	auto outputStream = move(inputStream) | scl::strm::regDecouple() | scl::strm::fifo(8, scl::FifoLatency(1)) | scl::strm::regDecouple();
	pinOut(outputStream, "outputStream");

	testCompilation();
	BOOST_TEST(exportContains(std::regex{"altera_mf.altera_mf_components.altdpram"}));
	BOOST_TEST(exportContains(std::regex{"ram_block_type => \"MLAB\""}));
	// This is also DONT_CARE + hazard logic (for a virtual new_data mode) because the mlab would only be able to do new_data with an output register, but that
	// is the register needed for retiming the read-port to be on the same cycle as the write.
	BOOST_TEST(exportContains(std::regex{"out_conflict_bypass_mux"}));
	BOOST_TEST(exportContains(std::regex{"read_during_write_mode_mixed_ports => \"DONT_CARE\""}));

	BOOST_TEST(timingMet(clock));
	BOOST_TEST(getFitterResourceUtilization("scl_fifo0").ALMsForMemory.inclChildren == 10.0);
	BOOST_TEST(getFitterResourceUtilization("scl_fifo0").M20Ks == 0);
	BOOST_TEST(getFitterResourceUtilization("scl_fifo0").ALMsNeeded.inclChildren < 20.0);
	BOOST_TEST(getFitterResourceUtilization("scl_fifo0").dedicatedLogicRegisters.inclChildren < 25.0);

}

BOOST_FIXTURE_TEST_CASE(fifoLutramFallthrough, TestWithCycloneDevice<gtry::IntelQuartusTestFixture>)
{
	using namespace gtry;

	Clock clock({
		.absoluteFrequency = {{500'000'000, 1}},
	});
	ClockScope clockScope(clock);

	scl::RvStream<BVec> inputStream = { 8_b };
	pinIn(inputStream, "inputStream");

	auto outputStream = move(inputStream) | scl::strm::regDecouple() | scl::strm::fifo(8, gtry::scl::FifoLatency(0)) | scl::strm::regDecouple();
	pinOut(outputStream, "outputStream");

	testCompilation();
	BOOST_TEST(exportContains(std::regex{"altera_mf.altera_mf_components.altdpram"}));
	BOOST_TEST(exportContains(std::regex{"ram_block_type => \"MLAB\""}));
	BOOST_TEST(exportContains(std::regex{"read_during_write_mode_mixed_ports => \"DONT_CARE\""}));
	
	BOOST_TEST(timingMet(clock));
	BOOST_TEST(getFitterResourceUtilization("scl_fifo0").ALMsForMemory.inclChildren == 10.0);
	BOOST_TEST(getFitterResourceUtilization("scl_fifo0").M20Ks == 0);
	BOOST_TEST(getFitterResourceUtilization("scl_fifo0").ALMsNeeded.inclChildren < 25.0);
	BOOST_TEST(getFitterResourceUtilization("scl_fifo0").dedicatedLogicRegisters.inclChildren < 25.0);
}

BOOST_FIXTURE_TEST_CASE(fifoBRam, TestWithCycloneDevice<gtry::IntelQuartusTestFixture>)
{
	using namespace gtry;


	Clock clock({
		.absoluteFrequency = {{400'000'000, 1}},
	});
	ClockScope clockScope(clock);

	scl::RvStream<BVec> inputStream = { 8_b };
	pinIn(inputStream, "inputStream");

	auto outputStream = move(inputStream) | scl::strm::regDecouple() | scl::strm::fifo(4096) | scl::strm::regDecouple();
	pinOut(outputStream, "outputStream");

	testCompilation();
	BOOST_TEST(exportContains(std::regex{"altera_mf.altera_mf_components.altsyncram"}));
	BOOST_TEST(exportContains(std::regex{"ram_block_type => \"M20K\""}));
	BOOST_TEST(exportContains(std::regex{"read_during_write_mode_mixed_ports => \"DONT_CARE\""}));
	BOOST_TEST(exportContains(std::regex{"read_during_write_mode_port_a => \"DONT_CARE\""}));
	BOOST_TEST(exportContains(std::regex{"read_during_write_mode_port_b => \"DONT_CARE\""}));
	
	BOOST_TEST(timingMet(clock));
	BOOST_TEST(getFitterResourceUtilization("scl_fifo0").ALMsForMemory.inclChildren == 0.0);
	BOOST_TEST(getFitterResourceUtilization("scl_fifo0").M20Ks == (8*4 + 20-1) / 20);
	BOOST_TEST(getFitterResourceUtilization("scl_fifo0").ALMsNeeded.inclChildren < 25.0);
	BOOST_TEST(getFitterResourceUtilization("scl_fifo0").dedicatedLogicRegisters.inclChildren < 30.0);
}

BOOST_FIXTURE_TEST_CASE(fifoBRamFallthrough, TestWithCycloneDevice<gtry::IntelQuartusTestFixture>)
{
	using namespace gtry;


	Clock clock({
		.absoluteFrequency = {{300'000'000, 1}},
	});
	ClockScope clockScope(clock);

	scl::RvStream<BVec> inputStream = { 8_b };
	pinIn(inputStream, "inputStream");

	auto outputStream = move(inputStream) | scl::strm::regDecouple() | scl::strm::fifo(4096, gtry::scl::FifoLatency(0)) | scl::strm::regDecouple();
	pinOut(outputStream, "outputStream");

	testCompilation();

	BOOST_TEST(exportContains(std::regex{"altera_mf.altera_mf_components.altsyncram"}));
	BOOST_TEST(exportContains(std::regex{"ram_block_type => \"M20K\""}));
	BOOST_TEST(exportContains(std::regex{"read_during_write_mode_mixed_ports => \"DONT_CARE\""}));
	BOOST_TEST(exportContains(std::regex{"read_during_write_mode_port_a => \"DONT_CARE\""}));
	BOOST_TEST(exportContains(std::regex{"read_during_write_mode_port_b => \"DONT_CARE\""}));
	
	BOOST_TEST(timingMet(clock));
	BOOST_TEST(getFitterResourceUtilization("scl_fifo0").ALMsForMemory.inclChildren == 0.0);
	BOOST_TEST(getFitterResourceUtilization("scl_fifo0").M20Ks == (8*4 + 20-1) / 20);
	BOOST_TEST(getFitterResourceUtilization("scl_fifo0").ALMsNeeded.inclChildren < 30.0);
	BOOST_TEST(getFitterResourceUtilization("scl_fifo0").dedicatedLogicRegisters.inclChildren < 40.0);
}

BOOST_FIXTURE_TEST_CASE(fifoBRamLarge, TestWithCycloneDevice<gtry::IntelQuartusTestFixture>)
{
	using namespace gtry;


	Clock clock({
		.absoluteFrequency = {{400'000'000, 1}},
	});
	ClockScope clockScope(clock);

	scl::RvStream<BVec> inputStream = { 256_b };
	pinIn(inputStream, "inputStream");

	auto outputStream = move(inputStream) | scl::strm::regDecouple() | scl::strm::fifo(1024) | scl::strm::regDecouple();
	pinOut(outputStream, "outputStream");

	testCompilation();

	BOOST_TEST(exportContains(std::regex{"altera_mf.altera_mf_components.altsyncram"}));
	BOOST_TEST(exportContains(std::regex{"ram_block_type => \"M20K\""}));
	BOOST_TEST(exportContains(std::regex{"read_during_write_mode_mixed_ports => \"DONT_CARE\""}));
	BOOST_TEST(exportContains(std::regex{"read_during_write_mode_port_a => \"DONT_CARE\""}));
	BOOST_TEST(exportContains(std::regex{"read_during_write_mode_port_b => \"DONT_CARE\""}));
	
	BOOST_TEST(timingMet(clock));
	BOOST_TEST(getFitterResourceUtilization("scl_fifo0").ALMsForMemory.inclChildren == 0.0);
	BOOST_TEST(getFitterResourceUtilization("scl_fifo0").M20Ks == (256 + 20-1) / 20);
	BOOST_TEST(getFitterResourceUtilization("scl_fifo0").ALMsNeeded.inclChildren < 25.0);
	BOOST_TEST(getFitterResourceUtilization("scl_fifo0").dedicatedLogicRegisters.inclChildren < 25.0);
}


BOOST_AUTO_TEST_SUITE_END()
