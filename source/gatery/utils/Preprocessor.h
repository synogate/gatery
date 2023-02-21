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
#pragma once

#include <iostream>

#if defined(_MSC_VER)

#define GET_FUNCTION_NAME __FUNCSIG__

#define DEBUG_BREAK() __debugbreak

#elif defined(__GNUC__)

#define GET_FUNCTION_NAME __PRETTY_FUNCTION__

#define DEBUG_BREAK() raise(SIGTRAP)

#else
#error "Unsupported platform!"
#endif


#ifdef __GNUC__
#define GTRY_PACKED( STRUCT ) struct __attribute__((__packed__)) STRUCT
#endif

#ifdef _MSC_VER
#define GTRY_PACKED( STRUCT ) __pragma( pack(push, 1) ) struct STRUCT __pragma( pack(pop))
#endif



#define HCL_NAMED(x) gtry::setName(x, #x)


#define HCL_ASSERT(x) { if (!(x)) { throw gtry::utils::InternalError(__FILE__, __LINE__, std::string("Assertion failed: ") + #x); }}
#define HCL_ASSERT_NOTHROW(x) { if (!(x)) { std::cerr << "Assertion failed: " << #x << '\n'; exit(1); }}
#define HCL_ASSERT_HINT(x, message) { if (!(x)) { throw gtry::utils::InternalError(__FILE__, __LINE__, std::string("Assertion failed: ") + #x + " Hint: " + message); }}


#define HCL_DESIGNCHECK(x) { if (!(x)) { throw gtry::utils::DesignError(__FILE__, __LINE__, std::string("Design failed: ") + #x); }}
#define HCL_DESIGNCHECK_HINT(x, message) { if (!(x)) { throw gtry::utils::DesignError(__FILE__, __LINE__, std::string("Design failed: ") + #x + " Hint: " + message); }}


