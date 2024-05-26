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

#include "IntelBlockram.h"
#include "IntelDevice.h"

#include "ALTSYNCRAM.h"
#include <gatery/hlim/postprocessing/MemoryDetector.h>
#include <gatery/hlim/supportNodes/Node_MemPort.h>
#include <gatery/hlim/supportNodes/Node_Memory.h>
#include <gatery/hlim/coreNodes/Node_Register.h>

#include <gatery/frontend.h>
#include <gatery/debug/DebugInterface.h>

namespace gtry::scl::arch::intel {

IntelBlockram::IntelBlockram(const IntelDevice &intelDevice) : m_intelDevice(intelDevice)
{
	m_desc.sizeCategory = MemoryCapabilities::SizeCategory::MEDIUM;
	m_desc.inputRegs = true;
	m_desc.outputRegs = 0;
	m_desc.supportsDualClock = true;
	m_desc.supportsPowerOnInitialization = true;
}

bool IntelBlockram::apply(hlim::NodeGroup *nodeGroup) const
{
	return false;

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


	auto &circuit = DesignScope::get()->getCircuit();
//DesignScope::get()->visualize(std::string("intelBRam")+std::to_string(__LINE__));
	memGrp->convertToReadBeforeWrite(circuit);
//DesignScope::get()->visualize(std::string("intelBRam")+std::to_string(__LINE__));
	memGrp->attemptRegisterRetiming(circuit);
//DesignScope::get()->visualize(std::string("intelBRam")+std::to_string(__LINE__));

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

	hlim::NodePort readEnable;
	if (rp.dedicatedReadLatencyRegisters.front()->hasEnable())
		readEnable = rp.dedicatedReadLatencyRegisters.front()->getDriver((size_t)hlim::Node_Register::Input::ENABLE);


	for (auto reg : rp.dedicatedReadLatencyRegisters) {
		if (reg->hasResetValue()) {
			dbg::log(dbg::LogMessage(nodeGroup) << dbg::LogMessage::LOG_WARNING << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING << 
					"Will not apply memory primitive " << getDesc().memoryName << " to " << memGrp->getMemory() 
					<< " because one of its output registers has a reset value.");
			return false;
		}

		if (readClock != reg->getClocks()[0]) {
			dbg::log(dbg::LogMessage(nodeGroup) << dbg::LogMessage::LOG_WARNING << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING << 
					"Will not apply memory primitive " << getDesc().memoryName << " to " << memGrp->getMemory() 
					<< " because its output registers have differing clocks.");
			return false;
		}
	}

	bool readFirst = false;
	bool writeFirst = false;
	if (memGrp->getWritePorts().size() > 0) {
		auto &wp = memGrp->getWritePorts().front();
		if (wp.node->isOrderedBefore(rp.node.get()))
			writeFirst = true;
		if (rp.node->isOrderedBefore(wp.node.get()))
			readFirst = true;
	}

	bool isDualClock = memGrp->getWritePorts().size() > 0 && writeClock != readClock;

	if (isDualClock && (readFirst || writeFirst)) {
		dbg::log(dbg::LogMessage(nodeGroup) << dbg::LogMessage::LOG_WARNING << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING << 
				"Will not apply memory primitive " << getDesc().memoryName << " to " << memGrp->getMemory() 
				<< " because explicit read during write behavior for dual clock rams is not supported yet.");
		return false;
	}


	memGrp->resolveWriteOrder(circuit);
	memGrp->updateNoConflictsAttrib();
	memGrp->buildReset(circuit);
	memGrp->bypassSignalNodes();
	memGrp->verify();
//DesignScope::get()->visualize(std::string("intelBRam")+std::to_string(__LINE__));

	auto *altsyncram = DesignScope::createNode<ALTSYNCRAM>(memGrp->getMemory()->getSize());
	if (memGrp->getMemory()->requiresPowerOnInitialization())
		altsyncram->setInitialization(memGrp->getMemory()->getPowerOnState());

	if (memGrp->getWritePorts().size() == 0)
		altsyncram->setupROM();
	else
		altsyncram->setupSimpleDualPort();

	altsyncram->setupRamType(m_desc.memoryName);
	altsyncram->setupSimulationDeviceFamily(m_intelDevice.getFamily());

	if (readFirst)
		altsyncram->setupMixedPortRdw(ALTSYNCRAM::RDWBehavior::OLD_DATA);
	else if (writeFirst) {
		HCL_ASSERT_HINT(false, "Intel BRAMs do not support write-first!");
		altsyncram->setupMixedPortRdw(ALTSYNCRAM::RDWBehavior::NEW_DATA_MASKED_UNDEFINED);
	} else
		altsyncram->setupMixedPortRdw(ALTSYNCRAM::RDWBehavior::DONT_CARE);


	bool useInternalOutputRegister = false;

	if (readClock->getRegAttribs().resetActive != hlim::RegisterAttributes::Active::HIGH)
		useInternalOutputRegister = false; // Anyways aborting if they have a reset, but just so this case is not forgotten once we build in support for resets.

	// TODO: Also don't enable this if the InternalOutputRegister needs an enable
	useInternalOutputRegister = false;

	size_t externalOutputRegistersStart = 1;
	if (useInternalOutputRegister) 
		externalOutputRegistersStart++;

	if (memGrp->getWritePorts().size() > 0) {
		auto &wp = memGrp->getWritePorts().front();
		ALTSYNCRAM::PortSetup portSetup;
		portSetup.inputRegs = true;
		portSetup.dualClock = readClock != writeClock;
		altsyncram->setupPortA(wp.node->getBitWidth(), portSetup);

		UInt wrData = (UInt) getBVecBefore({.node = wp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::wrData});
		UInt addr = (UInt) getBVecBefore({.node = wp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::address});
		Bit wrEn = getBitBefore({.node = wp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::wrEnable}, '1');

		altsyncram->setInput(ALTSYNCRAM::Inputs::IN_DATA_A, (BVec) wrData);
		altsyncram->setInput(ALTSYNCRAM::Inputs::IN_ADDRESS_A, (BVec) addr);
		altsyncram->setInput(ALTSYNCRAM::Inputs::IN_WREN_A, wrEn);

		altsyncram->attachClock(writeClock, (size_t)ALTSYNCRAM::Clocks::CLK_0);

		{
			ALTSYNCRAM::PortSetup portSetup;
			portSetup.inputRegs = true;
			portSetup.outputRegs = (rp.dedicatedReadLatencyRegisters.size() > 1) && useInternalOutputRegister;
			portSetup.dualClock = readClock != writeClock;
			altsyncram->setupPortB(rp.node->getBitWidth(), portSetup);

			UInt addr = (UInt) getBVecBefore({.node = rp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::address});
			BVec data = hookBVecAfter(rp.dataOutput);

			altsyncram->setInput(ALTSYNCRAM::Inputs::IN_ADDRESS_B, (BVec) addr);
			if (readEnable.node != nullptr)
				altsyncram->setInput(ALTSYNCRAM::Inputs::IN_RDEN_B, Bit(SignalReadPort{readEnable}));

			BVec readData = altsyncram->getOutputBVec(ALTSYNCRAM::Outputs::OUT_Q_B);

			{
				Clock clock(readClock);
				ClockScope cscope(clock);
				for (auto i : utils::Range(externalOutputRegistersStart, rp.dedicatedReadLatencyRegisters.size())) 
					ENIF (getBitBefore({.node = rp.dedicatedReadLatencyRegisters[i], .port = (size_t)hlim::Node_Register::ENABLE}, '1'))
						if (useInternalOutputRegister)
							readData = reg(readData);
						else
							readData = reg(readData, zext(BVec(0))); // reset first register to prevent MnK merge. reset others to prevent ALM split.
			}
			data.exportOverride(readData);

			if (portSetup.dualClock)
				altsyncram->attachClock(readClock, (size_t)ALTSYNCRAM::Clocks::CLK_1);
		}
	} else {
		ALTSYNCRAM::PortSetup portSetup;
		portSetup.inputRegs = true;
		portSetup.outputRegs = (rp.dedicatedReadLatencyRegisters.size() > 1) && useInternalOutputRegister;
		altsyncram->setupPortA(rp.node->getBitWidth(), portSetup);

		UInt addr = (UInt) getBVecBefore({.node = rp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::address});
		BVec data = hookBVecAfter(rp.dataOutput);

		altsyncram->setInput(ALTSYNCRAM::Inputs::IN_ADDRESS_A, (BVec) addr);
		if (readEnable.node != nullptr)
			altsyncram->setInput(ALTSYNCRAM::Inputs::IN_RDEN_A, Bit(SignalReadPort{readEnable}));
		BVec readData = altsyncram->getOutputBVec(ALTSYNCRAM::Outputs::OUT_Q_A);
		{
			Clock clock(readClock);
			ClockScope cscope(clock);
			for (auto i : utils::Range(externalOutputRegistersStart, rp.dedicatedReadLatencyRegisters.size())) 
				ENIF (getBitBefore({.node = rp.dedicatedReadLatencyRegisters[i], .port = (size_t)hlim::Node_Register::ENABLE}, '1'))
					if (useInternalOutputRegister)
						readData = reg(readData);
					else
						readData = reg(readData, zext(BVec(0))); // reset first register to prevent MnK merge. reset others to prevent ALM split.
		}
		data.exportOverride(readData);

		altsyncram->attachClock(readClock, (size_t)ALTSYNCRAM::Clocks::CLK_0);	
	}
//DesignScope::get()->visualize(std::string("intelBRam")+std::to_string(__LINE__));

	return true;
}



}
