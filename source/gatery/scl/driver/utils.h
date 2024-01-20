/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2023 Michael Offel, Andreas Ley

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

/*
 * Do not include the regular gatery headers since this is meant to compile stand-alone in driver/userspace application code. 
 */

#include <cstdint>
#include <string_view>


/**
 * @addtogroup gtry_scl_driver
 * @{
 */

namespace gtry::scl::driver {

typedef std::uint64_t PhysicalAddr;

/*
template<typename T>
consteval std::size_t typeIdNoRtti() {
    std::string_view name = __PRETTY_FUNCTION__;
    
	std::size_t hash = 0xcbf29ce484222325;
    for (char c : name)
        hash = (hash ^ c) * 1099511628211;

    return hash;
}
*/

}

/**@}*/
