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
#include "FPGADevice.h"

#include <gatery/frontend/GraphTools.h>

#include <gatery/hlim/postprocessing/MemoryDetector.h>
#include <gatery/hlim/supportNodes/Node_MemPort.h>
#include <gatery/hlim/supportNodes/Node_Memory.h>

namespace gtry::scl::arch {

GenericMemoryCapabilities::GenericMemoryCapabilities(const FPGADevice &targetDevice) : m_targetDevice(targetDevice) { }

GenericMemoryCapabilities::~GenericMemoryCapabilities()
{
}

GenericMemoryCapabilities::Choice GenericMemoryCapabilities::select(const Request &request) const
{
	const auto &embeddedMems = m_targetDevice.getEmbeddedMemories();

	auto *memChoice = embeddedMems.selectMemFor(request);

	HCL_DESIGNCHECK_HINT(memChoice != nullptr, "No suitable memory configuration could be found. Usually this means that the memory was restricted to a single size category that doesn't exist on the target device.");

    MemoryCapabilities::Choice result;
	const auto &desc = memChoice->getDesc();
	result.inputRegs = desc.inputRegs;
	result.outputRegs = desc.outputRegs;
	result.totalReadLatency = desc.outputRegs + (desc.inputRegs?1:0);
	return result;
}


void EmbeddedMemoryList::add(std::unique_ptr<EmbeddedMemory> mem)
{
	m_embeddedMemories.push_back(std::move(mem));
	std::sort(m_embeddedMemories.begin(), m_embeddedMemories.end(), [](const auto &lhs, const auto &rhs) {
		return lhs->getPriority() < rhs->getPriority();
	});
}

const EmbeddedMemory *EmbeddedMemoryList::selectMemFor(GenericMemoryCapabilities::Request request) const
{
	EmbeddedMemory *memChoice = nullptr;
	
	for (auto &mem : m_embeddedMemories) {
		const auto &desc = mem->getDesc();
		if (request.sizeCategory.contains(desc.sizeCategory) && 
			(1ull << mem->getDesc().addressBits) >= request.maxDepth) {
				memChoice = mem.get();
				break;
			}
	}

	if (memChoice == nullptr) {
		// Pick largest that works
		size_t choiceSize = 0;
		for (auto &mem : m_embeddedMemories) {
			const auto &desc = mem->getDesc();
			size_t size = (1ull << mem->getDesc().addressBits);
			if (request.sizeCategory.contains(desc.sizeCategory)) {
				if (memChoice == nullptr || size > choiceSize) {
					memChoice = mem.get();
					choiceSize = size;
				}
			}
		}
	}
	return memChoice;
}


bool EmbeddedMemoryPattern::scopedAttemptApply(hlim::NodeGroup *nodeGroup) const
{
	auto *memGrp = dynamic_cast<hlim::MemoryGroup*>(nodeGroup->getMetaInfo());
	if (memGrp == nullptr) return false;

	const auto &embeddedMems = m_targetDevice.getEmbeddedMemories();

	GenericMemoryCapabilities::Request request;
	request.size = memGrp->getMemory()->getSize();
	request.maxDepth = memGrp->getMemory()->getMaxDepth();

	switch (memGrp->getMemory()->type()) {
		case hlim::Node_Memory::MemType::SMALL:
			request.sizeCategory = MemoryCapabilities::SizeCategory::SMALL;
		break;
		case hlim::Node_Memory::MemType::MEDIUM:
			request.sizeCategory = MemoryCapabilities::SizeCategory::MEDIUM;
		break;
		case hlim::Node_Memory::MemType::LARGE:
			request.sizeCategory = MemoryCapabilities::SizeCategory::LARGE;
		break;
		case hlim::Node_Memory::MemType::EXTERNAL:
			return false;
		default:
		break;
	}

	auto *memChoice = embeddedMems.selectMemFor(request);
	return memChoice->apply(nodeGroup);
}


}


