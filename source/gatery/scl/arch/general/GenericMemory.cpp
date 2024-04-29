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

#include "GenericMemory.h"
#include "FPGADevice.h"
#include "MemoryTools.h"

#include <gatery/debug/DebugInterface.h>
#include <gatery/frontend/GraphTools.h>

#include <gatery/hlim/postprocessing/MemoryDetector.h>
#include <gatery/hlim/supportNodes/Node_MemPort.h>
#include <gatery/hlim/supportNodes/Node_Memory.h>

namespace gtry::scl::arch {

MemoryCapabilities::Choice EmbeddedMemory::select(hlim::NodeGroup *group, const MemoryCapabilities::Request &request) const
{
	MemoryCapabilities::Choice result;
	result.inputRegs = m_desc.inputRegs;
	result.outputRegs = m_desc.outputRegs;
	result.totalReadLatency = m_desc.outputRegs + (m_desc.inputRegs?1:0);

	if (result.outputRegs == 0 && m_desc.sizeCategory == MemoryCapabilities::SizeCategory::MEDIUM) {
		size_t numBlocksEstimate = (request.size + m_desc.size-1) / m_desc.size;
		if (numBlocksEstimate > 7) {
			result.outputRegs++;
			result.totalReadLatency++;
		}
	}	

	return result;
}

GenericMemoryCapabilities::GenericMemoryCapabilities(const FPGADevice &targetDevice) : m_targetDevice(targetDevice) { }

GenericMemoryCapabilities::~GenericMemoryCapabilities()
{
}

GenericMemoryCapabilities::Choice GenericMemoryCapabilities::select(hlim::NodeGroup *group, const Request &request) const
{
	const auto &embeddedMems = m_targetDevice.getEmbeddedMemories();

	auto *memChoice = embeddedMems.selectMemFor(group, request);

	HCL_DESIGNCHECK_HINT(memChoice != nullptr, "No suitable memory configuration could be found. Usually this means that the memory was restricted to a single size category that doesn't exist on the target device.");
	return memChoice->select(group, request);
}


void EmbeddedMemoryList::add(std::unique_ptr<EmbeddedMemory> mem)
{
	m_embeddedMemories.push_back(std::move(mem));
	std::sort(m_embeddedMemories.begin(), m_embeddedMemories.end(), [](const auto &lhs, const auto &rhs) {
		return lhs->getPriority() < rhs->getPriority();
	});
}

const EmbeddedMemory *EmbeddedMemoryList::selectMemFor(hlim::NodeGroup *group, GenericMemoryCapabilities::Request request) const
{
	EmbeddedMemory *memChoice = nullptr;

	if (group->config("memory_block")) {
		auto name = group->config("memory_block").as<std::string>();

		for (auto &mem : m_embeddedMemories) {
			const auto &desc = mem->getDesc();
			if (desc.memoryName == name) {
				memChoice = mem.get();
				dbg::log(dbg::LogMessage(group) << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING << "Choosing memory primitive " << desc.memoryName << " for " << group << " because of configuration override.");
				break;
			}
		}
		HCL_DESIGNCHECK_HINT(memChoice != nullptr, "The specified memory_block " + name + " is unknown or not available for the targeted device!");
	} else {
		for (auto &mem : m_embeddedMemories) {
			const auto &desc = mem->getDesc();
			if (request.sizeCategory.contains(desc.sizeCategory) && 
				(1ull << desc.addressBits) >= request.maxDepth) {
					if (request.dualClock && !desc.supportsDualClock) {
						dbg::log(dbg::LogMessage(group) << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING << "Not choosing memory primitive " << desc.memoryName 
								<< " for " << group << " because it does not support dual clock.");
						continue;
					} 

					if (request.powerOnInitialized && !desc.supportsPowerOnInitialization) {
						dbg::log(dbg::LogMessage(group) << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING << "Not choosing memory primitive " << desc.memoryName 
								<< " for " << group << " because it does not support power-on initialization of it's content.");
						continue;
					} 

					memChoice = mem.get();
					dbg::log(dbg::LogMessage(group) << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING << "Choosing memory primitive " << desc.memoryName 
							<< " for " << group << " because it is the smallest (of the selected categories) that meets or exceeds the the required memory depth.");
					break;
				}
		}

		if (memChoice == nullptr) {
			// Pick largest that works
			size_t choiceSize = 0;
			for (auto &mem : m_embeddedMemories) {
				const auto &desc = mem->getDesc();

				if (request.dualClock && !desc.supportsDualClock) continue;
				if (request.powerOnInitialized && !desc.supportsPowerOnInitialization) continue;

				size_t size = (1ull << mem->getDesc().addressBits);
				if (request.sizeCategory.contains(desc.sizeCategory)) {
					if (memChoice == nullptr || size > choiceSize) {
						memChoice = mem.get();
						choiceSize = size;
					}
				}
			}
			if (memChoice != nullptr)
				dbg::log(dbg::LogMessage(group) << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING << "Choosing memory primitive " << memChoice->getDesc().memoryName << " for " << group << " because it is the largest available.");
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
	if (memGrp->getWritePorts().size() == 0)
		request.mode = GenericMemoryCapabilities::Mode::ROM;
	else
		request.mode = GenericMemoryCapabilities::Mode::SIMPLE_DUAL_PORT;

	request.dualClock = !memtools::memoryIsSingleClock(nodeGroup);
	request.powerOnInitialized = memGrp->getMemory()->requiresPowerOnInitialization();

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
			dbg::log(dbg::LogMessage(nodeGroup) << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING 
				<< "Not mapping memory " << memGrp->getMemory() << " to any memory macros because it is EXTERNAL memory");
			return false;
		default:
		break;
	}

	if (request.maxDepth == 1) {
		dbg::log(dbg::LogMessage(nodeGroup) << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING << "Not mapping " << memGrp->getMemory() << " to a memory primitive because its depth is one and is better served by a register.");
		return false;
	}	

	auto *memChoice = embeddedMems.selectMemFor(nodeGroup, request);
	if (memChoice == nullptr) {
		dbg::log(dbg::LogMessage(nodeGroup) << dbg::LogMessage::LOG_WARNING << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING 
			<< "Not mapping memory " << memGrp->getMemory() << " because no suitable choice was found");
		return false;
	}
	if (memChoice->apply(nodeGroup)) {
		nodeGroup->getParent()->properties()["type"] = memChoice->getDesc().sizeCategory;
		nodeGroup->getParent()->properties()["primitive"] = memChoice->getDesc().memoryName;
		return true;
	} else {
		dbg::log(dbg::LogMessage(nodeGroup) << dbg::LogMessage::LOG_WARNING << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING << "Applying memory primitive " << memChoice->getDesc().memoryName << " to " << memGrp->getMemory() << " failed.");
		return false;
	}
}


}


