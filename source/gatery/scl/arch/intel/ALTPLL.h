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

#include <gatery/frontend/Clock.h>
#include <gatery/frontend/ExternalComponent.h>

namespace gtry::scl::arch::intel 
{
	class ALTPLL : public gtry::ExternalComponent
	{
	public:
		enum Clocks {
			CLK_IN0,
			CLK_IN1,
			CLK_IN2,
			CLK_IN3,

			CLK_COUNT
		};

		enum Inputs {
			IN_INCLK,
			// IN_FBIN,
			// IN_PHASEUPDOWN,
			// IN_PHASESTEP,
			// IN_PHASECOUNTERSELECT,
			// IN_PLLENA,
			// IN_CLKSWITCH,
			// IN_PFDENA,
			// IN_C_ENA,
			// IN_E_ENA,
			// IN_CONFIGUPDATE,
			// IN_SCANCLK,
			// IN_SCANCLKENA,
			// IN_SCANACLR,
			// IN_SCANDATA,
			// IN_SCANREAD,
			// IN_SCANWRITE,

			IN_COUNT
		};

		enum Outputs {
			OUT_CLK,
			// OUT_E,
			// OUT_CLKBAD,
			// OUT_ACTIVECLOCK,
			// OUT_CLKLOSS,
			OUT_LOCKED,
			// OUT_SCANDATAOUT,
			// OUT_FBOUT,
			// OUT_ENABLE0,
			// OUT_ENABLE1,
			// OUT_SCLKOUT0,
			// OUT_SCLKOUT1,
			// OUT_PHASEDONE,
			// OUT_SCANDONE,

			OUT_COUNT
		};

		ALTPLL(size_t availableOutputClocks = 5);

		void setClock(size_t idx, const Clock& clock);
		
		hlim::ClockRational inClkFrequency() const { HCL_DESIGNCHECK_HINT(m_inClk, "input clock not yet specified"); return m_inClk->absoluteFrequency(); }

		ALTPLL& configureDeviceFamily(std::string familyName);
		ALTPLL& configureClock(size_t idx, size_t mul, size_t div, size_t dutyCyclePercent, size_t phaseShiftPs);

		Clock generateOutClock(size_t idx, size_t mul, size_t div, size_t dutyCyclePercent, size_t phaseShiftPs, ClockConfig::ResetType resetType = ClockConfig::ResetType::ASYNCHRONOUS);

		Clock generateUnspecificClock(size_t mul, size_t div, size_t dutyCyclePercent, size_t phaseShiftPs, ClockConfig::ResetType resetType = ClockConfig::ResetType::ASYNCHRONOUS);
		virtual std::unique_ptr<BaseNode> cloneUnconnected() const override;

	private:
		std::vector<bool> m_availableOutputClocks;
		std::optional<Clock> m_inClk;
	};
}
