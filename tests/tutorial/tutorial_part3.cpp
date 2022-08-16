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

using BoostUnitTestSimulationFixture = gtry::BoostUnitTestSimulationFixture;

using namespace gtry;

/*******************************************************************************
If any of these are updated, please also update the tutorial / documentation!!!
********************************************************************************/

BOOST_FIXTURE_TEST_CASE(tutorial_part3_csa_pre_class, BoostUnitTestSimulationFixture)
{
    // Assume resized and all of equal width
    std::vector<UInt> summands = {
		UInt("10d10"),
		UInt("10d5"),
		UInt("10d3"),
		UInt("10d9"),
	};

    UInt result;

    if (summands.size() == 1) {
        result = summands[0];
    } else if (summands.size() >= 1) {
        UInt sum = summands[0];
        UInt carry = summands[1];

        // One full adder step per additional summand
        for (unsigned i = 2; i < summands.size(); i++) {
            UInt new_carry = (sum & carry) | (sum & summands[i]) | (carry & summands[i]);
            sum ^= carry ^ summands[i];
            carry = new_carry << 1;
        }

        result = sum + carry;
    }

	sim_assert(result == "10d27");

	runEvalOnlyTest();
}



class CarrySafeAdder {
	public:
		void add(const UInt &b) {
			if (m_count == 0)
				m_sum = b;
			else if (m_count == 1)
				m_carry = b;
			else {
				UInt new_carry = (m_sum & m_carry) | (m_sum & b) | (m_carry & b);
				m_sum ^= m_carry ^ b;
				m_carry = new_carry << 1;
			}
			m_count++; 
		}
		UInt sum() const {
			if (m_count <= 1)
				return m_sum;

			return m_sum + m_carry;
		}

		CarrySafeAdder operator + (const UInt& b) { CarrySafeAdder ret = *this; ret.add(b); return ret; }
		CarrySafeAdder& operator += (const UInt& b) { add(b); return *this; }
		operator UInt () const { return sum(); }

	protected:
		unsigned m_count = 0;
		UInt m_carry;
		UInt m_sum;
};


BOOST_FIXTURE_TEST_CASE(tutorial_part3_csa_class, BoostUnitTestSimulationFixture)
{
    // Assume resized and all of equal width
    std::vector<UInt> summands = {
		UInt("10d10"),
		UInt("10d5"),
		UInt("10d3"),
		UInt("10d9"),
	};

    // Just to demonstrate the usage, tying in here with the previous std::vector
    CarrySafeAdder adder;
    for (const auto &b : summands)
        adder.add(b);

    UInt result = adder.sum();


	sim_assert(result == "10d27");

	runEvalOnlyTest();
}



BOOST_FIXTURE_TEST_CASE(tutorial_part3_csa_class2, BoostUnitTestSimulationFixture)
{
	UInt result = CarrySafeAdder{} + UInt("10d10") + UInt("10d5") + UInt("10d3") + UInt("10d9");


	sim_assert(result == "10d27");

	runEvalOnlyTest();
}