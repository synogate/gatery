/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Synogate GbR

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
using UnitTestSimulationFixture = gtry::UnitTestSimulationFixture;
/*
BOOST_FIXTURE_TEST_CASE(ROM, UnitTestSimulationFixture)
{
    using namespace gtry;
    using namespace gtry::sim;
    using namespace gtry::utils;



    Memory<BVec> rom(16, 4_b);
    rom.setPowerOnState(createDefaultBitVectorState(16, 4, [](std::size_t i, std::size_t *words){
        words[DefaultConfig::VALUE] = i;
        words[DefaultConfig::DEFINED] = ~0ull;
    }));

    for (auto i : Range(16)) {
        BVec v = rom[i];
        sim_assert(v == i) << "rom["<<i<<"] is " << v << " but should be " << i;
    }

    eval(design.getCircuit());
}
*/
