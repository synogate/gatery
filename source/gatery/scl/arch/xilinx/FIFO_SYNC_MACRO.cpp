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

#include "FIFO_SYNC_MACRO.h"

#include <gatery/utils/Exceptions.h>
#include <gatery/utils/Preprocessor.h>

#include <boost/format.hpp>

namespace gtry::scl::arch::xilinx {

FIFO_SYNC_MACRO::FIFO_SYNC_MACRO(size_t width, FIFOSize fifoSize)
{
	m_libraryName = "UNIMACRO";
	m_packageName = "VCOMPONENTS";
	m_name = "FIFO_SYNC_MACRO";
	m_isEntity = false;
	m_clockNames = {"CLK"};
	m_resetNames = {"RST"};
	m_clocks.resize(CLK_COUNT);

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
				HCL_ASSERT_HINT(false, "The maximal data width of FIFO_SYNC_MACRO for 18Kb is 36 bits!");
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
				HCL_ASSERT_HINT(false, "The maximal data width of FIFO_SYNC_MACRO is 72 bits!");
		break;
	}
	m_genericParameters["DATA_WIDTH"] = width;

	resizeIOPorts(IN_COUNT, OUT_COUNT);

	declInputBit(IN_RDEN, "RDEN");
	declInputBit(IN_WREN, "WREN");
	declInputBitVector(IN_DI, "DI", m_width, "DATA_WIDTH");


	declOutputBit(OUT_ALMOSTEMPTY, "ALMOSTEMPTY");
	declOutputBit(OUT_ALMOSTFULL, "ALMOSTFULL");
	declOutputBit(OUT_EMPTY, "EMPTY");
	declOutputBit(OUT_FULL, "FULL");
	declOutputBit(OUT_RDERR, "RDERR");
	declOutputBit(OUT_WRERR, "WRERR");
	// UInts
	declOutputBitVector(OUT_DO, "DO", width, "DATA_WIDTH");
	declOutputBitVector(OUT_RDCOUNT, "RDCOUNT", counterWidth);
	declOutputBitVector(OUT_WRCOUNT, "WRCOUNT", counterWidth);
}


FIFO_SYNC_MACRO &FIFO_SYNC_MACRO::setAlmostEmpty(size_t numOccupied)
{
	m_genericParameters["ALMOST_EMPTY_OFFSET"] = numOccupied; //(boost::format("X%00000X") % numOccupied).str();
	return *this;
}

FIFO_SYNC_MACRO &FIFO_SYNC_MACRO::setAlmostFull(size_t numVacant)
{
	m_genericParameters["ALMOST_FULL_OFFSET"] = numVacant; //(boost::format("X%00000X") % numVacant).str();
	return *this;
}

FIFO_SYNC_MACRO &FIFO_SYNC_MACRO::setDevice(std::string device)
{
	m_genericParameters["DEVICE"] = std::move(device);
	return *this;
}

std::string FIFO_SYNC_MACRO::getTypeName() const
{
	return "FIFO_SYNC_MACRO";
}

void FIFO_SYNC_MACRO::assertValidity() const
{
}


std::unique_ptr<hlim::BaseNode> FIFO_SYNC_MACRO::cloneUnconnected() const
{
	FIFO_SYNC_MACRO *ptr;
	std::unique_ptr<BaseNode> res(ptr = new FIFO_SYNC_MACRO(m_width, m_fifoSize));
	copyBaseToClone(res.get());

	return res;
}

std::string FIFO_SYNC_MACRO::attemptInferOutputName(size_t outputPort) const
{
	auto driver = getDriver(IN_DI);
	
	if (driver.node == nullptr)
		return "";
	
	if (inputIsComingThroughParentNodeGroup(IN_DI)) 
		return "";

	if (driver.node->getName().empty())
		return "";

	return driver.node->getName() + "_" + getOutputName(outputPort);
}



}
