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
#include <gatery/hlim/supportNodes/Node_External.h>

namespace gtry::scl::arch::intel 
{
	class ALTPLL : public gtry::hlim::Node_External
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
			IN_CLK,
			IN_FBIN,
			IN_PHASEUPDOWN,
			IN_PHASESTEP,
			IN_PHASECOUNTERSELECT0,
			IN_PHASECOUNTERSELECT1,
			IN_PHASECOUNTERSELECT2,
			IN_PLLENA,
			IN_CLKSWITCH,
			IN_PFDENA,
			IN_C0_ENA,
			IN_E0_ENA0,
			IN_CONFIGUPDATE,
			IN_SCANCLK,
			IN_SCANCLKENA,
			IN_SCANACLR,
			IN_SCANDATA,
			IN_SCANREAD,
			IN_SCANWRITE,

			IN_COUNT
		};

		enum Outputs {
			OUT_CLK,
			OUT_E0,
			OUT_E1,
			OUT_E2,
			OUT_E3,
			OUT_E4,
			OUT_E5,
			OUT_E6,
			OUT_E7,
			OUT_E8,
			OUT_E9,
			OUT_CLKBAD0,
			OUT_CLKBAD1,
			OUT_CLKBAD2,
			OUT_CLKBAD3,
			OUT_CLKBAD4,
			OUT_CLKBAD5,
			OUT_CLKBAD6,
			OUT_CLKBAD7,
			OUT_CLKBAD8,
			OUT_CLKBAD9,
			OUT_ACTIVECLOCK,
			OUT_CLKLOSS,
			OUT_LOCKED,
			OUT_SCANDATAOUT,
			OUT_FBOUT,
			OUT_ENABLE0,
			OUT_ENABLE1,
			OUT_SCLKOUT0,
			OUT_SCLKOUT1,
			OUT_PHASEDONE,
			OUT_SCANDONE,

			OUT_COUNT
		};

		ALTPLL();

		void setInput(Inputs input, const BaseSignal auto& bit) { rewireInput(input, bit.readPort()); }
		Bit getOutput(Outputs output);
		BVec getOutputVec(Outputs output);

		void setClock(size_t idx, const Clock& clock);

		ALTPLL& configureDeviceFamily(std::string familyName);
		ALTPLL& configureClock(size_t idx, size_t mul, size_t div, size_t dutyCyclePercent, size_t phaseShift);

		virtual std::string getTypeName() const override;
		virtual void assertValidity() const override;
		virtual std::string getInputName(size_t idx) const override;
		virtual std::string getOutputName(size_t idx) const override;

		virtual std::unique_ptr<BaseNode> cloneUnconnected() const override;

		virtual std::string attemptInferOutputName(size_t outputPort) const override;
	};
}
