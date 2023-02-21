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

class OSERDESE2 : public gtry::ExternalComponent
{
	public:
		enum Clocks {
			CLK, // fast clock
			CLKDIV, // slow clock
			CLK_COUNT
		};

		enum Inputs {
			// D1 - D8: 1-bit (each) input: Parallel data inputs (1-bit each)
			IN_D1, 
			IN_D2, 
			IN_D3, 
			IN_D4, 
			IN_D5, 
			IN_D6, 
			IN_D7, 
			IN_D8, 
			IN_OCE,  // 1-bit input: Output data clock enable
			// SHIFTIN1 / SHIFTIN2: 1-bit (each) input: Data input expansion (1-bit each)
			IN_SHIFTIN1,
			IN_SHIFTIN2,
			// T1 - T4: 1-bit (each) input: Parallel 3-state inputs
			IN_T1,
			IN_T2,
			IN_T3,
			IN_T4,
			IN_TBYTEIN, // 1-bit input: Byte group tristate
			IN_TCE,	 // 1-bit input: 3-state clock enable
			IN_COUNT
		};
		enum Outputs {
			OUT_OFB, // 1-bit output: Feedback path for data
			OUT_OQ,  // 1-bit output: Data path output
			// SHIFTOUT1 / SHIFTOUT2: 1-bit (each) output: Data output expansion (1-bit each)
			OUT_SHIFTOUT1,
			OUT_SHIFTOUT2,
			OUT_TBYTEOUT, // 1-bit output: Byte group tristate
			OUT_TFB, // 1-bit output: 3-state control
			OUT_TQ, // 1-bit output: 3-state control
			OUT_COUNT
		};

		OSERDESE2(size_t width);

		void setSlave();

		virtual std::string getTypeName() const override;
		virtual void assertValidity() const override;

		virtual std::unique_ptr<BaseNode> cloneUnconnected() const override;

		virtual std::string attemptInferOutputName(size_t outputPort) const override;
	protected:
};


}
