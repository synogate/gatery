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

#include "tutorial/pch.h"

#include <boost/test/unit_test.hpp>

/*******************************************************************************
If any of these are updated, please also update the tutorial / documentation!!!
********************************************************************************/


BOOST_AUTO_TEST_CASE(tutorial_part1_final)
{
	using namespace gtry;
	using namespace gtry::vhdl;

    DesignScope design;

    {
        Clock clock({.absoluteFrequency = 125'000'000}); // 125MHz
        ClockScope clockScope{ clock };

        hlim::ClockRational blinkFrequency{1, 1}; // 1Hz

        size_t counterMax = hlim::floor(clock.absoluteFrequency() / blinkFrequency);

        UInt counter = BitWidth(utils::Log2C(counterMax+1));
        counter = reg(counter+1, 0);
        HCL_NAMED(counter);

        pinOut(counter.msb()).setName("led");
    }

    design.postprocess();

}
