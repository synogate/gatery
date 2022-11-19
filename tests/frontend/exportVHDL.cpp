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

BOOST_FIXTURE_TEST_CASE(unconnectedInputs, gtry::GHDLTestFixture)
{
	using namespace gtry;

    {
        UInt undefined = 3_b;
        Bit comparison = undefined == 0;
        pinOut(comparison).setName("out");
    }


	testCompilation();
}


BOOST_FIXTURE_TEST_CASE(loopyInputs, gtry::GHDLTestFixture)
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


BOOST_FIXTURE_TEST_CASE(literalComparison, gtry::GHDLTestFixture)
{
	using namespace gtry;

    {
        UInt undefined = "3bXXX";
        Bit comparison = undefined == 0;
        pinOut(comparison).setName("out");
    }

	testCompilation();
}



BOOST_FIXTURE_TEST_CASE(readOutput, gtry::GHDLTestFixture)
{
	using namespace gtry;

    {
		Bit input = pinIn().setName("input");
		Bit output;
		Bit output2;
		{
			Area area("mainArea", true);

			{
				Area area("producingSubArea", true);
				output = input ^ '1';
			}
			{
				Area area("consumingSubArea", true);
				output2 = output ^ '1';
			}
		}

        pinOut(output).setName("out");
        pinOut(output2).setName("out2");
    }

	testCompilation();
}




BOOST_FIXTURE_TEST_CASE(readOutputLocal, gtry::GHDLTestFixture)
{
	using namespace gtry;

    {
		Bit input = pinIn().setName("input");
		Bit output;
		Bit output2;
		{
			Area area("mainArea", true);

			output = input ^ '1';
			output2 = output ^ '1';
		}

        pinOut(output).setName("out");
        pinOut(output2).setName("out2");
    }

	testCompilation();
	DesignScope::visualize("test_outputLocal");
}


BOOST_FIXTURE_TEST_CASE(muxUndefined, gtry::GHDLTestFixture)
{
	using namespace gtry;

    {
		Bit input1 = pinIn().setName("input1");
		Bit input2 = pinIn().setName("input2");
		Bit output;

		Bit undefined = 'x';
		HCL_NAMED(undefined);

		IF (undefined)
			output = input1;
		ELSE
			output = input2;

        pinOut(output).setName("out");
    }

	testCompilation();
	DesignScope::visualize("test_muxUndefined");
}




BOOST_AUTO_TEST_SUITE_END()
