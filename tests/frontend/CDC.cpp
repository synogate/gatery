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
#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

#include <gatery/hlim/postprocessing/CDCDetection.h>
#include <gatery/hlim/Subnet.h>
#include <gatery/hlim/coreNodes/Node_Register.h>
#include <gatery/hlim/supportNodes/Node_MemPort.h>

using namespace boost::unit_test;

using BoostUnitTestSimulationFixture = gtry::BoostUnitTestSimulationFixture;


BOOST_FIXTURE_TEST_CASE(unintentionalCDCDetection, gtry::BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	Clock clock1({ .absoluteFrequency = 10'000 });
	Clock clock2({ .absoluteFrequency = 10'000 });

	UInt a = 8_b;
	UInt b = 8_b;

	{
		ClockScope clockScope(clock1);
		a = reg(b, 0);
	}
	
	{
		ClockScope clockScope(clock2);
		b = reg(a, 0);
	}

	size_t detections = 0;

	gtry::hlim::detectUnguardedCDCCrossings(
		design.getCircuit(), 
		gtry::hlim::ConstSubnet::all(design.getCircuit()), 
		[&detections](const gtry::hlim::BaseNode* node){

		BOOST_TEST(dynamic_cast<const gtry::hlim::Node_Register*>(node) != nullptr);
		detections++;
	});

	BOOST_TEST(detections == 2);
}

BOOST_FIXTURE_TEST_CASE(unintentionalCDCDetectionMemory, gtry::BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	Clock clock1({ .absoluteFrequency = 10'000 });
	Clock clock2({ .absoluteFrequency = 10'000 });

	UInt a = 8_b;
	UInt b = 8_b;

	Memory<UInt> mem(42, 8_b);

	{
		ClockScope clockScope(clock1);
		a = mem[a];
		a = reg(a, 0);
	}
	
	{
		ClockScope clockScope(clock2);
		mem[b] = b;
		b += 1;
		b = reg(b, 0);
	}

	size_t detections = 0;

	gtry::hlim::detectUnguardedCDCCrossings(
		design.getCircuit(), 
		gtry::hlim::ConstSubnet::all(design.getCircuit()), 
		[&detections](const gtry::hlim::BaseNode* node){

		BOOST_TEST(dynamic_cast<const gtry::hlim::Node_MemPort*>(node) != nullptr);
		detections++;
	});

	BOOST_TEST(detections != 0);
}

BOOST_FIXTURE_TEST_CASE(noUnintentionalCDCDetectionMemoryNoConflict, gtry::BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	Clock clock1({ .absoluteFrequency = 10'000 });
	Clock clock2({ .absoluteFrequency = 10'000 });

	UInt a = "8b0";
	UInt b = "8b0";

	Memory<UInt> mem(42, 8_b);
	mem.noConflicts();

	{
		ClockScope clockScope(clock1);
		a = mem[a];
	}
	
	{
		ClockScope clockScope(clock2);
		mem[b] = b;
		b += 1;
		b = reg(b);
	}

	size_t detections = 0;

	gtry::hlim::detectUnguardedCDCCrossings(
		design.getCircuit(), 
		gtry::hlim::ConstSubnet::all(design.getCircuit()), 
		[&detections](const gtry::hlim::BaseNode* node){

		detections++;
	});

	BOOST_TEST(detections == 0);
}


BOOST_FIXTURE_TEST_CASE(intentionalCDCDetection, gtry::BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	Clock clock1({ .absoluteFrequency = 10'000 });
	Clock clock2({ .absoluteFrequency = 10'000 });

	UInt a = 8_b;
	UInt b = 8_b;

	{
		ClockScope clockScope(clock1);
		a = reg(b, 0);
	}

	a = allowClockDomainCrossing(a, clock1, clock2);
	
	{
		ClockScope clockScope(clock2);
		b = reg(a, 0);
	}

	b = allowClockDomainCrossing(b, clock2, clock1);

	size_t detections = 0;

	gtry::hlim::detectUnguardedCDCCrossings(
		design.getCircuit(), 
		gtry::hlim::ConstSubnet::all(design.getCircuit()), 
		[&detections](const gtry::hlim::BaseNode* node){

		detections++;
	});

	BOOST_TEST(detections == 0);
}

