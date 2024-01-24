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
#include <boost/test/data/test_case.hpp>

#include <gatery/frontend.h>
#include <gatery/scl/math.h>

using namespace gtry;
using namespace gtry::scl;
using namespace boost::unit_test;

BOOST_FIXTURE_TEST_CASE(long_division_uint_div_uint_test, gtry::BoostUnitTestSimulationFixture) {

	Clock clk = Clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScope(clk);

	UInt numerator = 8_b;
	pinIn(numerator, "numerator");
	UInt denominator = 4_b;
	pinIn(denominator, "denominator");

	UInt quotient = longDivision(numerator, denominator, false);
	pinOut(quotient, "quotient");

	addSimulationProcess([&, this]()->SimProcess {
		for (size_t i = 0; i < numerator.width().count(); i++) {
			for (size_t j = 0; j < denominator.width().count(); j++){
				simu(numerator) = i;
				simu(denominator) = j;
				co_await WaitFor(Seconds{ 0,1 });
				if (j == 0) {
					BOOST_TEST(simu(quotient) == numerator.width().mask());
					BOOST_TEST(simu(quotient) == numerator.width().mask(), "division by 0 expect full mask (largest number)");
				}
				else {
					BOOST_TEST(simu(quotient) == i / j, "test case: " << i << " / " << j);
					BOOST_TEST(simu(quotient) == i / j);
				}
				co_await AfterClk(clk);
			}
		}

		co_await OnClk(clk);
		stopTest();
	});

	design.postprocess();
	if(false) recordVCD("dut.vcd");
	BOOST_TEST(!runHitsTimeout({ 41, 1'000'000 })); // 41 is a precise number, do not reduce
}

BOOST_FIXTURE_TEST_CASE(long_division_export, gtry::BoostUnitTestSimulationFixture) {
	Clock clk = Clock({ .absoluteFrequency = 400'000'000 });
	ClockScope clkScope(clk);

	UInt numerator = 48_b;
	pinIn(numerator, "numerator");
	UInt denominator = 48_b;
	pinIn(denominator, "denominator");

	pipeinputgroup(numerator, denominator);
	UInt quotient = longDivision(reg(numerator), reg(denominator), true);

	pinOut(reg(quotient), "quotient");

	design.postprocess();

	if(false) outputVHDL("export", true);
}

BOOST_FIXTURE_TEST_CASE(long_division_sint_div_uint_test, gtry::BoostUnitTestSimulationFixture) {

	Clock clk = Clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScope(clk);

	SInt numerator = 8_b;
	pinIn(numerator, "numerator");
	UInt denominator = 8_b; //to make use of cpp's 8bit support
	pinIn(denominator, "denominator");

	SInt quotient = longDivision(numerator, denominator, false);
	pinOut(quotient, "quotient");

	bool fastTest = true;
	addSimulationProcess([&, this]()->SimProcess {
		for (int i = -128; i < 128; fastTest? i+=7 : i++) {
			for (size_t j = 0; j < denominator.width().count(); fastTest? j+=11 : j++) {
				simu(numerator)		= (int8_t) i;
				simu(denominator)	= j;
				co_await WaitFor(Seconds{ 0,1 });
				if(j!=0) {
					BOOST_TEST(simu(quotient) == (int) i / (int) j, "test case: " << i << " / " << j);
					BOOST_TEST(simu(quotient) == (int) i / (int) j);
				}
				co_await AfterClk(clk);
			}
		}

		co_await OnClk(clk);
		stopTest();
		});

	design.postprocess();
	if(false) recordVCD("dut.vcd");
	BOOST_TEST(!runHitsTimeout({ 700, 1'000'000 }));
}

