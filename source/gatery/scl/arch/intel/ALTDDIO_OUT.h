/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2022 Michael Offel, Andreas Ley

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
#include <gatery/frontend/ExternalComponent.h>

#include "../general/BaseDDROutPattern.h"

namespace gtry::scl::arch::intel 
{
	class IntelDevice;

	class ALTDDIO_OUT : public gtry::ExternalComponent
	{
		public:
			enum Clocks {
				CLK_OUTCLOCK,

				CLK_COUNT
			};

			enum Inputs {
				IN_DATAIN_H,
				IN_DATAIN_L,
				IN_OUTCLOCKEN,
				IN_ACLR,
				IN_ASET,
				IN_OE,
				IN_SCLR,
				IN_SSET,

				IN_COUNT
			};

			enum Outputs {
				OUT_DATAOUT,

				OUT_COUNT
			};

			ALTDDIO_OUT(BitWidth width);

			ALTDDIO_OUT& setupSimulationDeviceFamily(std::string familyName);

			ALTDDIO_OUT& enableOutputRegister();
			ALTDDIO_OUT& powerUpHigh();

			virtual std::unique_ptr<BaseNode> cloneUnconnected() const override;
		protected:
			BitWidth m_width;
	};



	class ALTDDIO_OUTPattern : public BaseDDROutPattern
	{
		public:
			ALTDDIO_OUTPattern(const IntelDevice &intelDevice) : m_intelDevice(intelDevice) { m_patternName = "ALTDDIO_OUT"; }
			virtual ~ALTDDIO_OUTPattern() = default;
		protected:
			const IntelDevice &m_intelDevice;

			virtual bool performReplacement(hlim::NodeGroup *nodeGroup, ReplaceInfo &replacement) const override;
			virtual void performConstResetReplacement(hlim::NodeGroup *nodeGroup, ConstResetReplaceInfo &replacement) const override;
	};	
}
