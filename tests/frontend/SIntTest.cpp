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


BOOST_FIXTURE_TEST_CASE(SIntIterator, BoostUnitTestSimulationFixture)
{
	using namespace gtry;



	SInt a = SInt("b1100");
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

BOOST_FIXTURE_TEST_CASE(SIntIteratorArithmetic, BoostUnitTestSimulationFixture)
{
	using namespace gtry;



	SInt a = SInt("b1100");

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

BOOST_FIXTURE_TEST_CASE(SIntFrontBack, BoostUnitTestSimulationFixture)
{
	using namespace gtry;



	SInt a = SInt("b1100");
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


BOOST_FIXTURE_TEST_CASE(SIntSignalLoopSemanticTest, BoostUnitTestSimulationFixture)
{
	using namespace gtry;



	SInt unused = 2_b; // should not produce combinatorial loop errors

	SInt a = 2_b;
	sim_assert(a == SInt("b10")) << a << " should be 10";
	a = SInt("b10");

	SInt b = 2_b;
	b = SInt("b11");
	sim_assert(b == SInt("b11")) << b << " should be 11";

	SInt c;
	c = 2_b;
	sim_assert(c == SInt("b01")) << c << " should be 01";
	c = SInt("b01");
/*
	SInt shadowed = 2_b;
	shadowed[0] = '1';
	shadowed[1] = '0';
*/
	eval();
}

BOOST_FIXTURE_TEST_CASE(SIntSelectorAccess, BoostUnitTestSimulationFixture)
{
	using namespace gtry;



	SInt a = SInt("b11001110");

	sim_assert(a(2, 4_b) == SInt("b0011"));

	sim_assert(a(1, -1_b) == SInt("b1100111"));
/*
	sim_assert(a(-2, 2) == SInt("b11"));
	sim_assert(a(0, 4, 2) == "b1010");
	sim_assert(a(1, 4, 2) == "b1011");

	sim_assert(a(0, 4, 2)(0, 2, 2) == "b00");
	sim_assert(a(0, 4, 2)(1, 2, 2) == "b11");
*/
	eval();
}



BOOST_FIXTURE_TEST_CASE(SIntAbs, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	SInt negative = SInt(-5);
	SInt positive = SInt(10);

	sim_assert(abs(negative) == 5);
	sim_assert(abs(positive) == 10);

	eval();
}


BOOST_FIXTURE_TEST_CASE(SIntMul, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	SInt a = ext(SInt(-5), 32_b);
	SInt b = ext(SInt(-10), 32_b);
	SInt c = ext(SInt(5), 32_b);
	SInt d = ext(SInt(10), 32_b);

	sim_assert(a*a == ext(SInt(25)));
	sim_assert(a*b == ext(SInt(50)));
	sim_assert(a*c == ext(SInt(-25)));
	sim_assert(a*d == ext(SInt(-50)));

	sim_assert(b*a == ext(SInt(50)));
	sim_assert(b*b == ext(SInt(100)));
	sim_assert(b*c == ext(SInt(-50)));
	sim_assert(b*d == ext(SInt(-100)));

	eval();
}

