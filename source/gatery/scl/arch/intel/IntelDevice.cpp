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

#include "IntelDevice.h"

#include "../general/GenericMemory.h"

#include "M20K.h"
#include "MLAB.h"
#include "GLOBAL.h"

#include "ALTSYNCRAM.h"


#include <gatery/hlim/postprocessing/MemoryDetector.h>
#include <gatery/hlim/supportNodes/Node_MemPort.h>


namespace gtry::scl::arch::intel {



bool IntelMemoryPattern::scopedAttemptApply(hlim::NodeGroup *nodeGroup) const
{
	auto *memGrp = dynamic_cast<hlim::MemoryGroup*>(nodeGroup->getMetaInfo());
	if (memGrp == nullptr) return false;

	if (memGrp->getReadPorts().size() != 1) return false;
	if (memGrp->getWritePorts().size() > 1) return false;

    auto &rp = memGrp->getReadPorts().front();

    auto desc = buildM20KDesc(M20KVariants::Default);

    auto *altsyncram = DesignScope::createNode<ALTSYNCRAM>(memGrp->getMemory()->getSize());

    if (memGrp->getWritePorts().size() == 0)
        altsyncram->setupROM();
    else
        altsyncram->setupSimpleDualPort();

    //altsyncram->setupRamType(desc.memoryName);
    altsyncram->setupSimulationDeviceFamily(m_targetDevice.getFamily());
   
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



    if (memGrp->getWritePorts().size() > 0) {
        auto &wp = memGrp->getWritePorts().front();
        ALTSYNCRAM::PortSetup portSetup;
portSetup.rdw = ALTSYNCRAM::RDWBehavior::OLD_DATA;
        altsyncram->setupPortA(wp.node->getBitWidth(), portSetup);

        BVec wrData = hookBVecBefore({.node = wp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::wrData});
        BVec addr = hookBVecBefore({.node = wp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::address});
        Bit wrEn = hookBitBefore({.node = wp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::wrEnable});

        altsyncram->connectInput(ALTSYNCRAM::Inputs::IN_DATA_A, wrData);
        altsyncram->connectInput(ALTSYNCRAM::Inputs::IN_ADDRESS_A, addr);
        altsyncram->connectInput(ALTSYNCRAM::Inputs::IN_WREN_A, wrEn);

        altsyncram->attachClock(wp.node->getClocks()[0], (size_t)ALTSYNCRAM::Clocks::CLK_0);

        {
            ALTSYNCRAM::PortSetup portSetup;
portSetup.rdw = ALTSYNCRAM::RDWBehavior::OLD_DATA;
            portSetup.outputRegs = rp.dedicatedReadLatencyRegisters.size() > 1;
            altsyncram->setupPortB(rp.node->getBitWidth(), portSetup);

            BVec addr = hookBVecBefore({.node = rp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::address});
            BVec data = hookBVecAfter(rp.dataOutput);

            altsyncram->connectInput(ALTSYNCRAM::Inputs::IN_ADDRESS_B, addr);
            data.setExportOverride(altsyncram->getOutputBVec(ALTSYNCRAM::Outputs::OUT_Q_B));

            altsyncram->attachClock(rp.dedicatedReadLatencyRegisters.front()->getClocks()[0], (size_t)ALTSYNCRAM::Clocks::CLK_1);
        }        
    } else {
        ALTSYNCRAM::PortSetup portSetup;
portSetup.rdw = ALTSYNCRAM::RDWBehavior::OLD_DATA;
        portSetup.outputRegs = rp.dedicatedReadLatencyRegisters.size() > 1;
        altsyncram->setupPortA(rp.node->getBitWidth(), portSetup);

        BVec addr = hookBVecBefore({.node = rp.node.get(), .port = (size_t)hlim::Node_MemPort::Inputs::address});
        BVec data = hookBVecAfter(rp.dataOutput);

        altsyncram->connectInput(ALTSYNCRAM::Inputs::IN_ADDRESS_A, addr);
        data.setExportOverride(altsyncram->getOutputBVec(ALTSYNCRAM::Outputs::OUT_Q_A));

        altsyncram->attachClock(rp.dedicatedReadLatencyRegisters.front()->getClocks()[0], (size_t)ALTSYNCRAM::Clocks::CLK_0);    
    }

	return true;
}



void IntelDevice::setupArria10(std::string device)
{
	m_vendor = "intel";
	//m_family = "Arria 10";
    m_family = "MAX 10";
	m_device = std::move(device);

	m_embeddedMemories.reserve(6);

	m_embeddedMemories.push_back(buildMLABDesc(MLABVariants::Default, false));
	m_embeddedMemories.push_back(buildMLABDesc(MLABVariants::Default, true));

	m_embeddedMemories.push_back(buildM20KDesc(M20KVariants::Arria10_SDP_wBE));
	m_embeddedMemories.push_back(buildM20KDesc(M20KVariants::Arria10_SDP_woBE));
	m_embeddedMemories.push_back(buildM20KDesc(M20KVariants::Arria10_TDP_wBE));
	m_embeddedMemories.push_back(buildM20KDesc(M20KVariants::Arria10_TDP_woBE));

	m_memoryCapabilities = std::make_unique<GenericMemoryCapabilities>(*this); // Instantiate overload, if s.th. specific needed
	m_technologyMapping.addPattern(std::make_unique<IntelMemoryPattern>(*this));
    m_technologyMapping.addPattern(std::make_unique<GLOBALPattern>());
}


}