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

#include <gatery/simulation/Simulator.h>

#include <gatery/scl/memory/sdram.h>
#include <gatery/scl/memory/SdramTimer.h>

using namespace boost::unit_test;
using namespace gtry;

BOOST_FIXTURE_TEST_CASE(sdram_module_simulation_test, ClockedTest)
{
	using namespace gtry::scl::sdram;

	CommandBus bus{
		.a = 12_b,
		.ba = 2_b,
		.dq = 16_b,
		.dqm = 2_b,
	};
	pinIn(bus, "SDRAM");

	BVec dq = moduleSimulation(bus);
	pinOut(dq).setName("SDRAM_DQ_OUT");

	addSimulationProcess([=]()->SimProcess {

		simu(bus.cke) = 0;
		simu(bus.csn) = 1;
		simu(bus.rasn) = 1;
		simu(bus.casn) = 1;
		simu(bus.wen) = 1;
		co_await WaitClk(clock());

		// set Mode Register
		simu(bus.cke) = 1;
		simu(bus.csn) = 0;
		simu(bus.rasn) = 0;
		simu(bus.casn) = 0;
		simu(bus.wen) = 0;

		simu(bus.ba) = 0;
		simu(bus.a) = 0;
		co_await WaitClk(clock());
		simu(bus.csn) = 1;
		co_await WaitClk(clock());

		// set Extended Mode Register
		simu(bus.csn) = 0;
		simu(bus.ba) = 1;
		simu(bus.a) = 0;
		co_await WaitClk(clock());
		simu(bus.csn) = 1;
		co_await WaitClk(clock());

		// precharge bank 1
		simu(bus.csn) = 0;
		simu(bus.rasn) = 0;
		simu(bus.casn) = 1;
		simu(bus.wen) = 0;
		simu(bus.a) = 0;
		co_await WaitClk(clock());
		simu(bus.csn) = 1;
		for (size_t i = 0; i < 4; ++i)
			co_await WaitClk(clock());

		// precharge all
		simu(bus.csn) = 0;
		simu(bus.rasn) = 0;
		simu(bus.casn) = 1;
		simu(bus.wen) = 0;
		simu(bus.a) = 1 << 10;
		co_await WaitClk(clock());
		simu(bus.csn) = 1;
		for(size_t i = 0; i < 4; ++i)
			co_await WaitClk(clock());


		// RAS
		simu(bus.csn) = 0;
		simu(bus.rasn) = 0;
		simu(bus.casn) = 1;
		simu(bus.wen) = 1;
		simu(bus.a) = 1;
		co_await WaitClk(clock());
		simu(bus.csn) = 1;
		for (size_t i = 0; i < 2; ++i)
			co_await WaitClk(clock());

		// Write CAS
		simu(bus.csn) = 0;
		simu(bus.rasn) = 1;
		simu(bus.casn) = 0;
		simu(bus.wen) = 0;
		simu(bus.a) = 2;
		simu(bus.dqm) = 1;
		simu(bus.dq) = 0xCD13;
		co_await WaitClk(clock());

		// Read CAS
		simu(bus.wen) = 1;
		simu(bus.dq).invalidate();
		BOOST_TEST(simu(dq) == 0x13);
		co_await WaitClk(clock());

		// set Mode Register (CL = 2) (Burst = 4)
		size_t burst = 4;
		size_t cl = 2;

		simu(bus.cke) = 1;
		simu(bus.csn) = 0;
		simu(bus.rasn) = 0;
		simu(bus.casn) = 0;
		simu(bus.wen) = 0;

		simu(bus.ba) = 0;
		simu(bus.a) = (cl << 4) | gtry::utils::Log2C(burst);
		co_await WaitClk(clock());
		simu(bus.csn) = 1;
		co_await WaitClk(clock());

		// Write Burst CAS 
		simu(bus.csn) = 0;
		simu(bus.rasn) = 1;
		simu(bus.casn) = 0;
		simu(bus.wen) = 0;
		simu(bus.a) = 2;
		simu(bus.ba) = 1;
		simu(bus.dqm) = 3;

		for (size_t i = 0; i < burst; ++i)
		{
			simu(bus.dq) = 0xB00 + i;
			co_await WaitClk(clock());
			simu(bus.csn) = 1;
		}
		simu(bus.dq).invalidate();

		// Read Burst CAS
		simu(bus.csn) = 0;
		simu(bus.wen) = 1;
		co_await WaitClk(clock());
		simu(bus.csn) = 1;

		// check read data
		for (size_t i = 0; i < burst; ++i)
		{
			co_await WaitClk(clock());
			BOOST_TEST(simu(dq) == 0xB00 + i);
		}

		simu(bus.csn) = 1;
		for (size_t i = 0; i < 4; ++i)
			co_await WaitClk(clock());
		stopTest();
		co_return;
	});
}

