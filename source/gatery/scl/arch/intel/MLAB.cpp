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

}

bool MLAB::apply(hlim::NodeGroup *nodeGroup) const
{
	auto *memGrp = dynamic_cast<hlim::MemoryGroup*>(nodeGroup->getMetaInfo());
	if (memGrp == nullptr) return false;
	if (memGrp->getMemory()->type() == hlim::Node_Memory::MemType::EXTERNAL)
		return false;

	if (memGrp->getReadPorts().size() != 1) return false;
    const auto &rp = memGrp->getReadPorts().front();
	if (memGrp->getWritePorts().size() > 1) return false;
    if (memGrp->getMemory()->getRequiredReadLatency() > 1) return false;

	if (memGrp->getWritePorts().size() > 1) return false;

    for (auto reg : rp.dedicatedReadLatencyRegisters) {
        if (reg->hasResetValue()) return false;
        if (reg->hasEnable()) return false;
		if (memGrp->getWritePorts().size() > 0 && memGrp->getWritePorts().front().node->getClocks()[0] != reg->getClocks()[0]) return false;
		if (rp.dedicatedReadLatencyRegisters.front()->getClocks()[0] != reg->getClocks()[0]) return false;
    }

    size_t width = rp.node->getBitWidth();
    size_t depth = memGrp->getMemory()->getSize()/width;
    size_t addrBits = utils::Log2C(depth);

	bool supportsWriteFirst = memGrp->getMemory()->getRequiredReadLatency() == 1;

    if (memGrp->getWritePorts().size() != 0 && !supportsWriteFirst) return false; // Todo: Needs extra handling

    auto &circuit = DesignScope::get()->getCircuit();

	memGrp->convertToReadBeforeWrite(circuit);
    memGrp->attemptRegisterRetiming(circuit);
    memGrp->resolveWriteOrder(circuit);
    memGrp->updateNoConflictsAttrib();
    memGrp->buildReset(circuit);
    memGrp->bypassSignalNodes();
    memGrp->verify();

    GroupScope scope(memGrp->lazyCreateFixupNodeGroup());

    auto *altdpram = DesignScope::createNode<ALTDPRAM>(width, depth);

    altdpram->setupRamType(m_desc.memoryName);
    altdpram->setupSimulationDeviceFamily(m_intelDevice.getFamily());
   
    bool readFirst = false;
    bool writeFirst = false;
    if (memGrp->getWritePorts().size() > 1) {
        auto &wp = memGrp->getWritePorts().front();
        if (wp.node->isOrderedBefore(rp.node.get()))
            writeFirst = true;
        if (rp.node->isOrderedBefore(wp.node.get()))
            readFirst = true;
    }


	altdpram->setupMixedPortRdw(ALTDPRAM::RDWBehavior::NEW_DATA_MASKED_UNDEFINED);


    if (memGrp->getWritePorts().size() > 0) {
        auto &wp = memGrp->getWritePorts().front();
        ALTDPRAM::PortSetup portSetup;
		portSetup.inputRegs = true;
        altdpram->setupWritePort(portSetup);

        BVec wrData = hookBVecBefore({.node = wp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::wrData});
        BVec addr = hookBVecBefore({.node = wp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::address});
        Bit wrEn = hookBitBefore({.node = wp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::wrEnable});

        altdpram->connectInput(ALTDPRAM::Inputs::IN_DATA, wrData);
        altdpram->connectInput(ALTDPRAM::Inputs::IN_WRADDRESS, addr(0, addrBits));
        altdpram->connectInput(ALTDPRAM::Inputs::IN_WREN, wrEn);

        altdpram->attachClock(wp.node->getClocks()[0], (size_t)ALTDPRAM::Clocks::INCLOCK);
    }

    {
        ALTDPRAM::PortSetup portSetup;
        portSetup.outputRegs = rp.dedicatedReadLatencyRegisters.size() > 0;
        altdpram->setupReadPort(portSetup);

        BVec addr = hookBVecBefore({.node = rp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::address});
        BVec data = hookBVecAfter(rp.dataOutput);

        altdpram->connectInput(ALTDPRAM::Inputs::IN_RDADDRESS, addr(0, addrBits));
        data.setExportOverride(altdpram->getOutputBVec(ALTDPRAM::Outputs::OUT_Q));

        altdpram->attachClock(rp.dedicatedReadLatencyRegisters.front()->getClocks()[0], (size_t)ALTDPRAM::Clocks::OUTCLOCK);
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