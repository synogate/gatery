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
#include <gatery/frontend.h>

namespace gtry::scl
{
	struct DisplayModeDimension
	{
		UInt resolution;
		UInt syncStart;
		UInt syncEnd;
		UInt total;
	};

	struct DisplayMode
	{
		ClockConfig::ClockRational pixelFreq;
		DisplayModeDimension w;
		DisplayModeDimension h;
	};

	struct DisplaySync
	{
		DisplaySync() = default;
		DisplaySync(const DisplaySync&) = default;
		DisplaySync(DisplaySync&&) = default;

		void init(DisplayMode& mode);

		UInt x, y;
		Bit onScreen;
		Bit vsync;
		Bit hsync;
	};

	namespace DisplayModeLines
	{
		extern std::string_view _1080p_60hz;
		extern std::string_view _1080p_50hz;
		extern std::string_view _720p_60hz;
		extern std::string_view _720p_50hz;
	}

	// operators to parse
	std::istream& operator >> (std::istream& s, DisplayModeDimension& dim);
	std::istream& operator >> (std::istream& s, DisplayMode& dim);

}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::DisplayModeDimension, resolution, syncStart, syncEnd, total);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::DisplayMode, w, h);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::DisplaySync, x, y, onScreen, vsync, hsync);