BOOST_FIXTURE_TEST_CASE(sdram_timer_test, ClockedTest)
{
	scl::sdram::Timings timings{
		.cl = 2,
		.rcd = 2,
		.ras = 4,
		.rp = 2,
		.rc = 8,
		.rrd = 2,
		.refi = 1560,
	};

	scl::sdram::CommandBus bus{
		.a = 11_b,
		.ba = 1_b,
		.dq = 32_b,
		.dqm = 4_b,
	};
	pinIn(bus, "bus");

	UInt casLength = pinIn(4_b).setName("cas_length");

	scl::sdram::SdramTimer timer;
	timer.generate(timings, bus, casLength, 8);

	Bit b0Activate = timer.can(scl::sdram::CommandCode::Activate, "1b0");
	gtry::pinOut(b0Activate).setName("b0Activate");
	Bit b0Precharge = timer.can(scl::sdram::CommandCode::Precharge, "1b0");
	gtry::pinOut(b0Precharge).setName("b0Precharge");
	Bit b0Read = timer.can(scl::sdram::CommandCode::Read, "1b0");
	gtry::pinOut(b0Read).setName("b0Read");
	Bit b0Write = timer.can(scl::sdram::CommandCode::Write, "1b0");
	gtry::pinOut(b0Write).setName("b0Write");
	Bit b0BurstStop = timer.can(scl::sdram::CommandCode::BurstStop, "1b0");
	gtry::pinOut(b0BurstStop).setName("b0BurstStop");

	Bit b1Activate = timer.can(scl::sdram::CommandCode::Activate, 1);
	gtry::pinOut(b1Activate).setName("b1Activate");
	Bit b1Precharge = timer.can(scl::sdram::CommandCode::Precharge, 1);
	gtry::pinOut(b1Precharge).setName("b1Precharge");
	Bit b1Read = timer.can(scl::sdram::CommandCode::Read, 1);
	gtry::pinOut(b1Read).setName("b1Read");
	Bit b1Write = timer.can(scl::sdram::CommandCode::Write, 1);
	gtry::pinOut(b1Write).setName("b1Write");
	Bit b1BurstStop = timer.can(scl::sdram::CommandCode::BurstStop, 1);
	gtry::pinOut(b1BurstStop).setName("b1BurstStop");

	addSimulationProcess([=]()->SimProcess {

		simu(bus.cke) = 1;
		simu(bus.csn) = 1;
		simu(bus.rasn) = 1;
		simu(bus.casn) = 1;
		simu(bus.wen) = 1;
		simu(bus.ba) = 0;
		co_await WaitClk(clock());

		BOOST_TEST(simu(b0Activate) == 1);
		BOOST_TEST(simu(b0Precharge) == 1);
		BOOST_TEST(simu(b0Read) == 1);
		BOOST_TEST(simu(b0Write) == 1);
		BOOST_TEST(simu(b0BurstStop) == 1);
		BOOST_TEST(simu(b1Activate) == 1);
		BOOST_TEST(simu(b1Precharge) == 1);
		BOOST_TEST(simu(b1Read) == 1);
		BOOST_TEST(simu(b1Write) == 1);
		BOOST_TEST(simu(b1BurstStop) == 1);
		co_await WaitClk(clock());

		// RAS
		simu(bus.csn) = 0;
		simu(bus.rasn) = 0;
		co_await WaitClk(clock());
		simu(bus.csn) = 1;

		for (size_t i = 0; i < 9; ++i)
		{
			BOOST_TEST(simu(b0Activate) == (i < timings.rc - 1 ? 0 : 1));
			BOOST_TEST(simu(b0Precharge) == (i < timings.ras - 1 ? 0 : 1));
			BOOST_TEST(simu(b0Read) == (i < timings.rcd - 1 ? 0 : 1));
			BOOST_TEST(simu(b0Write) == (i < timings.rcd - 1 ? 0 : 1));
			BOOST_TEST(simu(b0BurstStop) == 1);
			BOOST_TEST(simu(b1Activate) == (i < timings.rrd - 1 ? 0 : 1));
			BOOST_TEST(simu(b1Precharge) == 1);
			BOOST_TEST(simu(b1Read) == 1);
			BOOST_TEST(simu(b1Write) == 1);
			BOOST_TEST(simu(b1BurstStop) == 1);
			co_await WaitClk(clock());
		}

		// Precharge
		simu(bus.csn) = 0;
		simu(bus.rasn) = 0;
		simu(bus.wen) = 0;
		co_await WaitClk(clock());
		simu(bus.csn) = 1;

		for (size_t i = 0; i < 3; ++i)
		{
			BOOST_TEST(simu(b0Activate) == (i < timings.rp - 1 ? 0 : 1));
			BOOST_TEST(simu(b0Precharge) == 1);
			BOOST_TEST(simu(b0Read) == 1);
			BOOST_TEST(simu(b0Write) == 1);
			BOOST_TEST(simu(b0BurstStop) == 1);
			BOOST_TEST(simu(b1Activate) == 1);
			BOOST_TEST(simu(b1Precharge) == 1);
			BOOST_TEST(simu(b1Read) == 1);
			BOOST_TEST(simu(b1Write) == 1);
			BOOST_TEST(simu(b1BurstStop) == 1);
			co_await WaitClk(clock());
		}

		// long write cas
		simu(bus.csn) = 0;
		simu(bus.rasn) = 1;
		simu(bus.casn) = 0;
		simu(bus.wen) = 0;
		simu(casLength) = 4;
		co_await WaitClk(clock());
		simu(bus.csn) = 1;

		for (size_t i = 0; i < 7; ++i)
		{
			BOOST_TEST(simu(b0Activate) == 1);
			BOOST_TEST(simu(b0Precharge) == (i < 3 ? 0 : 1));
			BOOST_TEST(simu(b0Read) == (i < 3 ? 0 : 1));
			BOOST_TEST(simu(b0Write) == (i < 3 ? 0 : 1));
			BOOST_TEST(simu(b0BurstStop) == 1);
			BOOST_TEST(simu(b1Activate) == 1);
			BOOST_TEST(simu(b1Precharge) == 1);
			BOOST_TEST(simu(b1Read) == (i < 3 ? 0 : 1));
			BOOST_TEST(simu(b1Write) == (i < 3 ? 0 : 1));
			BOOST_TEST(simu(b1BurstStop) == 1);
			co_await WaitClk(clock());
		}

		// long read cas
		simu(bus.csn) = 0;
		simu(bus.rasn) = 1;
		simu(bus.casn) = 0;
		simu(bus.wen) = 1;
		simu(casLength) = 4;
		co_await WaitClk(clock());
		simu(bus.csn) = 1;

		size_t writeDelay = (4 - 1) + timings.cl + (timings.wr - 1);
		for (size_t i = 0; i < 7; ++i)
		{
			BOOST_TEST(simu(b0Activate) == 1);
			BOOST_TEST(simu(b0Precharge) == (i < 3 ? 0 : 1));
			BOOST_TEST(simu(b0Read) == (i < 3 ? 0 : 1));
			BOOST_TEST(simu(b0Write) == (i < writeDelay ? 0 : 1));
			BOOST_TEST(simu(b0BurstStop) == 1);
			BOOST_TEST(simu(b1Activate) == 1);
			BOOST_TEST(simu(b1Precharge) == 1);
			BOOST_TEST(simu(b1Read) == (i < 3 ? 0 : 1));
			BOOST_TEST(simu(b1Write) == (i < writeDelay ? 0 : 1));
			BOOST_TEST(simu(b1BurstStop) == 1);
			co_await WaitClk(clock());
		}

		stopTest();
	});

	//design.postprocess();
	//dbg::vis();
}

