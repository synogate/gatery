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
#include "ALTDDIO_OUT.h"
#include "IntelDevice.h"

#include <gatery/hlim/coreNodes/Node_Register.h>
#include <gatery/hlim/coreNodes/Node_Clk2Signal.h>
#include <gatery/hlim/coreNodes/Node_ClkRst2Signal.h>
#include <gatery/hlim/NodeGroup.h>


#include <gatery/scl/io/ddr.h>


//#include <gatery/hlim/Clock.h>

namespace gtry::scl::arch::intel 
{
	ALTDDIO_OUT::ALTDDIO_OUT(BitWidth width) : m_width(width)
	{
		m_libraryName = "altera_mf";
		m_packageName = "altera_mf_components";
		m_name = "ALTDDIO_OUT";

		m_clockNames = { "OUTCLOCK" };
		m_resetNames = { "" };
		m_clocks.resize(1);

		resizeIOPorts(IN_COUNT, OUT_COUNT);

		m_genericParameters["extend_oe_disable"] = "OFF";
		m_genericParameters["invert_output"] = "OFF";
		m_genericParameters["lpm_hint"] = "UNUSED";
		m_genericParameters["lpm_type"] = "altddio_out";
		m_genericParameters["oe_reg"] = "UNREGISTERED";
		m_genericParameters["power_up_high"] = "OFF";
		m_genericParameters["width"] = width.value;

		declInputBitVector(IN_DATAIN_H, "DATAIN_H", width.value, "WIDTH");
		declInputBitVector(IN_DATAIN_L, "DATAIN_L", width.value, "WIDTH");

		declInputBit(IN_OUTCLOCKEN, "OUTCLOCKEN");
		declInputBit(IN_ACLR, "ACLR");
		declInputBit(IN_ASET, "ASET");
		declInputBit(IN_OE, "OE");
		declInputBit(IN_SCLR, "SCLR");
		declInputBit(IN_SSET, "SSET");

		declOutputBitVector(OUT_DATAOUT, "DATAOUT", width.value, "WIDTH");
	}

	ALTDDIO_OUT& ALTDDIO_OUT::setupSimulationDeviceFamily(std::string familyName)
	{
		m_genericParameters["intended_device_family"] = std::move(familyName);
		return *this;
	}

	ALTDDIO_OUT& ALTDDIO_OUT::enableOutputRegister()
	{
		m_genericParameters["oe_reg"] = "REGISTERED";
		return *this;
	}

	ALTDDIO_OUT& ALTDDIO_OUT::powerUpHigh()
	{
		m_genericParameters["power_up_high"] = "ON";
		return *this;
	}

	std::unique_ptr<hlim::BaseNode> ALTDDIO_OUT::cloneUnconnected() const
	{
		std::unique_ptr<BaseNode> res(new ALTDDIO_OUT(m_width));
		copyBaseToClone(res.get());
		return res;
	}




	bool ALTDDIO_OUTPattern::performReplacement(hlim::NodeGroup *nodeGroup, ReplaceInfo &replacement) const
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

		const auto& attr = replacement.clock->getRegAttribs();

		if (attr.resetType != hlim::RegisterAttributes::ResetType::NONE && 
			attr.resetType != hlim::RegisterAttributes::ResetType::SYNCHRONOUS) {
			dbg::log(dbg::LogMessage{nodeGroup} << dbg::LogMessage::LOG_ERROR << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING 
					<< "Not replacing " << nodeGroup << " with " << m_patternName << " because only synchronous and no resets are supported and the used clock is neither.");
			return false;
		}


		return splitByReset(nodeGroup, replacement);
	}

	void ALTDDIO_OUTPattern::performConstResetReplacement(hlim::NodeGroup *nodeGroup, ConstResetReplaceInfo &replacement) const
	{
		auto *params = dynamic_cast<gtry::scl::DDROutParams*>(nodeGroup->getMetaInfo());

		auto *ddr = DesignScope::createNode<ALTDDIO_OUT>(replacement.D[0].width());

		if (params->outputRegs)
			ddr->enableOutputRegister();

		ddr->attachClock(replacement.clock, ALTDDIO_OUT::CLK_OUTCLOCK);
		ddr->setInput(ALTDDIO_OUT::IN_DATAIN_H, replacement.D[0]);
		ddr->setInput(ALTDDIO_OUT::IN_DATAIN_L, replacement.D[1]);
		ddr->setInput(ALTDDIO_OUT::IN_OE, Bit('1'));
		if (replacement.reset) {
			const auto& attr = replacement.clock->getRegAttribs();
	
			if (attr.resetType != hlim::RegisterAttributes::ResetType::NONE) {

				auto *clk2rst = DesignScope::createNode<hlim::Node_ClkRst2Signal>();
				clk2rst->setClock(replacement.clock);

				Bit rstSignal = SignalReadPort(clk2rst);

				if(attr.resetActive != hlim::RegisterAttributes::Active::HIGH)
					rstSignal = !rstSignal;

				if (*replacement.reset)
					ddr->setInput(ALTDDIO_OUT::IN_SSET, rstSignal);
				else
					ddr->setInput(ALTDDIO_OUT::IN_SCLR, rstSignal);
			}

			if (attr.initializeRegs) {
				if (*replacement.reset)
					ddr->powerUpHigh();
			}
		}
		ddr->setupSimulationDeviceFamily(m_intelDevice.getFamily());

		replacement.O = ddr->getOutputBVec(ALTDDIO_OUT::OUT_DATAOUT);

	}



}
