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
#include "ALTDDIO_OUT.h"
#include "IntelDevice.h"

#include <gatery/hlim/coreNodes/Node_Register.h>
#include <gatery/hlim/NodeGroup.h>


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

	std::unique_ptr<hlim::BaseNode> ALTDDIO_OUT::cloneUnconnected() const
	{
		std::unique_ptr<BaseNode> res(new ALTDDIO_OUT(m_width));
		copyBaseToClone(res.get());
		return res;
	}




	/// @todo: Refactor with xilinx counterpart into BaseDDROutPattern

	bool ALTDDIO_OUTPattern::scopedAttemptApply(hlim::NodeGroup *nodeGroup) const
	{
		if (nodeGroup->getName() != "scl_oddr") return false;

		NodeGroupIO io(nodeGroup);

		if (!io.inputBits.contains("D0") && !io.inputBVecs.contains("D0")) {
			dbg::log(dbg::LogMessage{} << dbg::LogMessage::LOG_ERROR << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING 
					<< "Not replacing " << nodeGroup << " with ALTDDIO_OUT because the 'D0' signal could not be found!");
			return false;
		}

		bool vectorBased = io.inputBVecs.contains("D0");

		if (vectorBased) {
			if (!io.inputBVecs.contains("D1")) {
				dbg::log(dbg::LogMessage{} << dbg::LogMessage::LOG_ERROR << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING 
						<< "Not replacing " << nodeGroup << " with ALTDDIO_OUT because the 'D1' signal could not be found or is not a bit vector (as D0 is)!");
				return false;
			}

			if (!io.outputBVecs.contains("O")) {
				dbg::log(dbg::LogMessage{} << dbg::LogMessage::LOG_ERROR << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING 
						<< "Not replacing " << nodeGroup << " with ALTDDIO_OUT because the 'O' signal could not be found or is not a bit vector (as D0 is)!");
				return false;
			}
		} else {
			if (!io.inputBits.contains("D1")) {
				dbg::log(dbg::LogMessage{} << dbg::LogMessage::LOG_ERROR << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING 
						<< "Not replacing " << nodeGroup << " with ALTDDIO_OUT because the 'D1' signal could not be found or is not a bit!");
				return false;
			}

			if (!io.outputBits.contains("O")) {
				dbg::log(dbg::LogMessage{} << dbg::LogMessage::LOG_ERROR << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING 
						<< "Not replacing " << nodeGroup << " with ALTDDIO_OUT because the 'O' signal could not be found or is not a bit!");
				return false;
			}
		}


		auto locateRegister = [&](hlim::Node_Signal *node, const char *name)->hlim::Node_Register*{
			hlim::Node_Register *outputReg = nullptr;
			std::set<hlim::NodePort> alreadyHandled;
			for (auto nh : node->exploreOutput(0).skipDependencies()) {
				if(alreadyHandled.contains(nh.nodePort())) {
					nh.backtrack();
					continue;
				}
				if (auto* reg = dynamic_cast<hlim::Node_Register*>(nh.node())) {
					if (outputReg != nullptr) {
						if (reg->getClocks()[0] != outputReg->getClocks()[0]) {
							dbg::log(dbg::LogMessage{} << dbg::LogMessage::LOG_ERROR << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING 
									<< "Not replacing " << nodeGroup << " with ALTDDIO_OUT because the " << name << " input drives multiple registers with different clocks!");
							return nullptr;
						}
					} else
						outputReg = reg;
					nh.backtrack();
				}
				else if (!nh.isNodeType<hlim::Node_Signal>()) {
					dbg::log(dbg::LogMessage{} << dbg::LogMessage::LOG_ERROR << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING 
							<< "Not replacing " << nodeGroup << " with ALTDDIO_OUT because the " << name << " input drives things other than registers!");
					return nullptr;
				}
				alreadyHandled.insert(nh.nodePort());
			}
			return outputReg;
		};

		BVec D0, D1;
		hlim::Node_Register *reg0 = nullptr;
		hlim::Node_Register *reg1 = nullptr;
		if (vectorBased) {

			reg0 = locateRegister(io.inputBVecs["D0"].node(), "'D0'");
			if (reg0 == nullptr) return false;
			reg1 = locateRegister(io.inputBVecs["D1"].node(), "'D1'");
			if (reg1 == nullptr) return false;

			D0 = io.inputBVecs["D0"];
			D1 = io.inputBVecs["D1"];

			if (D0.size() != io.outputBVecs["O"].size()) {
				dbg::log(dbg::LogMessage{} << dbg::LogMessage::LOG_ERROR << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING 
						<< "Not replacing " << nodeGroup << " with ALTDDIO_OUT because the 'D0' and 'O' have different sizes!");
				return false;
			}

		} else {
			reg0 = locateRegister(io.inputBits["D0"].node(), "'D0'");
			if (reg0 == nullptr) return false;
			reg1 = locateRegister(io.inputBits["D1"].node(), "'D1'");
			if (reg1 == nullptr) return false;

			D0 = (BVec) cat(io.inputBits["D0"]);
			D1 = (BVec) cat(io.inputBits["D1"]);
		}

		if (D0.size() != D1.size()) {
			dbg::log(dbg::LogMessage{} << dbg::LogMessage::LOG_ERROR << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING 
					<< "Not replacing " << nodeGroup << " with ALTDDIO_OUT because the 'D0' and 'D1' have different sizes!");
			return false;
		}


		hlim::Clock *clock = reg0->getClocks()[0];
		if (reg1->getClocks()[0] != clock) {
			dbg::log(dbg::LogMessage{} << dbg::LogMessage::LOG_ERROR << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING 
					<< "Not replacing " << nodeGroup << " with ALTDDIO_OUT because the 'D0' and 'D1' have registers driven by different clocks!");
			return false;
		}

		auto *ddr = DesignScope::createNode<ALTDDIO_OUT>(D0.width());

		ddr->attachClock(clock, ALTDDIO_OUT::CLK_OUTCLOCK);
		ddr->setInput(ALTDDIO_OUT::IN_DATAIN_H, D0);
		ddr->setInput(ALTDDIO_OUT::IN_DATAIN_L, D1);
		ddr->setInput(ALTDDIO_OUT::IN_OE, Bit('1'));
		ddr->setupSimulationDeviceFamily(m_intelDevice.getFamily());

		if (vectorBased) {
			io.outputBVecs["O"].exportOverride(ddr->getOutputBVec(ALTDDIO_OUT::OUT_DATAOUT));
		} else {
			io.outputBits["O"].exportOverride(ddr->getOutputBVec(ALTDDIO_OUT::OUT_DATAOUT).lsb());
		}

		return true;
	}



}
