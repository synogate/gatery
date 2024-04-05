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

#include "eSRAM.h"
#include "IntelDevice.h"

#include "ALTSYNCRAM.h"

#include <gatery/frontend.h>

#include <gatery/hlim/postprocessing/MemoryDetector.h>
#include <gatery/hlim/supportNodes/Node_MemPort.h>
#include <gatery/hlim/supportNodes/Node_Memory.h>
#include <gatery/hlim/coreNodes/Node_Register.h>

namespace gtry::scl::arch::intel {

eSRAM::eSRAM(const IntelDevice &intelDevice) : m_intelDevice(intelDevice)
{
	m_desc.sizeCategory = MemoryCapabilities::SizeCategory::LARGE;
	m_desc.inputRegs = true;
	m_desc.outputRegs = 6+2;

	m_desc.memoryName = "eSRAM";

/*
	// Intel Agilex Embedded Memory User Guide "Table 1. Intel Agilex Embedded Memory Featuress"
	m_desc.size = 18 << 20;
	
	// Intel Agilex Embedded Memory User Guide "Table 9. Supported Embedded Memory Block Configurations"
	m_desc.addressBits = 10; // 1024
*/	
	m_desc.size = 32 * (64 << 10);
	m_desc.addressBits = 15; // 32 * 1024
}

bool eSRAM::apply(hlim::NodeGroup *nodeGroup) const
{
	auto *memGrp = dynamic_cast<hlim::MemoryGroup*>(nodeGroup->getMetaInfo());
	if (memGrp == nullptr) return false;
	if (memGrp->getMemory()->type() == hlim::Node_Memory::MemType::EXTERNAL)
		return false;

	// eSRAM only supports simple dual port
	// eSRAM can not be initialized, so ROM doesn't make sense
	if (memGrp->getMemory()->isROM()) return false;

	if (memGrp->getReadPorts().size() != 1) return false;
	const auto &rp = memGrp->getReadPorts().front();

	if (memGrp->getWritePorts().size() != 1) return false;
	const auto &wp = memGrp->getWritePorts().front();
	if (memGrp->getMemory()->getRequiredReadLatency() < 1+m_desc.outputRegs) return false;

	// eSRAM can not be initialized
	if (sim::anyDefined(memGrp->getMemory()->getPowerOnState()) && wp.node->getClocks()[0]->getRegAttribs().initializeMemory) return false;


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

	// eSRAM can only do write first or dont_care
	HCL_ASSERT(!readFirst);
	HCL_ASSERT(!writeFirst);
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

		UInt wrData = (UInt) getBVecBefore({.node = wp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::wrData});
		UInt addr = (UInt) getBVecBefore({.node = wp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::address});
		Bit wrEn = getBitBefore({.node = wp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::wrEnable}, '1');

		altsyncram->setInput(ALTSYNCRAM::Inputs::IN_DATA_A, (BVec) wrData);
		altsyncram->setInput(ALTSYNCRAM::Inputs::IN_ADDRESS_A, (BVec) addr);
		altsyncram->setInput(ALTSYNCRAM::Inputs::IN_WREN_A, wrEn);

		altsyncram->attachClock(wp.node->getClocks()[0], (size_t)ALTSYNCRAM::Clocks::CLK_0);

		{
			ALTSYNCRAM::PortSetup portSetup;
			portSetup.inputRegs = true;
			portSetup.outputRegs = (rp.dedicatedReadLatencyRegisters.size() > 1) && useInternalOutputRegister;
			altsyncram->setupPortB(rp.node->getBitWidth(), portSetup);

			UInt addr = (UInt) getBVecBefore({.node = rp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::address});
			BVec data = hookBVecAfter(rp.dataOutput);

			altsyncram->setInput(ALTSYNCRAM::Inputs::IN_ADDRESS_B, (BVec) addr);

			UInt readData = (UInt) altsyncram->getOutputBVec(ALTSYNCRAM::Outputs::OUT_Q_B);

			{
				Clock clock(readClock);
				ClockScope cscope(clock);
				for ([[maybe_unused]] auto i : utils::Range(numExternalOutputRegisters)) 
					readData = reg(readData);
			}
			data.exportOverride((BVec)readData);

			altsyncram->attachClock(readClock, (size_t)ALTSYNCRAM::Clocks::CLK_0);
		}		
	} else {
		ALTSYNCRAM::PortSetup portSetup;
		portSetup.inputRegs = true;
		portSetup.outputRegs = (rp.dedicatedReadLatencyRegisters.size() > 1) && useInternalOutputRegister;
		altsyncram->setupPortA(rp.node->getBitWidth(), portSetup);

		UInt addr = (UInt) getBVecBefore({.node = rp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::address});
		BVec data = hookBVecAfter(rp.dataOutput);

		altsyncram->setInput(ALTSYNCRAM::Inputs::IN_ADDRESS_A, (BVec) addr);
		BVec readData = altsyncram->getOutputBVec(ALTSYNCRAM::Outputs::OUT_Q_A);
		{
			Clock clock(readClock);
			ClockScope cscope(clock);
			for ([[maybe_unused]] auto i : utils::Range(numExternalOutputRegisters)) 
				readData = reg(readData);
		}
		data.exportOverride(readData);

		altsyncram->attachClock(readClock, (size_t)ALTSYNCRAM::Clocks::CLK_0);	
	}

	return true;
}



}
