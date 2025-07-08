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

#include <gatery/scl/Counter.h>
#include <gatery/scl/flag.h>
#include "DisplaySync.h"
#include "modulation.h"

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

	void DisplaySync::init(const DisplayMode& mode)
	{
		Area ent{ "scl_DisplaySync", true };
		HCL_NAMED(mode);

		scl::Counter cx{ mode.w.total };
		x = reg(cx.value(), 0);

		// the copy is needed to break the loop inside of flag. TODO analyze why this matters.
		hsync = copy(flag(cx.value() == mode.w.syncStart, cx.value() == mode.w.syncEnd));
		onScreen = copy(flag(cx.value() == 0, cx.value() == mode.w.resolution) | name("de_x"));

		scl::Counter cy{ mode.h.total };
		IF(cx.isLast())
			cy.inc();

		y = reg(cy.value(), 0);
		vsync = copy(flag(cy.value() == mode.h.syncStart, cy.value() == mode.h.syncEnd));
		onScreen &= flag(cy.value() == 0, cy.value() == mode.h.resolution) | name("de_y");

		HCL_NAMED(x);
		HCL_NAMED(y);
		HCL_NAMED(hsync);
		HCL_NAMED(vsync);
		HCL_NAMED(onScreen);
	}

	BacklightCtrl backlightController(UInt level, BacklightConfig cfg)
	{
		Area ent{ "scl_backlightController", true };

		Counter stepCtr{ cfg.powerOnSequenceStepTime };
		Counter phase(4);
		IF(stepCtr.isLast())
		{
			IF(!phase.isLast() & (level != 0))
				phase.inc();
			IF(!phase.isFirst() & (level == 0))
				phase.dec();
		}

		return BacklightCtrl{
			.powerEn = phase.value() > 0,
			.ledPwm = (phase.value() > 1) & pwm(level, cfg.pwmFreq),
			.ledEn = phase.value() > 2
		};
	}
}
