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

class RAM256X1D : public gtry::ExternalComponent
{
	public:
		enum Clocks {
			CLK_WR,
			CLK_COUNT
		};

		enum Inputs {
			IN_D,
			IN_A,
			IN_DPRA,
			IN_WE,

			IN_COUNT
		};
		enum Outputs {
			OUT_SPO,
			OUT_DPO,
			
			OUT_COUNT
		};

		RAM256X1D();

		Bit setupSDP(const UInt &wrAddr, const Bit &wrData, const Bit &wrEn, const UInt &rdAddr);

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

