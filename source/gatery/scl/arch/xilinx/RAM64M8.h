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

class RAM64M8 : public gtry::ExternalComponent
{
	public:
		enum Clocks {
			CLK_WR,
			CLK_COUNT
		};

		enum Inputs {
			IN_DI_A,
			IN_DI_B,
			IN_DI_C,
			IN_DI_D,
			IN_DI_E,
			IN_DI_F,
			IN_DI_G,
			IN_DI_H,

			IN_ADDR_A,
			IN_ADDR_B,
			IN_ADDR_C,
			IN_ADDR_D,
			IN_ADDR_E,
			IN_ADDR_F,
			IN_ADDR_G,
			IN_ADDR_H,

			IN_WE,

			IN_COUNT
		};
		enum Outputs {
			OUT_DO_A,
			OUT_DO_B,
			OUT_DO_C,
			OUT_DO_D,
			OUT_DO_E,
			OUT_DO_F,
			OUT_DO_G,
			OUT_DO_H,
			
			OUT_COUNT
		};

		RAM64M8();

		UInt setup64x7_SDP(const UInt &wrAddr, const UInt &wrData, const Bit &wrEn, const UInt &rdAddr);

		virtual std::string getTypeName() const override;
		virtual void assertValidity() const override;

		virtual std::unique_ptr<BaseNode> cloneUnconnected() const override;

		virtual std::string attemptInferOutputName(size_t outputPort) const override;

		void setInitialization(sim::DefaultBitVectorState memoryInitialization);
	protected:
		sim::DefaultBitVectorState m_memoryInitialization;
		virtual void copyBaseToClone(BaseNode *copy) const override;
};

}

