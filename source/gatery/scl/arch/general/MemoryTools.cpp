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

#include "MemoryTools.h"

#include <gatery/frontend.h>
#include <gatery/hlim/GraphTools.h>
#include <gatery/hlim/NodeGroup.h>
#include <gatery/hlim/postprocessing/MemoryDetector.h>
#include <gatery/hlim/coreNodes/Node_Register.h>

#include <boost/format.hpp>

namespace gtry::scl::arch::memtools {

bool memoryIsSingleClock(hlim::NodeGroup *group)
{
	auto *memGrp = dynamic_cast<hlim::MemoryGroup*>(group->getMetaInfo());
	HCL_ASSERT_HINT(memGrp != nullptr, "Not a memory group.");

	hlim::Clock *clock = nullptr;

	for (const auto &wp : memGrp->getWritePorts()) {
		if (clock == nullptr)
			clock = wp.node->getClocks()[0];
		else
			if (clock != wp.node->getClocks()[0])
				return false;
	}

	for (const auto &rp : memGrp->getReadPorts()) {
		auto *readClkDom = rp.node->getClocks()[0];
		if (clock == nullptr)
			clock = readClkDom;
		else
			if (clock != readClkDom)
				return false;
	}

	return true;
}


std::vector<SplitMemoryGroup> createDepthSplitMemories(hlim::NodeGroup *group, const std::span<size_t> &splits)
{
	auto *memGrp = dynamic_cast<hlim::MemoryGroup*>(group->getMetaInfo());
	HCL_ASSERT_HINT(memGrp != nullptr, "Not a memory group.");
	memGrp->bypassSignalNodes(); // don't want any signal nodes interfering
	memGrp->findRegisters(); // Assure we have all our registers

	HCL_ASSERT(memGrp->getMemory()->getMinPortWidth() == memGrp->getMemory()->getMaxPortWidth());


	size_t minWidth = memGrp->getMemory()->getMinPortWidth();
	size_t totalDepth = memGrp->getMemory()->getMaxDepth();
	const sim::DefaultBitVectorState &fullMemState = memGrp->getMemory()->getPowerOnState();

	std::vector<SplitMemoryGroup> subMems;
	subMems.resize(splits.size()+1);

	utils::StableSet<hlim::NodePort> subnetInputs;
	utils::StableSet<hlim::NodePort> subnetOutputs;

	for (const auto &rp : memGrp->getReadPorts()) {
		subnetInputs.insert({.node = rp.node.get(), .port = (size_t) hlim::Node_MemPort::Inputs::enable});
		subnetInputs.insert({.node = rp.node.get(), .port = (size_t) hlim::Node_MemPort::Inputs::wrEnable});
		subnetInputs.insert({.node = rp.node.get(), .port = (size_t) hlim::Node_MemPort::Inputs::address});
		subnetInputs.insert({.node = rp.node.get(), .port = (size_t) hlim::Node_MemPort::Inputs::wrData});

		for (const auto &reg : rp.dedicatedReadLatencyRegisters)
			subnetInputs.insert({.node = reg.get(), .port = (size_t) hlim::Node_Register::ENABLE});

		subnetOutputs.insert(rp.dataOutput);
	}
	for (const auto &wp : memGrp->getWritePorts()) {
		subnetInputs.insert({.node = wp.node.get(), .port = (size_t) hlim::Node_MemPort::Inputs::enable});
		subnetInputs.insert({.node = wp.node.get(), .port = (size_t) hlim::Node_MemPort::Inputs::wrEnable});
		subnetInputs.insert({.node = wp.node.get(), .port = (size_t) hlim::Node_MemPort::Inputs::address});
		subnetInputs.insert({.node = wp.node.get(), .port = (size_t) hlim::Node_MemPort::Inputs::wrData});
	}


	for (auto splitIdx : utils::Range(subMems.size())) {

		// create a sub entity, move everything in
		auto *subMemory = group->addChildNodeGroup(hlim::NodeGroupType::SFU, (boost::format("memory_split_%d") % splitIdx).str());
		auto *subMemInfo = subMemory->createMetaInfo<hlim::MemoryGroup>(subMemory);
		subMems[splitIdx].subGroup = subMemInfo;
		utils::StableMap<hlim::BaseNode*, hlim::BaseNode*> &mapSrc2Dst = subMems[splitIdx].original2subGroup;

		// Only addr lines change width, so we can copy everything else and rescale
		DesignScope::get()->getCircuit().copySubnet(
			subnetInputs,
			subnetOutputs,
			mapSrc2Dst,
			false
		);

		for (auto &p : mapSrc2Dst)
			p.second->moveToGroup(subMemory);

		hlim::Node_Memory *subMemNode = dynamic_cast<hlim::Node_Memory *>(mapSrc2Dst[memGrp->getMemory()]);

		// Connect dummy input signals (necessary for read vs write port detection).
		for (const auto &wp : memGrp->getWritePorts()) {
			auto *newMemPort = dynamic_cast<hlim::Node_MemPort *>(mapSrc2Dst[wp.node]);

			UInt wrData = ConstUInt(BitWidth(wp.node->getBitWidth()));
			newMemPort->connectWrData(wrData.readPort());
		}		


		// Copy subsection of memory content to subMemNode, thereby also resizing the memory to the correct size
		size_t depthStart = splitIdx == 0?0:splits[splitIdx-1];
		size_t depthEnd = splitIdx < splits.size()?splits[splitIdx]:totalDepth;

		HCL_ASSERT(depthStart < totalDepth);
		HCL_ASSERT(depthEnd <= totalDepth);

		size_t bitStart = depthStart * minWidth;
		size_t bitEnd = depthEnd * minWidth;

		sim::DefaultBitVectorState &subState = subMemNode->getPowerOnState();
		subState.resize(bitEnd - bitStart);
		subState.copyRange(0, fullMemState, bitStart, bitEnd - bitStart);
		
		// Reform the subMemInfo
		subMemInfo->pullInPorts(subMemNode);
		subMemInfo->findRegisters();
	}

	return subMems;
}


void splitMemoryAlongDepthMux(hlim::NodeGroup *group, size_t log2SplitDepth, bool placeInputRegs, bool placeOutputRegs)
{
	auto *memGrp = dynamic_cast<hlim::MemoryGroup*>(group->getMetaInfo());
	HCL_ASSERT_HINT(memGrp != nullptr, "Not a memory group.");
	memGrp->bypassSignalNodes(); // don't want any signal nodes interfering

	HCL_ASSERT(memGrp->getMemory()->getMinPortWidth() == memGrp->getMemory()->getMaxPortWidth());

	HCL_ASSERT_HINT(log2SplitDepth+1 == utils::Log2C(memGrp->getMemory()->getMaxDepth()), "Muxing on a single address bit, this only works if splitting on the highest addr bit!");

	//group->setName((boost::format("memory_split_at_addrbit_%d_w_mux") % log2SplitDepth).str());
	GroupScope scope(group);

	// Split memory
	std::array<size_t, 1> splitPos = { (size_t)1ull << log2SplitDepth };
	auto subMems = createDepthSplitMemories(group, splitPos);

	// These need to be drawn/removed from the sub memories
	HCL_ASSERT(!placeInputRegs);
	HCL_ASSERT(!placeOutputRegs);


	// Hook them up (with a regular mutex)
	for (const auto &rp : memGrp->getReadPorts()) {
		HCL_ASSERT(rp.dedicatedReadLatencyRegisters.size() == memGrp->getMemory()->getRequiredReadLatency()); // Ensure the regs have been found

		UInt rdAddr = (UInt) getBVecBefore({.node = rp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::address});
		Bit rdEn = getBitBefore({.node = rp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::enable}, '1');

		Bit addrHighBit = rdAddr[log2SplitDepth];
		HCL_NAMED(addrHighBit);
		UInt addrLowBits = rdAddr(0, BitWidth{ log2SplitDepth });
		HCL_NAMED(addrLowBits);

		std::array<UInt, 2> newReadData;
		for (size_t i : {0ull, 1ull}) {

			const auto &new_rp = subMems[i].subGroup->findReadPort((hlim::Node_MemPort*)subMems[i].original2subGroup[rp.node.get()]);
			HCL_ASSERT(new_rp.dedicatedReadLatencyRegisters.size() == memGrp->getMemory()->getRequiredReadLatency()); // Ensure the regs have been found

			for (auto j : utils::Range(rp.dedicatedReadLatencyRegisters.size()))
				new_rp.dedicatedReadLatencyRegisters[j]->connectInput(hlim::Node_Register::ENABLE, rp.dedicatedReadLatencyRegisters[j]->getDriver(hlim::Node_Register::ENABLE));

			Bit newRdEn = rdEn & (addrHighBit == bool(i));
			newRdEn.setName((boost::format("cascade_%d_rdEn") % i).str());

			BitWidth addrBits{ new_rp.node->getExpectedAddressBits() };
			HCL_ASSERT(addrLowBits.width() >= addrBits);
			UInt newRdAddr = addrLowBits(0, addrBits); // happens if one chunk is significantly smaller than the other.

			new_rp.node->connectEnable(newRdEn.readPort());
			new_rp.node->connectAddress(newRdAddr.readPort());
			newReadData[i] = UInt(SignalReadPort(new_rp.dataOutput));
		}

		Bit addrHighBitDelayed = addrHighBit;

		for (auto reg : rp.dedicatedReadLatencyRegisters) {
			Clock clock(reg->getClocks()[0]);
			addrHighBitDelayed = clock(addrHighBitDelayed);
		}

		BVec rdDataHook = hookBVecAfter(rp.dataOutput);
		rdDataHook = (BVec)mux(addrHighBitDelayed, newReadData);
		rdDataHook.setName("cascade_rdData");		
	}
	for (const auto &wp : memGrp->getWritePorts()) {
		UInt wrAddr = (UInt) getBVecBefore({.node = wp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::address});
		UInt wrData = (UInt) getBVecBefore({.node = wp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::wrData});
		Bit wrEn = getBitBefore({.node = wp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::wrEnable}, '1');

		Bit addrHighBit = wrAddr[log2SplitDepth];
		HCL_NAMED(addrHighBit);
		UInt addrLowBits = wrAddr(0, BitWidth{ log2SplitDepth });
		HCL_NAMED(addrLowBits);

		for (size_t i : {0ull, 1ull}) {

			const auto &new_wp = subMems[i].subGroup->findWritePort((hlim::Node_MemPort*)subMems[i].original2subGroup[wp.node.get()]);

			Bit newWrEn = wrEn & (addrHighBit == bool(i));
			newWrEn.setName((boost::format("cascade_%d_wrEn") % i).str());

			BitWidth addrBits{ new_wp.node->getExpectedAddressBits() };
			HCL_ASSERT(addrLowBits.width() >= addrBits);
			UInt newWrAddr = addrLowBits(0, addrBits); // happens if one chunk is significantly smaller than the other.


			new_wp.node->connectEnable(newWrEn.readPort());
			new_wp.node->connectWrEnable(newWrEn.readPort());
			new_wp.node->connectAddress(newWrAddr.readPort());
			new_wp.node->connectWrData(wrData.readPort());
		}
	}

	// Drop old memory info
	memGrp = nullptr;
	group->dropMetaInfo();
	group->setGroupType(hlim::NodeGroupType::ENTITY);
}



std::vector<SplitMemoryGroup> createWidthSplitMemories(hlim::NodeGroup *group, const std::span<size_t> &splits)
{
	auto *memGrp = dynamic_cast<hlim::MemoryGroup*>(group->getMetaInfo());
	HCL_ASSERT_HINT(memGrp != nullptr, "Not a memory group.");
	memGrp->bypassSignalNodes(); // don't want any signal nodes interfering
	memGrp->findRegisters(); // Assure we have all our registers

	HCL_ASSERT(memGrp->getMemory()->getDriver((size_t) hlim::Node_Memory::Inputs::INITIALIZATION_DATA).node == nullptr);

	HCL_ASSERT(memGrp->getMemory()->getMinPortWidth() == memGrp->getMemory()->getMaxPortWidth());
	size_t width = memGrp->getMemory()->getMinPortWidth();
	size_t depth = memGrp->getMemory()->getMaxDepth();

	const sim::DefaultBitVectorState &fullMemState = memGrp->getMemory()->getPowerOnState();

	std::vector<SplitMemoryGroup> subMems;
	subMems.resize(splits.size()+1);

	utils::StableSet<hlim::NodePort> subnetInputs;
	utils::StableSet<hlim::NodePort> subnetOutputs;

	for (const auto &rp : memGrp->getReadPorts()) {
		subnetInputs.insert({.node = rp.node.get(), .port = (size_t) hlim::Node_MemPort::Inputs::enable});
		subnetInputs.insert({.node = rp.node.get(), .port = (size_t) hlim::Node_MemPort::Inputs::wrEnable});
		subnetInputs.insert({.node = rp.node.get(), .port = (size_t) hlim::Node_MemPort::Inputs::address});
		subnetInputs.insert({.node = rp.node.get(), .port = (size_t) hlim::Node_MemPort::Inputs::wrData});

		for (const auto &reg : rp.dedicatedReadLatencyRegisters)
			subnetInputs.insert({.node = reg.get(), .port = (size_t) hlim::Node_Register::ENABLE});

		subnetOutputs.insert(rp.dataOutput);
	}
	for (const auto &wp : memGrp->getWritePorts()) {
		subnetInputs.insert({.node = wp.node.get(), .port = (size_t) hlim::Node_MemPort::Inputs::enable});
		subnetInputs.insert({.node = wp.node.get(), .port = (size_t) hlim::Node_MemPort::Inputs::wrEnable});
		subnetInputs.insert({.node = wp.node.get(), .port = (size_t) hlim::Node_MemPort::Inputs::address});
		subnetInputs.insert({.node = wp.node.get(), .port = (size_t) hlim::Node_MemPort::Inputs::wrData});
	}


	for (auto splitIdx : utils::Range(subMems.size())) {

		// create a sub entity, move everything in
		auto *subMemory = group->addChildNodeGroup(hlim::NodeGroupType::SFU, (boost::format("memory_split_%d") % splitIdx).str());
		auto *subMemInfo = subMemory->createMetaInfo<hlim::MemoryGroup>(subMemory);
		subMems[splitIdx].subGroup = subMemInfo;
		utils::StableMap<hlim::BaseNode*, hlim::BaseNode*> &mapSrc2Dst = subMems[splitIdx].original2subGroup;

		// The data output width changes, so we have to resize the output registers
		DesignScope::get()->getCircuit().copySubnet(
			subnetInputs,
			subnetOutputs,
			mapSrc2Dst,
			false
		);

		for (auto &p : mapSrc2Dst)
			p.second->moveToGroup(subMemory);


		// Copy subsection of memory content to subMemNode, thereby also resizing the memory to the correct size
		size_t widthStart = splitIdx == 0?0:splits[splitIdx-1];
		size_t widthEnd = splitIdx < splits.size()?splits[splitIdx]:width;

		HCL_ASSERT(widthStart < width);
		HCL_ASSERT(widthEnd <= width);

		hlim::Node_Memory *subMemNode = dynamic_cast<hlim::Node_Memory *>(mapSrc2Dst[memGrp->getMemory()]);
		sim::DefaultBitVectorState &subState = subMemNode->getPowerOnState();
		subState.resize(depth * (widthEnd - widthStart));
		for (auto i : utils::Range(depth))
			subState.copyRange(i * (widthEnd - widthStart), fullMemState, i * width + widthStart, widthEnd - widthStart);




		// Resize registers, crop reset values
		{
			GroupScope scope(subMemory);
			for (const auto &rp : memGrp->getReadPorts()) {
				auto *newPort = (hlim::Node_MemPort*)mapSrc2Dst[rp.node.get()];

				// unhook before changing bitwdith
				for (auto &reg : rp.dedicatedReadLatencyRegisters) {
					auto *newReg = (hlim::Node_Register*)mapSrc2Dst[reg.get()];
					newReg->rewireInput(hlim::Node_Register::DATA, {});
				}

				newPort->changeBitWidth(widthEnd - widthStart);

				hlim::NodePort output = {.node = newPort, .port = (size_t) hlim::Node_MemPort::Outputs::rdData};
				for (auto &reg : rp.dedicatedReadLatencyRegisters) {
					auto *newReg = (hlim::Node_Register*)mapSrc2Dst[reg.get()];
					// Crop reset value
					if (reg->hasResetValue()) {
						UInt resetValue = (UInt) getBVecBefore(hlim::NodePort{.node = newReg, .port = hlim::Node_Register::RESET_VALUE});
						UInt croppedResetValue = resetValue(widthStart, BitWidth{ widthEnd - widthStart });
						newReg->connectInput(hlim::Node_Register::RESET_VALUE, croppedResetValue.readPort());
					}

					// reconnect one after another to resize
					newReg->connectInput(hlim::Node_Register::DATA, output);
					output = hlim::NodePort{.node = newReg, .port = 0ull};
				}
			}
		}

		// Connect dummy input signals (necessary for read vs write port detection).
		for (const auto &wp : memGrp->getWritePorts()) {
			auto *newMemPort = dynamic_cast<hlim::Node_MemPort *>(mapSrc2Dst[wp.node]);
			newMemPort->changeBitWidth(widthEnd - widthStart);

			UInt wrData = ConstUInt(BitWidth(widthEnd - widthStart));
			newMemPort->connectWrData(wrData.readPort());
		}

		// Reform the subMemInfo
		subMemInfo->pullInPorts(subMemNode);
		subMemInfo->findRegisters();
	}

	return subMems;
}



void splitMemoryAlongWidth(hlim::NodeGroup *group, size_t maxWidth)
{
	auto *memGrp = dynamic_cast<hlim::MemoryGroup*>(group->getMetaInfo());
	HCL_ASSERT_HINT(memGrp != nullptr, "Not a memory group.");
	memGrp->bypassSignalNodes(); // don't want any signal nodes interfering

	HCL_ASSERT(memGrp->getMemory()->getMinPortWidth() == memGrp->getMemory()->getMaxPortWidth());
	size_t width = memGrp->getMemory()->getMinPortWidth();
	HCL_ASSERT(maxWidth < width);

	//group->setName((boost::format("memory_split_every_%d_bits") % maxWidth).str());
	GroupScope scope(group);

	// Split memory
	std::vector<size_t> splitPositions;
	splitPositions.resize((width + maxWidth - 1) / maxWidth - 1);
	for (auto i : utils::Range(splitPositions.size()))
		splitPositions[i] = maxWidth * (1+i);

	auto subMems = createWidthSplitMemories(group, splitPositions);

	// Hook them up
	for (const auto &rp : memGrp->getReadPorts()) {
		std::vector<UInt> partialReads(subMems.size());
		for (size_t i : utils::Range(subMems.size())) {
			const auto &new_rp = subMems[i].subGroup->findReadPort((hlim::Node_MemPort*)subMems[i].original2subGroup[rp.node.get()]);
			HCL_ASSERT(new_rp.dedicatedReadLatencyRegisters.size() == memGrp->getMemory()->getRequiredReadLatency()); // Ensure the regs have been found

			for (auto j : utils::Range(rp.dedicatedReadLatencyRegisters.size()))
				new_rp.dedicatedReadLatencyRegisters[j]->connectInput(hlim::Node_Register::ENABLE, rp.dedicatedReadLatencyRegisters[j]->getDriver(hlim::Node_Register::ENABLE));

			new_rp.node->connectEnable(rp.node->getDriver((size_t)hlim::Node_MemPort::Inputs::enable));
			new_rp.node->connectAddress(rp.node->getDriver((size_t)hlim::Node_MemPort::Inputs::address));
			partialReads[i] = UInt(SignalReadPort(new_rp.dataOutput));
		}

		BVec rdDataHook = hookBVecAfter(rp.dataOutput);
		rdDataHook = (BVec) pack(partialReads);
		rdDataHook.setName("concatenated_rdData");			
	}
	for (const auto &wp : memGrp->getWritePorts()) {
		UInt wrData = (UInt) getBVecBefore({.node = wp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::wrData});
		for (size_t i : utils::Range(subMems.size())) {
			const auto &new_wp = subMems[i].subGroup->findWritePort((hlim::Node_MemPort*)subMems[i].original2subGroup[wp.node.get()]);

			new_wp.node->connectEnable(wp.node->getDriver((size_t)hlim::Node_MemPort::Inputs::enable));
			new_wp.node->connectWrEnable(wp.node->getDriver((size_t)hlim::Node_MemPort::Inputs::wrEnable));
			new_wp.node->connectAddress(wp.node->getDriver((size_t)hlim::Node_MemPort::Inputs::address));

			size_t widthStart = i == 0?0:splitPositions[i-1];
			size_t widthEnd = i < splitPositions.size()?splitPositions[i]:width;
			UInt wrDataCrop = wrData(widthStart, BitWidth{ widthEnd - widthStart });

			new_wp.node->connectWrData(wrDataCrop.readPort());
		}
	}

	// Drop old memory info
	memGrp = nullptr;
	group->dropMetaInfo();
	group->setGroupType(hlim::NodeGroupType::ENTITY);
}

//void splitMemoryAlongDepthMux(hlim::NodeGroup *group, size_t maxDepthAddrBits);



}
