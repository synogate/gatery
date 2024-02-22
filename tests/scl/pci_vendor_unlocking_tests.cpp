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
#include <gatery/scl/stream/SimuHelpers.h>
#include <iostream>
#include <iomanip>

#include <gatery/scl/io/pci/pci.h>
#include <gatery/scl/sim/SimPci.h> 

using namespace boost::unit_test;
using namespace gtry;

BOOST_FIXTURE_TEST_CASE(ptile_vhdl_test, BoostUnitTestSimulationFixture)
{
	//auto device = std::make_unique<scl::IntelDevice>();
	//device->setupDevice("AGFB014R24B2E2V");
	//design.setTargetTechnology(std::move(device));
	//
	//PciePTile ptileInstance("ptile", PciePTile::Presets::Gen3x16_256());
	//
	//IntelResetIP rstInstance;
	//
	//ptileInstance.connectNInitDone(rstInstance.ninit_done());
	//
	//
	//Clock clk = ptileInstance.userClock();
	//ClockScope clkScp(clk);
	//
	//scl::strm::RvPacketStream<BVec, scl::Error, PciePTile::Header, PciePTile::Prefix> txStream(256_b);
	//
	//valid(txStream) = '0';
	//ptileInstance.tx(move(txStream));
	//
	//scl::strm::RvPacketStream rxStream(move(ptileInstance.rx()));
	//ready(rxStream) = '1';
	//
	//design.postprocess();
	//
	//if (false) {
	//	m_vhdlExport.emplace("dut_project/top.vhd");
	//	m_vhdlExport->targetSynthesisTool(new gtry::IntelQuartus());
	//	//m_vhdlExport->writeStandAloneProjectFile("export_thing.qsf");
	//	//m_vhdlExport->writeConstraintsFile("top_constraints.sdc");
	//	//m_vhdlExport->writeClocksFile("top_clocks.sdc");
	//	(*m_vhdlExport)(design.getCircuit());
	//	//outputVHDL("dut.vhd");
	//}
}



