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

#include "OverrideResolution.h"

#include "../supportNodes/Node_RegSpawner.h"
#include "../supportNodes/Node_RegHint.h"
#include "../supportNodes/Node_NegativeRegister.h"
#include "../supportNodes/Node_RetimingBlocker.h"
#include "../coreNodes/Node_Register.h"

#include <gatery/debug/DebugInterface.h>

#include "../CNF.h"
#include "../Circuit.h"
#include "../RegisterRetiming.h"
#include "../GraphTools.h"

#include "../../export/DotExport.h"

#include <queue>
#include <limits>
#include <iostream>

namespace gtry::hlim {

void determineNegativeRegisterEnables(Circuit &circuit, Subnet &subnet)
{
	std::map<Conjunction, NodePort> buildEnableSignalCache;

	for (auto& n : subnet)
		if (auto* negReg = dynamic_cast<Node_NegativeRegister*>(n)) {
			Conjunction enable = suggestForwardRetimingEnableCondition(circuit, subnet, negReg->getDriver(0));

			NodePort enableDriver;

			auto it = buildEnableSignalCache.find(enable);
			if (it != buildEnableSignalCache.end())
				enableDriver = it->second;
			else {
				enableDriver = enable.build(*negReg->getGroup(), &subnet, false);
				buildEnableSignalCache[enable] = enableDriver;
			}

			negReg->expectedEnable(enableDriver);

			while (!negReg->getDirectlyDriven((size_t)Node_NegativeRegister::Outputs::enable).empty()) {
				auto driven = negReg->getDirectlyDriven((size_t)Node_NegativeRegister::Outputs::enable).front();
				driven.node->rewireInput(driven.port, enableDriver);
			}
		}
}


void resolveRetimingHints(Circuit &circuit, Subnet &subnet)
{
	// Locate all spawners in subnet
	std::vector<Node_RegSpawner*> spawner;
	for (auto &n : subnet)
		if (auto *regSpawner = dynamic_cast<Node_RegSpawner*>(n))
			spawner.push_back(regSpawner);
/*
	// Locate all register hints in subnet that can be reached from the spawners
	auto regHints = getRegHintDistanceToSpawners(spawner, subnet);
	std::sort(regHints.begin(), regHints.end(), [](const auto &lhs, const auto &rhs){ 
		if (lhs.first < rhs.first) return true;
		if (lhs.first > rhs.first) return false;
		return lhs.second->getId() > rhs.second->getId();
	});
*/
	std::vector<Node_RegHint*> regHints;
	for (auto& n : subnet)
		if (auto* regHint = dynamic_cast<Node_RegHint*>(n))
			regHints.push_back(regHint);			

	/**
	 * @todo This sorting is actually not sufficient. They need to be properly topologically sorted (with the added difficult that the graph can be cyclic).
	 * For now, we get around this by disabling forward retiming for downstream registers, but this is not a good solution.
	 */

	// Resolve all register hints back to front
	//for (std::size_t i = regHints.size()-1; i < regHints.size(); i--) {
	for (std::size_t i = 0; i < regHints.size(); i++) {
		//auto node = regHints[i].second;
		auto node = regHints[i];
		// Skip orphaned retiming hints
		if (node->getDirectlyDriven(0).empty()) continue;

/*
		{
			DotExport exp("state.dot");
			exp(circuit);
			exp.runGraphViz("state.svg");

			std::cout << "Retiming node " << node->getId() << std::endl;
		}
		{
			DotExport exp("subnet.dot");
			exp(circuit, subnet.asConst());
			exp.runGraphViz("subnet.svg");

			std::cout << "Retiming node " << node->getId() << std::endl;
		}
*/
		retimeForwardToOutput(circuit, subnet, {.node = node, .port = 0}, {.downstreamDisableForwardRT = true});
		node->bypassOutputToInput(0, 0);
	}

	for (auto *regSpawner : spawner) {
		regSpawner->markResolved();

		dbg::log(dbg::LogMessage{}	
					<< dbg::LogMessage::LOG_INFO
					<< dbg::LogMessage::LOG_POSTPROCESSING
					<< dbg::LogMessage::Anchor{ regSpawner->getGroup() }
					<< "Registers spawner " << regSpawner << " delayed its signals by " << regSpawner->getNumStagesSpawned() << " cycle(s).");
	}
/*
	for (auto& n : subnet)
		if (auto* regHint = dynamic_cast<Node_RegHint*>(n)) {
			bool found = false;
			for (auto h : regHints)
				if (h.second == regHint)
					found = true;
			HCL_ASSERT(found);
			HCL_ASSERT(regHint->getDirectlyDriven(0).empty());
		}
*/
}

void annihilateNegativeRegisters(Circuit &circuit, Subnet &subnet)
{
	for (auto& n : subnet)
		if (auto* negReg = dynamic_cast<Node_NegativeRegister*>(n)) {
			auto driver = negReg->getNonSignalDriver(Node_Register::DATA);
			auto *reg = dynamic_cast<Node_Register*>(driver.node);

			if (reg == nullptr) {
				dbg::log(dbg::LogMessage() << dbg::LogMessage::LOG_ERROR << dbg::LogMessage::LOG_POSTPROCESSING << "Can not resolve negative register " << negReg << " because it is driven by " << driver.node << " which is not a register with which it can be fused. This usually means that retiming was unsuccessfull (negative register within a combinatorical loop?).");
				HCL_ASSERT_HINT(false, "Can not resolve negative register because the register to resolve with was not found.");
			} else {
				// todo: check enables compatible
				Conjunction regEnable;
				if (reg->getDriver(Node_Register::ENABLE).node != nullptr)
					regEnable = Conjunction::fromInput( {.node = reg, .port = Node_Register::ENABLE});

				Conjunction negRegEnable;
				if (negReg->expectedEnable().node != nullptr)
					negRegEnable = Conjunction::fromOutput(negReg->expectedEnable());

				if (!regEnable.isEqualTo(negRegEnable)) {
					dbg::log(dbg::LogMessage() << dbg::LogMessage::LOG_ERROR << dbg::LogMessage::LOG_POSTPROCESSING 
							<< "Can not resolve negative register " << negReg << " because it is driven by register " << driver.node << " which has an incompatible enable signal.");
					HCL_ASSERT_HINT(false, "Can not resolve negative register because of incompatible enable signals");
				} else {
					// Rewire data output to the regs input
					auto dataDriven = negReg->getDirectlyDriven((size_t)Node_NegativeRegister::Outputs::data);
					for (const auto &driven : dataDriven)
						driven.node->rewireInput(driven.port, reg->getDriver(Node_Register::DATA));

					// Rewire enable output to the regs enable
					auto enableDriven = negReg->getDirectlyDriven((size_t)Node_NegativeRegister::Outputs::enable);
					for (const auto &driven : enableDriven)
						driven.node->rewireInput(driven.port, reg->getDriver(Node_Register::ENABLE));
				}
			}
		}
}


void bypassRetimingBlockers(Circuit &circuit, Subnet &subnet)
{
	for (auto &n : subnet)
		if (dynamic_cast<Node_RetimingBlocker*>(n))
			n->bypassOutputToInput(0, 0);
}

}
