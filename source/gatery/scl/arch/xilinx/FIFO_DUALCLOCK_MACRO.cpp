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
#include "gatery/pch.h"

#include "FIFO_DUALCLOCK_MACRO.h"

#include <gatery/utils/Exceptions.h>
#include <gatery/utils/Preprocessor.h>

namespace gtry::scl::arch::xilinx {

FIFO_DUALCLOCK_MACRO::FIFO_DUALCLOCK_MACRO(unsigned width, FIFOSize fifoSize)
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
    		m_genericParameters["FIFO_SIZE"] = "\"18Kb\"";
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
    		m_genericParameters["FIFO_SIZE"] = "\"36Kb\"";
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
	m_genericParameters["DATA_WIDTH"] = std::to_string(width);

    resizeInputs(IN_COUNT);
    resizeOutputs(OUT_COUNT);

	// default to bool, then override UInts
	for (auto i : utils::Range(OUT_COUNT))
		setOutputConnectionType(i, {.interpretation = hlim::ConnectionType::BOOL, .width=1});

	setOutputConnectionType(OUT_DO, {.interpretation = hlim::ConnectionType::BITVEC, .width=width});
	setOutputConnectionType(OUT_RDCOUNT, {.interpretation = hlim::ConnectionType::BITVEC, .width=counterWidth});
	setOutputConnectionType(OUT_WRCOUNT, {.interpretation = hlim::ConnectionType::BITVEC, .width=counterWidth});
}


FIFO_DUALCLOCK_MACRO &FIFO_DUALCLOCK_MACRO::setAlmostEmpty(size_t numOccupied)
{
	m_genericParameters["ALMOST_EMPTY_OFFSET"] = std::to_string(numOccupied);
	return *this;
}

FIFO_DUALCLOCK_MACRO &FIFO_DUALCLOCK_MACRO::setAlmostFull(size_t numVacant)
{
	m_genericParameters["ALMOST_FULL_OFFSET"] = std::to_string(numVacant);
	return *this;
}

FIFO_DUALCLOCK_MACRO &FIFO_DUALCLOCK_MACRO::setDevice(std::string device)
{
	m_genericParameters["DEVICE"] = std::move(device);
	return *this;
}

FIFO_DUALCLOCK_MACRO &FIFO_DUALCLOCK_MACRO::setFirstWordFallThrough(bool enable)
{
	m_genericParameters["FIRST_WORD_FALL_THROUGH"] = enable?"TRUE":"FALSE";
	return *this;
}



void FIFO_DUALCLOCK_MACRO::connectInput(Inputs input, const Bit &bit)
{
	switch (input) {
		case IN_RDEN:
        case IN_WREN:
			NodeIO::connectInput(input, bit.readPort());
		break;
		default:
			HCL_DESIGNCHECK_HINT(false, "Trying to connect bit to UInt input of FIFO_DUALCLOCK_MACRO!");
	}
}

void FIFO_DUALCLOCK_MACRO::connectInput(Inputs input, const UInt &UInt)
{
	switch (input) {
		case IN_DI:
			HCL_DESIGNCHECK_HINT(UInt.size() == m_width, "Data input UInt to FIFO_DUALCLOCK_MACRO has different width than previously specified!");
			NodeIO::connectInput(input, UInt.readPort());
		break;
		default:
			HCL_DESIGNCHECK_HINT(false, "Trying to connect UInt to bit input of FIFO_DUALCLOCK_MACRO!");
	}
}

std::string FIFO_DUALCLOCK_MACRO::getTypeName() const
{
    return "FIFO_DUALCLOCK_MACRO";
}

void FIFO_DUALCLOCK_MACRO::assertValidity() const
{
}

std::string FIFO_DUALCLOCK_MACRO::getInputName(size_t idx) const
{
    switch (idx) {
		case IN_RDEN: return "RDEN";
		case IN_WREN: return "WREN";
		case IN_DI: return "DI";

		default: return "";
	}
}

std::string FIFO_DUALCLOCK_MACRO::getOutputName(size_t idx) const
{
    switch (idx) {
		case OUT_ALMOSTEMPTY: return "ALMOSTEMPTY";
		case OUT_ALMOSTFULL: return "ALMOSTFULL";
		case OUT_EMPTY: return "EMPTY";
		case OUT_FULL: return "FULL";
		case OUT_RDERR: return "RDERR";
		case OUT_WRERR: return "WRERR";
		case OUT_DO: return "DO";
		case OUT_RDCOUNT: return "RDCOUNT";
		case OUT_WRCOUNT: return "WRCOUNT";

		default: return "";
	}
}

std::unique_ptr<hlim::BaseNode> FIFO_DUALCLOCK_MACRO::cloneUnconnected() const
{
    FIFO_DUALCLOCK_MACRO *ptr;
    std::unique_ptr<BaseNode> res(ptr = new FIFO_DUALCLOCK_MACRO(m_width, m_fifoSize));
    copyBaseToClone(res.get());

    ptr->m_width = m_width;
    ptr->m_fifoSize = m_fifoSize;

    return res;
}

std::string FIFO_DUALCLOCK_MACRO::attemptInferOutputName(size_t outputPort) const
{
    auto driver = getDriver(IN_DI);
    
	if (driver.node == nullptr)
		return "";
    
	if (driver.node->getName().empty())
		return "";

	return driver.node->getName() + "_" + getOutputName(outputPort);
}



}
