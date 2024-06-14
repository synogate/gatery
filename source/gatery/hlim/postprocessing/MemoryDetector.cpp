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
#include "MemoryDetector.h"

#include "../Circuit.h"
#include "../Clock.h"
#include "../CNF.h"

#include "../coreNodes/Node_Signal.h"
#include "../coreNodes/Node_Register.h"
#include "../coreNodes/Node_Constant.h"
#include "../coreNodes/Node_Compare.h"
#include "../coreNodes/Node_Logic.h"
#include "../coreNodes/Node_Multiplexer.h"
#include "../coreNodes/Node_Rewire.h"
#include "../coreNodes/Node_Arithmetic.h"
#include "../coreNodes/Node_Pin.h"
#include "../coreNodes/Node_ClkRst2Signal.h"
#include "../supportNodes/Node_Memory.h"
#include "../supportNodes/Node_MemPort.h"
#include "../GraphExploration.h"
#include "../RegisterRetiming.h"
#include "../GraphTools.h"

#include "../../simulation/SimulationContext.h"

#include "ExternalMemorySimulation.h"

//#define DEBUG_OUTPUT

#ifdef DEBUG_OUTPUT
#include "../Subnet.h"
#include "../../export/DotExport.h"
#endif

#include "../../debug/DebugInterface.h"

#include <iostream>
#include <sstream>
#include <vector>
#include <set>
#include <optional>

