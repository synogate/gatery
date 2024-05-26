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

#include "BlockramUltrascale.h"
#include "XilinxDevice.h"

#include "RAMB18E2.h"
#include "RAMB36E2.h"

#include <gatery/frontend/Clock.h>

#include <gatery/hlim/postprocessing/MemoryDetector.h>
#include <gatery/hlim/supportNodes/Node_MemPort.h>
#include <gatery/hlim/supportNodes/Node_Memory.h>
#include <gatery/hlim/coreNodes/Node_Register.h>

#include <gatery/frontend/DesignScope.h>
#include <gatery/frontend/GraphTools.h>
#include <gatery/frontend/Attributes.h>

#include "../general/MemoryTools.h"
#include <gatery/debug/DebugInterface.h>

#include <iostream>

namespace gtry::scl::arch::xilinx {

BlockramUltrascale::BlockramUltrascale(const XilinxDevice &xilinxDevice) : XilinxBlockram(xilinxDevice)
{
	m_desc.memoryName = "RAMBxE2";

	m_desc.size = 36 << 10;
	m_desc.addressBits = 15;
	m_desc.supportsDualClock = true;
}

bool BlockramUltrascale::apply(hlim::NodeGroup *nodeGroup) const
{
	auto *memGrp = dynamic_cast<hlim::MemoryGroup*>(nodeGroup->getMetaInfo());
	if (memGrp == nullptr) return false;
	if (memGrp->getMemory()->type() == hlim::Node_Memory::MemType::EXTERNAL) {
		dbg::log(dbg::LogMessage(nodeGroup) << dbg::LogMessage::LOG_WARNING << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING << "Will not apply memory primitive " << getDesc().memoryName << " to " << memGrp->getMemory() 
				<< " because it is external memory.");
		return false;
	}

	if (memGrp->getReadPorts().size() == 0) {
		dbg::log(dbg::LogMessage(nodeGroup) << dbg::LogMessage::LOG_WARNING << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING << 
				"Will not apply memory primitive " << getDesc().memoryName << " to " << memGrp->getMemory() 
				<< " because it has no read ports.");
		return false;
	}

	if (memGrp->getReadPorts().size() > 1) {
		dbg::log(dbg::LogMessage(nodeGroup) << dbg::LogMessage::LOG_WARNING << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING << 
				"Will not apply memory primitive " << getDesc().memoryName << " to " << memGrp->getMemory() 
				<< " because it has more than one read port and so far only one read port is supported.");
		return false;
	}
	const auto &rp = memGrp->getReadPorts().front();
	if (memGrp->getWritePorts().size() > 1) {
		dbg::log(dbg::LogMessage(nodeGroup) << dbg::LogMessage::LOG_WARNING << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING << 
				"Will not apply memory primitive " << getDesc().memoryName << " to " << memGrp->getMemory() 
				<< " because it has more than one write port and so far only one write port is supported.");

		return false;
	}
	if (memGrp->getMemory()->getRequiredReadLatency() == 0) {
		dbg::log(dbg::LogMessage(nodeGroup) << dbg::LogMessage::LOG_WARNING << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING << 
				"Will not apply memory primitive " << getDesc().memoryName << " to " << memGrp->getMemory() 
				<< " because it is asynchronous (zero latency reads) and the targeted block ram needs at least one cycle latency.");

		return false;
	}

	if (memGrp->getMemory()->getMinPortWidth() != memGrp->getMemory()->getMaxPortWidth()) return false;
	//if (!memtools::memoryIsSingleClock(nodeGroup)) return false;

	// At this point we are sure we can handle it (as long as register retiming doesn't fail of course).


	// Everything else needs this, so do it first. Also we want rmw logic as far outside as possible
	// The reset could potentially be delayed for shorter resets (but with more reset logic).
	auto &circuit = DesignScope::get()->getCircuit();
	memGrp->convertToReadBeforeWrite(circuit);
	memGrp->attemptRegisterRetiming(circuit);



	hlim::Clock* writeClock = nullptr;
	if (memGrp->getWritePorts().size() > 0) {
		writeClock = memGrp->getWritePorts().front().node->getClocks()[0];
		if (writeClock->getTriggerEvent() != hlim::Clock::TriggerEvent::RISING) {
			dbg::log(dbg::LogMessage(nodeGroup) << dbg::LogMessage::LOG_WARNING << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING << 
					"Will not apply memory primitive " << getDesc().memoryName << " to " << memGrp->getMemory() 
					<< " because its write clock is not triggering on rising clock edges.");
			return false;
		}
	}

	auto *readClock = rp.dedicatedReadLatencyRegisters.front()->getClocks()[0];
	if (readClock->getTriggerEvent() != hlim::Clock::TriggerEvent::RISING) {
		dbg::log(dbg::LogMessage(nodeGroup) << dbg::LogMessage::LOG_WARNING << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING << 
				"Will not apply memory primitive " << getDesc().memoryName << " to " << memGrp->getMemory() 
				<< " because its read clock is not triggering on rising clock edges.");
		return false;
	}

	for (auto reg : rp.dedicatedReadLatencyRegisters) {
		if (reg->hasResetValue()) {
			dbg::log(dbg::LogMessage(nodeGroup) << dbg::LogMessage::LOG_WARNING << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING << 
					"Will not apply memory primitive " << getDesc().memoryName << " to " << memGrp->getMemory() 
					<< " because one of its output registers has a reset value.");
			return false;
		}
/*
		if (reg->hasEnable()) {
			dbg::log(dbg::LogMessage(nodeGroup) << dbg::LogMessage::LOG_WARNING << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING << 
					"Will not apply memory primitive " << getDesc().memoryName << " to " << memGrp->getMemory() 
					<< " because one of its output registers has an enable.");
			return false;
		}
*/
		if (readClock != reg->getClocks()[0]) {
			dbg::log(dbg::LogMessage(nodeGroup) << dbg::LogMessage::LOG_WARNING << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING << 
					"Will not apply memory primitive " << getDesc().memoryName << " to " << memGrp->getMemory() 
					<< " because its output registers have differing clocks.");
			return false;
		}
	}

	memGrp->resolveWriteOrder(circuit);
	memGrp->updateNoConflictsAttrib();
	memGrp->buildReset(circuit);
	memGrp->bypassSignalNodes();
	memGrp->verify(); // This one can actually go


	reccursiveBuild(nodeGroup);

	return true;
}

void BlockramUltrascale::reccursiveBuild(hlim::NodeGroup *nodeGroup) const
{
//	DesignScope::visualize("process");


	auto *memGrp = dynamic_cast<hlim::MemoryGroup*>(nodeGroup->getMetaInfo());

	size_t width = memGrp->getMemory()->getMinPortWidth();

	size_t maxDepth = memGrp->getMemory()->getMaxDepth();
	size_t maxDepth36k = 1ull << m_desc.addressBits;
	size_t maxDepth18k = maxDepth36k / 2;

	size_t oddDepth = maxDepth % maxDepth36k;

	if (maxDepth > maxDepth36k && oddDepth > 0 && oddDepth <= maxDepth18k) {
		// After building cascades of 36k, we will be able to fit a single 18k at the end which might span more than one bit width, so split that off first.
		memtools::splitMemoryAlongDepthMux(nodeGroup, utils::Log2(maxDepth-1), false, false);
		
		// Rinse and repeat
		for (auto &c : nodeGroup->getChildren()) reccursiveBuild(c.get());
		return;
	} 

	bool isSDP = true;

	size_t numCascadesNeeded36k = (maxDepth + maxDepth36k-1) / maxDepth36k;
	size_t depthHandledBy36k = std::min(maxDepth, numCascadesNeeded36k * maxDepth36k);
	size_t addrWidth36k = utils::Log2C(depthHandledBy36k / numCascadesNeeded36k);

	size_t widthSingle36k;
	HCL_ASSERT(addrWidth36k <= 15);
	switch (addrWidth36k) {
		case 15: widthSingle36k = 1; break;
		case 14: widthSingle36k = 2; break;
		case 13: widthSingle36k = 4; break;
		case 12: widthSingle36k = 9; break;
		case 11: widthSingle36k = 18; break;
		case 10: widthSingle36k = 36; break;
		default: widthSingle36k = isSDP?72:36; break; // only in SDP
	}

	if (widthSingle36k < width) {
		// We may need to cascade eventually, but split width first to allow hardware cascading at lowest level
		memtools::splitMemoryAlongWidth(nodeGroup, widthSingle36k);

		// Rinse and repeat
		for (auto &c : nodeGroup->getChildren()) reccursiveBuild(c.get());
		return;
	}

	if (numCascadesNeeded36k > 1) {
		// Todo: use hardware cascading
		memtools::splitMemoryAlongDepthMux(nodeGroup, utils::Log2(maxDepth-1), false, false);

		// Rinse and repeat
		for (auto &c : nodeGroup->getChildren()) reccursiveBuild(c.get());
		return;
	}
	

	size_t numCascadesNeeded18k = (maxDepth + maxDepth18k-1) / maxDepth18k;
	size_t depthHandledBy18k = std::min(maxDepth, numCascadesNeeded18k * maxDepth18k);
	size_t addrWidth18k = utils::Log2C(depthHandledBy18k / numCascadesNeeded18k);

	size_t widthSingle18k;
	HCL_ASSERT(addrWidth18k <= 14);
	switch (addrWidth18k) {
		case 14: widthSingle18k = 1; break;
		case 13: widthSingle18k = 2; break;
		case 12: widthSingle18k = 4; break;
		case 11: widthSingle18k = 9; break;
		case 10: widthSingle18k = 18; break;
		default: widthSingle18k = isSDP?36:18; break; // only in SDP
	}

	size_t num36kPerCascade = (width + widthSingle36k-1) / widthSingle36k;
	size_t num18kPerCascade = (width + widthSingle18k-1) / widthSingle18k;

	HCL_ASSERT(numCascadesNeeded36k == 1);
	HCL_ASSERT(num36kPerCascade == 1);

	auto &rp = memGrp->getReadPorts().front();
	for (auto reg : rp.dedicatedReadLatencyRegisters) {
		HCL_ASSERT(!reg->hasResetValue());
//		HCL_ASSERT(!reg->hasEnable());
	}

	GroupScope scope(nodeGroup->getParent());

	// Is a single RAMB18K enough?
	if (num18kPerCascade == 1 && numCascadesNeeded18k == 1) {
		// Yep
		auto *bram = DesignScope::createNode<RAMB18E2>();
		hookUpSingleBRamSDP(bram, m_desc.addressBits-1, widthSingle18k, memGrp);
	} else {
		// Nope, need a RAMB36K
		auto *bram = DesignScope::createNode<RAMB36E2>();
		hookUpSingleBRamSDP(bram, m_desc.addressBits, widthSingle36k, memGrp);
	}

	HCL_ASSERT(!memGrp->getMemory()->requiresPowerOnInitialization()); // todo: implement
}

void BlockramUltrascale::hookUpSingleBRamSDP(RAMBxE2 *bram, size_t addrSize, size_t width, hlim::MemoryGroup *memGrp) const
{
	bool hasWritePort = !memGrp->getWritePorts().empty();
	auto &rp = memGrp->getReadPorts().front();

	bool crossPortReadFirst = false;

	bool readFirst = false;
	bool writeFirst = false;
	if (hasWritePort) {
		auto &wp = memGrp->getWritePorts().front();
		if (wp.node->isOrderedBefore(rp.node.get()))
			writeFirst = true;
		if (rp.node->isOrderedBefore(wp.node.get()))
			readFirst = true;
	}

	HCL_ASSERT(!writeFirst);

	crossPortReadFirst = readFirst;


	bram->defaultInputs(false, hasWritePort);
	bram->setupClockDomains(RAMBxE2::ClockDomains::COMMON);

	RAMBxE2::PortSetup rdPortSetup = {
		.readWidth = width,
	};
	bram->setupPortA(rdPortSetup);

	UInt rdAddr = (UInt) getBVecBefore({.node = rp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::address});
	Bit rdEn = getBitBefore({.node = rp.dedicatedReadLatencyRegisters[0], .port = (size_t)hlim::Node_Register::ENABLE}, '1');

	bram->connectAddressPortA(rdAddr);
	bram->setInput(RAMBxE2::Inputs::IN_EN_A_RD_EN, rdEn);

	auto *readClock = rp.dedicatedReadLatencyRegisters.front()->getClocks()[0];
	HCL_ASSERT(readClock->getTriggerEvent() == hlim::Clock::TriggerEvent::RISING);		
	bram->attachClock(readClock, (size_t)RAMBxE2::Clocks::CLK_A_RD);   

	BVec readData = (BVec) bram->getReadDataPortA(width);
	
	for (size_t i = 1; i < rp.dedicatedReadLatencyRegisters.size(); i++) {
		auto &reg = rp.dedicatedReadLatencyRegisters[i];
		Clock clock(reg->getClocks()[0]);
		ENIF (getBitBefore({.node = reg, .port = (size_t)hlim::Node_Register::ENABLE}, '1'))
			readData = clock(readData);
		attribute(readData, {.allowFusing = false});
	}		

	BVec rdDataHook = hookBVecAfter(rp.dataOutput);
	rdDataHook.exportOverride(readData(0, rdDataHook.width()));

	if (hasWritePort) {
		auto &wp = memGrp->getWritePorts().front();

		UInt wrAddr = (UInt) getBVecBefore({.node = wp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::address});
		BVec wrData = getBVecBefore({.node = wp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::wrData});
		Bit wrEn = getBitBefore({.node = wp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::wrEnable}, '1');

		HCL_ASSERT(rdAddr.size() == wrAddr.size());

		RAMBxE2::PortSetup wrPortSetup = {
			.writeMode = (crossPortReadFirst?RAMBxE2::WriteMode::READ_FIRST:RAMBxE2::WriteMode::NO_CHANGE),
			.writeWidth = width,
		};
		bram->setupPortB(wrPortSetup);
		bram->connectAddressPortB(wrAddr);
		bram->setInput(RAMBxE2::Inputs::IN_EN_B_WR_EN, wrEn);
		bram->connectWriteDataPortB(zext(wrData, BitWidth{ width }));

		hlim::Clock* writeClock = memGrp->getWritePorts().front().node->getClocks()[0];
		HCL_ASSERT(writeClock->getTriggerEvent() == hlim::Clock::TriggerEvent::RISING);

		bram->attachClock(writeClock, (size_t)RAMBxE2::Clocks::CLK_B_WR);
		
		if (writeClock != readClock)
			bram->setupClockDomains(RAMBxE2::ClockDomains::INDEPENDENT);
	}
}



#if 0

