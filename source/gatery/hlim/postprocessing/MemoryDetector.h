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
#pragma once

#include "../NodePtr.h"
#include "../NodePort.h"
#include "../NodeGroup.h"
#include "../supportNodes/MemoryTypes.h"

#include "TechnologyMapping.h"

#include <ranges>

namespace gtry::hlim {

class Circuit;
class Clock;
class Node_Memory;
class Node_MemPort;
class Node_Register;

class MemoryGroup : public NodeGroupMetaInfo
{
	public:
		struct MemPort {
			NodePtr<Node_MemPort> node;
			std::vector<NodePtr<Node_Register>> dedicatedReadLatencyRegisters;
			RefCtdNodePort dataOutput;

			bool findOutputRegisters(size_t readLatency, NodeGroup *memoryNodeGroup);

			bool isReadPort() const;
			bool isWritePort() const;
			memory::ReadDuringWrite samePortRDWBehavior() const;
			BitWidth addressWidth() const;
		};

		MemoryGroup(NodeGroup *group);
		
		void pullInPorts(Node_Memory *memory);

		void convertReadFirstToDelayedWriteFirst(Circuit &circuit);
		void resolveReadAfterWrite(Circuit &circuit);
		void resolveWriteOrder(Circuit &circuit);
		void findRegisters();
		void attemptRegisterRetiming(Circuit &circuit);
		void updateNoConflictsAttrib();
		void buildReset(Circuit &circuit);
		void verify();
		void replaceWithIOPins(Circuit &circuit);
		void bypassSignalNodes();
		NodeGroup *lazyCreateFixupNodeGroup();

		Node_Memory *getMemory() { return m_memory; }
		auto getWritePorts() const { return m_ports | std::views::filter([](const MemPort &port) { return port.isWritePort(); }); }
		auto getWritePorts() { return m_ports | std::views::filter([](const MemPort &port) { return port.isWritePort(); }); }
		auto getReadPorts() const { return m_ports | std::views::filter([](const MemPort &port) { return port.isReadPort(); }); }
		auto getReadPorts() { return m_ports | std::views::filter([](const MemPort &port) { return port.isReadPort(); }); }
		auto getPorts() const { return m_ports; }
		auto getPorts() { return m_ports; }

		inline size_t getNumReadPorts() const { return std::ranges::count_if(m_ports, [](const MemPort &port) { return port.isReadPort(); }); }
		inline size_t getNumWritePorts() const { return std::ranges::count_if(m_ports, [](const MemPort &port) { return port.isWritePort(); }); }
		inline bool isROM() const { return getWritePorts().empty(); }

		bool canReadBeforeAllOthersWrite(const MemPort &rdPort) const;
		bool canReadAfterAllOthersWrite(const MemPort &rdPort) const;
		bool readUnorderedToAllOthersWrite(const MemPort &rdPort) const;

		struct ReadWriteOrdering {
			bool anyMustReadFirst;
			bool allCanReadFirst;
		};
		ReadWriteOrdering getReadWriteOrdering() const;

		const MemPort &findPort(Node_MemPort *memPort);

		inline NodeGroup *getNodeGroup() const { return m_nodeGroup; }
		inline NodeGroup *getFixupNodeGroup() const { return m_fixupNodeGroup; }

		void emulateResetOfOutputRegisters(Circuit &circuit);
		void emulateResetOfFirstReadPortOutputRegister(Circuit &circuit, MemPort &rp);

		//void fuseReadAndWritePorts(Circuit &circuit);
	protected:
		NodePtr<Node_Memory> m_memory;
		std::vector<MemPort> m_ports;

		NodeGroup *m_nodeGroup;
		NodeGroup *m_fixupNodeGroup = nullptr;


		void ensureNotEnabledFirstCycles(Circuit &circuit, NodeGroup *ng, Node_MemPort *writePort, size_t numCycles);


		void buildResetLogic(Circuit &circuit);
		void buildResetRom(Circuit &circuit);
		void buildResetOverrides(Circuit &circuit, NodePort writeAddr, NodePort writeData, Node_MemPort *resetWritePort);
		Clock *buildResetClock(Circuit &circuit,Clock *clockDomain);


		Node_MemPort *findSuitableResetWritePort();
		NodePort buildResetAddrCounter(Circuit &circuit,size_t width, Clock *resetClock);

		void giveName(Circuit &circuit, NodePort &nodePort, std::string name);

};

	class Memory2VHDLPattern : public TechnologyMappingPattern
	{
		public:
			Memory2VHDLPattern();
			virtual bool attemptApply(Circuit &circuit, hlim::NodeGroup *nodeGroup) const override;
	};


MemoryGroup *formMemoryGroupIfNecessary(Circuit &circuit, Node_Memory *memory);

void findMemoryGroups(Circuit &circuit);
void buildExplicitMemoryCircuitry(Circuit &circuit);


}
