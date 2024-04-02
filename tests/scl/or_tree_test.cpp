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
#include "scl/pch.h"

#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>


#include <gatery/scl/stream/utils.h>
#include <gatery/scl/OrTree.h>

using namespace gtry;

BOOST_FIXTURE_TEST_CASE(or_tree_simple_test, BoostUnitTestSimulationFixture)
{
	Clock clk({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clk);

	BitWidth dataW = 4_b;
	size_t numberOfStreams = 11;

	Vector<scl::VStream<UInt>> inVec;

	for (size_t i = 0; i < numberOfStreams; i++) {
		inVec.emplace_back(ConstUInt(i, dataW));
		setName(*inVec[i], "data_" + std::to_string(i));
		pinIn(valid(inVec[i]), "in_" + std::to_string(i));
	}

							
	scl::OrTree<scl::VStream<UInt>> orTree;

	for (auto& in : inVec)
		IF(valid(in))
			orTree.attach(in);

	auto out = orTree.generate();
	pinOut(out, "out");
	
	//sequentially send 
	addSimulationProcess([&, this]()->SimProcess {
		for (auto& in : inVec)
			simu(valid(in)) = '0';
		co_await OnClk(clk);
		for (auto& in : inVec) {
			simu(valid(in)) = '1';
			co_await OnClk(clk);
			simu(valid(in)) = '0';
		}
	});

	//sequentially receive
	addSimulationProcess([&, this]()->SimProcess {
		for (size_t i = 0; i < numberOfStreams; i++) {
			co_await performTransferWait(out, clk);
			BOOST_TEST(simu(*out) == i);
		}
		co_await OnClk(clk);
		stopTest();
	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 1, 1'000'000 }));
}	