	bool hasWritePort = !memGrp->getWritePorts().empty();
/*
	auto &circuit = DesignScope::get()->getCircuit();

	memGrp->convertToReadBeforeWrite(circuit);
	memGrp->attemptRegisterRetiming(circuit);
*/


	bool crossPortReadFirst = false;


	size_t numOutputRegisters = (rp.dedicatedReadLatencyRegisters.size() - 1 + 1)/2;
	size_t numAdditionalInputRegisters = rp.dedicatedReadLatencyRegisters.size() - 1 - numOutputRegisters;


	size_t maxWidth = rp.node->getBitWidth();
	size_t minWidth = rp.node->getBitWidth();
	if (hasWritePort) {
		auto &wp = memGrp->getWritePorts().front();
		maxWidth = std::max(maxWidth, wp.node->getBitWidth());
		minWidth = std::min(minWidth, wp.node->getBitWidth());
	}
	HCL_ASSERT(maxWidth == minWidth);
	size_t width = minWidth;

	//size_t maxDepth = memGrp->getMemory()->getSize() / minWidth;

	size_t maxDepth36k = 1ull << m_desc.addressBits;
	size_t maxDepth18k = maxDepth36k / 2;

	size_t numCascadesNeeded36k = (maxDepth + maxDepth18k-1) / maxDepth36k;
	size_t depthHandledBy36k = std::min(maxDepth, numCascadesNeeded36k * maxDepth36k);
	size_t addrWidth36k = utils::Log2C(depthHandledBy36k / numCascadesNeeded36k);


