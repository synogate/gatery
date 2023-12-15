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
 * Do not include the regular gatery stuff since this is meant to compile stand-alone in driver/userspace application code. 
 */


#include <stdint.h>

/**
 * @addtogroup gtry_scl_driver
 * @{
 */

#ifdef __cplusplus
namespace gtry::scl::driver {
#endif

struct MemoryMapEntry {
    uint64_t addr = 0;
    uint64_t width;
    uint8_t flags;
	enum Flags {
		READABLE  = 0b00000001,
		WRITEABLE = 0b00000010,
	};

    const char *name;
    const char *shortDesc;
    const char *longDesc;

    uint32_t childrenStart;
    uint32_t childrenCount;
};

#ifdef __cplusplus
}
#endif

/**@}*/
