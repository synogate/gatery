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

#include <boost/multiprecision/cpp_int.hpp>
#include <boost/random.hpp>

#ifdef _WIN32
#pragma warning(push, 0)
#pragma warning(disable : 4307) // boost "'*': signed integral constant overflow"
#endif

using namespace boost::unit_test;

using BoostUnitTestSimulationFixture = gtry::BoostUnitTestSimulationFixture;

BOOST_DATA_TEST_CASE_F(BoostUnitTestSimulationFixture, BigIntArithmetic, data::make({60, 65, 128, 260}), bitsize)
{

	using namespace gtry;

	UInt a = pinIn(BitWidth((size_t)bitsize));
	UInt b = pinIn(BitWidth((size_t)bitsize));

	UInt add = a+b;
	UInt sub = a-b;
	UInt mul = a*b;
	UInt div = a/b;
	UInt rem = a%b;

	addSimulationProcess([=, this]()->SimProcess {
		BigInt mask = (BigInt(1) << (size_t)bitsize)-1;

		boost::random::mt19937 mt;
   		boost::random::uniform_int_distribution<sim::BigInt> ui(0, mask);

		for ([[maybe_unused]] auto i : gtry::utils::Range(100)) {
			sim::BigInt in1 = ui(mt);
			sim::BigInt in2 = ui(mt);

			simu(a) = in1;
			simu(b) = in2;

			co_await WaitFor({1,1000000});
			
			BOOST_TEST(simu(add).allDefined());
			BOOST_CHECK_MESSAGE((sim::BigInt)simu(add) == ((in1+in2) & mask), "Addition failed: in1: 0x" << std::hex << in1 << " in2: 0x" << std::hex << in2 << " result: 0x" << std::hex << (sim::BigInt)simu(add) << " should be 0x" << ((in1+in2) & mask));
			
			sim::BigInt difference = in1 - in2;
			if (difference < 0)
				difference = sim::bitwiseNegation(-difference, bitsize) + 1;

			BOOST_TEST(simu(sub).allDefined());
			BOOST_CHECK_MESSAGE((sim::BigInt)simu(sub) == (difference & mask), "Subtraction failed: in1: 0x" << std::hex << in1 << " in2: 0x" << std::hex << in2 << " result: 0x" << std::hex << (sim::BigInt)simu(sub) << " should be 0x" << (difference & mask));

			BOOST_TEST(simu(mul).allDefined());
			BOOST_CHECK_MESSAGE((sim::BigInt)simu(mul) == ((in1*in2) & mask), "Multiplication failed: in1: 0x" << std::hex << in1 << " in2: 0x" << std::hex << in2 << " result: 0x" << std::hex << (sim::BigInt)simu(mul) << " should be 0x" << ((in1*in2) & mask));

			if (in2 != 0) {
				BOOST_TEST(simu(div).allDefined());
				BOOST_TEST((sim::BigInt)simu(div) == ((in1/in2) & mask));

				BOOST_TEST(simu(rem).allDefined());
				BOOST_TEST((sim::BigInt)simu(rem) == ((in1%in2) & mask));
			} else {
				BOOST_TEST(!simu(div).allDefined());
				BOOST_TEST(!simu(rem).allDefined());
			}
		}

		stopTest();
	});


	design.postprocess();

	runTest({ 1,1000 });
}


BOOST_DATA_TEST_CASE_F(BoostUnitTestSimulationFixture, BigIntCompare, data::make({60, 65, 128, 260}), bitsize)
{
	using namespace gtry;

	UInt a = pinIn(BitWidth((size_t)bitsize));
	UInt b = pinIn(BitWidth((size_t)bitsize));

	Bit le = a<b;
	Bit leq = a<=b;
	Bit eq = a==b;
	Bit neq = a!=b;
	Bit ge = a>b;
	Bit geq = a>=b;

	addSimulationProcess([=, this]()->SimProcess {
		boost::random::mt19937 mt;
   		boost::random::uniform_int_distribution<sim::BigInt> ui(0, (BigInt(1) << (size_t)bitsize) -1);

		for ([[maybe_unused]] auto i : gtry::utils::Range(100)) {
			sim::BigInt in1 = ui(mt);
			sim::BigInt in2 = ui(mt);

			simu(a) = in1;
			simu(b) = in2;

			co_await WaitFor({1,1000000});
			
			BOOST_TEST(simu(le).allDefined());
			BOOST_TEST(simu(le) == (in1<in2));
			
			BOOST_TEST(simu(leq).allDefined());
			BOOST_TEST(simu(leq) == (in1<=in2));

			BOOST_TEST(simu(eq).allDefined());
			BOOST_TEST(simu(eq) == (in1 == in2));

			BOOST_TEST(simu(neq).allDefined());
			BOOST_TEST(simu(neq) == (in1 != in2));

			BOOST_TEST(simu(ge).allDefined());
			BOOST_TEST(simu(ge) == (in1 > in2));

			BOOST_TEST(simu(geq).allDefined());
			BOOST_TEST(simu(geq) == (in1 >= in2));
		}

		stopTest();
	});


	design.postprocess();

	runTest({ 1,1000 });
}