class SdramControllerTest : public ClockedTest, protected scl::sdram::Controller
{
public:
	SdramControllerTest()
	{
		timings({
			.cl = 2,
			.rcd = 18,
			.ras = 42,
			.rp = 18,
			.rc = 42 + 18 + 20,
			.rrd = 12,
			.refi = 1560,
		});
		
		dataBusWidth(16_b);
		addressMap({
			.column = Selection::Slice(1, 8),
			.row = Selection::Slice(9, 12),
			.bank = Selection::Slice(21, 2)
		});
		
		burstLimit(3);

		timeout(hlim::ClockRational{ 2, 1'000'000 });
	}

	void makeBusPins(const scl::sdram::CommandBus& in, std::string prefix) override
	{
		Bit outEnable = m_dataOutEnable;
		scl::sdram::CommandBus bus = in;
		if (m_useOutputRegister)
		{
			bus = gtry::reg(in);
			bus.cke = gtry::reg(in.cke, '0');
			bus.dqm = gtry::reg(in.dqm, ConstBVec(0, in.dqm.width()));
			outEnable = gtry::reg(outEnable, '0');
		}

		pinOut(bus.cke).setName(prefix + "CKE");
		pinOut(bus.csn).setName(prefix + "CSn");
		pinOut(bus.rasn).setName(prefix + "RASn");
		pinOut(bus.casn).setName(prefix + "CASn");
		pinOut(bus.wen).setName(prefix + "WEn");
		pinOut(bus.a).setName(prefix + "A");
		pinOut(bus.ba).setName(prefix + "BA");
		pinOut(bus.dqm).setName(prefix + "DQM");
		pinOut(bus.dq).setName(prefix + "DQ_OUT");
		pinOut(outEnable).setName(prefix + "DQ_OUT_EN");

		BVec moduleData = scl::sdram::moduleSimulation(bus);
		HCL_NAMED(moduleData);

		m_dataIn = ConstBVec(moduleData.width());
		IF(!outEnable)
			m_dataIn = moduleData;
		pinOut(m_dataIn).setName(prefix + "DQ_IN");
	}

