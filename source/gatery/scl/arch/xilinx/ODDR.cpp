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
#include "gatery/scl_pch.h"
#include "ODDR.h"

#include <gatery/hlim/coreNodes/Node_Register.h>
#include <gatery/hlim/coreNodes/Node_Clk2Signal.h>
#include <gatery/hlim/coreNodes/Node_ClkRst2Signal.h>
#include <gatery/hlim/NodeGroup.h>

#include <gatery/scl/io/ddr.h>

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


bool ODDRPattern::performReplacement(hlim::NodeGroup *nodeGroup, ReplaceInfo &replacement) const
{
	auto *params = dynamic_cast<gtry::scl::DDROutParams*>(nodeGroup->getMetaInfo());
	if (params == nullptr) {
		dbg::log(dbg::LogMessage{nodeGroup} << dbg::LogMessage::LOG_ERROR << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING 
				<< "Not replacing " << nodeGroup << " with " << m_patternName << " because it doesn't have the DDROutParams meta parameters attached!");
		return false;
	}

	if (!params->inputRegs) {
		dbg::log(dbg::LogMessage{nodeGroup} << dbg::LogMessage::LOG_ERROR << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING 
				<< "Not replacing " << nodeGroup << " with " << m_patternName << " because the area doesn't have input registers (which "<<m_patternName<<" requires).");
		return false;
	}

	if (params->outputRegs) {
		dbg::log(dbg::LogMessage{nodeGroup} << dbg::LogMessage::LOG_ERROR << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING 
				<< "Not replacing " << nodeGroup << " with " << m_patternName << " because the area has output registers (which "<<m_patternName<<" doesn't support).");
		return false;
	}

	const auto& attr = replacement.clock->getRegAttribs();

	if (attr.resetType != hlim::RegisterAttributes::ResetType::NONE && 
		attr.resetType != hlim::RegisterAttributes::ResetType::SYNCHRONOUS) {
		dbg::log(dbg::LogMessage{nodeGroup} << dbg::LogMessage::LOG_ERROR << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING 
				<< "Not replacing " << nodeGroup << " with " << m_patternName << " because only synchronous and no resets are supported and the used clock is neither.");
		return false;
	}



	return splitByReset(nodeGroup, replacement);
}

void ODDRPattern::performConstResetReplacement(hlim::NodeGroup *nodeGroup, ConstResetReplaceInfo &replacement) const
{
	replacement.O = ConstBVec(replacement.D[0].width());
	for (auto bitIdx : utils::Range(replacement.D[0].size())) {

		auto *ddr = DesignScope::createNode<ODDR>();

		ddr->attachClock(replacement.clock, ODDR::CLK_IN);
		ddr->setInput(ODDR::IN_D1, replacement.D[0][bitIdx]);
		ddr->setInput(ODDR::IN_D2, replacement.D[1][bitIdx]);
		ddr->setInput(ODDR::IN_CE, Bit('1'));

		replacement.O[bitIdx] = ddr->getOutputBit(ODDR::OUT_Q);

		if (replacement.reset) {
			const auto& attr = replacement.clock->getRegAttribs();

			if (attr.resetType != hlim::RegisterAttributes::ResetType::NONE) {

				auto *clk2rst = DesignScope::createNode<hlim::Node_ClkRst2Signal>();
				clk2rst->setClock(replacement.clock);

				Bit rstSignal = SignalReadPort(clk2rst);

				if(attr.resetActive != hlim::RegisterAttributes::Active::HIGH)
					rstSignal = !rstSignal;

				ddr->setResetType(ODDR::SRTYPE::SYNC);
				if (*replacement.reset)
					ddr->setInput(ODDR::IN_SET, rstSignal);
				else
					ddr->setInput(ODDR::IN_RESET, rstSignal);
			}

			if (attr.initializeRegs)
				ddr->setInitialOutputValue(*replacement.reset);
		}
	}
}




}
