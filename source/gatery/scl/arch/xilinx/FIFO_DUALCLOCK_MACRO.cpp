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

#include "FIFO_DUALCLOCK_MACRO.h"

#include <gatery/utils/Exceptions.h>
#include <gatery/utils/Preprocessor.h>

namespace gtry::scl::arch::xilinx {

FIFO_DUALCLOCK_MACRO::FIFO_DUALCLOCK_MACRO(size_t width, FIFOSize fifoSize)
{
	m_libraryName = "UNIMACRO";
	m_packageName = "VCOMPONENTS";
	m_name = "FIFO_DUALCLOCK_MACRO";
	m_isEntity = false;
	m_clockNames = {"RDCLK", "WRCLK"};
	m_resetNames = {"RST", ""}; // TODO: Just one reset, async, for both

	m_width = width;
	m_fifoSize = fifoSize;
	unsigned counterWidth;
	switch (fifoSize) {
		case SIZE_18Kb:
			m_genericParameters["FIFO_SIZE"] = "18Kb";
			if (width < 5)
				counterWidth = 12; else
			if (width < 10)
				counterWidth = 11; else
			if (width < 19)
				counterWidth = 10; else
			if (width < 37)
				counterWidth = 9; else
				HCL_ASSERT_HINT(false, "The maximal data width of FIFO_DUALCLOCK_MACRO for 18Kb is 36 bits!");
		break;
		case SIZE_36Kb:
			m_genericParameters["FIFO_SIZE"] = "36Kb";
			if (width < 5)
				counterWidth = 13; else
			if (width < 10)
				counterWidth = 12; else
			if (width < 19)
				counterWidth = 11; else
			if (width < 37)
				counterWidth = 10; else
			if (width < 73)
				counterWidth = 9; else
				HCL_ASSERT_HINT(false, "The maximal data width of FIFO_DUALCLOCK_MACRO is 72 bits!");
		break;
	}
	m_genericParameters["DATA_WIDTH"] = width;

	resizeIOPorts(IN_COUNT, OUT_COUNT);

	declInputBit(IN_RDEN, "RDEN");
	declInputBit(IN_WREN, "WREN");
	declInputBitVector(IN_DI, "DI", width, "DATA_WIDTH");


	declOutputBit(OUT_ALMOSTEMPTY, "ALMOSTEMPTY");
	declOutputBit(OUT_ALMOSTFULL, "ALMOSTFULL");
	declOutputBit(OUT_EMPTY, "EMPTY");
	declOutputBit(OUT_FULL, "FULL");
	declOutputBit(OUT_RDERR, "RDERR");
	declOutputBit(OUT_WRERR, "WRERR");
	// UInts
	declOutputBitVector(OUT_DO, "DO", width, "DATA_WIDTH");
	declOutputBitVector(OUT_RDCOUNT, "RDCOUNT", counterWidth, "xil_UNM_GCW(DATA_WIDTH, FIFO_SIZE, DEVICE)");
	declOutputBitVector(OUT_WRCOUNT, "WRCOUNT", counterWidth, "xil_UNM_GCW(DATA_WIDTH, FIFO_SIZE, DEVICE)");

}


FIFO_DUALCLOCK_MACRO &FIFO_DUALCLOCK_MACRO::setAlmostEmpty(size_t numOccupied)
{
	m_genericParameters["ALMOST_EMPTY_OFFSET"].setBitVector(16, numOccupied, GenericParameter::BitFlavor::BIT);
	return *this;
}

FIFO_DUALCLOCK_MACRO &FIFO_DUALCLOCK_MACRO::setAlmostFull(size_t numVacant)
{
	m_genericParameters["ALMOST_EMPTY_OFFSET"].setBitVector(16, numVacant, GenericParameter::BitFlavor::BIT);
	return *this;
}

FIFO_DUALCLOCK_MACRO &FIFO_DUALCLOCK_MACRO::setDevice(std::string device)
{
	m_genericParameters["DEVICE"] = std::move(device);
	return *this;
}

FIFO_DUALCLOCK_MACRO &FIFO_DUALCLOCK_MACRO::setFirstWordFallThrough(bool enable)
{
	m_genericParameters["FIRST_WORD_FALL_THROUGH"].setBoolean(enable);
	return *this;
}


std::unique_ptr<hlim::BaseNode> FIFO_DUALCLOCK_MACRO::cloneUnconnected() const
{
	std::unique_ptr<BaseNode> res(new FIFO_DUALCLOCK_MACRO(m_width, m_fifoSize));
	copyBaseToClone(res.get());
	return res;
}

void FIFO_DUALCLOCK_MACRO::copyBaseToClone(BaseNode *copy) const
{
	ExternalComponent::copyBaseToClone(copy);
	auto *other = (FIFO_DUALCLOCK_MACRO*)copy;

	other->m_width = m_width;
	other->m_fifoSize = m_fifoSize;
}



}
