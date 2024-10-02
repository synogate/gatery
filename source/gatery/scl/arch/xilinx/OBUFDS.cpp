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
#include "OBUFDS.h"

namespace gtry::scl::arch::xilinx {

OBUFDS::OBUFDS()
{
	m_libraryName = "UNISIM";
	m_name = "OBUFDS";
	m_genericParameters["IOSTANDARD"] = "DEFAULT";
	m_genericParameters["SLEW"] = "SLOW";
	m_clockNames = {};
	m_resetNames = {};

	resizeIOPorts(1, 2);
	declInputBit(0, "I");

	declOutputBit(0, "O");
	declOutputBit(1, "OB");
}

std::string OBUFDS::getTypeName() const
{
	return "OBUFDS";
}

void OBUFDS::assertValidity() const
{
}

std::unique_ptr<hlim::BaseNode> OBUFDS::cloneUnconnected() const
{
	OBUFDS *ptr;
	std::unique_ptr<BaseNode> res(ptr = new OBUFDS());
	copyBaseToClone(res.get());

	return res;
}

std::string OBUFDS::attemptInferOutputName(size_t outputPort) const
{
	auto driver = getDriver(0);
	if (driver.node == nullptr)
		return "";
	if (inputIsComingThroughParentNodeGroup(0)) 
		return "";
	if (driver.node->getName().empty())
		return "";

	if (outputPort == 0)
		return driver.node->getName() + "_pos";
	else
		return driver.node->getName() + "_neg";
}



}
