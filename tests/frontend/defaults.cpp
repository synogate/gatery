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


BOOST_FIXTURE_TEST_CASE(SimpleDefault, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	Bit defaultOne = BitDefault('1');
	sim_assert(defaultOne == true) << "defaultOne is " << defaultOne << " but should be true!";

	Bit defaultZero = BitDefault('0');
	sim_assert(defaultZero == false) << "defaultZero is " << defaultZero << " but should be false!";

	design.postprocess();

	eval();
}



BOOST_FIXTURE_TEST_CASE(LogicWithDefault, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	Bit defaultValue = BitDefault('1');

	defaultValue &= true;
	defaultValue |= false;

	sim_assert(defaultValue == true) << "defaultValue is " << defaultValue << " but should be true!";

	defaultValue &= false;

	sim_assert(defaultValue == false) << "defaultValue is " << defaultValue << " but should be false!";

	design.postprocess();

	eval();
}


BOOST_FIXTURE_TEST_CASE(ConditionalsWithDefault, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	Bit defaultValue = BitDefault('1');

	sim_assert(defaultValue == true) << "defaultValue is " << defaultValue << " but should be true!";

	IF (defaultValue)
		defaultValue = false;

	sim_assert(defaultValue == false) << "defaultValue is " << defaultValue << " but should be false!";

	design.postprocess();

	eval();
}



BOOST_FIXTURE_TEST_CASE(NonLoopWithDefault, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	Bit defaultValue = BitDefault('1');

	sim_assert(defaultValue == false) << "defaultValue is " << defaultValue << " but should be false!";

	defaultValue = false;

	sim_assert(defaultValue == false) << "defaultValue is " << defaultValue << " but should be false!";

	design.postprocess();

	eval();
}





struct MyStruct {
	gtry::Bit value = gtry::BitDefault('1');
};

BOOST_FIXTURE_TEST_CASE(StructsWithDefault, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	MyStruct s;

	sim_assert(s.value == true) << "s.value is " << s.value << " but should be true!";

	s.value &= false;

	sim_assert(s.value == false) << "s.value is " << s.value << " but should be false!";

	design.postprocess();

	eval();
}
