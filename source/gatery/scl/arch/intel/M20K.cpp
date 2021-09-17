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

#include "M20K.h"
#include "IntelDevice.h"

#include "ALTSYNCRAM.h"
#include <gatery/hlim/postprocessing/MemoryDetector.h>
#include <gatery/hlim/supportNodes/Node_MemPort.h>

namespace gtry::scl::arch::intel {

M20K::M20K(const IntelDevice &intelDevice) : m_intelDevice(intelDevice)
{
	m_desc.memoryName = "M20K";
	m_desc.sizeCategory = MemoryCapabilities::SizeCategory::MEDIUM;
	m_desc.inputRegs = true;
	m_desc.outputRegs = 0;

	// Embedded Memory User Guide "Table 3. Embedded Memory Blocks in Intel FPGA Devices"
	m_desc.size = 20 << 10;
	
	// Embedded Memory User Guide "Table 6. Valid Range of Maximum Block Depth for Various Embedded Memory Blocks"
	m_desc.addressBits = 14; // 16384
}

bool M20K::apply(hlim::NodeGroup *nodeGroup) const
{
	auto *memGrp = dynamic_cast<hlim::MemoryGroup*>(nodeGroup->getMetaInfo());
	if (memGrp == nullptr) return false;
	if (memGrp->getMemory()->type() == hlim::Node_Memory::MemType::EXTERNAL)
		return false;

	if (memGrp->getReadPorts().size() != 1) return false;
	const auto &rp = memGrp->getReadPorts().front();

	if (memGrp->getWritePorts().size() > 1) return false;
	if (memGrp->getMemory()->getRequiredReadLatency() == 0) return false;


	auto &circuit = DesignScope::get()->getCircuit();

	memGrp->convertToReadBeforeWrite(circuit);
	memGrp->attemptRegisterRetiming(circuit);

	auto *readClock = rp.dedicatedReadLatencyRegisters.front()->getClocks()[0];

	for (auto reg : rp.dedicatedReadLatencyRegisters) {
		if (reg->hasResetValue()) return false;
		if (reg->hasEnable()) return false;

		// For now, no true dual port, so only single clock
		if (memGrp->getWritePorts().size() > 0 && memGrp->getWritePorts().front().node->getClocks()[0] != reg->getClocks()[0]) return false;
		if (readClock != reg->getClocks()[0]) return false;        
	}


	memGrp->resolveWriteOrder(circuit);
	memGrp->updateNoConflictsAttrib();
	memGrp->buildReset(circuit);
	memGrp->bypassSignalNodes();
	memGrp->verify();

	auto *altsyncram = DesignScope::createNode<ALTSYNCRAM>(memGrp->getMemory()->getSize());

	if (memGrp->getWritePorts().size() == 0)
		altsyncram->setupROM();
	else
		altsyncram->setupSimpleDualPort();

	altsyncram->setupRamType(m_desc.memoryName);
	altsyncram->setupSimulationDeviceFamily(m_intelDevice.getFamily());
   
	bool readFirst = false;
	bool writeFirst = false;
	if (memGrp->getWritePorts().size() > 1) {
		auto &wp = memGrp->getWritePorts().front();
		if (wp.node->isOrderedBefore(rp.node.get()))
			writeFirst = true;
		if (rp.node->isOrderedBefore(wp.node.get()))
			readFirst = true;
	}

	if (readFirst)
		altsyncram->setupMixedPortRdw(ALTSYNCRAM::RDWBehavior::OLD_DATA);
	else if (writeFirst)
		altsyncram->setupMixedPortRdw(ALTSYNCRAM::RDWBehavior::NEW_DATA_MASKED_UNDEFINED);
	else
		altsyncram->setupMixedPortRdw(ALTSYNCRAM::RDWBehavior::DONT_CARE);


	bool useInternalOutputRegister = false;

	size_t numExternalOutputRegisters = rp.dedicatedReadLatencyRegisters.size()-1;
	if (useInternalOutputRegister) 
		numExternalOutputRegisters--;

	if (memGrp->getWritePorts().size() > 0) {
		auto &wp = memGrp->getWritePorts().front();
		ALTSYNCRAM::PortSetup portSetup;
		portSetup.inputRegs = true;
		altsyncram->setupPortA(wp.node->getBitWidth(), portSetup);

		BVec wrData = getBVecBefore({.node = wp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::wrData});
		BVec addr = getBVecBefore({.node = wp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::address});
		Bit wrEn = getBitBefore({.node = wp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::wrEnable}, '1');

		altsyncram->connectInput(ALTSYNCRAM::Inputs::IN_DATA_A, wrData);
		altsyncram->connectInput(ALTSYNCRAM::Inputs::IN_ADDRESS_A, addr);
		altsyncram->connectInput(ALTSYNCRAM::Inputs::IN_WREN_A, wrEn);

		altsyncram->attachClock(wp.node->getClocks()[0], (size_t)ALTSYNCRAM::Clocks::CLK_0);

		{
			ALTSYNCRAM::PortSetup portSetup;
			portSetup.inputRegs = true;
			portSetup.outputRegs = (rp.dedicatedReadLatencyRegisters.size() > 1) && useInternalOutputRegister;
			altsyncram->setupPortB(rp.node->getBitWidth(), portSetup);

			BVec addr = getBVecBefore({.node = rp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::address});
			BVec data = hookBVecAfter(rp.dataOutput);

			altsyncram->connectInput(ALTSYNCRAM::Inputs::IN_ADDRESS_B, addr);

			BVec readData = altsyncram->getOutputBVec(ALTSYNCRAM::Outputs::OUT_Q_B);

			{
				Clock clock(readClock);
				ClockScope cscope(clock);
				for (auto i : utils::Range(numExternalOutputRegisters)) 
					if (useInternalOutputRegister)
						readData = reg(readData);
					else
						readData = reg(readData, 0); // reset first register to prevent M20K merge. reset others to prevent ALM split.
			}
			data.setExportOverride(readData);

			altsyncram->attachClock(readClock, (size_t)ALTSYNCRAM::Clocks::CLK_0);
		}        
	} else {
		ALTSYNCRAM::PortSetup portSetup;
		portSetup.inputRegs = true;
		portSetup.outputRegs = (rp.dedicatedReadLatencyRegisters.size() > 1) && useInternalOutputRegister;
		altsyncram->setupPortA(rp.node->getBitWidth(), portSetup);

		BVec addr = getBVecBefore({.node = rp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::address});
		BVec data = hookBVecAfter(rp.dataOutput);

		altsyncram->connectInput(ALTSYNCRAM::Inputs::IN_ADDRESS_A, addr);
		BVec readData = altsyncram->getOutputBVec(ALTSYNCRAM::Outputs::OUT_Q_A);
		{
			Clock clock(readClock);
			ClockScope cscope(clock);
			for (auto i : utils::Range(numExternalOutputRegisters)) 
				if (useInternalOutputRegister)
					readData = reg(readData);
				else
					readData = reg(readData, 0); // reset first register to prevent M20K merge. reset others to prevent ALM split.
		}
		data.setExportOverride(readData);

		altsyncram->attachClock(readClock, (size_t)ALTSYNCRAM::Clocks::CLK_0);    
	}

	return true;
}



#if 0

GenericMemoryDesc buildM20KDesc(M20KVariants variant)
{
	GenericMemoryDesc res;

	// Embedded Memory User Guide "Table 3. Embedded Memory Blocks in Intel FPGA Devices"
	const size_t size = 20 << 10;

	res.memoryName = "M20K";
	res.sizeCategory = MemoryCapabilities::SizeCategory::MEDIUM;

	// Embedded Memory User Guide "Table 6. Valid Range of Maximum Block Depth for Various Embedded Memory Blocks"
	for (size_t i = 512; i <= 16384; i *= 2) {
		res.sizeConfigs.push_back({
			.width = size/i,
			.depth = i,
		});
	}	

	// Embedded Memory User Guide "3.9. Byte Enable"
	res.byteEnableByteWidths = {8, 9};
	// Educated guess, based on Embedded Memory User Guide "3.5. Mixed-width Ratio Configuration" and Intel Agilex Embedded Memory User Guide "Table 47. Device Family Support for Width Ratios"
	res.mixedWidthRatios = {1, 2, 4, 8, 16, 32};

	// See Embedded Memory User Guide "Table 5. Supported Mixed-Width Ratio Configurations for Intel Arria 10"
	switch (variant) {
		case M20KVariants::Arria10_SDP_woBE:
			res.byteEnableByteWidths = {};
		break;
		case M20KVariants::Arria10_TDP_woBE:
			res.byteEnableByteWidths = {};
			res.mixedWidthRatios = {1, 2, 4, 8, 16};
		break;
		case M20KVariants::Arria10_SDP_wBE:
			res.mixedWidthRatios = {1, 2, 4};
		break;
		case M20KVariants::Arria10_TDP_wBE:
			res.mixedWidthRatios = {1, 2};
		break;
	}


	res.numWritePorts = 0;
	res.numReadPorts = 0;
	res.numReadWritePorts = 2;

	res.portsCanDisable = true;
	res.portsMustShareClocks = false;

	res.samePortReadDuringWrite
		.insert(GenericMemoryDesc::ReadDuringWriteBehavior::READ_FIRST)
		.insert(GenericMemoryDesc::ReadDuringWriteBehavior::WRITE_FIRST)
		.insert(GenericMemoryDesc::ReadDuringWriteBehavior::READ_UNDEFINED);
	res.crossPortReadDuringWrite
		.insert(GenericMemoryDesc::ReadDuringWriteBehavior::READ_FIRST)
		.insert(GenericMemoryDesc::ReadDuringWriteBehavior::READ_UNDEFINED);

	res.readAddrRegister
		.insert(GenericMemoryDesc::RegisterFlags::EXISTS)
		.insert(GenericMemoryDesc::RegisterFlags::CAN_RESET)
		.insert(GenericMemoryDesc::RegisterFlags::CAN_STALL);
	res.dataOutputRegisters
		.insert(GenericMemoryDesc::RegisterFlags::EXISTS)
		.insert(GenericMemoryDesc::RegisterFlags::OPTIONAL)
		.insert(GenericMemoryDesc::RegisterFlags::CAN_RESET)
		.insert(GenericMemoryDesc::RegisterFlags::CAN_STALL);
	res.readLatencies = {1, 2};

	res.costPerUnitSize = 1000;
	res.unitSize = size;

	return res;
}

#endif

}
