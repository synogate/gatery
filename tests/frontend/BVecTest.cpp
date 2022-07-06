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

using namespace boost::unit_test;

using BoostUnitTestSimulationFixture = gtry::BoostUnitTestSimulationFixture;


BOOST_FIXTURE_TEST_CASE(BVecIterator, BoostUnitTestSimulationFixture)
{
	using namespace gtry;



	BVec a = BVec("b1100");
	BOOST_TEST(a.size() == 4);
	BOOST_TEST(!a.empty());

	size_t counter = 0;
	for (auto it = a.cbegin(); it != a.cend(); ++it)
	{
		if (counter < 2)
			sim_assert(!*it);
		else
			sim_assert(*it);

		counter++;

	}
	BOOST_TEST(a.size() == counter);

	counter = 0;
	for ([[maybe_unused]] auto &b : a)
		counter++;
	BOOST_TEST(a.size() == counter);

	sim_assert(a[0] == false) << "a[0] is " << a[0] << " but should be false";
	sim_assert(a[1] == false) << "a[1] is " << a[1] << " but should be false";
	sim_assert(a[2] == true) << "a[2] is " << a[2] << " but should be true";
	sim_assert(a[3] == true) << "a[3] is " << a[3] << " but should be true";

	a[0] = true;
	sim_assert(a[0] == true) << "a[0] is " << a[0] << " after setting it explicitely to true";

	for (auto &b : a)
		b = true;
	sim_assert(a[1] == true) << "a[1] is " << a[1] << " after setting all bits to true";


	eval();
}

BOOST_FIXTURE_TEST_CASE(BVecIteratorArithmetic, BoostUnitTestSimulationFixture)
{
	using namespace gtry;



	BVec a = BVec("b1100");

	auto it1 = a.begin();
	auto it2 = it1 + 1;
	BOOST_CHECK(it1 != it2);
	BOOST_CHECK(it1 <= it2);
	BOOST_CHECK(it1 < it2);
	BOOST_CHECK(it2 >= it1);
	BOOST_CHECK(it2 > it1);
	BOOST_CHECK(it1 == a.begin());
	BOOST_CHECK(it2 - it1 == 1);
	BOOST_CHECK(it2 - 1 == it1);

	auto it3 = it1++;
	BOOST_CHECK(it3 == a.begin());
	BOOST_CHECK(it1 == it2);

	auto it4 = it1--;
	BOOST_CHECK(it4 == it2);
	BOOST_CHECK(it1 == a.begin());

	auto it5 = ++it1;
	BOOST_CHECK(it5 == it1);
	BOOST_CHECK(it5 == it2);

	it5 = --it1;
	BOOST_CHECK(it5 == it1);
	BOOST_CHECK(it5 == a.begin());
}

BOOST_FIXTURE_TEST_CASE(BVecFrontBack, BoostUnitTestSimulationFixture)
{
	using namespace gtry;



	BVec a = BVec("b1100");
	sim_assert(!a.front());
	sim_assert(a.back());
	sim_assert(!a.lsb());
	sim_assert(a.msb());

	a.front() = true;
	sim_assert(a.front());

	a.back() = false;
	sim_assert(!a.back());

	eval();
}

BOOST_FIXTURE_TEST_CASE(BitSignalLoopSemanticTest, BoostUnitTestSimulationFixture)
{
	using namespace gtry;



	Bit unused; // should not produce combinatorial loop errors

	Bit a;
	sim_assert(a) << a << " should be 1";
	a = '1';

	Bit b;
	b = '1';
	sim_assert(b) << b << " should be 1";

	eval();
}

BOOST_FIXTURE_TEST_CASE(BVecSignalLoopSemanticTest, BoostUnitTestSimulationFixture)
{
	using namespace gtry;



	BVec unused = 2_b; // should not produce combinatorial loop errors

	BVec a = 2_b;
	sim_assert(a == "b10") << a << " should be 10";
	a = "b10";

	BVec b = 2_b;
	b = "b11";
	sim_assert(b == "b11") << b << " should be 11";

	BVec c;
	c = 2_b;
	sim_assert(c == "b01") << c << " should be 01";
	c = "b01";
/*
	BVec shadowed = 2_b;
	shadowed[0] = '1';
	shadowed[1] = '0';
*/
	eval();
}

