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


#include <gatery/frontend/GHDLTestFixture.h>

#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>


using namespace boost::unit_test;

boost::test_tools::assertion_result canExport(boost::unit_test::test_unit_id)
{
	return gtry::GHDLGlobalFixture::hasGHDL();
}


BOOST_AUTO_TEST_SUITE(Export, * precondition(canExport))

BOOST_FIXTURE_TEST_CASE(testExportUnconnectedInputs, gtry::GHDLTestFixture)
{
	using namespace gtry;

    {
        UInt undefined = 3_b;
        Bit comparison = undefined == 0;
        pinOut(comparison).setName("out");
    }


	testCompilation();
}


BOOST_FIXTURE_TEST_CASE(testExportLoopyInputs, gtry::GHDLTestFixture)
{
	using namespace gtry;

    {
        UInt undefined = 3_b;
		HCL_NAMED(undefined); // signal node creates a loop
        Bit comparison = undefined == 0;
        pinOut(comparison).setName("out");
    }


	testCompilation();
}


BOOST_FIXTURE_TEST_CASE(testExportLiteralComparison, gtry::GHDLTestFixture)
{
	using namespace gtry;

    {
        UInt undefined = "3bXXX";
        Bit comparison = undefined == 0;
        pinOut(comparison).setName("out");
    }

	testCompilation();
}


BOOST_AUTO_TEST_SUITE_END()
