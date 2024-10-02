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
#include "BaseDDROutPattern.h"

#include <gatery/hlim/coreNodes/Node_Register.h>
#include <gatery/hlim/coreNodes/Node_Constant.h>
#include <gatery/hlim/coreNodes/Node_Clk2Signal.h>
#include <gatery/hlim/coreNodes/Node_Signal.h>
#include <gatery/hlim/NodeGroup.h>
#include <gatery/frontend/GraphTools.h>
#include <gatery/frontend/Pack.h>
#include <gatery/debug/DebugInterface.h>

namespace gtry::scl::arch {


bool BaseDDROutPattern::scopedAttemptApply(hlim::NodeGroup *nodeGroup) const
{
	if (nodeGroup->getName() != "scl_oddr") return false;

	NodeGroupIO io(nodeGroup);

	if (!io.inputBits.contains("D0") && !io.inputBVecs.contains("D0")) {
		dbg::log(dbg::LogMessage{nodeGroup} << dbg::LogMessage::LOG_ERROR << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING 
				<< "Not replacing " << nodeGroup << " with " << m_patternName << " because the 'D0' signal could not be found!");
		return false;
	}

	bool vectorBased = io.inputBVecs.contains("D0");

 	if (vectorBased) {
		if (!io.inputBVecs.contains("D1")) {
			dbg::log(dbg::LogMessage{nodeGroup} << dbg::LogMessage::LOG_ERROR << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING 
					<< "Not replacing " << nodeGroup << " with " << m_patternName << " because the 'D1' signal could not be found or is not a bit vector (as D0 is)!");
			return false;
		}
/*
		if (!io.inputBVecs.contains("reset")) {
			dbg::log(dbg::LogMessage{nodeGroup} << dbg::LogMessage::LOG_ERROR << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING 
					<< "Not replacing " << nodeGroup << " with " << m_patternName << " because the 'reset' signal could not be found or is not a bit vector (as D0 is)!");
			return false;
		}
*/
		if (!io.outputBVecs.contains("O")) {
			dbg::log(dbg::LogMessage{nodeGroup} << dbg::LogMessage::LOG_ERROR << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING 
					<< "Not replacing " << nodeGroup << " with " << m_patternName << " because the 'O' signal could not be found or is not a bit vector (as D0 is)!");
			return false;
		}
	} else {
		if (!io.inputBits.contains("D1")) {
			dbg::log(dbg::LogMessage{nodeGroup} << dbg::LogMessage::LOG_ERROR << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING 
					<< "Not replacing " << nodeGroup << " with " << m_patternName << " because the 'D1' signal could not be found or is not a bit!");
			return false;
		}
/*
		if (!io.inputBits.contains("reset")) {
			dbg::log(dbg::LogMessage{nodeGroup} << dbg::LogMessage::LOG_ERROR << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING 
					<< "Not replacing " << nodeGroup << " with " << m_patternName << " because the 'reset' signal could not be found or is not a bit!");
			return false;
		}
*/
		if (!io.outputBits.contains("O")) {
			dbg::log(dbg::LogMessage{nodeGroup} << dbg::LogMessage::LOG_ERROR << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING 
					<< "Not replacing " << nodeGroup << " with " << m_patternName << " because the 'O' signal could not be found or is not a bit!");
			return false;
		}
	}


	ReplaceInfo replaceInfo;

	if (vectorBased) {
		replaceInfo.D[0] = io.inputBVecs["D0"];
		replaceInfo.D[1] = io.inputBVecs["D1"];
		if (io.inputBVecs.contains("reset"))
			replaceInfo.reset = io.inputBVecs["reset"];

		if (replaceInfo.D[0].size() != io.outputBVecs["O"].size()) {
			dbg::log(dbg::LogMessage{nodeGroup} << dbg::LogMessage::LOG_ERROR << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING 
					<< "Not replacing " << nodeGroup << " with " << m_patternName << " because the 'D0' and 'O' have different sizes!");
			return false;
		}

	} else {
		replaceInfo.D[0] = (BVec) cat(io.inputBits["D0"]);
		replaceInfo.D[1] = (BVec) cat(io.inputBits["D1"]);
		if (io.inputBits.contains("reset"))
			replaceInfo.reset = (BVec) cat(io.inputBits["reset"]);
	}

	if (replaceInfo.D[0].size() != replaceInfo.D[1].size()) {
		dbg::log(dbg::LogMessage{nodeGroup} << dbg::LogMessage::LOG_ERROR << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING 
				<< "Not replacing " << nodeGroup << " with " << m_patternName << " because the 'D0' and 'D1' have different sizes!");
		return false;
	}

	replaceInfo.O = replaceInfo.D[0].width();


	NodeGroupSurgeryHelper area(nodeGroup);
	auto *clkSignal = area.getSignal("CLK");
	if (clkSignal == nullptr) {
		dbg::log(dbg::LogMessage{nodeGroup} << dbg::LogMessage::LOG_ERROR << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING 
				<< "Not replacing " << nodeGroup << " with " << m_patternName << " because no 'CLK' signal was found!");
		return false;
	}
	
	auto *clk2signal = dynamic_cast<hlim::Node_Clk2Signal*>(clkSignal->getNonSignalDriver(0).node);
	if (clk2signal == nullptr) {
		dbg::log(dbg::LogMessage{nodeGroup} << dbg::LogMessage::LOG_ERROR << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING 
				<< "Not replacing " << nodeGroup << " with " << m_patternName << " because no 'CLK' signal not driven by clock!");
		return false;
	}

	replaceInfo.clock = clk2signal->getClocks()[0];

	if (!performReplacement(nodeGroup, replaceInfo))
		return false;

	if (vectorBased) {
		io.outputBVecs["O"].exportOverride(replaceInfo.O);
	} else {
		io.outputBits["O"].exportOverride(replaceInfo.O.lsb());
	}

	return true;
}

namespace {

template<typename Functor>
void forEachConsecutiveStretch(const sim::DefaultBitVectorState &value, Functor functor)
{
	if (value.size() == 0) return;

	bool lastValue = value.get(sim::DefaultConfig::VALUE, 0);
	bool lastDefined = value.get(sim::DefaultConfig::DEFINED, 0);
	size_t startIndex = 0;

	for (size_t i = 1; i < value.size(); i++) {
		bool v = value.get(sim::DefaultConfig::VALUE, i);
		bool d = value.get(sim::DefaultConfig::DEFINED, i);

		if (lastDefined != d || lastValue != v) {
			functor(lastValue, lastDefined, startIndex, i);
			lastDefined = d;
			lastValue = v;
			startIndex = i;
		}
	}
	functor(lastValue, lastDefined, startIndex, value.size());
}

}

bool BaseDDROutPattern::splitByReset(hlim::NodeGroup *nodeGroup, ReplaceInfo &replacement) const
{
	hlim::NodePort driver;
	if (replacement.reset.node() != nullptr)
		driver = replacement.reset.readPort();
	if (driver.node != nullptr) {
		if (dynamic_cast<hlim::Node_Signal*>(driver.node) != nullptr)
			driver = driver.node->getNonSignalDriver(0);
		
		if (auto *constNode = dynamic_cast<hlim::Node_Constant*>(driver.node)) {
			const auto &resetValue = constNode->getValue();

			forEachConsecutiveStretch(resetValue, [&](bool value, bool defined, size_t start, size_t end) {
				ConstResetReplaceInfo info;
				info.clock = replacement.clock;
				info.D[0] = replacement.D[0](Selection::Range(start, end));
				info.D[1] = replacement.D[1](Selection::Range(start, end));
				if (defined)
					info.reset = value;
				else
					info.reset.reset();
				performConstResetReplacement(nodeGroup, info);
				replacement.O(Selection::Range(start, end)) = info.O;
			});
		} else {
			auto resetValue = evaluateStatically(driver);
			if (sim::allDefined(resetValue, 0, resetValue.size())) {
				forEachConsecutiveStretch(resetValue, [&](bool value, bool defined, size_t start, size_t end) {
					ConstResetReplaceInfo info;
					info.clock = replacement.clock;
					info.D[0] = replacement.D[0](Selection::Range(start, end));
					info.D[1] = replacement.D[1](Selection::Range(start, end));
					if (defined)
						info.reset = value;
					else
						info.reset.reset();
					performConstResetReplacement(nodeGroup, info);
					replacement.O(Selection::Range(start, end)) = info.O;
				});
			} else {
				dbg::log(dbg::LogMessage{nodeGroup} << dbg::LogMessage::LOG_ERROR << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING 
						<< "Not replacing " << nodeGroup << " with " << m_patternName << " because the reset signal is not fully constant!");
				return false;
			}
		}
	} else {
		ConstResetReplaceInfo info;
		info.clock = replacement.clock;
		info.reset.reset();
		info.D[0] = replacement.D[0];
		info.D[1] = replacement.D[1];

		performConstResetReplacement(nodeGroup, info);
		replacement.O = info.O;
	}
	return true;
}

}
