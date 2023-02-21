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

#include <gatery/frontend/ExternalComponent.h>

namespace gtry::scl::arch::xilinx {

class MMCME2_BASE : public gtry::ExternalComponent
{
	public:
		enum Clocks {
			CLK_IN,
			CLK_COUNT
		};

		enum Inputs {
			IN_PWRDWN,
			IN_CLKFBIN,

			IN_COUNT
		};
		enum Outputs {
        	OUT_CLKOUT0,
        	OUT_CLKOUT0B,
        	OUT_CLKOUT1,
        	OUT_CLKOUT1B,
        	OUT_CLKOUT2,
        	OUT_CLKOUT2B,
        	OUT_CLKOUT3,
        	OUT_CLKOUT3B,
        	OUT_CLKOUT4,
        	OUT_CLKOUT5,
        	OUT_CLKOUT6,
        	OUT_CLKFBOUT,
        	OUT_CLKFBOUTB,
        	OUT_LOCKED,

			OUT_COUNT
		};

		MMCME2_BASE();

		void setClock(hlim::Clock *clock);

		virtual std::unique_ptr<BaseNode> cloneUnconnected() const override;
	protected:
};


}
