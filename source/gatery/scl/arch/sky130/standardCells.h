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

//#include <gatery/frontend/Clock.h>
//#include <gatery/frontend/ExternalModule.h>
#include <gatery/frontend.h>

namespace gtry::scl::arch::sky130 {

	enum class Strength {
		one = 1,
		two = 2,
		four = 4,
		six = 6,
		eight = 8,
		twelve = 12,
		sixteen = 16
	};

	enum class Library {
		highDensity,
		highDensityLowLeakage,
		highSpeed,
		lowSpeed,
		mediumSpeed
	};

	static std::string toLibraryName(Library lib) {
		switch (lib) {
			case Library::highDensity: return "hd";
			case Library::highDensityLowLeakage: return "hdll";
			case Library::highSpeed: return "hs";
			case Library::lowSpeed: return "ls";
			case Library::mediumSpeed: return "ms";
		}
		HCL_ASSERT(false);
		return "";
	}
}