	size_t numCascadesNeeded18k = (maxDepth - depthHandledBy36k + maxDepth18k-1) / maxDepth18k;
	size_t depthHandledBy18k = std::min(maxDepth - depthHandledBy36k, numCascadesNeeded18k * maxDepth18k);
	size_t addrWidth18k = numCascadesNeeded18k > 0?utils::Log2C(depthHandledBy18k / numCascadesNeeded18k):0;


	size_t widthSingle36k;
	HCL_ASSERT(addrWidth36k <= 15);
	switch (addrWidth36k) {
		case 15: widthSingle36k = 1; break;
		case 14: widthSingle36k = 2; break;
		case 13: widthSingle36k = 4; break;
		case 12: widthSingle36k = 9; break;
		case 11: widthSingle36k = 18; break;
		default: widthSingle36k = 36; break;
	}

	size_t widthSingle18k;
	HCL_ASSERT(addrWidth18k <= 14);
	switch (addrWidth18k) {
		case 14: widthSingle18k = 1; break;
		case 13: widthSingle18k = 2; break;
		case 12: widthSingle18k = 4; break;
		case 11: widthSingle18k = 9; break;
		default: widthSingle18k = 18; break;
	}

	size_t num36kPerCascade = (minWidth + widthSingle36k-1) / widthSingle36k;
	size_t num18kPerCascade = (minWidth + widthSingle18k-1) / widthSingle18k;