BOOST_FIXTURE_TEST_CASE(ConstantDataStringParser, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	BOOST_CHECK(parseBitVector("32x1bBXx").size() == 32);
	BOOST_CHECK(parseBitVector("x1bBX").size() == 16);
	BOOST_CHECK(parseBitVector("o170X").size() == 12);
	BOOST_CHECK(parseBitVector("b10xX").size() == 4);
}

BOOST_FIXTURE_TEST_CASE(BVecSelectorAccess, BoostUnitTestSimulationFixture)
{
	using namespace gtry;



	BVec a = BVec("b11001110");

	sim_assert(a(2, 4) == "b0011");

	sim_assert(a(1, -1) == "b1100111");
	sim_assert(a(-2, 2) == "b11");
/*
	sim_assert(a(0, 4, 2) == "b1010");
	sim_assert(a(1, 4, 2) == "b1011");

	sim_assert(a(0, 4, 2)(0, 2, 2) == "b00");
	sim_assert(a(0, 4, 2)(1, 2, 2) == "b11");
*/
	eval();
}


BOOST_FIXTURE_TEST_CASE(BitAliasTest, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	UInt a = 1337;

	a[1] ^= '1';
	a += 1;

	sim_assert(a == ((1337 ^ 0b10)+1));
	eval();
}



BOOST_FIXTURE_TEST_CASE(DynamicBitSliceRead, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	size_t v = 0b11001010; 

	UInt a = v;

	UInt index = pinIn(3_b);

	Bit b = a[index];

	addSimulationProcess([=, this]()->SimProcess {
	
		for (auto i : gtry::utils::Range(8)) {
			simu(index) = i;
			co_await WaitFor({1,1000});
			BOOST_TEST(simu(b) == ((v >> i) & 1));
		}

		stopTest();
	});

	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});

	runTest({ 1,1 });
}

BOOST_FIXTURE_TEST_CASE(DynamicBitSliceOfSliceRead, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	size_t v = 0b11001010; 

	UInt a = v;

	UInt index = pinIn(2_b);

	Bit b = a(2,4)[index];

	addSimulationProcess([=, this]()->SimProcess {
		size_t v_ = (v >> 2) & 0b1111;
		for (auto i : gtry::utils::Range(4)) {
			simu(index) = i;
			co_await WaitFor({1,1000});
			BOOST_TEST(simu(b) == ((v_ >> i) & 1));
		}

		stopTest();
	});

	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});

	runTest({ 1,1 });
}



BOOST_FIXTURE_TEST_CASE(DynamicBitSliceWrite, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	size_t a_ = 0xC3;
	size_t v = 0b11001010; 

	UInt a = a_;
	Bit b = pinIn();
	UInt index = pinIn(3_b);

	a[index] = b;

	addSimulationProcess([=, this]()->SimProcess {
		for (auto i : gtry::utils::Range(8)) {
			simu(index) = i;
			simu(b) = (v >> i) & 1;
			co_await WaitFor({1,1000});
			size_t mask = 1ull << i;
			BOOST_TEST(simu(a) == (a_ & ~mask | v & mask));
		}

		stopTest();
	});


	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});

	runTest({ 1,1 });
}

BOOST_FIXTURE_TEST_CASE(DynamicBitSliceOfSliceWrite, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	size_t a_ = 0xC3;
	size_t v = 0b11001010; 

	UInt a = a_;
	Bit b = pinIn();
	UInt index = pinIn(2_b);

	a(2,4)[index] = b;

	addSimulationProcess([=, this]()->SimProcess {
		for (auto i : gtry::utils::Range(4)) {
			simu(index) = i;
			simu(b) = (v >> (i+2)) & 1;
			co_await WaitFor({1,1000});

			size_t mask = (1ull << (i+2));
			BOOST_TEST(simu(a) == (a_ & ~mask | v & mask));
		}

		stopTest();
	});


	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});

	runTest({ 1,1 });
}

BOOST_FIXTURE_TEST_CASE(DynamicBitSliceConstReduction, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	Bit b;
	{
		UInt a = pinIn(8_b);
		UInt index = "3b1";

		b = a[index];
	}

	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
	
	auto rewire = dynamic_cast<hlim::Node_Rewire*>(b.node()->getNonSignalDriver(0).node);
	// Ensure that the dynamic multiplexer gets folded by the postprocessing into a rewire node ...
	BOOST_REQUIRE(rewire != nullptr);
	// ... that is directly fed from the pin node.
	BOOST_REQUIRE(rewire->getNumInputPorts() == 1);
	BOOST_REQUIRE(dynamic_cast<hlim::Node_Pin*>(rewire->getNonSignalDriver(0).node) != nullptr);
}



