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

#include <gatery/utils/BitManipulation.h>

using namespace boost::unit_test;

BOOST_AUTO_TEST_CASE(CheckEndianFlip)
{
	using namespace gtry::utils;


	BOOST_TEST(flipEndian((std::uint8_t) 0x12) == 0x12);
	BOOST_TEST(flipEndian((std::uint16_t) 0x1234) == 0x3412);
	BOOST_TEST(flipEndian((std::uint32_t) 0x12345678) == 0x78563412);
	BOOST_TEST(flipEndian((std::uint64_t) 0x123456789ABCDEF0) == 0xF0DEBC9A78563412);

	BOOST_TEST(flipEndian((std::int8_t) 0x12) == 0x12);
	BOOST_TEST(flipEndian((std::int16_t) 0x1234) == 0x3412);
	BOOST_TEST(flipEndian((std::int32_t) 0x12345678) == 0x78563412);
	BOOST_TEST(flipEndian((std::int64_t) 0x123456789ABCDEF0) == 0xF0DEBC9A78563412);
}

