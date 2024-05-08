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
//	return gtry::scl::IntelQuartusGlobalFixture::hasIntelQuartus();
	return true;
}


namespace {

template<class Fixture>
struct TestWithAgilexDevice : public Fixture
{
	TestWithAgilexDevice() {
		auto device = std::make_unique<gtry::scl::IntelDevice>();
		//device->setupMAX10();
		device->setupAgilex();
		//AGFB022R25A2E2V
		Fixture::design.setTargetTechnology(std::move(device));
	}
};

}



BOOST_AUTO_TEST_SUITE(IntelQuartusTests, * precondition(canRunQuartus))

BOOST_FIXTURE_TEST_CASE(fifoLutram, TestWithAgilexDevice<gtry::scl::IntelQuartusTestFixture>)
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
	BOOST_TEST(exportContains(std::regex{"read_during_write_mode_mixed_ports => \"DONT_CARE\""}));
}

BOOST_FIXTURE_TEST_CASE(fifoLutramSingleCycle, TestWithAgilexDevice<gtry::scl::IntelQuartusTestFixture>)
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
	BOOST_TEST(exportContains(std::regex{"read_during_write_mode_mixed_ports => \"NEW_DATA\""}));
}

BOOST_FIXTURE_TEST_CASE(fifoLutramFallthrough, TestWithAgilexDevice<gtry::scl::IntelQuartusTestFixture>)
{
	using namespace gtry;
	/*
	ALMs used in final placement: 49 (25.5)
	ALMs used for memory: 10
	M20Ks: 0
	Dedicated Logic Registers: 61 (38)
	*/


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
}

BOOST_FIXTURE_TEST_CASE(fifoBRam, TestWithAgilexDevice<gtry::scl::IntelQuartusTestFixture>)
{
	using namespace gtry;


	Clock clock({
		.absoluteFrequency = {{500'000'000, 1}},
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
}

BOOST_FIXTURE_TEST_CASE(fifoBRamFallthrough, TestWithAgilexDevice<gtry::scl::IntelQuartusTestFixture>)
{
	using namespace gtry;


	Clock clock({
		.absoluteFrequency = {{500'000'000, 1}},
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
}

BOOST_FIXTURE_TEST_CASE(fifoBRamLarge, TestWithAgilexDevice<gtry::scl::IntelQuartusTestFixture>)
{
	using namespace gtry;


	Clock clock({
		.absoluteFrequency = {{500'000'000, 1}},
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
}


BOOST_AUTO_TEST_SUITE_END()
