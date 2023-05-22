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

#include <gatery/debug/websocks/WebSocksInterface.h>
#include <gatery/scl/synthesisTools/IntelQuartus.h>

#include <gatery/scl/tilelink/tilelink.h>
#include <gatery/scl/tilelink/TileLinkMasterModel.h>
#include <gatery/scl/Avalon.h>
#include <gatery/scl/TL2AMM.h>

using namespace boost::unit_test;
using namespace gtry;
using namespace gtry::scl;

scl::TileLinkUL ub2ul(scl::TileLinkUB& link)
{
	scl::TileLinkUL out;

	out.a = constructFrom(link.a);
	link.a <<= out.a;

	*out.d = constructFrom(*link.d);
	*out.d <<= *link.d;

	return out;
}
void setupAmmforEmif(AvalonMM amm) {
	amm.read.emplace();
	amm.readDataValid.emplace();
	amm.ready.emplace();
	amm.write.emplace();
	amm.address = 8_b;
	amm.byteEnable = 4_b;
	amm.writeData = 32_b;
	amm.readData = 32_b;
}

BOOST_FIXTURE_TEST_CASE(tl2amm_basic_test, BoostUnitTestSimulationFixture) {
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	AvalonMM out;
	setupAmmForEmif();
	


	scl::TileLinkUL in;

	scl::TileLinkMasterModel linkModel;
	linkModel.init("tlmm_", 8_b, 16_b, 1_b, 4_b);
	in <<= ub2ul(linkModel.getLink());

	pinOut(out.address, );

	addSimulationProcess([&]()->SimProcess {

		stopTest();
		});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 50, 1'000'000 }));
}
