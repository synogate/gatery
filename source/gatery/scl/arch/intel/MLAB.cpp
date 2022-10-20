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

#include "MLAB.h"
#include "IntelDevice.h"

#include "ALTDPRAM.h"
#include <gatery/hlim/postprocessing/MemoryDetector.h>
#include <gatery/hlim/supportNodes/Node_MemPort.h>
#include <gatery/hlim/coreNodes/Node_Register.h>

namespace gtry::scl::arch::intel {

MLAB::MLAB(const IntelDevice &intelDevice) : m_intelDevice(intelDevice)
{
	m_desc.memoryName = "MLAB";
	m_desc.sizeCategory = MemoryCapabilities::SizeCategory::SMALL;
	m_desc.inputRegs = false;
	m_desc.outputRegs = 1;

	// Embedded Memory User Guide "Table 3. Embedded Memory Blocks in Intel FPGA Devices"
	m_desc.size = 640;

	// Embedded Memory User Guide "Table 6. Valid Range of Maximum Block Depth for Various Embedded Memory Blocks"
	m_desc.addressBits = 8; // 64
//	if (m_intelDevice.getFamily() == "Arria 5" || m_intelDevice.getFamily() == "Cyclone 5" || m_intelDevice.getFamily() == "Stratix 10")
//		m_desc.addressBits = 4; // 32

	m_desc.supportsDualClock = false; // not yet implemented
	m_desc.supportsPowerOnInitialization = true;
}

bool MLAB::apply(hlim::NodeGroup *nodeGroup) const
{
	auto *memGrp = dynamic_cast<hlim::MemoryGroup*>(nodeGroup->getMetaInfo());
	if (memGrp == nullptr) return false;
	if (memGrp->getMemory()->type() == hlim::Node_Memory::MemType::EXTERNAL) {
		dbg::log(dbg::LogMessage() << dbg::LogMessage::LOG_WARNING << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING << "Will not apply memory primitive " << getDesc().memoryName << " to " << memGrp->getMemory() 
				<< " because it is external memory.");
		return false;
	}
	if (memGrp->getReadPorts().size() == 0) {
		dbg::log(dbg::LogMessage() << dbg::LogMessage::LOG_WARNING << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING << 
				"Will not apply memory primitive " << getDesc().memoryName << " to " << memGrp->getMemory() 
				<< " because it has no read ports.");
		return false;
	}
	if (memGrp->getReadPorts().size() > 1) {
		dbg::log(dbg::LogMessage() << dbg::LogMessage::LOG_WARNING << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING << 
				"Will not apply memory primitive " << getDesc().memoryName << " to " << memGrp->getMemory() 
				<< " because it has more than one read port and so far only one read port is supported.");
		return false;
	}
	const auto &rp = memGrp->getReadPorts().front();
	if (memGrp->getWritePorts().size() > 1) {
		dbg::log(dbg::LogMessage() << dbg::LogMessage::LOG_WARNING << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING << 
				"Will not apply memory primitive " << getDesc().memoryName << " to " << memGrp->getMemory() 
				<< " because it has more than one write port and so far only one write port is supported.");

		return false;
	}

	size_t width = rp.node->getBitWidth();
	size_t depth = memGrp->getMemory()->getSize()/width;
	BitWidth addrBits = BitWidth::count(depth);


	/*

		We need the new-data rdw mode because an async read is expected to return data
		written on the last cycle which, due to the write inputs being registered is
		actually the current cycle. (We could build a bypass for this)

		This new-data mode is only possible if the output register is used, presumably because 
		this is precisely what allows quartus to time the read correctly in relation to the write.

		This only applies for single-clock configurations, multi-clock is more complicated.

		Fow now, only allow writes if we have that read register to work with.
	*/

	bool supportsWriteFirst = memGrp->getMemory()->getRequiredReadLatency() >= 1;

	if (memGrp->getWritePorts().size() != 0 && !supportsWriteFirst)  {
		dbg::log(dbg::LogMessage() << dbg::LogMessage::LOG_WARNING << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING << 
				"Will not apply memory primitive " << getDesc().memoryName << " to " << memGrp->getMemory() 
				<< " because automatic building of read during write bypasses for MLABs is not yet implemented.");

		return false; // Todo: Needs extra handling
	}

	auto &circuit = DesignScope::get()->getCircuit();

	memGrp->convertToReadBeforeWrite(circuit);
	memGrp->attemptRegisterRetiming(circuit);

	hlim::Clock *readClock = nullptr;
	if (!rp.dedicatedReadLatencyRegisters.empty())
		readClock = rp.dedicatedReadLatencyRegisters.front()->getClocks()[0];

	hlim::Clock *writeClock = nullptr;
	if (!memGrp->getWritePorts().empty())
		writeClock = memGrp->getWritePorts().front().node->getClocks()[0];


	for (auto reg : rp.dedicatedReadLatencyRegisters) {
		if (reg->hasResetValue()) {
			// actually for mlabs, the output register is cleared to zero. If we check for zero reset values, we can relax this.

			dbg::log(dbg::LogMessage() << dbg::LogMessage::LOG_WARNING << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING << 
					"Will not apply memory primitive " << getDesc().memoryName << " to " << memGrp->getMemory() 
					<< " because one of its output registers has a reset value.");
			return false;
		}
		if (reg->hasEnable()) {
			dbg::log(dbg::LogMessage() << dbg::LogMessage::LOG_WARNING << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING << 
					"Will not apply memory primitive " << getDesc().memoryName << " to " << memGrp->getMemory() 
					<< " because one of its output registers has an enable.");
			return false;
		}
		if (readClock != reg->getClocks()[0]) {
			dbg::log(dbg::LogMessage() << dbg::LogMessage::LOG_WARNING << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING << 
					"Will not apply memory primitive " << getDesc().memoryName << " to " << memGrp->getMemory() 
					<< " because its output registers have differing clocks.");
			return false;
		}
	}

	if (readClock != nullptr) {
		if (readClock->getTriggerEvent() != hlim::Clock::TriggerEvent::RISING) {
			dbg::log(dbg::LogMessage() << dbg::LogMessage::LOG_WARNING << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING << 
					"Will not apply memory primitive " << getDesc().memoryName << " to " << memGrp->getMemory() 
					<< " because its read clock is not triggering on rising clock edges.");
			return false;
		}
	}

	if (writeClock != nullptr) {
		if (writeClock->getTriggerEvent() != hlim::Clock::TriggerEvent::RISING) {
			dbg::log(dbg::LogMessage() << dbg::LogMessage::LOG_WARNING << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING << 
					"Will not apply memory primitive " << getDesc().memoryName << " to " << memGrp->getMemory() 
					<< " because its write clock is not triggering on rising clock edges.");
			return false;
		}
	}

	if (writeClock != readClock) {
		dbg::log(dbg::LogMessage() << dbg::LogMessage::LOG_WARNING << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING << 
				"Will not apply memory primitive " << getDesc().memoryName << " to " << memGrp->getMemory() 
				<< " because differing read and write clocks are not yet supported.");
		return false;
	}

	memGrp->resolveWriteOrder(circuit);
	memGrp->updateNoConflictsAttrib();
	memGrp->buildReset(circuit);
	memGrp->bypassSignalNodes();
	memGrp->verify();

	auto *altdpram = DesignScope::createNode<ALTDPRAM>(width, depth);
	altdpram->setInitialization(memGrp->getMemory()->getPowerOnState());

	altdpram->setupRamType(m_desc.memoryName);
	altdpram->setupSimulationDeviceFamily(m_intelDevice.getFamily());
   
	bool readFirst = false;
	bool writeFirst = false;
	if (memGrp->getWritePorts().size() > 0) {
		auto &wp = memGrp->getWritePorts().front();
		if (wp.node->isOrderedBefore(rp.node.get()))
			writeFirst = true;
		if (rp.node->isOrderedBefore(wp.node.get()))
			readFirst = true;
	}

	HCL_ASSERT(!writeFirst);
	altdpram->setupMixedPortRdw(ALTDPRAM::RDWBehavior::NEW_DATA_MASKED_UNDEFINED);


	if (memGrp->getWritePorts().size() > 0) {
		auto &wp = memGrp->getWritePorts().front();
		ALTDPRAM::PortSetup portSetup;
		portSetup.inputRegs = true;
		altdpram->setupWritePort(portSetup);

		UInt wrData = (UInt) getBVecBefore({.node = wp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::wrData});
		UInt addr = (UInt) getBVecBefore({.node = wp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::address});
		Bit wrEn = getBitBefore({.node = wp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::wrEnable}, '1');

		altdpram->setInput(ALTDPRAM::Inputs::IN_DATA, (BVec) wrData);
		altdpram->setInput(ALTDPRAM::Inputs::IN_WRADDRESS, (BVec) addr(0, addrBits));
		altdpram->setInput(ALTDPRAM::Inputs::IN_WREN, wrEn);

		altdpram->attachClock(writeClock, (size_t)ALTDPRAM::Clocks::INCLOCK);

		auto wrWordEnableSignal = wp.node->getNonSignalDriver((size_t)hlim::Node_MemPort::Inputs::wrWordEnable);
		if (wrWordEnableSignal.node != nullptr)
			altdpram->setInput(ALTDPRAM::Inputs::IN_BYTEENA, getBVecBefore({.node = wp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::wrWordEnable}));
	}

	{
		ALTDPRAM::PortSetup portSetup;
		portSetup.outputRegs = rp.dedicatedReadLatencyRegisters.size() > 0;
		size_t numExternalOutputRegisters = rp.dedicatedReadLatencyRegisters.size()-1;
		altdpram->setupReadPort(portSetup);

		UInt addr = (UInt) getBVecBefore({.node = rp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::address});
		BVec data = hookBVecAfter(rp.dataOutput);

		altdpram->setInput(ALTDPRAM::Inputs::IN_RDADDRESS, (BVec) addr(0, addrBits));

		BVec readData = altdpram->getOutputBVec(ALTDPRAM::Outputs::OUT_Q);
		{
			Clock clock(readClock);
			ClockScope cscope(clock);
			for ([[maybe_unused]] auto i : utils::Range(numExternalOutputRegisters)) 
				readData = reg(readData);
		}
		data.exportOverride(readData);

		if (portSetup.outputRegs)
			altdpram->attachClock(readClock, (size_t)ALTDPRAM::Clocks::OUTCLOCK);
	}
	
	return true;
}




#if 0
GenericMemoryDesc buildMLABDesc(MLABVariants variant, bool withOutputReg)
{
	GenericMemoryDesc res;

	// Embedded Memory User Guide "Table 3. Embedded Memory Blocks in Intel FPGA Devices"
	const size_t size = 640;

	res.memoryName = "MLAB";
	res.sizeCategory = MemoryCapabilities::SizeCategory::SMALL;

	// Embedded Memory User Guide "Table 6. Valid Range of Maximum Block Depth for Various Embedded Memory Blocks"
	size_t maxDepth = 64;
	if (variant == MLABVariants::ArriaV || variant == MLABVariants::CycloneV)
		maxDepth = 32;
	for (size_t i = 32; i <= 64; i *= 2) {
		res.sizeConfigs.push_back({
			.width = size/i,
			.depth = i,
		});
	}	
	// NOTE: MLABs can't do mixed width ports!

	// See Embedded Memory User Guide "Table 5. Supported Mixed-Width Ratio Configurations for Intel Arria 10"
	// need to check this, note that on arria 10 it changes the possible port aspect ratios.
	// Embedded Memory User Guide "3.9. Byte Enable"
	res.byteEnableByteWidths = {5, 8, 9, 10};
	
	res.numWritePorts = 1;
	res.numReadPorts = 1;
	res.numReadWritePorts = 0;

	res.portsCanDisable = false;
	res.portsMustShareClocks = true;

	if (withOutputReg) {
		res.crossPortReadDuringWrite
			.insert(GenericMemoryDesc::ReadDuringWriteBehavior::MUST_NOT_HAPPEN);
	} else {
		res.crossPortReadDuringWrite
			.insert(GenericMemoryDesc::ReadDuringWriteBehavior::READ_UNDEFINED);
	}

	if (withOutputReg) {
		res.readLatencies = {0};
	} else {
		res.dataOutputRegisters
			.insert(GenericMemoryDesc::RegisterFlags::EXISTS);
			//.insert(GenericMemoryDesc::RegisterFlags::CAN_RESET) // unsure about these
			//.insert(GenericMemoryDesc::RegisterFlags::CAN_STALL); // unsure about these
		res.readLatencies = {1};
	}

	res.costPerUnitSize = 250; // about 1/4th of a block ram since there are ~ 4x more lutrams
	res.unitSize = size;

	return res;
}

#endif

}
