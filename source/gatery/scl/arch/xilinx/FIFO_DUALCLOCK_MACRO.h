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

namespace gtry::scl::arch::xilinx {

class FIFO_DUALCLOCK_MACRO : public gtry::hlim::Node_External
{
	public:
		// Asynchronous reset of all FIFO functions, flags, and pointers. RST must be asserted for five read and write clock cycles.
		enum Clocks {
			RD_CLK,
			WR_CLK,
			CLK_COUNT
		};


		// WREN and RDEN must be held Low before RST is asserted and during the Reset cycle.
		enum Inputs {
			// Bits
			IN_RDEN, 
			IN_WREN, 
			// UInts
			IN_DI,

			IN_COUNT
		};
		enum Outputs {
			// Bits
			OUT_ALMOSTEMPTY,
			OUT_ALMOSTFULL,
			OUT_EMPTY,
			OUT_FULL,
			OUT_RDERR,
			OUT_WRERR,
			// UInts
			OUT_DO,
			OUT_RDCOUNT,
			OUT_WRCOUNT,

			OUT_COUNT
		};

		enum FIFOSize {
			SIZE_18Kb,
			SIZE_36Kb
		};

		FIFO_DUALCLOCK_MACRO(unsigned width, FIFOSize fifoSize);

		FIFO_DUALCLOCK_MACRO &setAlmostEmpty(size_t numOccupied);
		FIFO_DUALCLOCK_MACRO &setAlmostFull(size_t numVacant);
		FIFO_DUALCLOCK_MACRO &setDevice(std::string device);
		FIFO_DUALCLOCK_MACRO &setFirstWordFallThrough(bool enable);

		void connectInput(Inputs input, const Bit &bit);
		void connectInput(Inputs input, const UInt &UInt);
		Bit getOutputBit(Outputs output);
		UInt getOutputUInt(Outputs output);

		virtual std::string getTypeName() const override;
		virtual void assertValidity() const override;
		virtual std::string getInputName(size_t idx) const override;
		virtual std::string getOutputName(size_t idx) const override;

		virtual std::unique_ptr<BaseNode> cloneUnconnected() const override;

		virtual std::string attemptInferOutputName(size_t outputPort) const override;
	protected:
		unsigned m_width;
		FIFOSize m_fifoSize;
};

}