namespace gtry::hlim {

MemoryGroup *formMemoryGroupIfNecessary(Circuit &circuit, Node_Memory *memory)
{
	auto* memoryGroup = dynamic_cast<MemoryGroup*>(memory->getGroup()->getMetaInfo());
	if (memoryGroup == nullptr) {
		dbg::log(dbg::LogMessage(memory->getGroup()) << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_POSTPROCESSING << "Forming memory group around " << memory);

		HCL_ASSERT(memory->getGroup()->getMetaInfo() == nullptr);		

		auto *logicalMemNodeGroup = memory->getGroup();

		auto *physMemNodeGroup = logicalMemNodeGroup->addChildNodeGroup(NodeGroupType::ENTITY, "physical_memory");
		physMemNodeGroup->recordStackTrace();
		memory->moveToGroup(physMemNodeGroup);

		memoryGroup = memory->getGroup()->createMetaInfo<MemoryGroup>(memory->getGroup());
		memoryGroup->pullInPorts(memory);
	}
	return memoryGroup;
}

void findMemoryGroups(Circuit &circuit)
{
	for (auto &node : circuit.getNodes())
		if (auto *memory = dynamic_cast<Node_Memory*>(node.get()))
			formMemoryGroupIfNecessary(circuit, memory);
}


bool MemoryGroup::ReadPort::findOutputRegisters(size_t readLatency, NodeGroup *memoryNodeGroup)
{
	// keep a list of encoutnered signal nodes to move into memory group.
	std::vector<BaseNode*> signalNodes;

	// Clear all and start from scratch
	dedicatedReadLatencyRegisters.resize(readLatency);
	for (auto &reg : dedicatedReadLatencyRegisters)
		reg = nullptr;

	Clock *clock = nullptr;

	// Start from the read port
	dataOutput = {.node = node.get(), .port = (size_t)Node_MemPort::Outputs::rdData};
	for (auto i : utils::Range(dedicatedReadLatencyRegisters.size())) {
		signalNodes.clear();

		// For each output (read port or register in the chain) ensure that it only drives another register, then add that register to the list
		Node_Register *reg = nullptr;
		for (auto nh : dataOutput.node->exploreOutput(dataOutput.port)) {
			if (nh.isSignal()) {
				signalNodes.push_back(nh.node());
			} else {
				if (nh.isNodeType<Node_Register>()) {
					auto dataReg = (Node_Register *) nh.node();
/*
	Actually, this must be possible (or be added by additional logic if not possible).

					// The register can't have a reset (since it's essentially memory).
					if (dataReg->hasResetValue())
						break;
*/
					if (reg == nullptr)
						reg = dataReg;
					else {
						// if multiple registers are driven, don't fuse them here but, but fail and let the register retiming handle the fusion
						reg = nullptr;
						break;
					}
				} else {
					// Don't make use of the regsister if other stuff is also directly driven by the port's output
					reg = nullptr;
					break;
				}
				nh.backtrack();
			}
		}

		// If there is a register, move it and all the signal nodes on the way into the memory group.
		if (reg != nullptr) {
			if (clock == nullptr)
				clock = reg->getClocks()[0];
			else if (clock != reg->getClocks()[0])
				break; // Hit a clock domain crossing, break early

			reg->getFlags().clear(Node_Register::Flags::ALLOW_RETIMING_BACKWARD).clear(Node_Register::Flags::ALLOW_RETIMING_FORWARD).insert(Node_Register::Flags::IS_BOUND_TO_MEMORY);
			// Move the entire signal path and the data register into the memory node group
			for (auto opt : signalNodes)
				opt->moveToGroup(memoryNodeGroup);
			reg->moveToGroup(memoryNodeGroup);
			dedicatedReadLatencyRegisters[i] = reg;
			
			// continue from this register and mark it as the output of the read port
			dataOutput = {.node = reg, .port = 0};

			signalNodes.clear();
		} else 
			break;
	}

	return dedicatedReadLatencyRegisters.back() != nullptr; // return true if all were found
}

MemoryGroup::MemoryGroup(NodeGroup *group) : m_nodeGroup(group)
{
	m_nodeGroup->setGroupType(NodeGroupType::SFU);
}

const MemoryGroup::ReadPort &MemoryGroup::findReadPort(Node_MemPort *memPort)
{
	for (auto &rp : m_readPorts)
		if (rp.node == memPort)
			return rp;
		
	HCL_ASSERT(false);
}

const MemoryGroup::WritePort &MemoryGroup::findWritePort(Node_MemPort *memPort)
{
	for (auto &wp : m_writePorts)
		if (wp.node == memPort)
			return wp;
		
	HCL_ASSERT(false);
}

void MemoryGroup::pullInPorts(Node_Memory *memory)
{
	m_memory = memory;

	// Initial naive grabbing of everything that might be usefull
	for (auto &np : m_memory->getPorts()) {
		auto *port = dynamic_cast<Node_MemPort*>(np.node);
		HCL_ASSERT(port->isWritePort() || port->isReadPort());
		// Check all write ports
		if (port->isWritePort()) {
			HCL_ASSERT_HINT(!port->isReadPort(), "For now I don't want to mix read and write ports");
			m_writePorts.push_back({.node=NodePtr<Node_MemPort>{port}});
			port->moveToGroup(m_nodeGroup);
		}
		// Check all read ports
		if (port->isReadPort()) {
			m_readPorts.push_back({.node = NodePtr<Node_MemPort>{port}});
			ReadPort &rp = m_readPorts.back();
			port->moveToGroup(m_nodeGroup);
			rp.dataOutput = {.node = port, .port = (size_t)Node_MemPort::Outputs::rdData};

			//NodePort readPortEnable = port->getNonSignalDriver((size_t)Node_MemPort::Inputs::enable);

			// Try and grab as many output registers as possible (up to read latency)
			// rp.findOutputRegisters(m_memory->getRequiredReadLatency(), this);
			// Actually, don't do this yet, makes things easier
		}
	}

	// Verify writing is only happening with one clock:
	{
		Node_MemPort *firstWritePort = nullptr;
		for (auto &np : m_memory->getPorts()) {
			auto *port = dynamic_cast<Node_MemPort*>(np.node);
			if (port->isWritePort()) {
				if (firstWritePort == nullptr)
					firstWritePort = port;
				else {
					if (firstWritePort->getClocks()[0] != port->getClocks()[0]) {
						std::stringstream issues;
						issues << "All write ports to a memory must have the same clock!\n";
						issues << "from:\n" << firstWritePort->getStackTrace() << "\n and from:\n" << port->getStackTrace();
						HCL_DESIGNCHECK_HINT(false, issues.str());
					}
				}
			}
		}
	}
}

NodeGroup *MemoryGroup::lazyCreateFixupNodeGroup()
{
	if (m_fixupNodeGroup == nullptr) {
		std::string name;
		if (m_memory->getName().empty())
			name = "Memory_Helper";
		else
			name = m_memory->getName()+"_Memory_Helper";
		m_fixupNodeGroup = m_nodeGroup->getParent()->addChildNodeGroup(NodeGroupType::ENTITY, name);
		m_fixupNodeGroup->recordStackTrace();
		m_fixupNodeGroup->setComment("Auto generated to handle various memory access issues such as read during write and read modify write hazards.");
	}
	return m_fixupNodeGroup;
}

void MemoryGroup::convertToReadBeforeWrite(Circuit &circuit)
{
	// If an async read happens after a write, it must
	// check if an address collision ocurred and if so directly forward the new value.
	for (auto &rp : m_readPorts) {

		// Iteratively push the read port up the dependency chain until the top is reached.
		// If dependent on a write port, build explicit hazard logic.
		// If any ports are dependent on us, make them dependent on the previous port.
		// Afterwards make this port dependent on whatever the previous port depends on.

		while (rp.node->getDriver((size_t)Node_MemPort::Inputs::orderAfter).node != nullptr) {
			auto *prevPort = dynamic_cast<Node_MemPort*>(rp.node->getDriver((size_t)Node_MemPort::Inputs::orderAfter).node);

			if (prevPort->isWritePort()) {
				auto *wp = prevPort;
				lazyCreateFixupNodeGroup();

				auto *addrCompNode = circuit.createNode<Node_Compare>(Node_Compare::EQ);
				addrCompNode->recordStackTrace();
				addrCompNode->moveToGroup(m_fixupNodeGroup);
				addrCompNode->setComment("Compare read and write addr for conflicts");
				addrCompNode->connectInput(0, rp.node->getDriver((size_t)Node_MemPort::Inputs::address));
				addrCompNode->connectInput(1, wp->getDriver((size_t)Node_MemPort::Inputs::address));

				NodePort conflict = {.node = addrCompNode, .port = 0ull};
				circuit.appendSignal(conflict)->setName("conflict");

				if (rp.node->getDriver((size_t)Node_MemPort::Inputs::enable).node != nullptr) {
					auto *logicAnd = circuit.createNode<Node_Logic>(Node_Logic::AND);
					logicAnd->moveToGroup(m_fixupNodeGroup);
					logicAnd->recordStackTrace();
					logicAnd->connectInput(0, conflict);
					logicAnd->connectInput(1, rp.node->getDriver((size_t)Node_MemPort::Inputs::enable));
					conflict = {.node = logicAnd, .port = 0ull};
					circuit.appendSignal(conflict)->setName("conflict_and_rdEn");
				}

				HCL_ASSERT(wp->getDriver((size_t)Node_MemPort::Inputs::enable).node == nullptr || wp->getNonSignalDriver((size_t)Node_MemPort::Inputs::enable) == wp->getNonSignalDriver((size_t)Node_MemPort::Inputs::wrEnable));
				if (wp->getDriver((size_t)Node_MemPort::Inputs::wrEnable).node != nullptr) {
					auto *logicAnd = circuit.createNode<Node_Logic>(Node_Logic::AND);
					logicAnd->moveToGroup(m_fixupNodeGroup);
					logicAnd->recordStackTrace();
					logicAnd->connectInput(0, conflict);
					logicAnd->connectInput(1, wp->getDriver((size_t)Node_MemPort::Inputs::wrEnable));
					conflict = {.node = logicAnd, .port = 0ull};
					circuit.appendSignal(conflict)->setName("conflict_and_wrEn");
				}

				NodePort wrData = wp->getDriver((size_t)Node_MemPort::Inputs::wrData);

				// If the read data gets delayed, we will have to delay the write data and conflict decision as well
				// Actually: Don't fetch them beforehand, makes things easier
				HCL_ASSERT(rp.dedicatedReadLatencyRegisters.empty());

				std::vector<NodePort> consumers = rp.dataOutput.node->getDirectlyDriven(rp.dataOutput.port);

				// Finally the actual mux to arbitrate between the actual read and the forwarded write data.
				auto *muxNode = circuit.createNode<Node_Multiplexer>(2);

				// Then bind the mux
				muxNode->recordStackTrace();
				muxNode->moveToGroup(m_fixupNodeGroup);
				muxNode->setComment("If read and write addr match and read and write are enabled, forward write data to read output.");
				muxNode->connectSelector(conflict);
				muxNode->connectInput(0, rp.dataOutput);
				muxNode->connectInput(1, wrData);

				NodePort muxOut = {.node = muxNode, .port=0ull};

				circuit.appendSignal(muxOut)->setName("conflict_bypass_mux");

				// Rewire all original consumers to the mux output
				for (auto np : consumers)
					np.node->rewireInput(np.port, muxOut);
			}

			// Make everything that was dependent on us depend on the prev port
			while (!rp.node->getDirectlyDriven((size_t)Node_MemPort::Outputs::orderBefore).empty()) {
				auto np = rp.node->getDirectlyDriven((size_t)Node_MemPort::Outputs::orderBefore).back();
				np.node->rewireInput(np.port, {.node = prevPort, .port = (size_t)Node_MemPort::Outputs::orderBefore});
			}

			// Move up the chain
			rp.node->rewireInput((size_t)Node_MemPort::Inputs::orderAfter, prevPort->getDriver((size_t)Node_MemPort::Inputs::orderAfter));
		}
	}
}


void MemoryGroup::resolveWriteOrder(Circuit &circuit)
{
	// If two write ports have an explicit ordering, then the later write always trumps the former if both happen to the same address.
	// Search for such cases and build explicit logic that disables the earlier write.

	// NOT: THIS ASSUMES THAT THERE IS NO MORE ANY WRITE BEFORE READ!!!

	// For each write port:
	for (auto &wp1 : m_writePorts)
		// Scan dependency chain for other write ports
		while (wp1.node->getDriver((size_t)Node_MemPort::Inputs::orderAfter).node != nullptr) {
			auto *prevPort = dynamic_cast<Node_MemPort*>(wp1.node->getDriver((size_t)Node_MemPort::Inputs::orderAfter).node);

			if (prevPort->isReadPort()) {
				HCL_ASSERT_HINT(prevPort->getDriver((size_t)Node_MemPort::Inputs::orderAfter).node == nullptr, "MemoryGroup::resolveWriteOrder assumes that there is no write-before-read anymore!");
				break;
			}

			if (prevPort->isWritePort()) {
				auto *wp2 = prevPort;
				// wp2 is supposed to happen before wp1. Write conflict detection logic and disable wp2 if a conflict happens
		 
				lazyCreateFixupNodeGroup();


				auto *addrCompNode = circuit.createNode<Node_Compare>(Node_Compare::NEQ);
				addrCompNode->recordStackTrace();
				addrCompNode->moveToGroup(m_fixupNodeGroup);
				addrCompNode->setComment("We can enable the former write if the write adresses differ.");
				addrCompNode->connectInput(0, wp1.node->getDriver((size_t)Node_MemPort::Inputs::address));
				addrCompNode->connectInput(1, wp2->getDriver((size_t)Node_MemPort::Inputs::address));

				// Enable write if addresses differ
				NodePort newWrEn2 = {.node = addrCompNode, .port = 0ull};
				circuit.appendSignal(newWrEn2)->setName("newWrEn");

				// Alternatively, enable write if wp1 does not write (no connection on enable means yes)
				HCL_ASSERT(wp1.node->getDriver((size_t)Node_MemPort::Inputs::enable).node == nullptr || wp1.node->getNonSignalDriver((size_t)Node_MemPort::Inputs::enable) == wp1.node->getNonSignalDriver((size_t)Node_MemPort::Inputs::wrEnable));
				if (wp1.node->getDriver((size_t)Node_MemPort::Inputs::wrEnable).node != nullptr) {

					auto *logicNot = circuit.createNode<Node_Logic>(Node_Logic::NOT);
					logicNot->moveToGroup(m_fixupNodeGroup);
					logicNot->recordStackTrace();
					logicNot->connectInput(0, wp1.node->getDriver((size_t)Node_MemPort::Inputs::wrEnable));

					auto *logicOr = circuit.createNode<Node_Logic>(Node_Logic::OR);
					logicOr->moveToGroup(m_fixupNodeGroup);
					logicOr->setComment("We can also enable the former write if the latter write is disabled.");
					logicOr->recordStackTrace();
					logicOr->connectInput(0, newWrEn2);
					logicOr->connectInput(1, {.node = logicNot, .port = 0ull});
					newWrEn2 = {.node = logicOr, .port = 0ull};
					circuit.appendSignal(newWrEn2)->setName("newWrEn");
				}

				// But only enable write if wp2 actually wants to write (no connection on enable means yes)
				HCL_ASSERT(wp2->getDriver((size_t)Node_MemPort::Inputs::enable).node == nullptr || wp2->getNonSignalDriver((size_t)Node_MemPort::Inputs::enable) == wp2->getNonSignalDriver((size_t)Node_MemPort::Inputs::wrEnable));
				if (wp2->getDriver((size_t)Node_MemPort::Inputs::wrEnable).node != nullptr) {
					auto *logicAnd = circuit.createNode<Node_Logic>(Node_Logic::AND);
					logicAnd->moveToGroup(m_fixupNodeGroup);
					logicAnd->setComment("But we can only enable the former write if the former write actually wants to write.");
					logicAnd->recordStackTrace();
					logicAnd->connectInput(0, newWrEn2);
					logicAnd->connectInput(1, wp2->getDriver((size_t)Node_MemPort::Inputs::wrEnable));
					newWrEn2 = {.node = logicAnd, .port = 0ull};
					circuit.appendSignal(newWrEn2)->setName("newWrEn");
				}


				//wp2->rewireInput((size_t)Node_MemPort::Inputs::enable, newWrEn2);
				wp2->rewireInput((size_t)Node_MemPort::Inputs::enable, {});
				wp2->rewireInput((size_t)Node_MemPort::Inputs::wrEnable, newWrEn2);
			}

			// Make everything that was dependent on us depend on the prev port
			while (!wp1.node->getDirectlyDriven((size_t)Node_MemPort::Outputs::orderBefore).empty()) {
				auto np = wp1.node->getDirectlyDriven((size_t)Node_MemPort::Outputs::orderBefore).back();
				np.node->rewireInput(np.port, {.node = prevPort, .port = (size_t)Node_MemPort::Outputs::orderBefore});
			}

			// Move up the chain
			wp1.node->rewireInput((size_t)Node_MemPort::Inputs::orderAfter, prevPort->getDriver((size_t)Node_MemPort::Inputs::orderAfter));
		}

}



void MemoryGroup::ensureNotEnabledFirstCycles(Circuit &circuit, NodeGroup *ng, Node_MemPort *writePort, size_t numCycles)
{
	std::vector<BaseNode*> nodesToMove;
	auto moveNodes = [&] {
		for (auto n : nodesToMove)
			n->moveToGroup(ng);
		nodesToMove.clear();
	};


	// Ensure enable is low in first cycles
	auto enableDriver = writePort->getNonSignalDriver((size_t)Node_MemPort::Inputs::enable);
	auto wrEnableDriver = writePort->getNonSignalDriver((size_t)Node_MemPort::Inputs::wrEnable);
	HCL_ASSERT(enableDriver.node == nullptr || enableDriver == wrEnableDriver);

	NodePort input = {.node = writePort, .port = (size_t)Node_MemPort::Inputs::wrEnable};
	size_t unhandledCycles = numCycles;
	while (unhandledCycles > 0) {
		auto driver = input.node->getDriver(input.port);

		if (driver.node == nullptr) break;

		// something else is driven by the same signal, abort here
		bool onlyUser = true;
		utils::UnstableSet<BaseNode*> alreadyEncountered;
		for (auto nh : driver.node->exploreOutput(driver.port)) {
			if (alreadyEncountered.contains(nh.node())) {
				nh.backtrack();
				continue;
			}
			alreadyEncountered.insert(nh.node());

			if (nh.isSignal()) continue;
			if (nh.node() == writePort && (nh.port() == (size_t)Node_MemPort::Inputs::enable || nh.port() == (size_t)Node_MemPort::Inputs::wrEnable)) {
				nh.backtrack();
				continue;
			}
			if (nh.nodePort() == input) {
				nh.backtrack();
				continue;
			}
			onlyUser = false;
			break;
		}		
		if (!onlyUser)
			break;

		nodesToMove.push_back(driver.node);

		// If signal, continue scanning input chain
		if (auto *signal = dynamic_cast<Node_Signal*>(driver.node)) {
			input = {.node = signal, .port = 0ull};
			continue;
		}

		// Check if already driven by register
		if (auto *enableReg = dynamic_cast<Node_Register*>(driver.node)) {

			// If that register is already resetting to zero everything is fine
			auto resetDriver = enableReg->getNonSignalDriver(Node_Register::RESET_VALUE);
			if (resetDriver.node != nullptr) {
				auto resetValue = evaluateStatically(circuit, resetDriver);
				HCL_ASSERT(resetValue.size() == 1);
				if (resetValue.get(sim::DefaultConfig::DEFINED, 0) && !resetValue.get(sim::DefaultConfig::VALUE, 0)) {
					input = {.node = enableReg, .port = 0ull};
					unhandledCycles--;
					continue;
				}
			}

			sim::DefaultBitVectorState state;
			state.resize(1);
			state.set(sim::DefaultConfig::DEFINED, 0);
			state.set(sim::DefaultConfig::VALUE, 0, false);
			auto *constZero = circuit.createNode<Node_Constant>(state, ConnectionType::BOOL);
			constZero->recordStackTrace();
			constZero->moveToGroup(ng);
			enableReg->connectInput(Node_Register::RESET_VALUE, {.node = constZero, .port = 0ull});

			input = {.node = enableReg, .port = 0ull};
			unhandledCycles--;
			moveNodes();
			continue;
		}

		break;
	}

	// if there are cycles remaining, build counter and AND the enable signal
	if (unhandledCycles > 0) {
		moveNodes();

		NodePort newEnable;

		// no counter necessary, just use a single register
		if (unhandledCycles == 1) {

			// Build single register with reset 0 and input 1 
			sim::DefaultBitVectorState state;
			state.resize(1);
			state.set(sim::DefaultConfig::DEFINED, 0);
			state.set(sim::DefaultConfig::VALUE, 0, false);
			auto *constZero = circuit.createNode<Node_Constant>(state, ConnectionType::BOOL);
			constZero->recordStackTrace();
			constZero->moveToGroup(ng);

			state.set(sim::DefaultConfig::VALUE, 0, true);
			auto *constOne = circuit.createNode<Node_Constant>(state, ConnectionType::BOOL);
			constOne->recordStackTrace();
			constOne->moveToGroup(ng);

			auto *reg = circuit.createNode<Node_Register>();
			reg->recordStackTrace();
			reg->moveToGroup(ng);
			reg->setComment("Register that generates a zero after reset and a one on all later cycles");
			reg->setClock(writePort->getClocks()[0]);

			reg->connectInput(Node_Register::Input::RESET_VALUE, {.node = constZero, .port = 0ull});
			reg->connectInput(Node_Register::Input::DATA, {.node = constOne, .port = 0ull});
			reg->getFlags().insert(Node_Register::Flags::ALLOW_RETIMING_BACKWARD).insert(Node_Register::Flags::ALLOW_RETIMING_FORWARD);

			newEnable = {.node = reg, .port = 0ull};
		} else {
			size_t counterWidth = utils::Log2C(unhandledCycles)+1;

			/*
				Build a counter which starts at unhandledCycles-1 but with one bit more than needed.
				Subtract from it and use the MSB as the indicator that zero was reached, which is the output but also, when negated, the enable to the register.
			*/



			auto *reg = circuit.createNode<Node_Register>();
			reg->moveToGroup(ng);
			reg->recordStackTrace();
			reg->setClock(writePort->getClocks()[0]);
			reg->getFlags().insert(Node_Register::Flags::ALLOW_RETIMING_BACKWARD).insert(Node_Register::Flags::ALLOW_RETIMING_FORWARD);

			sim::DefaultBitVectorState state;
			state.resize(counterWidth);
			state.setRange(sim::DefaultConfig::DEFINED, 0, counterWidth);
			state.insertNonStraddling(sim::DefaultConfig::VALUE, 0, counterWidth, unhandledCycles-1);

			auto *resetConst = circuit.createNode<Node_Constant>(state, ConnectionType::BITVEC);
			resetConst->moveToGroup(ng);
			resetConst->recordStackTrace();
			reg->connectInput(Node_Register::RESET_VALUE, {.node = resetConst, .port = 0ull});


			NodePort counter = {.node = reg, .port = 0ull};
			circuit.appendSignal(counter)->setName("delayedWrEnableCounter");


			// build a one
			state.insertNonStraddling(sim::DefaultConfig::VALUE, 0, counterWidth, 1);
			auto *constOne = circuit.createNode<Node_Constant>(state, ConnectionType::BITVEC);
			constOne->moveToGroup(ng);
			constOne->recordStackTrace();

			auto *subNode = circuit.createNode<Node_Arithmetic>(Node_Arithmetic::SUB);
			subNode->moveToGroup(ng);
			subNode->recordStackTrace();
			subNode->connectInput(0, counter);
			subNode->connectInput(1, {.node = constOne, .port = 0ull});

			reg->connectInput(Node_Register::DATA, {.node = subNode, .port = 0ull});


			auto *rewireNode = circuit.createNode<Node_Rewire>(1);
			rewireNode->moveToGroup(ng);
			rewireNode->recordStackTrace();
			rewireNode->connectInput(0, counter);
			rewireNode->setExtract(counterWidth-1, 1);
			rewireNode->changeOutputType({.type = ConnectionType::BOOL, .width = 1});

			NodePort counterExpired = {.node = rewireNode, .port = 0ull};
			circuit.appendSignal(counterExpired)->setName("delayedWrEnableCounterExpired");

			auto *logicNot = circuit.createNode<Node_Logic>(Node_Logic::NOT);
			logicNot->moveToGroup(ng);
			logicNot->recordStackTrace();
			logicNot->connectInput(0, counterExpired);
			reg->connectInput(Node_Register::ENABLE, {.node = logicNot, .port = 0ull});

			newEnable = counterExpired;
		}

		auto driver = input.node->getDriver(input.port);
		if (driver.node != nullptr) {

			// AND to existing enable input
			auto *logicAnd = circuit.createNode<Node_Logic>(Node_Logic::AND);
			logicAnd->moveToGroup(ng);
			logicAnd->recordStackTrace();
			logicAnd->connectInput(0, newEnable);
			logicAnd->connectInput(1, driver);

			newEnable = {.node = logicAnd, .port = 0ull};
		}

		input.node->rewireInput(input.port, newEnable);

		//writePort->rewireInput((size_t)Node_MemPort::Inputs::wrEnable, writePort->getDriver((size_t)Node_MemPort::Inputs::enable));
		writePort->rewireInput((size_t)Node_MemPort::Inputs::enable, {});
	}
}


void MemoryGroup::findRegisters()
{
	for (auto &rp : m_readPorts)
		HCL_ASSERT(rp.findOutputRegisters(m_memory->getRequiredReadLatency(), m_nodeGroup));
}

void MemoryGroup::attemptRegisterRetiming(Circuit &circuit)
{
	if (m_memory->getRequiredReadLatency() == 0) return;

	dbg::log(dbg::LogMessage(m_memory->getGroup()) << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_POSTPROCESSING << "Attempting register retiming for memory " << m_memory.get());

	//visualize(circuit, "beforeRetiming");

	utils::StableSet<Node_MemPort*> retimeableWritePorts;
	for (auto np : m_memory->getPorts()) {
		auto *memPort = dynamic_cast<Node_MemPort*>(np.node);
		if (memPort->isWritePort()) {
			HCL_ASSERT_HINT(!memPort->isReadPort(), "Retiming for combined read and write ports not yet implemented!");
			retimeableWritePorts.insert(memPort);
		}
	}



	utils::StableMap<Node_MemPort*, size_t> actuallyRetimedWritePorts;

	// If we are aiming for memory with a read latency > 0
	// Check if any read ports are lacking the registers that models that read latency.
	// If they do, scan the read data output bus for any registers buried in the combinatorics that could be pulled back and fused.
	// Keep note of which write ports are "delayed" through this retiming to then, in a second step, build rw hazard bypass logic.

	for (auto &rp : m_readPorts) {
		// Start open minded about the enable condition
		std::optional<Conjunction> enableCondition;
		auto extractEnableCondition = [&]{
			if (!enableCondition && !rp.dedicatedReadLatencyRegisters.empty() && rp.dedicatedReadLatencyRegisters.front() != nullptr) {
				if (rp.dedicatedReadLatencyRegisters.front()->getDriver(Node_Register::ENABLE).node != nullptr)
					enableCondition = Conjunction::fromInput({.node = rp.dedicatedReadLatencyRegisters.front().get(), .port = (size_t)Node_Register::ENABLE});
				else
					enableCondition = {};
			}
		};

		while (!rp.findOutputRegisters(m_memory->getRequiredReadLatency(), m_nodeGroup)) {

			// Once we retimed, make sure further registers will use the same enable condition.
			extractEnableCondition();

			auto subnet = Subnet::all(circuit);
			Subnet retimedArea;
			// On multi-readport memories there can already appear a register due to the retiming of other read ports. In this case, retimeBackwardtoOutput is a no-op.
			retimeBackwardtoOutput(circuit, subnet, retimeableWritePorts, enableCondition, retimedArea, rp.dataOutput, true, true);

			//visualize(circuit, "afterRetiming");

			for (auto wp : retimeableWritePorts) {
				if (retimedArea.contains(wp)) {
					// Take note that this write port is delayed by one more cycle
					actuallyRetimedWritePorts[wp]++;
				}
			}
		}
		extractEnableCondition();

		// Store the enable condition in the read port so it can be used more easily.
		if (enableCondition) {
			rp.node->rewireInput((size_t)Node_MemPort::Inputs::enable, enableCondition->build(*lazyCreateFixupNodeGroup()));
		}
	}

	//visualize(circuit, "afterRetiming");

	if (actuallyRetimedWritePorts.empty()) return;

	lazyCreateFixupNodeGroup();

	// For all WPs that got retimed:
	std::vector<std::pair<Node_MemPort*, size_t>> sortedWritePorts;
	for (auto wp : actuallyRetimedWritePorts) {
		// ... prep a list to sort them in write order
		sortedWritePorts.push_back(wp);
		// ... Ensure their (write-)enable is deasserted for at least as long as they were delayed.
		ensureNotEnabledFirstCycles(circuit, m_fixupNodeGroup, wp.first, wp.second); 
	}

	//visualize(circuit, "afterEnableFix");	

	std::sort(sortedWritePorts.begin(), sortedWritePorts.end(), [](const auto &left, const auto &right)->bool {
		return left.first->isOrderedBefore(right.first);
	});

	if (sortedWritePorts.size() >= 2)
		HCL_ASSERT(sortedWritePorts[0].first->isOrderedBefore(sortedWritePorts[1].first));

	auto *clock = sortedWritePorts.front().first->getClocks()[0];
	ReadModifyWriteHazardLogicBuilder rmwBuilder(circuit, clock, m_fixupNodeGroup);
	
	size_t maxLatency = 0;
	
	for (auto &rp : m_readPorts)
		rmwBuilder.addReadPort(ReadModifyWriteHazardLogicBuilder::ReadPort{
			.addrInputDriver = rp.node->getDriver((size_t)Node_MemPort::Inputs::address),
			.enableInputDriver = rp.node->getDriver((size_t)Node_MemPort::Inputs::enable),
			.dataOutOutputDriver = (NodePort) rp.dataOutput,
		});

	for (auto wp : sortedWritePorts) {
		HCL_ASSERT(wp.first->getDriver((size_t)Node_MemPort::Inputs::enable).node == nullptr || wp.first->getDriver((size_t)Node_MemPort::Inputs::enable) == wp.first->getDriver((size_t)Node_MemPort::Inputs::wrEnable));
		rmwBuilder.addWritePort(ReadModifyWriteHazardLogicBuilder::WritePort{
			.addrInputDriver = wp.first->getDriver((size_t)Node_MemPort::Inputs::address),
			.enableInputDriver = wp.first->getDriver((size_t)Node_MemPort::Inputs::wrEnable),
			.enableMaskInputDriver = {},
			.dataInInputDriver = wp.first->getDriver((size_t)Node_MemPort::Inputs::wrData),
			.latencyCompensation = wp.second
		});

		maxLatency = std::max(maxLatency, wp.second);
	}

	bool useMemory = maxLatency > 2;
	rmwBuilder.retimeRegisterToMux();
	rmwBuilder.build(useMemory);


	// The rmw builder also builds logic for read during write collision, so we can set read ports to be independent of write ports
	for (auto &rp : m_readPorts) {
		rp.node->rewireInput((size_t)Node_MemPort::Inputs::orderAfter, {});
		while (!rp.node->getDirectlyDriven((size_t)Node_MemPort::Outputs::orderBefore).empty()) {
			auto driven = rp.node->getDirectlyDriven((size_t)Node_MemPort::Outputs::orderBefore).back();
			driven.node->rewireInput(driven.port, {});
		}
	}

	//visualize(circuit, "afterRMW");
/*
	{
		auto all = ConstSubnet::all(circuit);
		DotExport exp("afterRMW.dot");
		exp(circuit, all);
		exp.runGraphViz("afterRMW.svg");			
	}		
*/	
}

void MemoryGroup::updateNoConflictsAttrib()
{
	bool conflicts = false;
	for (auto &rp : m_readPorts)	
		if (rp.node->getDriver((size_t)Node_MemPort::Inputs::orderAfter).node != nullptr) {
			conflicts = true;
			break;
		}

	if (!conflicts) {
		for (auto &wp : m_writePorts)
			if (wp.node->getDriver((size_t)Node_MemPort::Inputs::orderAfter).node != nullptr) {
				conflicts = true;
				break;
			}
	}

	m_memory->getAttribs().noConflicts = !conflicts;
}

void MemoryGroup::buildReset(Circuit &circuit)
{
	if (m_readPorts.empty()) return;
	if (m_memory->getNonSignalDriver((size_t)Node_Memory::Inputs::INITIALIZATION_DATA).node != nullptr) {
		buildResetLogic(circuit);
		// Disconnect initialization network's output from memory node
		m_memory->rewireInput((size_t)Node_Memory::Inputs::INITIALIZATION_DATA, {});
	} else {
		if (sim::anyDefined(m_memory->getPowerOnState()) && !m_memory->isROM())
			buildResetRom(circuit);
	}
}

void MemoryGroup::buildResetLogic(Circuit &circuit)
{
	lazyCreateFixupNodeGroup();

	auto *resetWritePort = findSuitableResetWritePort();
	HCL_ASSERT_HINT(resetWritePort != nullptr, "No suitable write port was found to reset initialize memory " + m_memory->getName());
	auto *clockDomain = resetWritePort->getClocks()[0];

	if (clockDomain->getRegAttribs().memoryResetType == RegisterAttributes::ResetType::NONE) return;
	HCL_ASSERT(clockDomain->getRegAttribs().resetType != RegisterAttributes::ResetType::NONE);

	dbg::log(dbg::LogMessage(m_memory->getGroup()) << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_POSTPROCESSING << "Building reset logic for memory " << m_memory);

	Clock *resetClock = buildResetClock(circuit, clockDomain);

	// Move entire initialization network into the helper group
	for (auto nh : m_memory->exploreInput((size_t)Node_Memory::Inputs::INITIALIZATION_DATA)) {
		if (nh.node() == m_memory) {
			nh.backtrack();
		} else {
			nh.node()->moveToGroup(m_fixupNodeGroup);		
		}
	}

	NodePort initData = m_memory->getDriver((size_t)Node_Memory::Inputs::INITIALIZATION_DATA);

	// Compute required writes
	size_t wordWidth = resetWritePort->getBitWidth();
	HCL_ASSERT(m_memory->getPowerOnState().size() % wordWidth == 0);
	HCL_ASSERT(wordWidth == getOutputWidth(initData));
	size_t numEntries = m_memory->getPowerOnState().size() / wordWidth;
	size_t numRequiredCycles = numEntries;
	if (clockDomain->getRegAttribs().resetType == RegisterAttributes::ResetType::ASYNCHRONOUS)
		numRequiredCycles += 1;
	resetClock->setMinResetCycles(numRequiredCycles);

	// Build counter for writes
	size_t addrCounterSize = utils::Log2C(numEntries);
	NodePort addrCounter = buildResetAddrCounter(circuit, addrCounterSize, resetClock);

	// Rewire initializaiton network's input to the counter
	while (!m_memory->getDirectlyDriven((size_t)Node_Memory::Outputs::INITIALIZATION_ADDR).empty()) {
		auto np = m_memory->getDirectlyDriven((size_t)Node_Memory::Outputs::INITIALIZATION_ADDR).back();
		np.node->rewireInput(np.port, addrCounter);
	}

	// Build overrides
	buildResetOverrides(circuit, addrCounter, initData, resetWritePort);
}

void MemoryGroup::buildResetRom(Circuit &circuit)
{   
	lazyCreateFixupNodeGroup();

	auto *resetWritePort = findSuitableResetWritePort();
	HCL_ASSERT_HINT(resetWritePort != nullptr, "No suitable write port was found to reset initialize memory " + m_memory->getName());
	auto *clockDomain = resetWritePort->getClocks()[0];

	if (clockDomain->getRegAttribs().memoryResetType == RegisterAttributes::ResetType::NONE) return;
	HCL_ASSERT(clockDomain->getRegAttribs().resetType != RegisterAttributes::ResetType::NONE);

	dbg::log(dbg::LogMessage(m_memory->getGroup()) << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_POSTPROCESSING << "Building reset rom for memory " << m_memory);

	Clock *resetClock = buildResetClock(circuit, clockDomain);

	auto *memory = circuit.createNode<Node_Memory>();
	memory->recordStackTrace();
	memory->moveToGroup(m_fixupNodeGroup);
	memory->setType(Node_Memory::MemType::DONT_CARE, 1); 
	memory->setNoConflicts();
	memory->setPowerOnState(m_memory->getPowerOnState());
	if (m_memory->getName().empty())
		memory->setName("reset_value_rom");
	else
		memory->setName(m_memory->getName()+"_reset_value_rom");

	size_t wordWidth = resetWritePort->getBitWidth();
	HCL_ASSERT(m_memory->getPowerOnState().size() % wordWidth == 0);
	size_t numEntries = m_memory->getPowerOnState().size() / wordWidth;

	size_t numRequiredCycles = numEntries + 1;
	if (clockDomain->getRegAttribs().resetType == RegisterAttributes::ResetType::ASYNCHRONOUS)
		numRequiredCycles += 1;
	resetClock->setMinResetCycles(numRequiredCycles);

	size_t addrCounterSize = utils::Log2C(numEntries);

	NodePort addrCounter = buildResetAddrCounter(circuit, addrCounterSize, resetClock);


	auto *romReadPort = circuit.createNode<hlim::Node_MemPort>(wordWidth);
	romReadPort->moveToGroup(m_fixupNodeGroup);
	romReadPort->recordStackTrace();
	romReadPort->connectMemory(memory);
	romReadPort->connectAddress(addrCounter);
	romReadPort->setClock(clockDomain);


	auto *addrReg = circuit.createNode<Node_Register>();
	addrReg->moveToGroup(m_fixupNodeGroup);
	addrReg->recordStackTrace();
	addrReg->setClock(resetClock);
	addrReg->getFlags().insert(Node_Register::Flags::ALLOW_RETIMING_BACKWARD).insert(Node_Register::Flags::ALLOW_RETIMING_FORWARD);
	addrReg->connectInput(Node_Register::DATA, addrCounter);
	NodePort writeAddr = {.node = addrReg, .port = 0ull};
	giveName(circuit, writeAddr, "reset_write_addr");


	auto *dataReg = circuit.createNode<Node_Register>();
	dataReg->moveToGroup(m_fixupNodeGroup);
	dataReg->recordStackTrace();
	dataReg->setClock(resetClock);
	dataReg->getFlags().insert(Node_Register::Flags::ALLOW_RETIMING_BACKWARD).insert(Node_Register::Flags::ALLOW_RETIMING_FORWARD);
	dataReg->connectInput(Node_Register::DATA, {.node = romReadPort, .port = (size_t)hlim::Node_MemPort::Outputs::rdData});
	NodePort writeData = {.node = dataReg, .port = 0ull};
	giveName(circuit, writeData, "reset_write_data");

	buildResetOverrides(circuit, writeAddr, writeData, resetWritePort);

	formMemoryGroupIfNecessary(circuit, memory);
}

void MemoryGroup::buildResetOverrides(Circuit &circuit, NodePort writeAddr, NodePort writeData, Node_MemPort *resetWritePort)
{
	auto *clockDomain = resetWritePort->getClocks()[0];

	auto *resetPin = circuit.createNode<Node_ClkRst2Signal>();
	resetPin->moveToGroup(m_fixupNodeGroup);
	resetPin->recordStackTrace();
	resetPin->setClock(clockDomain);
	NodePort inResetMode = {.node = resetPin, .port = 0ull};

	if (!(clockDomain->getRegAttribs().resetActive == hlim::RegisterAttributes::Active::HIGH)) {
		auto *notNode = circuit.createNode<Node_Logic>(Node_Logic::NOT);
		notNode->moveToGroup(m_fixupNodeGroup);
		notNode->recordStackTrace();
		notNode->setComment("The clock domain uses a low-active reset so we need to negate it.");
		notNode->connectInput(0, inResetMode);
		inResetMode = {.node = notNode, .port = 0ull};
	}

	auto *muxNodeAddr = circuit.createNode<Node_Multiplexer>(2);
	muxNodeAddr->moveToGroup(m_fixupNodeGroup);
	muxNodeAddr->recordStackTrace();
	muxNodeAddr->setComment("For reset, mux address between actual address (non-reset case) and initializaiton counter (reset case).");
	muxNodeAddr->connectSelector(inResetMode);
	muxNodeAddr->connectInput(0, resetWritePort->getDriver((size_t)Node_MemPort::Inputs::address));
	muxNodeAddr->connectInput(1, writeAddr);
	resetWritePort->rewireInput((size_t)Node_MemPort::Inputs::address, {.node = muxNodeAddr, .port = 0ull});


	auto *muxNodeData = circuit.createNode<Node_Multiplexer>(2);
	muxNodeData->moveToGroup(m_fixupNodeGroup);
	muxNodeData->recordStackTrace();
	muxNodeData->setComment("For reset, mux data between actual write data (non-reset case) and the initialization data (reset case).");
	muxNodeData->connectSelector(inResetMode);
	muxNodeData->connectInput(0, resetWritePort->getDriver((size_t)Node_MemPort::Inputs::wrData));
	muxNodeData->connectInput(1, writeData);
	resetWritePort->rewireInput((size_t)Node_MemPort::Inputs::wrData, {.node = muxNodeData, .port = 0ull});

	HCL_ASSERT(resetWritePort->getDriver((size_t)Node_MemPort::Inputs::enable).node == nullptr || resetWritePort->getDriver((size_t)Node_MemPort::Inputs::enable) == resetWritePort->getDriver((size_t)Node_MemPort::Inputs::wrEnable));
	if (resetWritePort->getDriver((size_t)Node_MemPort::Inputs::wrEnable).node == nullptr) {
		// This might seem unintuitive, but leaving it unconnected in this case is the correct thing.
		// If it was unconnected before, it was always writing. Now, we want to always write during reset 
		// and always write outside of reset, so we can leave it unconnected.
	} else {
		auto *orNodeEnable = circuit.createNode<Node_Logic>(Node_Logic::OR);
		orNodeEnable->moveToGroup(m_fixupNodeGroup);
		orNodeEnable->recordStackTrace();
		orNodeEnable->setComment("During reset, enable write to initialize the memory.");
		orNodeEnable->connectInput(0, resetWritePort->getDriver((size_t)Node_MemPort::Inputs::wrEnable));
		orNodeEnable->connectInput(1, inResetMode);

		//resetWritePort->rewireInput((size_t)Node_MemPort::Inputs::enable, {.node = orNodeEnable, .port = 0ull});
		resetWritePort->rewireInput((size_t)Node_MemPort::Inputs::enable, {});
		resetWritePort->rewireInput((size_t)Node_MemPort::Inputs::wrEnable, {.node = orNodeEnable, .port = 0ull});
	}
}

Clock *MemoryGroup::buildResetClock(Circuit &circuit, Clock *clockDomain)
{
	Clock *resetClock = circuit.createClock<DerivedClock>(clockDomain);
	resetClock->getRegAttribs().resetActive = !clockDomain->getRegAttribs().resetActive;
	resetClock->getRegAttribs().initializeRegs = true;

	return resetClock;
}

Node_MemPort *MemoryGroup::findSuitableResetWritePort()
{
	if (m_writePorts.empty()) return nullptr;
	for (auto &wp : m_writePorts)
		if (wp.node->getBitWidth() == m_memory->getInitializationDataWidth() || m_memory->getInitializationDataWidth() == 0)
			return wp.node.get();

	HCL_ASSERT_HINT(false, "No write port matches the size of the initialization width!");
	return nullptr;
}

NodePort MemoryGroup::buildResetAddrCounter(Circuit &circuit, size_t width, Clock *resetClock)
{
	sim::DefaultBitVectorState state;
	state.resize(width);
	state.setRange(sim::DefaultConfig::DEFINED, 0, width);
	state.clearRange(sim::DefaultConfig::VALUE, 0, width);

	auto *resetConst = circuit.createNode<Node_Constant>(state, ConnectionType::BITVEC);
	resetConst->moveToGroup(m_fixupNodeGroup);
	resetConst->recordStackTrace();

	if (width == 0)
		return { .node = resetConst, .port = 0ull };

	auto *reg = circuit.createNode<Node_Register>();
	reg->moveToGroup(m_fixupNodeGroup);
	reg->recordStackTrace();
	reg->setClock(resetClock);
	reg->getFlags().insert(Node_Register::Flags::ALLOW_RETIMING_BACKWARD).insert(Node_Register::Flags::ALLOW_RETIMING_FORWARD);
	reg->connectInput(Node_Register::RESET_VALUE, {.node = resetConst, .port = 0ull});


	// build a one
	state.setRange(sim::DefaultConfig::VALUE, 0, 1);
	auto *constOne = circuit.createNode<Node_Constant>(state, ConnectionType::BITVEC);
	constOne->moveToGroup(m_fixupNodeGroup);
	constOne->recordStackTrace();


	auto *addNode = circuit.createNode<Node_Arithmetic>(Node_Arithmetic::ADD);
	addNode->moveToGroup(m_fixupNodeGroup);
	addNode->recordStackTrace();
	addNode->connectInput(1, {.node = constOne, .port = 0ull});

	reg->connectInput(Node_Register::DATA, {.node = addNode, .port = 0ull});

	NodePort counter = {.node = reg, .port = 0ull};
	giveName(circuit, counter, "reset_addr_counter");

	addNode->connectInput(0, counter);

	return counter;
}


void MemoryGroup::verify()
{
	switch (m_memory->type()) {
		case Node_Memory::MemType::MEDIUM:
			for (auto &rp : m_readPorts) {
				if (rp.dedicatedReadLatencyRegisters.size() < 1) {
					std::stringstream issue;
					issue << "Memory can not become BRAM because a read port is missing it's data register.\nMemory from:\n"
						<< m_memory->getStackTrace() << "\nRead port from:\n" << rp.node->getStackTrace();
					HCL_DESIGNCHECK_HINT(false, issue.str());
				}
			}
			/*
			if (m_readPorts.size() + m_writePorts.size() > 2) {
				std::stringstream issue;
				issue << "Memory can not become BRAM because it has too many memory ports.\nMemory from:\n"
					  << m_memory->getStackTrace();
				HCL_DESIGNCHECK_HINT(false, issue.str());
			}
			*/
		break;
		case Node_Memory::MemType::SMALL:
			if (m_readPorts.size() > 1) {
				std::stringstream issue;
				issue << "Memory can not become LUTRAM because it has too many read ports.\nMemory from:\n"
					  << m_memory->getStackTrace();
				HCL_DESIGNCHECK_HINT(false, issue.str());
			}
			if (m_writePorts.size() > 1) {
				std::stringstream issue;
				issue << "Memory can not become LUTRAM because it has too many write ports.\nMemory from:\n"
					  << m_memory->getStackTrace();
				HCL_DESIGNCHECK_HINT(false, issue.str());
			}
		break;
		default:
		break;
	}
}

void MemoryGroup::replaceWithIOPins(Circuit &circuit)
{
	HCL_ASSERT_HINT(!m_memory->requiresPowerOnInitialization(), "No power on state for external memory possible!");

	auto &memGroupProps = m_memory->getGroup()->properties();

	lazyCreateFixupNodeGroup();
	std::string prefix;
	if(!m_memory->getName().empty())
		prefix = m_memory->getName() + '_';

	MemorySimConfig memSimConfig; 
	memSimConfig.size = m_memory->getSize();
	memSimConfig.readPorts.reserve(m_readPorts.size());
	memSimConfig.writePorts.reserve(m_writePorts.size());

	size_t portIdx = 0;

	memGroupProps["numPorts"] = m_readPorts.size() + m_writePorts.size();

	for (auto &rp : m_readPorts) {
		Clock *clock = nullptr;
		for (auto &r : rp.dedicatedReadLatencyRegisters) {
			HCL_ASSERT(r->getNonSignalDriver(Node_Register::ENABLE).node == nullptr);
			HCL_ASSERT(r->getNonSignalDriver(Node_Register::RESET_VALUE).node == nullptr);
			if (clock == nullptr)
				clock = r->getClocks()[0];
			else
				HCL_ASSERT_HINT(clock == r->getClocks()[0], "All read latency registers must have the same clock!");
		}

		auto *pinRdAddr = circuit.createNode<Node_Pin>(false, true, false);
		pinRdAddr->setClockDomain(clock);
		pinRdAddr->setName(prefix +"rd_address");
		pinRdAddr->moveToGroup(m_nodeGroup->getParent());
		pinRdAddr->recordStackTrace();
		pinRdAddr->connect(rp.node->getDriver((size_t)Node_MemPort::Inputs::address));

		memGroupProps[(boost::format("port_%d_pinName_addr") % portIdx).str()] = pinRdAddr->getName();
		memGroupProps[(boost::format("port_%d_width_addr") % portIdx).str()] = getOutputWidth(pinRdAddr->getDriver(0));

		Node_Pin *pinRdEn = nullptr;
		if (rp.node->getDriver((size_t)Node_MemPort::Inputs::enable).node != nullptr) {
			pinRdEn = circuit.createNode<Node_Pin>(false, true, false);
			pinRdEn->setClockDomain(clock);
			pinRdEn->setName(prefix +"rd_read");
			pinRdEn->moveToGroup(m_nodeGroup->getParent());
			pinRdEn->recordStackTrace();
			pinRdEn->connect(rp.node->getDriver((size_t)Node_MemPort::Inputs::enable));

			memGroupProps[(boost::format("port_%d_has_readEnable") % portIdx).str()] = true;
			memGroupProps[(boost::format("port_%d_pinName_readEnable") % portIdx).str()] = pinRdEn->getName();
		} else
			memGroupProps[(boost::format("port_%d_has_readEnable") % portIdx).str()] = false;

		auto *pinRdData = circuit.createNode<Node_Pin>(true, false, false);
		pinRdData->setClockDomain(clock);
		pinRdData->setName(prefix +"rd_readdata");
		pinRdData->moveToGroup(m_nodeGroup->getParent());
		pinRdData->recordStackTrace();
		if (getOutputConnectionType(rp.dataOutput).isBool())
			pinRdData->setBool();
		else
			pinRdData->setWidth(getOutputWidth(rp.dataOutput));

		memGroupProps[(boost::format("port_%d_pinName_readData") % portIdx).str()] = pinRdData->getName();
		memGroupProps[(boost::format("port_%d_width_readData") % portIdx).str()] = getOutputWidth({.node = pinRdData, .port = 0ull});
		
		while (!rp.dataOutput.node->getDirectlyDriven(rp.dataOutput.port).empty()) {
			auto input = rp.dataOutput.node->getDirectlyDriven(rp.dataOutput.port).front();
			input.node->rewireInput(input.port, {.node = pinRdData, .port = 0ull});
		}

		memSimConfig.readPorts.push_back({
			.clk = clock,
			.addr = sim::SigHandle(pinRdAddr->getDriver(0)),
			.data = sim::SigHandle({.node=pinRdData, .port = 0ull}),
			.width = getOutputWidth({.node=pinRdData, .port = 0ull}),
		});

		if (rp.dedicatedReadLatencyRegisters.size() > 0) {
			memSimConfig.readPorts.back().inputLatency = 1;
			memSimConfig.readPorts.back().outputLatency = rp.dedicatedReadLatencyRegisters.size()-1;
		} else {
			memSimConfig.readPorts.back().inputLatency = 0;
			memSimConfig.readPorts.back().outputLatency = 0;
		}

		if (pinRdEn) memSimConfig.readPorts.back().en = sim::SigHandle(pinRdEn->getDriver(0));

		portIdx++;
	}

	for (auto &wp : m_writePorts) {
		auto clock = wp.node->getClocks()[0];

		auto *pinWrAddr = circuit.createNode<Node_Pin>(false, true, false);
		pinWrAddr->setClockDomain(clock);
		pinWrAddr->setName(prefix +"wr_address");
		pinWrAddr->moveToGroup(m_nodeGroup->getParent());
		pinWrAddr->recordStackTrace();
		pinWrAddr->connect(wp.node->getDriver((size_t)Node_MemPort::Inputs::address));

		memGroupProps[(boost::format("port_%d_pinName_addr") % portIdx).str()] = pinWrAddr->getName();
		memGroupProps[(boost::format("port_%d_width_addr") % portIdx).str()] = getOutputWidth(pinWrAddr->getDriver(0));

		auto *pinWrData = circuit.createNode<Node_Pin>(false, true, false);
		pinWrData->setClockDomain(clock);
		pinWrData->setName(prefix +"wr_writedata");
		pinWrData->moveToGroup(m_nodeGroup->getParent());
		pinWrData->recordStackTrace();
		pinWrData->connect(wp.node->getDriver((size_t)Node_MemPort::Inputs::wrData));

		memGroupProps[(boost::format("port_%d_pinName_writeData") % portIdx).str()] = pinWrData->getName();
		memGroupProps[(boost::format("port_%d_width_writeData") % portIdx).str()] = getOutputWidth(pinWrData->getDriver(0));

		Node_Pin *pinWrEn = nullptr;
		if (wp.node->getDriver((size_t)Node_MemPort::Inputs::wrEnable).node != nullptr) {
			pinWrEn = circuit.createNode<Node_Pin>(false, true, false);
			pinWrEn->setClockDomain(clock);
			pinWrEn->setName(prefix +"wr_write");
			pinWrEn->moveToGroup(m_nodeGroup->getParent());
			pinWrEn->recordStackTrace();
			pinWrEn->connect(wp.node->getDriver((size_t)Node_MemPort::Inputs::wrEnable));

			memGroupProps[(boost::format("port_%d_has_writeEnable") % portIdx).str()] = true;
			memGroupProps[(boost::format("port_%d_pinName_writeEnable") % portIdx).str()] = pinWrEn->getName();
		} else 
			memGroupProps[(boost::format("port_%d_has_writeEnable") % portIdx).str()] = false;

		memSimConfig.writePorts.push_back({
			.clk = clock,			
			.addr = sim::SigHandle(pinWrAddr->getDriver(0)),
			.data = sim::SigHandle(pinWrData->getDriver(0)),
			.width = getOutputWidth(pinWrData->getDriver(0)),
			.inputLatency = 1,
		});
		if (pinWrEn) memSimConfig.writePorts.back().en = sim::SigHandle(pinWrEn->getDriver(0));

		portIdx++;
	}

	for (auto rdPortIdx : utils::Range(m_readPorts.size())) {
		bool anyReadFirst = false;
		bool anyWriteFirst = false;
		for (auto wrPortIdx : utils::Range(m_writePorts.size())) {
			if (m_readPorts[rdPortIdx].node->isOrderedBefore(m_writePorts[wrPortIdx].node))
				anyReadFirst = true;
			else if (m_writePorts[wrPortIdx].node->isOrderedBefore(m_readPorts[rdPortIdx].node))
				anyWriteFirst = true;
		}

		HCL_ASSERT_HINT(!(anyReadFirst && anyWriteFirst), "The external memory simulator can not handle read ports being read-first wrt. some write ports and write-first wrt. others!");
		auto rdwName = (boost::format("port_%d_crossPortReadDuringWrite") % rdPortIdx).str();

		if (anyReadFirst) {
			memSimConfig.readPorts[rdPortIdx].rdw = MemorySimConfig::RdPrtNodePorts::READ_BEFORE_WRITE;
			memGroupProps[rdwName] = "READ_FIRST";
		} else if (anyWriteFirst) {
			memSimConfig.readPorts[rdPortIdx].rdw = MemorySimConfig::RdPrtNodePorts::READ_AFTER_WRITE;
			memGroupProps[rdwName] = "WRITE_FIRST";
		} else {
			memSimConfig.readPorts[rdPortIdx].rdw = MemorySimConfig::RdPrtNodePorts::READ_UNDEFINED;
			memGroupProps[rdwName] = "DONT_CARE";
		}
	}

	addExternalMemorySimulator(circuit, std::move(memSimConfig));

	m_readPorts.clear();
	m_writePorts.clear();
	m_memory = nullptr;
}

void MemoryGroup::bypassSignalNodes()
{
	for (auto n : m_nodeGroup->getNodes())
		if (dynamic_cast<Node_Signal*>(n))
			n->bypassOutputToInput(0, 0);
}

void MemoryGroup::giveName(Circuit &circuit, NodePort &nodePort, std::string name)
{
	lazyCreateFixupNodeGroup();
	auto *sig = circuit.appendSignal(nodePort);
	sig->setName(std::move(name));
	sig->moveToGroup(m_fixupNodeGroup);
}

void MemoryGroup::emulateResetOfOutputRegisters(Circuit &circuit)
{
	for (auto &rp : m_readPorts)
		if (rp.dedicatedReadLatencyRegisters.size() == 1)
			emulateResetOfFirstReadPortOutputRegister(circuit, rp);
}

void MemoryGroup::emulateResetOfFirstReadPortOutputRegister(Circuit &circuit, MemoryGroup::ReadPort &rp)
{
	auto *reg = rp.dedicatedReadLatencyRegisters.front().get();
	if (reg->getNonSignalDriver(Node_Register::RESET_VALUE).node == nullptr) 
		return;

	dbg::log(dbg::LogMessage(m_nodeGroup) << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING << "Emulating reset logic for output register " << reg);

	lazyCreateFixupNodeGroup();

	auto *clockDomain = reg->getClocks()[0];
	
	auto *resetPin = circuit.createNode<Node_ClkRst2Signal>();
	resetPin->moveToGroup(m_fixupNodeGroup);
	resetPin->recordStackTrace();
	resetPin->setClock(clockDomain);

	bool resetHighActive = clockDomain->getRegAttribs().resetActive == RegisterAttributes::Active::HIGH;

	sim::DefaultBitVectorState state;
	state.resize(1);
	state.setRange(sim::DefaultConfig::DEFINED, 1, 1);
	state.clearRange(sim::DefaultConfig::VALUE, resetHighActive?1:0, 1);

	auto *resetConst = circuit.createNode<Node_Constant>(state, ConnectionType::BOOL);
	resetConst->moveToGroup(m_fixupNodeGroup);
	resetConst->recordStackTrace();


	auto *regReset = circuit.createNode<Node_Register>();
	regReset->recordStackTrace();
	regReset->setClock(clockDomain);
	regReset->connectInput(Node_Register::DATA, {.node = resetPin, .port = 0 });
	regReset->connectInput(Node_Register::RESET_VALUE, {.node = resetConst, .port = 0 });
	regReset->connectInput(Node_Register::ENABLE, reg->getDriver(Node_Register::ENABLE));
	regReset->moveToGroup(m_fixupNodeGroup);
	regReset->setComment("This register was created to create a delayed reset for use in emulating the reset of a memory output register.");


	auto driven = reg->getDirectlyDriven(0);

	auto *muxNode = circuit.createNode<Node_Multiplexer>(2);
	muxNode->recordStackTrace();
	muxNode->moveToGroup(m_fixupNodeGroup);
	muxNode->connectSelector({ .node = regReset, .port = 0 });
	if (resetHighActive) {
		muxNode->connectInput(0, { .node = reg, .port =  0ull });
		muxNode->connectInput(1, reg->getDriver(Node_Register::RESET_VALUE));
	} else {
		muxNode->connectInput(0, reg->getDriver(Node_Register::RESET_VALUE));
		muxNode->connectInput(1, { .node = reg, .port =  0ull });
	}
	muxNode->setComment("Emulate the reset of a memory output register.");

	for (auto d : driven)
		d.node->rewireInput(d.port, { .node = muxNode, .port = 0 });

	reg->disconnectInput(Node_Register::RESET_VALUE);
}



Memory2VHDLPattern::Memory2VHDLPattern()
{
	m_priority = EXPORT_LANGUAGE_MAPPING + 100;
}

bool Memory2VHDLPattern::attemptApply(Circuit &circuit, hlim::NodeGroup *nodeGroup) const
{
	auto* memoryGroup = dynamic_cast<MemoryGroup*>(nodeGroup->getMetaInfo());

	if (!memoryGroup)
		return false;

	dbg::log(dbg::LogMessage(nodeGroup) << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING << "Preparing memory in " << nodeGroup << " for vhdl export");

	memoryGroup->convertToReadBeforeWrite(circuit);
	memoryGroup->attemptRegisterRetiming(circuit);
	memoryGroup->resolveWriteOrder(circuit);
	memoryGroup->updateNoConflictsAttrib();
	memoryGroup->buildReset(circuit);
	memoryGroup->emulateResetOfOutputRegisters(circuit);
	memoryGroup->bypassSignalNodes();
	memoryGroup->verify();
	if (memoryGroup->getMemory()->type() == Node_Memory::MemType::EXTERNAL) {
		nodeGroup->getParent()->properties()["primitive"] = "io-pins";
		memoryGroup->replaceWithIOPins(circuit);
	} else {
		nodeGroup->getParent()->properties()["primitive"] = "vhdl";
	}

	return true;
}

}
