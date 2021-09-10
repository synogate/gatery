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

#include "GenericMemory.h"
#include "TargetDevice.h"

#include <gatery/frontend/GraphTools.h>

#include <gatery/hlim/postprocessing/MemoryDetector.h>
#include <gatery/hlim/supportNodes/Node_MemPort.h>

namespace gtry::scl::arch {

GenericMemoryCapabilities::GenericMemoryCapabilities(const TargetDevice &targetDevice) : m_targetDevice(targetDevice) { }

GenericMemoryCapabilities::~GenericMemoryCapabilities()
{
}

GenericMemoryCapabilities::Choice GenericMemoryCapabilities::select(const Request &request) const
{
	return MemoryCapabilities::select(request); // todo
}


bool GenericMemoryPattern::scopedAttemptApply(hlim::NodeGroup *nodeGroup) const
{
	auto *memGrp = dynamic_cast<hlim::MemoryGroup*>(nodeGroup->getMetaInfo());
	if (memGrp == nullptr) return false;


	// simplifications for now:
	// - not building extra read/write ports
	// - only extend width
	// - read latency must match (not building extra regs)

	if (memGrp->getReadPorts().size() + memGrp->getWritePorts().size() > 2) return false;

	bool requiresCrossPortReadFirst = false;
	bool requiresCrossPortWriteFirst = false;
	//bool portsMustDisable = 

	size_t readLateny = ~0ull;
	if (!memGrp->getReadPorts().empty())
		readLateny = memGrp->getReadPorts().front().dedicatedReadLatencyRegisters.size();

	for (auto &rp : memGrp->getReadPorts()) {
		HCL_ASSERT(readLateny == rp.dedicatedReadLatencyRegisters.size());
		for (auto &wp : memGrp->getWritePorts()) {
			if (rp.node->isOrderedBefore(wp.node.get()))
				requiresCrossPortReadFirst = true;
			if (wp.node->isOrderedBefore(rp.node.get()))
				requiresCrossPortWriteFirst = true;
		}
	}


	auto &memoryTypes = m_targetDevice.getEmbeddedMemories();

	std::vector<bool> compatibleMemTypes(memoryTypes.size(), true);
	for (auto i : utils::Range(memoryTypes.size())) {
		if (
				memGrp->getReadPorts().size() > memoryTypes[i].numReadPorts + memoryTypes[i].numReadWritePorts ||
				memGrp->getWritePorts().size() > memoryTypes[i].numWritePorts + memoryTypes[i].numReadWritePorts ||
				memGrp->getReadPorts().size() + memGrp->getWritePorts().size() > memoryTypes[i].numReadWritePorts + memoryTypes[i].numWritePorts + memoryTypes[i].numReadWritePorts
				) {
			compatibleMemTypes[i] = false;
			continue;
		}

		if (requiresCrossPortReadFirst) {
			if (!memoryTypes[i].crossPortReadDuringWrite.contains(GenericMemoryDesc::ReadDuringWriteBehavior::READ_FIRST)) {
				compatibleMemTypes[i] = false;
				continue;
			}
		}
		if (requiresCrossPortWriteFirst) {
			if (!memoryTypes[i].crossPortReadDuringWrite.contains(GenericMemoryDesc::ReadDuringWriteBehavior::WRITE_FIRST)) {
				compatibleMemTypes[i] = false;
				continue;
			}
		}

		if (memoryTypes[i].crossPortReadDuringWrite.contains(GenericMemoryDesc::ReadDuringWriteBehavior::ALL_MEMORY_UNDEFINED) ||
			memoryTypes[i].crossPortReadDuringWrite.contains(GenericMemoryDesc::ReadDuringWriteBehavior::MUST_NOT_HAPPEN)) {
			// Can't detect that yet, so don't touch that stuff
			compatibleMemTypes[i] = false;
			continue;
		}

		const auto &latencies = memoryTypes[i].readLatencies;
		if (std::find(latencies.begin(), latencies.end(), readLateny) == latencies.end()) {
			compatibleMemTypes[i] = false;
			continue;
		}
	}


	return true;
}

}