	void setupLink(BitWidth addrWidth = 23_b, BitWidth sizeWidth = 2_b, BitWidth sourceWidth = 4_b, BitWidth dataWidth = 16_b)
	{
		link.a->address = addrWidth;
		link.a->size = sizeWidth;
		link.a->source = sourceWidth;
		link.a->mask = dataWidth / 8;
		link.a->data = dataWidth;

		(*link.d)->size = sizeWidth;
		(*link.d)->source = sourceWidth;
		(*link.d)->sink = 0_b;
		(*link.d)->data = dataWidth;
	
		pinIn(link, "link");
		//setFullByteEnableMask(link.a);

		DesignScope::get()->getCircuit().addSimulationProcess([&]() -> SimProcess {
			simu(valid(link.a)) = 0;
			//simu(ready(link.d)) = 0;
			co_return;
		});
	}

	void issueRead(size_t address, size_t size, size_t tag = 0)
	{
		simu(link.a->opcode) = scl::TileLinkA::Get;
		simu(link.a->param) = 0;
		simu(link.a->address) = address;
		simu(link.a->size) = gtry::utils::Log2C(size);
		simu(link.a->source) = tag;
		simu(link.a->mask) = link.a->mask.width().mask();
		simu(link.a->data).invalidate();

		simu(valid(link.a)) = 1;
	}

	void issueWrite(size_t address, size_t byteSize, size_t tag = 0)
	{
		simu(link.a->opcode) = scl::TileLinkA::PutFullData;
		simu(link.a->param) = 0;
		simu(link.a->address) = address;
		simu(link.a->size) = gtry::utils::Log2C(byteSize);
		simu(link.a->source) = tag;
		simu(link.a->mask) = link.a->mask.width().mask();

		simu(valid(link.a)) = 1;
	}

	static bool transfer(const scl::StreamSignal auto& stream)
	{
		return simu(valid(stream)) != 0 && simu(ready(stream)) != 0;
	}
	
	scl::TileLinkUL link;
};


BOOST_FIXTURE_TEST_CASE(sdram_constroller_init_test, SdramControllerTest)
{
	setupLink();
	generate(link);

	addSimulationProcess([=]()->SimProcess {
		co_await WaitClk(clock());
		issueWrite(0, 4, 1);
		simu(link.a->data) = 0xCDCD;

		while (!transfer(link.a))
			co_await WaitClk(clock());
		co_await WaitClk(clock());
		simu(link.a->data) = 0xCECE;
		while (!transfer(link.a))
			co_await WaitClk(clock());
		co_await WaitClk(clock());



		issueRead(0, 2);
		while (!transfer(link.a))
			co_await WaitClk(clock());
		co_await WaitClk(clock());

		issueRead(0, 4);
		while (!transfer(link.a))
			co_await WaitClk(clock());
		co_await WaitClk(clock());


		issueRead(512, 1);
		while (!transfer(link.a))
			co_await WaitClk(clock());
		co_await WaitClk(clock());
		simu(valid(link.a)) = 0;

		for (size_t i = 0; i < 16; ++i)
			co_await WaitClk(clock());

		stopTest();
	});

	//dbg::vis();
}
