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