BOOST_FIXTURE_TEST_CASE(DynamicBVecSliceRead, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	size_t v = 0b11001010; 

	UInt a = v;

	UInt index = pinIn(3_b);

	UInt b = a(index, 2);

	addSimulationProcess([=, this]()->SimProcess {
	
		for (auto i : gtry::utils::Range(7)) {
			simu(index) = i;
			co_await WaitFor({1,1000});
			BOOST_TEST(simu(b) == ((v >> i) & 0b11));
		}

		simu(index) = 7;
		co_await WaitFor({1,1000});
		BOOST_TEST(!simu(b).allDefined());

		stopTest();
	});

	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});

	runTest({ 1,1 });
}



BOOST_FIXTURE_TEST_CASE(DynamicBVecSliceOfStaticSliceRead, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	size_t v = 0b101100101000ull;
	size_t v_ = (v >> 2) & 0xFF;

	UInt a = v;

	UInt index = pinIn(3_b);

	UInt b = a(2, 8)(index, 2);

	addSimulationProcess([=, this]()->SimProcess {
	
		for (auto i : gtry::utils::Range(7)) {
			simu(index) = i;
			co_await WaitFor({1,1000});
			BOOST_TEST(simu(b) == ((v_ >> i) & 0b11));
		}

		simu(index) = 7;
		co_await WaitFor({1,1000});
		BOOST_TEST(!simu(b).allDefined());

		stopTest();
	});

	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});

	runTest({ 1,1 });
}



BOOST_FIXTURE_TEST_CASE(DynamicBVecSliceOfStaticSliceReverseRead, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	size_t v = 0b101100101000ull;

	UInt a = v;

	UInt index = pinIn(3_b);

	UInt b = a(index, 8)(2, 2);

	addSimulationProcess([=, this]()->SimProcess {
	
		for (auto i : gtry::utils::Range(5)) {
			simu(index) = i;
			co_await WaitFor({1,1000});
			BOOST_TEST(simu(b) == ((v >> (i+2)) & 0b11));
		}

		simu(index) = 5;
		co_await WaitFor({1,1000});
		BOOST_TEST(!simu(b).allDefined());

		stopTest();
	});

	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});

	runTest({ 1,1 });
}



BOOST_FIXTURE_TEST_CASE(DynamicBVecSliceOfDynamicSliceRead, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	size_t v = 0b1011'0010'1000ull;

	UInt a = v;
	HCL_NAMED(a);

	UInt index1 = pinIn(3_b);
	UInt index2 = pinIn(3_b);
	HCL_NAMED(index1);
	HCL_NAMED(index2);

	UInt b = a(index1, 8)(index2, 2);
	HCL_NAMED(b);

	addSimulationProcess([=, this]()->SimProcess {
	
		for (auto i : gtry::utils::Range(5)) {
			for (auto j : gtry::utils::Range(7)) {
				simu(index1) = i;
				simu(index2) = j;
				co_await WaitFor({1,1000});
				BOOST_TEST(simu(b) == ((v >> (i+j)) & 0b11));
			}
		}

		simu(index1) = 5;
		simu(index2) = 3;
		co_await WaitFor({1,1000});
		BOOST_TEST(!simu(b).allDefined());

		simu(index1) = 0;
		simu(index2) = 7;
		co_await WaitFor({1,1000});
		BOOST_TEST(!simu(b).allDefined());

		stopTest();
	});

	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});

	runTest({ 1,1 });
}



BOOST_FIXTURE_TEST_CASE(DynamicBVecSliceWithStaticBitSliceRead, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	size_t v = 0b11001010; 

	UInt a = v;
	HCL_NAMED(a);

	UInt index = pinIn(3_b);
	HCL_NAMED(index);

	Bit b = a(index, 2)[1];
	HCL_NAMED(b);

	addSimulationProcess([=, this]()->SimProcess {
	
		for (auto i : gtry::utils::Range(7)) {
			simu(index) = i;
			co_await WaitFor({1,1000});
			BOOST_TEST(simu(b) == ((v >> (i+1)) & 0b1));
		}

		simu(index) = 7;
		co_await WaitFor({1,1000});
		BOOST_TEST(!simu(b).allDefined());

		stopTest();
	});

	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});

	runTest({ 1,1 });
}


