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

#include "XilinxLutram.h"
#include "XilinxDevice.h"

#include "RAM64M8.h"
#include "RAM256X1D.h"

#include <gatery/hlim/postprocessing/MemoryDetector.h>
#include <gatery/hlim/supportNodes/Node_MemPort.h>
#include <gatery/hlim/coreNodes/Node_Register.h>

#include "../general/MemoryTools.h"


namespace gtry::scl::arch::xilinx {

XilinxLutram::XilinxLutram(const XilinxDevice &xilinxDevice) : m_xilinxDevice(xilinxDevice)
{
	m_desc.sizeCategory = MemoryCapabilities::SizeCategory::SMALL;
	m_desc.inputRegs = false;
	m_desc.outputRegs = 1;

	m_desc.memoryName = "RAM64";

	m_desc.size = 448;
	m_desc.addressBits = 8;	
}


bool XilinxLutram::apply(hlim::NodeGroup *nodeGroup) const
{
	auto *memGrp = dynamic_cast<hlim::MemoryGroup*>(nodeGroup->getMetaInfo());
	if (memGrp == nullptr) return false;
	if (memGrp->getMemory()->type() == hlim::Node_Memory::MemType::EXTERNAL)
		return false;

	if (memGrp->getReadPorts().size() != 1) return false;
	if (memGrp->getWritePorts().size() > 1) return false;
	if (memGrp->getMemory()->getRequiredReadLatency() == 0) return false;
	if (memGrp->getMemory()->getMinPortWidth() != memGrp->getMemory()->getMaxPortWidth()) return false;
	if (!memtools::memoryIsSingleClock(nodeGroup)) return false;

	// At this point we are sure we can handle it (as long as register retiming doesn't fail of course).

	// Everything else needs this, so do it first. Also we want rmw logic as far outside as possible
	// The reset could potentially be delayed for shorter resets (but with more reset logic).
	auto &circuit = DesignScope::get()->getCircuit();
	memGrp->convertToReadBeforeWrite(circuit);
	memGrp->attemptRegisterRetiming(circuit);
	memGrp->resolveWriteOrder(circuit);
	memGrp->updateNoConflictsAttrib();
	memGrp->buildReset(circuit);
	memGrp->bypassSignalNodes();
	memGrp->verify(); // This one can actually go


	reccursiveBuild(nodeGroup);

	return true;
}

void XilinxLutram::reccursiveBuild(hlim::NodeGroup *nodeGroup) const
{
//	DesignScope::visualize("process");


	auto *memGrp = dynamic_cast<hlim::MemoryGroup*>(nodeGroup->getMetaInfo());

	size_t width = memGrp->getMemory()->getMinPortWidth();

	size_t maxDepth = memGrp->getMemory()->getMaxDepth();
	size_t maxDepthLutram = 1ull << m_desc.addressBits;


	size_t numCascadesNeeded = (maxDepth + maxDepthLutram-1) / maxDepthLutram;
	size_t depthHandledBy = std::min(maxDepth, numCascadesNeeded * maxDepthLutram);
	size_t addrWidthLutram = utils::Log2C(depthHandledBy / numCascadesNeeded);

	size_t widthSingleLutram;
	HCL_ASSERT(addrWidthLutram <= 8);
	switch (addrWidthLutram) {
		case 8: widthSingleLutram = 1; break; // RAM256X1D
		case 4: widthSingleLutram = 1; break; // RAM128X1D or half of RAM256X1D
		default: 
			widthSingleLutram = 7; break; // RAM64M8
	}

	if (widthSingleLutram < width) {
		memtools::splitMemoryAlongWidth(nodeGroup, widthSingleLutram);

		// Rinse and repeat
		for (auto &c : nodeGroup->getChildren()) reccursiveBuild(c.get());
		return;
	}

	if (numCascadesNeeded > 1) {
		// Todo: use hardware cascading
		memtools::splitMemoryAlongDepthMux(nodeGroup, utils::Log2(maxDepth-1), false, false);

		// Rinse and repeat
		for (auto &c : nodeGroup->getChildren()) reccursiveBuild(c.get());
		return;
	}

	auto &rp = memGrp->getReadPorts().front();
	for (auto reg : rp.dedicatedReadLatencyRegisters) {
		HCL_ASSERT(!reg->hasResetValue());
		HCL_ASSERT(!reg->hasEnable());
	}

	bool hasWritePort = !memGrp->getWritePorts().empty();

	GroupScope scope(nodeGroup->getParent());

	UInt readData;

	if (widthSingleLutram == 1) {
		auto *ram = DesignScope::createNode<RAM256X1D>();

		UInt rdAddr = getUIntBefore({.node = rp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::address});

		HCL_ASSERT(hasWritePort);
		if (hasWritePort) {
			auto &wp = memGrp->getWritePorts().front();

			UInt wrAddr = getUIntBefore({.node = wp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::address});
			UInt wrData = getUIntBefore({.node = wp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::wrData});
			Bit wrEn = getBitBefore({.node = wp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::enable}, '1');

			HCL_ASSERT(wrAddr.size() <= 8);
			HCL_ASSERT(wrData.size() == 1);
			HCL_ASSERT(rdAddr.size() <= 8);
			HCL_ASSERT(rp.node->getBitWidth() == 1);

			Bit rdDataBit = ram->setupSDP(zext(wrAddr, 8-wrAddr.size()), wrData[0], wrEn, zext(rdAddr, 8-rdAddr.size()));
			readData = pack(rdDataBit);

			hlim::Clock* writeClock = memGrp->getWritePorts().front().node->getClocks()[0];
			HCL_ASSERT(writeClock->getTriggerEvent() == hlim::Clock::TriggerEvent::RISING);

			ram->attachClock(writeClock, (size_t)RAM64M8::CLK_WR);
		}
	} else {
		auto *ram = DesignScope::createNode<RAM64M8>();

		UInt rdAddr = getUIntBefore({.node = rp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::address});

		HCL_ASSERT(hasWritePort);
		if (hasWritePort) {
			auto &wp = memGrp->getWritePorts().front();

			UInt wrAddr = getUIntBefore({.node = wp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::address});
			UInt wrData = getUIntBefore({.node = wp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::wrData});
			Bit wrEn = getBitBefore({.node = wp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::enable}, '1');

			HCL_ASSERT(wrAddr.size() <= 6);
			HCL_ASSERT(wrData.size() <= 7);
			HCL_ASSERT(rdAddr.size() <= 6);
			HCL_ASSERT(rp.node->getBitWidth() <= 7);

			UInt rdData_7wide = ram->setup64x7_SDP(zext(wrAddr, 6-wrAddr.size()), zext(wrData, 7-wrData.size()), wrEn, zext(rdAddr, 6-rdAddr.size()));
			readData = rdData_7wide(0, rp.node->getBitWidth());

			hlim::Clock* writeClock = memGrp->getWritePorts().front().node->getClocks()[0];
			HCL_ASSERT(writeClock->getTriggerEvent() == hlim::Clock::TriggerEvent::RISING);

			ram->attachClock(writeClock, (size_t)RAM64M8::CLK_WR);
		}
	}

	for (size_t i = 0; i < rp.dedicatedReadLatencyRegisters.size(); i++) {
		auto &reg = rp.dedicatedReadLatencyRegisters[i];
		Clock clock(reg->getClocks()[0]);
		readData = clock(readData);
		if (i > 0)
			attribute(readData, {.allowFusing = false});
	}		

	UInt rdDataHook = hookUIntAfter(rp.dataOutput);
	rdDataHook.exportOverride(readData(0, rdDataHook.size()));	
}

}