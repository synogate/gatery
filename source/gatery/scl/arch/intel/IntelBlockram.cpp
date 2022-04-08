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

#include "IntelBlockram.h"
#include "IntelDevice.h"

#include "ALTSYNCRAM.h"
#include <gatery/hlim/postprocessing/MemoryDetector.h>
#include <gatery/hlim/supportNodes/Node_MemPort.h>
#include <gatery/hlim/coreNodes/Node_Register.h>

namespace gtry::scl::arch::intel {

IntelBlockram::IntelBlockram(const IntelDevice &intelDevice) : m_intelDevice(intelDevice)
{
	m_desc.sizeCategory = MemoryCapabilities::SizeCategory::MEDIUM;
	m_desc.inputRegs = true;
	m_desc.outputRegs = 0;
}

bool IntelBlockram::apply(hlim::NodeGroup *nodeGroup) const
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

    hlim::Clock* writeClock = nullptr;
    if (memGrp->getWritePorts().size() > 0) {
        writeClock = memGrp->getWritePorts().front().node->getClocks()[0];
        if (writeClock->getTriggerEvent() != hlim::Clock::TriggerEvent::RISING) return false;
    }

    auto *readClock = rp.dedicatedReadLatencyRegisters.front()->getClocks()[0];
    if (readClock->getTriggerEvent() != hlim::Clock::TriggerEvent::RISING) return false;

    for (auto reg : rp.dedicatedReadLatencyRegisters) {
        if (reg->hasResetValue()) return false;
        if (reg->hasEnable()) return false;

        // For now, no true dual port, so only single clock
		if (memGrp->getWritePorts().size() > 0 && writeClock != reg->getClocks()[0]) return false;
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
    if (memGrp->getWritePorts().size() > 0) {
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

    if (readClock->getRegAttribs().resetActive != hlim::RegisterAttributes::Active::HIGH)
        useInternalOutputRegister = false; // Anyways aborting if they have a reset, but just so this case is not forgotten once we build in support for resets.

    size_t numExternalOutputRegisters = rp.dedicatedReadLatencyRegisters.size()-1;
    if (useInternalOutputRegister) 
        numExternalOutputRegisters--;

    if (memGrp->getWritePorts().size() > 0) {
        auto &wp = memGrp->getWritePorts().front();
        ALTSYNCRAM::PortSetup portSetup;
        portSetup.inputRegs = true;
        altsyncram->setupPortA(wp.node->getBitWidth(), portSetup);

        UInt wrData = getUIntBefore({.node = wp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::wrData});
        UInt addr = getUIntBefore({.node = wp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::address});
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

            UInt addr = getUIntBefore({.node = rp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::address});
            UInt data = hookUIntAfter(rp.dataOutput);

            altsyncram->connectInput(ALTSYNCRAM::Inputs::IN_ADDRESS_B, addr);

            UInt readData = altsyncram->getOutputUInt(ALTSYNCRAM::Outputs::OUT_Q_B);

            {
                Clock clock(readClock);
                ClockScope cscope(clock);
                for ([[maybe_unused]] auto i : utils::Range(numExternalOutputRegisters)) 
                    if (useInternalOutputRegister)
                        readData = reg(readData);
                    else
                        readData = reg(readData, 0); // reset first register to prevent MnK merge. reset others to prevent ALM split.
            }
            data.exportOverride(readData);

            altsyncram->attachClock(readClock, (size_t)ALTSYNCRAM::Clocks::CLK_0);
        }        
    } else {
        ALTSYNCRAM::PortSetup portSetup;
        portSetup.inputRegs = true;
        portSetup.outputRegs = (rp.dedicatedReadLatencyRegisters.size() > 1) && useInternalOutputRegister;
        altsyncram->setupPortA(rp.node->getBitWidth(), portSetup);

        UInt addr = getUIntBefore({.node = rp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::address});
        UInt data = hookUIntAfter(rp.dataOutput);

        altsyncram->connectInput(ALTSYNCRAM::Inputs::IN_ADDRESS_A, addr);
        UInt readData = altsyncram->getOutputUInt(ALTSYNCRAM::Outputs::OUT_Q_A);
        {
            Clock clock(readClock);
            ClockScope cscope(clock);
            for ([[maybe_unused]] auto i : utils::Range(numExternalOutputRegisters)) 
				if (useInternalOutputRegister)
					readData = reg(readData);
				else
					readData = reg(readData, 0); // reset first register to prevent MnK merge. reset others to prevent ALM split.
        }
        data.exportOverride(readData);

        altsyncram->attachClock(readClock, (size_t)ALTSYNCRAM::Clocks::CLK_0);    
    }

	return true;
}



}
