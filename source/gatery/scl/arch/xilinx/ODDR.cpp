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
#include "gatery/pch.h"
#include "ODDR.h"

#include <gatery/hlim/coreNodes/Node_Register.h>
#include <gatery/hlim/coreNodes/Node_Clk2Signal.h>
#include <gatery/hlim/NodeGroup.h>

namespace gtry::scl::arch::xilinx {

ODDR::ODDR()
{
	m_libraryName = "UNISIM";
	m_packageName = "VCOMPONENTS";
	m_name = "ODDR";
	m_clockNames = {"C"};
	m_resetNames = {""};
	m_clocks.resize(CLK_COUNT);

	resizeIOPorts(IN_COUNT, OUT_COUNT);

	declInputBit(IN_D1, "D1");
	declInputBit(IN_D2, "D2");
	declInputBit(IN_SET, "S");
	declInputBit(IN_RESET, "R");
	declInputBit(IN_CE, "CE");
	declOutputBit(OUT_Q, "Q");
}

void ODDR::setInput(size_t input, const Bit &bit)
{
	if (input == IN_SET)
		HCL_DESIGNCHECK_HINT(getDriver(IN_RESET).node == nullptr, "Only the set or the reset pin of a Xilinx ODDR can be used, but not both together!");

	if (input == IN_RESET)
		HCL_DESIGNCHECK_HINT(getDriver(IN_SET).node == nullptr, "Only the set or the reset pin of a Xilinx ODDR can be used, but not both together!");

	gtry::ExternalComponent::setInput(input, bit);
}

void ODDR::setEdgeMode(DDR_CLK_EDGE edgeMode)
{
	switch (edgeMode) {
		case OPPOSITE_EDGE: m_genericParameters["DDR_CLK_EDGE"] = "OPPOSITE_EDGE"; break;
		case SAME_EDGE: m_genericParameters["DDR_CLK_EDGE"] = "SAME_EDGE"; break;
	}
}

void ODDR::setResetType(SRTYPE srtype)
{
	switch (srtype) {
		case ASYNC: m_genericParameters["SRTYPE"] = "ASYNC"; break;
		case SYNC: m_genericParameters["SRTYPE"] = "SYNC"; break;
	}
}

void ODDR::setInitialOutputValue(bool value)
{
	m_genericParameters["INIT"].setBit(value?'1':'0');
}


std::unique_ptr<hlim::BaseNode> ODDR::cloneUnconnected() const
{
	std::unique_ptr<BaseNode> res(new ODDR());
	copyBaseToClone(res.get());

	return res;
}







bool ODDRPattern::scopedAttemptApply(hlim::NodeGroup *nodeGroup) const
{
	if (nodeGroup->getName() != "scl_oddr") return false;


	NodeGroupIO io(nodeGroup);

	if (!io.inputBits.contains("D0")) {
		dbg::log(dbg::LogMessage{} << dbg::LogMessage::LOG_ERROR << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING 
				<< "Not replacing " << nodeGroup << " with ODDR because the 'D0' signal could not be found or is not a bit!");
		return false;
	}

	if (!io.inputBits.contains("D1")) {
		dbg::log(dbg::LogMessage{} << dbg::LogMessage::LOG_ERROR << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING 
				<< "Not replacing " << nodeGroup << " with ODDR because the 'D1' signal could not be found or is not a bit!");
		return false;
	}

	if (!io.outputBits.contains("O")) {
		dbg::log(dbg::LogMessage{} << dbg::LogMessage::LOG_ERROR << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING 
				<< "Not replacing " << nodeGroup << " with ODDR because the 'O' signal could not be found or is not a bit!");
		return false;
	}


	Bit &D0 = io.inputBits["D0"];
	Bit &D1 = io.inputBits["D1"];



	NodeGroupSurgeryHelper area(nodeGroup);
	auto *clkSignal = area.getSignal("CLK");
	if (clkSignal == nullptr) {
		dbg::log(dbg::LogMessage{} << dbg::LogMessage::LOG_ERROR << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING 
				<< "Not replacing " << nodeGroup << " with ODDR because no 'CLK' signal was found!");
		return false;
	}
	
	auto *clk2signal = dynamic_cast<hlim::Node_Clk2Signal*>(clkSignal->getNonSignalDriver(0).node);
	if (clk2signal == nullptr) {
		dbg::log(dbg::LogMessage{} << dbg::LogMessage::LOG_ERROR << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING 
				<< "Not replacing " << nodeGroup << " with ODDR because no 'CLK' signal not driven by clock!");
		return false;
	}

	hlim::Clock *clock = clk2signal->getClocks()[0];

	Bit &output = io.outputBits["O"];

	auto *ddr = DesignScope::createNode<ODDR>();

	ddr->attachClock(clock, ODDR::CLK_IN);
	ddr->setInput(ODDR::IN_D1, D0);
	ddr->setInput(ODDR::IN_D2, D1);
	ddr->setInput(ODDR::IN_CE, Bit('1'));

	output.exportOverride(ddr->getOutputBit(ODDR::OUT_Q));

	return true;
}

}