	size_t totalAllocatedSize = numCascadesNeeded36k * num36kPerCascade * (36 << 10) + numCascadesNeeded18k * num18kPerCascade * (18 << 10);
	HCL_ASSERT(totalAllocatedSize >= memGrp->getMemory()->getSize());
	size_t overhead = totalAllocatedSize - memGrp->getMemory()->getSize();




	{
		Clock clock(readClock);

		ClockScope cscope(clock);
		for ([[maybe_unused]] auto i : utils::Range(numAdditionalInputRegisters)) {
			rdAddr = reg(rdAddr);
			attribute(rdAddr, {.allowFusing = false});
			rdEn = reg(rdEn);
			attribute(rdEn, {.allowFusing = false});

			if (hasWritePort) {
				wrAddr = reg(wrAddr);
				attribute(wrAddr, {.allowFusing = false});
				wrData = reg(wrData);
				attribute(wrData, {.allowFusing = false});
				wrEn = reg(wrEn);
				attribute(wrEn, {.allowFusing = false});
			}
		}
	}

	UInt readData = ConstUInt(BitWidth(width));

	for (auto cascade : utils::Range(numCascadesNeeded36k)) {
		Bit cascadeRdEnabled = rdEn;
		if (rdAddr.size() > addrWidth36k) {
			UInt addrHighBits = rdAddr(addrWidth36k, rdAddr.size()-addrWidth36k);
			cascadeRdEnabled &= addrHighBits == cascade;
			//cascadeRdEnabled &= rdAddr(Selection::From(addrWidth36k)) == cascade;
		}
		cascadeRdEnabled.setName((boost::format("rdEn_cascade_%d") % cascade).str());

		Bit cascadeWrEnabled;
		if (hasWritePort) {
			cascadeWrEnabled = wrEn;
			if (wrAddr.size() > addrWidth36k)
				cascadeWrEnabled &= wrAddr(Selection::From(addrWidth36k)) == cascade;

			cascadeWrEnabled.setName((boost::format("wrEn_cascade_%d") % cascade).str());
		}

		UInt cascadeReadData = ConstUInt(BitWidth(width));

		for (auto widthInst : utils::Range(num36kPerCascade)) {
			auto *bram = DesignScope::createNode<RAMB36E2>();
			bram->defaultInputs(false, hasWritePort);
			bram->setupClockDomains(RAMB36E2::ClockDomains::COMMON);

			RAMB36E2::PortSetup rdPortSetup = {
				.readWidth = widthSingle36k,
			};
			bram->setupPortA(rdPortSetup);
			bram->connectInput(RAMB36E2::Inputs::IN_ADDR_A_RDADDR, rdAddr(0, addrWidth36k));
			bram->connectInput(RAMB36E2::Inputs::IN_EN_A_RD_EN, cascadeRdEnabled);
			bram->attachClock(readClock, (size_t)RAMB36E2::Clocks::CLK_A_RD);   

			cascadeReadData(Selection::Symbol(widthInst, BitWidth(widthSingle36k))) = bram->getReadDataPortA(widthSingle36k);

			if (hasWritePort) {
				RAMB36E2::PortSetup wrPortSetup = {
					.writeMode = (crossPortReadFirst?RAMB36E2::WriteMode::READ_FIRST:RAMB36E2::WriteMode::NO_CHANGE),
					.writeWidth = widthSingle36k,
				};
				bram->setupPortB(wrPortSetup);
				bram->connectInput(RAMB36E2::Inputs::IN_ADDR_B_WRADDR, wrAddr(0, addrWidth36k));
				bram->connectInput(RAMB36E2::Inputs::IN_EN_B_WR_EN, cascadeWrEnabled);
				bram->connectWriteDataPortB(wrData(Selection::Symbol(widthInst, BitWidth(widthSingle36k))));
				bram->attachClock(writeClock, (size_t)RAMB36E2::Clocks::CLK_B_WR);   
			}
		}

		IF (cascadeRdEnabled)
			readData = cascadeReadData;
	}

	{
		Clock clock(readClock);

		ClockScope cscope(clock);
		for ([[maybe_unused]] auto i : utils::Range(numOutputRegisters)) {
			readData = reg(readData);
			attribute(readData, {.allowFusing = false});
		}
	}

	rdDataHook.exportOverride(readData);

	nodeGroup->properties()["memory_type"] = m_desc.memoryName;
	return true;
}
#endif

/*

pro addr bit 2 cycles more (one front, one back).

*/


}
