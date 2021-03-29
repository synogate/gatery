#pragma once
#include <hcl/frontend.h>

namespace hcl::stl
{
	struct DisplayModeDimension
	{
		BVec resolution;
		BVec syncStart;
		BVec syncEnd;
		BVec total;
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

		BVec x, y;
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

BOOST_HANA_ADAPT_STRUCT(hcl::stl::DisplayModeDimension, resolution, syncStart, syncEnd, total);
BOOST_HANA_ADAPT_STRUCT(hcl::stl::DisplayMode, w, h);
BOOST_HANA_ADAPT_STRUCT(hcl::stl::DisplaySync, x, y, onScreen, vsync, hsync);