BOOST_FIXTURE_TEST_CASE(DynamicBVecSliceWithDynamicBitSliceRead, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	size_t v = 0b101100101000ull;

	UInt a = v;
	HCL_NAMED(a);

	UInt index1 = pinIn(3_b);
	UInt index2 = pinIn(4_b);
	HCL_NAMED(index1);
	HCL_NAMED(index2);

	Bit b = a(index1, 8)[index2];
	HCL_NAMED(b);

	addSimulationProcess([=, this]()->SimProcess {
	
		for (auto i : gtry::utils::Range(5)) {
			for (auto j : gtry::utils::Range(8)) {
				simu(index1) = i;
				simu(index2) = j;
				co_await WaitFor({1,1000});
				BOOST_TEST(simu(b) == ((v >> (i+j)) & 0b1));
			}
		}

		simu(index1) = 5;
		simu(index2) = 3;
		co_await WaitFor({1,1000});
		BOOST_TEST(!simu(b).allDefined());

		simu(index1) = 0;
		simu(index2) = 8;
		co_await WaitFor({1,1000});
		BOOST_TEST(!simu(b).allDefined());

		stopTest();
	});


	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});

	runTest({ 1,1 });
}

BOOST_FIXTURE_TEST_CASE(DynamicBitSliceOfDynamicSliceWrite, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	size_t a_ = 0xC3;
	size_t v = 0b11001010; 

	UInt a = a_;
	Bit b = pinIn();
	HCL_NAMED(b);

	UInt index1 = pinIn(3_b);
	UInt index2 = pinIn(4_b);
	HCL_NAMED(index1);
	HCL_NAMED(index2);

	a(index1,4)[index2] = b;

	addSimulationProcess([=, this]()->SimProcess {
		for (auto i : gtry::utils::Range(4)) {
			for (auto j : gtry::utils::Range(4)) {
				simu(index1) = i;
				simu(index2) = j;
				simu(b) = (v >> (i + j)) & 1;
				co_await WaitFor({1,1000});

				size_t mask = (0b1ull << (i+j));
				BOOST_TEST(simu(a) == (a_ & ~mask | v & mask));
			}
		}

		stopTest();
	});


	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});

	runTest({ 1,1 });
}


BOOST_FIXTURE_TEST_CASE(DynamicBVecSliceWrite, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	size_t a_ = 0xC3;
	size_t v = 0b11001010; 

	UInt a = a_;
	UInt b = pinIn(3_b);
	HCL_NAMED(b);

	UInt index = pinIn(3_b);
	HCL_NAMED(index);

	a(index,3) = b;
	HCL_NAMED(a);


	addSimulationProcess([=, this]()->SimProcess {
		for (auto i : gtry::utils::Range(6)) {
			simu(index) = i;
			simu(b) = (v >> i) & 0b111;
			co_await WaitFor({1,1000});

			size_t mask = (0b111ull << i);
			BOOST_TEST(simu(a) == (a_ & ~mask | v & mask));
		}

		stopTest();
	});


	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});

	runTest({ 1,1 });
}

BOOST_FIXTURE_TEST_CASE(DynamicBVecSliceOfSliceWrite, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	size_t a_ = 0xC3;
	size_t v = 0b11001010; 

	UInt a = a_;
	a.setName("a_before");
	UInt b = pinIn(2_b);
	HCL_NAMED(b);

	UInt index = pinIn(3_b);
	HCL_NAMED(index);

	a(index,3)(1,2) = b;
	HCL_NAMED(a);


	addSimulationProcess([=, this]()->SimProcess {
		for (auto i : gtry::utils::Range(6)) {
			simu(index) = i;
			simu(b) = (v >> (i+1)) & 0b11;
			co_await WaitFor({1,1000});

			size_t mask = (0b110 << i);
			BOOST_TEST(simu(a) == (a_ & ~mask | v & mask));
		}

		stopTest();
	});


	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});

	runTest({ 1,1 });
}


BOOST_FIXTURE_TEST_CASE(DynamicBVecSliceAutoWidth, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	size_t v = 0b11001010; 

	UInt a = v;

	UInt index = pinIn(1_b);

	UInt b = a(index);

	BOOST_TEST(b.size() == 7);
}


