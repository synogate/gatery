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
#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

using namespace boost::unit_test;

using BoostUnitTestSimulationFixture = gtry::BoostUnitTestSimulationFixture;



BOOST_FIXTURE_TEST_CASE(EnumCreation, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	enum MyClassicalEnum { A, B, C, D };
	Enum<MyClassicalEnum> enumSignal = A;


	enum class MyModernEnum { A, B, C, D };
	Enum<MyModernEnum> enumSignal2 = MyModernEnum::B;
}

BOOST_FIXTURE_TEST_CASE(EnumComparison, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	enum MyClassicalEnum { A, B, C, D };

	Enum<MyClassicalEnum> enumSignal = A;

	sim_assert(enumSignal == A);
	sim_assert(enumSignal != B);
	sim_assert(enumSignal != C);
	sim_assert(enumSignal != D);


	enum class MyModernEnum { A, B, C, D };

	Enum<MyModernEnum> enumSignal2 = MyModernEnum::B;

	sim_assert(enumSignal2 != MyModernEnum::A);
	sim_assert(enumSignal2 == MyModernEnum::B);
	sim_assert(enumSignal2 != MyModernEnum::C);
	sim_assert(enumSignal2 != MyModernEnum::D);

	eval();
}


BOOST_FIXTURE_TEST_CASE(EnumRegCompileTest, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	enum MyClassicalEnum { A, B, C, D };

	Enum<MyClassicalEnum> enumSignal = A;

	enumSignal = reg(enumSignal);
}



BOOST_FIXTURE_TEST_CASE(EnumRegister, gtry::BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	Clock clock({ .absoluteFrequency = 10'000 });
	ClockScope clockScope(clock);

	enum MyClassicalEnum { A, B, C, D };
	Enum<MyClassicalEnum> inSignal = (Enum<MyClassicalEnum>) pinIn(2_b);

	Enum<MyClassicalEnum> resetSignal = C;

	Enum<MyClassicalEnum> outSignal = reg(inSignal);
	pinOut(outSignal.numericalValue());

	Enum<MyClassicalEnum> outSignalReset = reg(inSignal, resetSignal);
	pinOut(outSignalReset.numericalValue());

	addSimulationProcess([=, this]()->SimProcess {

		BOOST_TEST(simu(outSignalReset) == C);

		simu(inSignal) = D;
		co_await AfterClk(clock);
		BOOST_TEST(simu(outSignal) == D);
		BOOST_TEST(simu(outSignalReset) == D);

		stopTest();
	});

	design.postprocess();
	runTest({ 1,1 });
}



BOOST_FIXTURE_TEST_CASE(EnumMemoryCompileTest, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	enum MyClassicalEnum { A, B, C, D };

	Enum<MyClassicalEnum> enumSignal = A;

	Memory<Enum<MyClassicalEnum>> mem(32, A);

	Enum<MyClassicalEnum> sig2 = mem[0];
	mem[1] = enumSignal;
}

BOOST_FIXTURE_TEST_CASE(EnumInStructCompileTest, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	enum MyClassicalEnum { A, B, C, D };

	struct TestStruct {
		Enum<MyClassicalEnum> enumSignal = A;
		// BVec b = 32_b; // causes massive compile time and memory usage on msvc
		Bit c;
	};

	TestStruct s;
	s = reg(s);
	HCL_NAMED(s);
}

BOOST_FIXTURE_TEST_CASE(EnumValueTest, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	enum MyClassicalEnum { 
		A = 2, 
		B = 8, 
		C = 3,
	};

	Enum<MyClassicalEnum> enumSignal = A;

	UInt asUint = enumSignal.numericalValue();
	sim_assert(asUint == 2);

	asUint += 6;

	enumSignal = (Enum<MyClassicalEnum>) asUint;
	sim_assert(enumSignal == B);

	asUint -= 5;

	enumSignal = (Enum<MyClassicalEnum>) asUint;
	sim_assert(enumSignal == C);


	eval();
}
