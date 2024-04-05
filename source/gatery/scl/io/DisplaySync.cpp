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
#include "gatery/scl_pch.h"
#include "DisplaySync.h"

namespace gtry::scl
{
	namespace DisplayModeLines
	{
		std::string_view _1080p_60hz = "148.5 1920 2008 2052 2200 1080 1084 1089 1125 +hsync +vsync";
		std::string_view _1080p_50hz = "148.5 1920 2448 2492 2640 1080 1084 1089 1125 +hsync +vsync";
		std::string_view _720p_60hz = "74.25 1280 1390 1430 1650 720 725 730 750 +hsync +vsync";
		std::string_view _720p_50hz = "74.25 1280 1720 1760 1980 720 725 730 750 +hsync +vsync";
	}

	std::istream& operator>>(std::istream& s, DisplayModeDimension& dim)
	{
		size_t res = 0, start = 0, end = 0, total = 0;
		s >> res >> start >> end >> total;

		dim.resolution = res;
		dim.syncStart = start;
		dim.syncEnd = end;
		dim.total = total;
		return s;
	}

	std::istream& operator>>(std::istream& s, DisplayMode& dim)
	{
		double mhz = 0;
		s >> mhz >> dim.w >> dim.h;

		dim.pixelFreq.assign(uint64_t(mhz * 1'000'000), 1);
		return s;
	}

	void DisplaySync::init(DisplayMode& mode)
	{
		GroupScope ent{ GroupScope::GroupType::ENTITY, "DisplaySync" };

		HCL_NAMED(mode);

		x = mode.w.total.width();
		y = mode.h.total.width();

		x += 1;
		IF(x == mode.w.total)
		{
			x = 0;
			y += 1;

			IF(y == mode.h.total)
				y = 0;
		}
		HCL_NAMED(x);
		HCL_NAMED(y);

		hsync = reg(x >= mode.w.syncStart & x < mode.w.syncEnd);
		vsync = reg(y >= mode.h.syncStart & y < mode.h.syncEnd);
		onScreen = reg(x < mode.w.resolution & y < mode.h.resolution);
		HCL_NAMED(hsync);
		HCL_NAMED(vsync);
		HCL_NAMED(onScreen);

		x = reg(x);
		y = reg(y);
	}
}
